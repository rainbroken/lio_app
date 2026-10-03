#include "pointcloudprocessing.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <cstdio>

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  CloudOperation op;
  QVector<CloudPoint> small{{0,0,0,1,0,0},{0.01f,0,0,0,1,0},{0,0.01f,0,0,0,1},{8,8,8,1,1,1}};
  op.kind = CloudOperation::DeleteRegion; op.min=QVector3D(7,7,7); op.max=QVector3D(9,9,9);
  auto deleted = processCloud(small,op);
  if (!deleted.ok() || deleted.points.size()!=3) return 1;
  op.kind = CloudOperation::RadiusOutlier; op.radius=0.02f; op.minNeighbors=2;
  auto filtered=processCloud(small,op);
  if (!filtered.ok() || filtered.points.size()!=3) return 2;
  op.kind=CloudOperation::VoxelDownsample; op.voxel=0.1f;
  auto voxel=processCloud(small,op);
  if (!voxel.ok() || voxel.points.size()!=2 || std::abs(voxel.points[0].r-1.0f/3)>1e-6) return 3;
  QVector<CloudPoint> plane;
  for (int x=-10; x<=10; ++x) for (int y=-10; y<=10; ++y)
    plane.append({float(x),float(y),float(2+0.1*x+0.05*y),0.2f,0.4f,0.6f});
  op.kind=CloudOperation::LevelGround; op.groundTolerance=0.02f;
  auto leveled=processCloud(plane,op);
  if (!leveled.ok() || leveled.points.size()!=plane.size()) { std::fprintf(stderr,"ground: %s\n",qPrintable(leveled.error)); return 4; }
  for (const auto &p : leveled.points) if (std::abs(p.z)>0.001f) return 5;
  if (argc > 1) {
    PointCloudData source;
    QString error;
    QElapsedTimer timer; timer.start();
    if (!source.load(QString::fromLocal8Bit(argv[1]),&error)) { std::fprintf(stderr,"load: %s\n",qPrintable(error)); return 6; }
    const qint64 loadMs=timer.restart();
    const QString mode = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QStringLiteral("voxel");
    if (mode == QStringLiteral("stats")) {
      QVector<float> z;
      for (qsizetype i=0;i<source.points().size();i+=100) z.append(source.points()[i].z);
      std::sort(z.begin(),z.end());
      std::printf("bounds x[%f,%f] y[%f,%f] z[%f,%f]; z quantiles 5%% %.3f 20%% %.3f 50%% %.3f 80%% %.3f\n",
          source.minBound().x(),source.maxBound().x(),source.minBound().y(),source.maxBound().y(),source.minBound().z(),source.maxBound().z(),
          z[int(z.size()*0.05)],z[int(z.size()*0.20)],z[int(z.size()*0.50)],z[int(z.size()*0.80)]);
      return 0;
    }
    if (mode == QStringLiteral("radius")) { op.kind=CloudOperation::RadiusOutlier; op.radius=0.05f; op.minNeighbors=3; }
    else if (mode == QStringLiteral("ground")) { op.kind=CloudOperation::LevelGround; op.groundTolerance=0.05f; }
    else if (mode == QStringLiteral("ground_region")) { op.kind=CloudOperation::LevelGround; op.groundTolerance=0.05f; op.useFitRegion=true; op.min=QVector3D(source.minBound().x(),source.minBound().y(),-13.0f); op.max=QVector3D(source.maxBound().x(),source.maxBound().y(),-11.0f); }
    else { op.kind=CloudOperation::VoxelDownsample; op.voxel=0.03f; }
    auto result=processCloud(source.points(),op);
    const qint64 processMs=timer.restart();
    if (!result.ok()) { std::fprintf(stderr,"process: %s\n",qPrintable(result.error)); return 7; }
    if (result.points.isEmpty() || (mode != QStringLiteral("ground") && mode != QStringLiteral("ground_region") && result.points.size()>=source.points().size())) return 7;
    PointCloudData output; output.setPoints(result.points);
    QTemporaryDir dir;
    const QString path=dir.filePath(QStringLiteral("processed.pcd"));
    if (!output.exportPcd(path,&error)) return 8;
    PointCloudData reopened;
    if (!reopened.load(path,&error)) return 9;
    const qint64 roundtripMs=timer.elapsed();
    if (reopened.points().size()!=result.points.size()) return 10;
    for (qsizetype i=0;i<result.points.size();++i) {
      const auto &a=result.points[i], &b=reopened.points()[i];
      if (a.x!=b.x || a.y!=b.y || a.z!=b.z) return 11;
      auto channel = [](float value) { return std::clamp(int(std::lround(value*255)),0,255); };
      if (channel(a.r)!=channel(b.r) || channel(a.g)!=channel(b.g) || channel(a.b)!=channel(b.b)) return 12;
    }
    std::printf("real %s: %lld -> %lld points, load %lld ms, process %lld ms, export+reload %lld ms; %s\n", qPrintable(mode),
      static_cast<long long>(source.points().size()),static_cast<long long>(result.points.size()),
      static_cast<long long>(loadMs),static_cast<long long>(processMs),static_cast<long long>(roundtripMs),qPrintable(result.detail));
  }
  return 0;
}
