#pragma once

#include "cloudview.h"
#include "pointclouddata.h"
#include "rangeslider.h"
#include <QMainWindow>
#include <QVector3D>

class QLabel;
class QSlider;
class QCheckBox;
class QDoubleSpinBox;
class QTreeWidget;
class QTreeWidgetItem;

class MainWindow : public QMainWindow {
  Q_OBJECT
public:
  explicit MainWindow(QWidget *parent = nullptr);
  bool loadFilePath(const QString &path);

private slots:
  void openFile();
  void chooseMeasureMode();
  void receivePoint(const QVector3D &point);
  void receivePreview(const QVector3D &point);
  void cancelMeasurement();
  void exportPly();
  void exportPcd();
  void exportCsv();
  void saveScreenshot();
  void updateDisplayMode();
  void updateCropRange();
  void toggleFullscreen();

private:
  void buildUi();
  void updateCloudLabels();
  void updateMeasureLabels();
  void setStatus(const QString &message);

  PointCloudData data_;
  CloudView *view_ = nullptr;
  QLabel *pointLabel_ = nullptr;
  QLabel *boundsLabel_ = nullptr;
  QCheckBox *showAllCheck_ = nullptr;
  QVector<RangeSlider*> cropSliders_;
  QVector<QLabel*> cropMinLabels_, cropMaxLabels_;
  QVector3D cropMin_, cropMax_;
  QLabel *measureLabel_ = nullptr;
  QLabel *toolLabel_ = nullptr;
  QTreeWidget *fileTree_ = nullptr;
  QTreeWidgetItem *selectedPcdItem_ = nullptr;
  int cropSequence_ = 1;
  QVector<QVector3D> measurePoints_;
};
