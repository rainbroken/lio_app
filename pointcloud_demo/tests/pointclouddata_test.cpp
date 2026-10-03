#include "pointclouddata.h"

#include <QCoreApplication>
#include <QTemporaryDir>
#include <cstdio>

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QTemporaryDir dir;
  if (!dir.isValid()) return 1;
  const QVector<CloudPoint> expected{
      {1.25f, -2.5f, 3.75f, 1.0f, 0.0f, 0.0f},
      {-12.125f, 0.0f, 100.5f, 1.0f / 255.0f, 127.0f / 255.0f, 1.0f},
      {0.0f, 0.001f, -7.0f, 0.0f, 1.0f, 0.0f}};
  PointCloudData source;
  QString error;
  if (argc > 1) {
    if (!source.load(QString::fromLocal8Bit(argv[1]), &error)) { std::fprintf(stderr, "source: %s\n", qPrintable(error)); return 7; }
  } else source.setPoints(expected);
  const QString path = dir.filePath(QStringLiteral("roundtrip.pcd"));
  if (!source.exportPcd(path, &error)) { std::fprintf(stderr, "export: %s\n", qPrintable(error)); return 2; }
  PointCloudData loaded;
  if (!loaded.load(path, &error)) { std::fprintf(stderr, "load: %s\n", qPrintable(error)); return 3; }
  if (loaded.points().size() != source.points().size()) return 4;
  for (int i = 0; i < source.points().size(); ++i) {
    const auto &a = source.points()[i], &b = loaded.points()[i];
    if (a.x != b.x || a.y != b.y || a.z != b.z) return 5;
    if (a.r != b.r || a.g != b.g || a.b != b.b) return 6;
  }
  std::fprintf(stdout, "verified %lld points: coordinates and RGB exact\n",
               static_cast<long long>(loaded.points().size()));
  return 0;
}
