#pragma once

#include "pointclouddata.h"
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWidget>
#include <QPoint>
#include <QVector3D>

class CloudView : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
  Q_OBJECT
public:
  explicit CloudView(QWidget *parent = nullptr);
  void setPoints(const QVector<CloudPoint> &points, bool resetView = false);
  qsizetype pointCount() const { return points_.size(); }
  void setBounds(const QVector3D &minBound, const QVector3D &maxBound);
  void setCropRange(const QVector3D &minRange, const QVector3D &maxRange);
  QVector3D projectPoint(const QVector3D &p) const;
  void setPointSize(float size);
  void fitView();
  void setMeasureMode(bool enabled);
  void setCloudVisible(bool visible) { cloudVisible_ = visible; update(); }
  bool measureMode() const { return measureMode_; }

signals:
  void pointPicked(const QVector3D &point);
  void measurementPreview(const QVector3D &point);
  void measurementCanceled();
  void fpsChanged(int fps);

protected:
  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private:
  void upload();
  bool pickPoint(const QPoint &screen, QVector3D *point) const;
  QVector<CloudPoint> points_;
  QVector3D minBound_{-1, -1, -1}, maxBound_{1, 1, 1};
  QVector3D center_;
  float radius_ = 1.0f;
  QVector3D cropMin_{-1e9,-1e9,-1e9}, cropMax_{1e9,1e9,1e9};
  float pointSize_ = 2.0f;
  float yaw_ = 225.0f, pitch_ = 28.0f, distance_ = 5.0f;
  QPoint lastMouse_;
  Qt::MouseButton dragButton_ = Qt::NoButton;
  bool measureMode_ = false;
  bool cloudVisible_ = true;
  bool hasMeasureAnchor_ = false;
  bool hasMeasurePreview_ = false;
  QVector3D measureAnchor_;
  QVector3D measurePreview_;
  unsigned int vao_ = 0, vbo_ = 0, shader_ = 0;
  int uMvp_ = -1, uPointSize_ = -1, uCropMin_ = -1, uCropMax_ = -1;
};
