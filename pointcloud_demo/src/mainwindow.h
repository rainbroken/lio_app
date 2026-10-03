#pragma once

#include "cloudview.h"
#include "pointclouddata.h"
#include "pointcloudprocessing.h"
#include "rangeslider.h"
#include <QMainWindow>
#include <QVector3D>
#include <QHash>
#include <QMatrix4x4>

class QLabel;
class QSlider;
class QCheckBox;
class QDoubleSpinBox;
class QTreeWidget;
class QTreeWidgetItem;
class QAction;
class QPushButton;
class QComboBox;
class QSpinBox;
class QLineEdit;
template<class T> class QFutureWatcher;

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
  void rotateCloud();
  void undo();
  void redo();
  void previewProcessing();
  void applyProcessing();
  void clearProcessingPreview();

private:
  struct History {
    QVector<CloudPoint> original;
    QVector<CloudOperation> operations;
    int position = 0;
  };
  void buildUi();
  void updateCloudLabels();
  void updateMeasureLabels();
  void setStatus(const QString &message);
  void showCurrentCloud(bool resetView = false);
  void restoreHistory();
  void applyOperation(const CloudOperation &operation);
  PointCloudData previewData() const;
  bool hasPreview() const;

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
  QTreeWidgetItem *activePcdItem_ = nullptr;
  QHash<QTreeWidgetItem*, History> histories_;
  QAction *undoAction_ = nullptr;
  QAction *redoAction_ = nullptr;
  QLabel *previewLabel_ = nullptr;
  QComboBox *processMode_ = nullptr;
  QDoubleSpinBox *radiusInput_ = nullptr;
  QDoubleSpinBox *voxelInput_ = nullptr;
  QDoubleSpinBox *groundInput_ = nullptr;
  QSpinBox *neighborsInput_ = nullptr;
  QLabel *processInfo_ = nullptr;
  QPushButton *processPreviewButton_ = nullptr;
  QPushButton *processApplyButton_ = nullptr;
  QFutureWatcher<ProcessingResult> *processWatcher_ = nullptr;
  ProcessingResult processPreview_;
  bool processPreviewActive_ = false;
  int processGeneration_ = 0;
  QLineEdit *frameEdit_ = nullptr;
  QComboBox *unitBox_ = nullptr;
  QTreeWidget *measurementTree_ = nullptr;
  struct Measurement { QVector3D a, b; QString frame, unit, source; };
  QVector<Measurement> measurements_;
  QVector<QVector3D> measurePoints_;
};
