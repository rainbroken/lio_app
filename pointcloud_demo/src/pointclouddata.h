#pragma once

#include <QVector>
#include <QString>
#include <QVector3D>

struct CloudPoint {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float r = 0.75f;
  float g = 0.85f;
  float b = 1.0f;
};

class PointCloudData {
public:
  bool load(const QString &path, QString *error = nullptr);
  void setPoints(const QVector<CloudPoint> &points);
  bool exportPcd(const QString &path, double zMin, double zMax, QString *error = nullptr) const;
  bool exportPly(const QString &path, double zMin, double zMax, QString *error = nullptr) const;

  const QVector<CloudPoint> &points() const { return points_; }
  QVector<CloudPoint> displaySample(int maxPoints = 1500000) const;
  QVector3D minBound() const { return minBound_; }
  QVector3D maxBound() const { return maxBound_; }
  QString sourcePath() const { return sourcePath_; }

private:
  QVector<CloudPoint> points_;
  QVector3D minBound_;
  QVector3D maxBound_;
  QString sourcePath_;
};
