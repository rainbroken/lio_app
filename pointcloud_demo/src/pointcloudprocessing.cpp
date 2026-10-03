#include "pointcloudprocessing.h"

#include <Eigen/Dense>
#include <nanoflann.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <unordered_map>

namespace {
bool inside(const CloudPoint &p, const QVector3D &lo, const QVector3D &hi) {
  return p.x >= lo.x() && p.x <= hi.x() && p.y >= lo.y() && p.y <= hi.y() && p.z >= lo.z() && p.z <= hi.z();
}

struct Key {
  std::int64_t x, y, z;
  bool operator==(const Key &other) const { return x == other.x && y == other.y && z == other.z; }
};
struct KeyHash {
  std::size_t operator()(const Key &key) const {
    std::size_t hash = std::hash<std::int64_t>{}(key.x);
    hash ^= std::hash<std::int64_t>{}(key.y) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    hash ^= std::hash<std::int64_t>{}(key.z) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    return hash;
  }
};

struct PointAdaptor {
  const QVector<CloudPoint> &points;
  std::size_t kdtree_get_point_count() const { return std::size_t(points.size()); }
  float kdtree_get_pt(std::size_t i, std::size_t dim) const {
    const auto &p = points[qsizetype(i)]; return dim == 0 ? p.x : dim == 1 ? p.y : p.z;
  }
  template<class BBOX> bool kdtree_get_bbox(BBOX &) const { return false; }
};

Eigen::Vector3d vector(const CloudPoint &p) { return {p.x, p.y, p.z}; }

bool fitGround(const QVector<CloudPoint> &points, float tolerance, QMatrix4x4 *matrix, QString *detail) {
  if (points.size() < 30 || tolerance <= 0) return false;
  QVector<int> sample;
  const qsizetype step = std::max<qsizetype>(1, points.size() / 100000);
  for (qsizetype i = 0; i < points.size(); i += step) sample.append(int(i));
  std::sort(sample.begin(), sample.end(), [&](int a, int b) { return points[a].z < points[b].z; });
  sample.resize(std::max(30, int(sample.size() * 0.4)));
  std::mt19937 rng(0x5343414e);
  std::uniform_int_distribution<int> pick(0, sample.size() - 1);
  Eigen::Vector3d bestNormal(0, 0, 1);
  double bestD = 0;
  int bestCount = 0;
  for (int trial = 0; trial < 1000; ++trial) {
    const int ia = pick(rng), ib = pick(rng), ic = pick(rng);
    if (ia == ib || ia == ic || ib == ic) continue;
    const Eigen::Vector3d a = vector(points[sample[ia]]);
    Eigen::Vector3d n = (vector(points[sample[ib]]) - a).cross(vector(points[sample[ic]]) - a);
    const double length = n.norm();
    if (length < 1e-8) continue;
    n /= length;
    if (n.z() < 0) n = -n;
    if (n.z() < 0.7) continue;
    const double d = -n.dot(a);
    int count = 0;
    for (int index : sample) if (std::abs(n.dot(vector(points[index])) + d) <= tolerance) ++count;
    if (count > bestCount) { bestCount = count; bestNormal = n; bestD = d; }
  }
  if (bestCount < std::max(30, int(sample.size() * 0.15))) return false;
  Eigen::Vector3d mean = Eigen::Vector3d::Zero();
  int count = 0;
  for (int index : sample) if (std::abs(bestNormal.dot(vector(points[index])) + bestD) <= tolerance) { mean += vector(points[index]); ++count; }
  mean /= count;
  Eigen::Matrix3d covariance = Eigen::Matrix3d::Zero();
  for (int index : sample) {
    const Eigen::Vector3d p = vector(points[index]);
    if (std::abs(bestNormal.dot(p) + bestD) <= tolerance) covariance += (p - mean) * (p - mean).transpose();
  }
  Eigen::Vector3d normal = Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d>(covariance).eigenvectors().col(0);
  if (normal.z() < 0) normal = -normal;
  if (normal.z() < 0.7) return false;
  const double d = -normal.dot(mean);
  double squared = 0;
  int refined = 0;
  for (int index : sample) {
    const double residual = normal.dot(vector(points[index])) + d;
    if (std::abs(residual) <= tolerance) { squared += residual * residual; ++refined; }
  }
  if (refined < std::max(30, int(sample.size() * 0.15))) return false;
  const double tilt = std::acos(std::clamp(normal.z(), -1.0, 1.0)) * 180.0 / M_PI;
  const double rms = std::sqrt(squared / refined);
  const QVector3D axis = QVector3D::crossProduct(QVector3D(float(normal.x()), float(normal.y()), float(normal.z())), QVector3D(0, 0, 1));
  matrix->setToIdentity();
  matrix->translate(0, 0, float(d));
  if (axis.length() > 1e-8f) matrix->rotate(float(tilt), axis.normalized());
  *detail = QStringLiteral("地面候选 %1，内点 %2 (%3%)，RMS %4，倾角 %5°；拟合面移至 Z=0")
      .arg(sample.size()).arg(refined).arg(100.0 * refined / sample.size(), 0, 'f', 1)
      .arg(rms, 0, 'f', 4).arg(tilt, 0, 'f', 3);
  return true;
}
}

ProcessingResult processCloud(const QVector<CloudPoint> &source, const CloudOperation &operation) {
  ProcessingResult result;
  result.operation = operation;
  if (source.isEmpty()) { result.error = QStringLiteral("当前点云为空"); return result; }
  if (operation.kind == CloudOperation::Crop || operation.kind == CloudOperation::DeleteRegion) {
    result.points.reserve(source.size());
    for (const auto &p : source) {
      const bool hit = inside(p, operation.min, operation.max);
      if (hit == (operation.kind == CloudOperation::Crop)) result.points.append(p);
    }
  } else if (operation.kind == CloudOperation::VoxelDownsample) {
    if (!std::isfinite(operation.voxel) || operation.voxel <= 0) { result.error = QStringLiteral("体素边长必须大于 0"); return result; }
    struct Sum { double x=0,y=0,z=0,r=0,g=0,b=0; int n=0; };
    std::unordered_map<Key, std::size_t, KeyHash> index;
    std::vector<Sum> sums;
    index.reserve(std::size_t(std::min<qsizetype>(source.size(), 2000000)));
    for (const auto &p : source) {
      const Key key{std::int64_t(std::floor(double(p.x)/operation.voxel)), std::int64_t(std::floor(double(p.y)/operation.voxel)), std::int64_t(std::floor(double(p.z)/operation.voxel))};
      const auto found = index.try_emplace(key, sums.size());
      if (found.second) sums.emplace_back();
      auto &s = sums[found.first->second];
      s.x+=p.x; s.y+=p.y; s.z+=p.z; s.r+=p.r; s.g+=p.g; s.b+=p.b; ++s.n;
    }
    result.points.reserve(qsizetype(sums.size()));
    for (const auto &s : sums) result.points.append({float(s.x/s.n),float(s.y/s.n),float(s.z/s.n),float(s.r/s.n),float(s.g/s.n),float(s.b/s.n)});
  } else if (operation.kind == CloudOperation::RadiusOutlier) {
    if (!std::isfinite(operation.radius) || operation.radius <= 0 || operation.minNeighbors < 1) { result.error = QStringLiteral("半径和最少邻点数必须大于 0"); return result; }
    PointAdaptor adaptor{source};
    using Tree = nanoflann::KDTreeSingleIndexAdaptor<nanoflann::L2_Simple_Adaptor<float, PointAdaptor>, PointAdaptor, 3, std::size_t>;
    Tree tree(3, adaptor, {16}); tree.buildIndex();
    const std::size_t k = std::size_t(operation.minNeighbors) + 1;
    std::vector<std::size_t> indices(k);
    std::vector<float> distances(k);
    result.points.reserve(source.size());
    for (const auto &p : source) {
      const float query[3]{p.x,p.y,p.z};
      const std::size_t found = tree.knnSearch(query, k, indices.data(), distances.data());
      if (found == k && distances[k-1] <= operation.radius * operation.radius) result.points.append(p);
    }
  } else {
    QMatrix4x4 matrix = operation.matrix;
    if (operation.kind == CloudOperation::LevelGround && !operation.fitted) {
      QVector<CloudPoint> fitPoints;
      if (operation.useFitRegion) for (const auto &p : source) if (inside(p, operation.min, operation.max)) fitPoints.append(p);
      const auto &candidates = operation.useFitRegion ? fitPoints : source;
      if (!fitGround(candidates, operation.groundTolerance, &matrix, &result.detail)) { result.error = QStringLiteral("未找到可信地面平面；请用 XYZ 范围框选地面或调整容差"); return result; }
      result.operation.matrix = matrix;
      result.operation.fitted = true;
    }
    result.points = source;
    for (auto &p : result.points) { const QVector3D v = matrix.map(QVector3D(p.x,p.y,p.z)); p.x=v.x(); p.y=v.y(); p.z=v.z(); }
  }
  if (result.points.isEmpty()) result.error = QStringLiteral("处理后没有点；请调整参数");
  return result;
}
