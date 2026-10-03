#include "pointclouddata.h"

#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <vector>

void PointCloudData::setPoints(const QVector<CloudPoint> &points) {
  points_ = points;
  minBound_ = QVector3D(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
  maxBound_ = QVector3D(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
  for (const auto &p : points_) {
    minBound_.setX(std::min(minBound_.x(), p.x)); minBound_.setY(std::min(minBound_.y(), p.y)); minBound_.setZ(std::min(minBound_.z(), p.z));
    maxBound_.setX(std::max(maxBound_.x(), p.x)); maxBound_.setY(std::max(maxBound_.y(), p.y)); maxBound_.setZ(std::max(maxBound_.z(), p.z));
  }
  if (points_.isEmpty()) minBound_ = maxBound_ = QVector3D();
  sourcePath_.clear();
}

bool PointCloudData::load(const QString &path, QString *error) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) { if (error) *error = file.errorString(); return false; }
  QByteArray line; QByteArray data;
  std::vector<QString> fields; std::vector<int> sizes, counts; std::vector<QChar> types;
  int points = 0; bool binary = false;
  while (!(line = file.readLine()).isEmpty()) {
    const QByteArray trimmed = line.trimmed();
    if (trimmed.startsWith("FIELDS")) { for (const auto &v : trimmed.mid(6).split(' ')) if (!v.isEmpty()) fields.push_back(QString::fromLatin1(v)); }
    else if (trimmed.startsWith("SIZE")) { for (const auto &v : trimmed.mid(4).split(' ')) if (!v.isEmpty()) sizes.push_back(v.toInt()); }
    else if (trimmed.startsWith("TYPE")) { for (const auto &v : trimmed.mid(4).split(' ')) if (!v.isEmpty()) types.push_back(QChar(v.at(0))); }
    else if (trimmed.startsWith("COUNT")) { for (const auto &v : trimmed.mid(5).split(' ')) if (!v.isEmpty()) counts.push_back(v.toInt()); }
    else if (trimmed.startsWith("POINTS")) points = trimmed.mid(6).trimmed().toInt();
    else if (trimmed.startsWith("DATA")) { binary = trimmed.mid(4).trimmed() == "binary"; break; }
  }
  if (fields.empty() || points <= 0 || !binary) { if (error) *error = QStringLiteral("仅支持包含 FIELDS/POINTS/DATA binary 的 PCD"); return false; }
  if (counts.empty()) counts.assign(fields.size(), 1);
  if (sizes.size() != fields.size() || types.size() != fields.size() || counts.size() != fields.size()) { if (error) *error = QStringLiteral("PCD 字段定义不完整"); return false; }
  std::vector<int> offsets(fields.size(), 0); int stride = 0;
  for (size_t i=0; i<fields.size(); ++i) { if (sizes[i] <= 0 || counts[i] <= 0 || sizes[i] > 8 || counts[i] > 1024) { if (error) *error = QStringLiteral("PCD 字段大小无效"); return false; } offsets[i] = stride; stride += sizes[i] * counts[i]; }
  auto fieldIndex = [&fields](const QString &name) { for (size_t i=0;i<fields.size();++i) if (fields[i] == name) return int(i); return -1; };
  const int ix=fieldIndex("x"), iy=fieldIndex("y"), iz=fieldIndex("z"), irgb=fieldIndex("rgb");
  if (ix < 0 || iy < 0 || iz < 0) { if (error) *error = QStringLiteral("PCD 缺少 x/y/z 字段"); return false; }
  for (int index : {ix, iy, iz}) if (types[index] != 'F' || sizes[index] != 4 || counts[index] != 1) { if (error) *error = QStringLiteral("PCD x/y/z 必须为 float32"); return false; }
  if (irgb >= 0 && (sizes[irgb] != 4 || counts[irgb] != 1)) { if (error) *error = QStringLiteral("PCD rgb 必须为 4 字节"); return false; }
  data = file.readAll(); if (data.size() < qint64(points) * stride) { if (error) *error = QStringLiteral("PCD 二进制数据不完整"); return false; }
  auto number = [&](const char *ptr, int index) -> float { if (types[index] == 'F' && sizes[index] == 4) { float v; std::memcpy(&v, ptr, 4); return v; } if (types[index] == 'U' && sizes[index] == 4) { quint32 v; std::memcpy(&v,ptr,4); return float(v); } return 0.0f; };
  points_.clear();
  points_.reserve(points);
  minBound_ = QVector3D( std::numeric_limits<float>::max(),  std::numeric_limits<float>::max(),  std::numeric_limits<float>::max());
  maxBound_ = QVector3D(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
  for (int i=0; i<points; ++i) {
    const char *base = data.constData() + qint64(i) * stride; const float x=number(base+offsets[ix],ix), y=number(base+offsets[iy],iy), z=number(base+offsets[iz],iz);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) continue;
    float r=.75f,g=.85f,b=1.0f;
    if (irgb >= 0) { quint32 packed=0; std::memcpy(&packed,base+offsets[irgb],4); r=((packed>>16)&255)/255.f; g=((packed>>8)&255)/255.f; b=(packed&255)/255.f; }
    points_.append({x,y,z,r,g,b});
    minBound_.setX(std::min(minBound_.x(), x)); minBound_.setY(std::min(minBound_.y(), y)); minBound_.setZ(std::min(minBound_.z(), z));
    maxBound_.setX(std::max(maxBound_.x(), x)); maxBound_.setY(std::max(maxBound_.y(), y)); maxBound_.setZ(std::max(maxBound_.z(), z));
  }
  sourcePath_ = path;
  return !points_.isEmpty();
}

QVector<CloudPoint> PointCloudData::displaySample(int maxPoints) const {
  QVector<CloudPoint> result;
  if (points_.isEmpty()) return result;
  const int step = std::max(1, static_cast<int>((points_.size() + maxPoints - 1) / maxPoints));
  result.reserve((points_.size() + step - 1) / step);
  for (int i = 0; i < points_.size(); i += step) result.append(points_[i]);
  return result;
}

bool PointCloudData::exportPly(const QString &path, QString *error) const {
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { if (error) *error = file.errorString(); return false; }
  QTextStream out(&file);
  out << "ply\nformat ascii 1.0\nelement vertex " << points_.size() << "\n"
      << "property float x\nproperty float y\nproperty float z\n"
      << "property uchar red\nproperty uchar green\nproperty uchar blue\nend_header\n";
  out.setRealNumberNotation(QTextStream::SmartNotation); out.setRealNumberPrecision(9);
  auto color = [](float value) { return std::clamp(int(std::lround(value * 255.0f)), 0, 255); };
  for (const auto &p : points_)
    out << p.x << ' ' << p.y << ' ' << p.z << ' ' << color(p.r) << ' ' << color(p.g) << ' ' << color(p.b) << '\n';
  out.flush();
  if (out.status() != QTextStream::Ok || !file.commit()) { if (error) *error = file.errorString(); return false; }
  return true;
}

bool PointCloudData::exportPcd(const QString &path, QString *error) const {
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) { if (error) *error = file.errorString(); return false; }
  const QByteArray header = QByteArray("# ScanForge PCD\nVERSION 0.7\nFIELDS x y z rgb\nSIZE 4 4 4 4\nTYPE F F F U\nCOUNT 1 1 1 1\nWIDTH ")
      + QByteArray::number(points_.size()) + "\nHEIGHT 1\nPOINTS " + QByteArray::number(points_.size()) + "\nDATA binary\n";
  if (file.write(header) != header.size()) { if (error) *error = file.errorString(); return false; }
  auto color = [](float value) { return quint32(std::clamp(int(std::lround(value * 255.0f)), 0, 255)); };
  QByteArray block; block.reserve(16 * 4096);
  for (const auto &p : points_) {
    for (float value : {p.x, p.y, p.z}) {
      quint32 bits; std::memcpy(&bits, &value, 4);
      bits = qToLittleEndian(bits); block.append(reinterpret_cast<const char *>(&bits), 4);
    }
    quint32 rgb = qToLittleEndian((color(p.r) << 16) | (color(p.g) << 8) | color(p.b));
    block.append(reinterpret_cast<const char *>(&rgb), 4);
    if (block.size() >= 16 * 4096) {
      if (file.write(block) != block.size()) { if (error) *error = file.errorString(); return false; }
      block.clear();
    }
  }
  if ((!block.isEmpty() && file.write(block) != block.size()) || !file.commit()) { if (error) *error = file.errorString(); return false; }
  return true;
}
