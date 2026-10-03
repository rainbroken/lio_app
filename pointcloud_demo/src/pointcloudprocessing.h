#pragma once

#include "pointclouddata.h"
#include <QMatrix4x4>

struct CloudOperation {
  enum Kind { Crop, Transform, DeleteRegion, RadiusOutlier, VoxelDownsample, LevelGround } kind = Crop;
  QVector3D min, max;
  QMatrix4x4 matrix;
  float radius = 0.05f;
  int minNeighbors = 3;
  float voxel = 0.02f;
  float groundTolerance = 0.03f;
  bool fitted = false;
  bool useFitRegion = false;
};

struct ProcessingResult {
  QVector<CloudPoint> points;
  CloudOperation operation;
  QString detail;
  QString error;
  bool ok() const { return error.isEmpty(); }
};

ProcessingResult processCloud(const QVector<CloudPoint> &source, const CloudOperation &operation);
