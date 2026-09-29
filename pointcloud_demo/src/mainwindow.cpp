#include "mainwindow.h"

#include <QApplication>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMenuBar>
#include <QKeySequence>
#include <QPushButton>
#include <QSlider>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QRegularExpression>
#include <QtMath>

namespace {
QLabel *sectionTitle(const QString &text) {
  auto *label = new QLabel(text.toUpper());
  label->setObjectName("sectionTitle");
  return label;
}
QPushButton *button(const QString &text, const QString &objectName = {}) {
  auto *b = new QPushButton(text); b->setObjectName(objectName); return b;
}
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint);
  setAttribute(Qt::WA_DeleteOnClose, false);
  setWindowTitle(QStringLiteral("ScanForge · 点云后处理 Demo"));
  resize(1440, 900);
  buildUi();
  auto *fileMenu = menuBar()->addMenu(QStringLiteral("文件")); fileMenu->addAction(QStringLiteral("打开 PCD"), this, &MainWindow::openFile); fileMenu->addAction(QStringLiteral("退出"), this, &QWidget::close);
  auto *viewMenu = menuBar()->addMenu(QStringLiteral("视图")); viewMenu->addAction(QStringLiteral("适配视图"), view_, &CloudView::fitView); auto *full = viewMenu->addAction(QStringLiteral("全屏"), this, &MainWindow::toggleFullscreen); full->setShortcut(QKeySequence(Qt::Key_F11));
  auto *helpMenu = menuBar()->addMenu(QStringLiteral("帮助"));
  helpMenu->addAction(QStringLiteral("操作说明"), this, [this]{ QMessageBox::information(this, QStringLiteral("操作说明"), QStringLiteral("左键拖动：旋转\n中键拖动：平移\n右键上下拖动：缩放\n滚轮：缩放\n双击：复位视角\n测量模式：点击两个点\n右侧栏按钮：收起或展开属性面板")); });
  helpMenu->addAction(QStringLiteral("关于 ScanForge"), this, [this]{ QMessageBox::about(this, QStringLiteral("关于"), QStringLiteral("ScanForge 点云查看器")); });
  setStyleSheet(R"(
    QWidget { background:#0b0f14; color:#e7edf5; font-family:"Noto Sans CJK SC","Microsoft YaHei",sans-serif; font-size:12px; }
    QMainWindow { background:#0b0f14; }
    #topbar,#statusbar { background:#0e141c; border:0; }
    #logo { font-size:18px; font-weight:800; letter-spacing:1px; }
    #logoAccent { color:#35a7ff; }
    #project { color:#8996a7; border-left:1px solid #273241; padding-left:16px; }
    #sidebar,#inspector { background:#111720; }
    #sidebar { border-right:1px solid #273241; }
    #inspector { border-left:1px solid #273241; }
    #sectionTitle { color:#738195; font-size:10px; font-weight:700; letter-spacing:1px; padding-top:8px; }
    QLabel#value { color:#35a7ff; }
    QLabel#muted { color:#8996a7; }
    QPushButton { background:#18212d; color:#dce5ef; border:1px solid #304052; border-radius:5px; padding:7px 10px; }
    QPushButton:hover { background:#1d2936; border-color:#4c6680; }
    QPushButton#primary { background:#1768a2; border-color:#2587c9; }
    QSlider::groove:horizontal { height:4px; background:#273241; border-radius:2px; }
    QSlider::handle:horizontal { width:12px; margin:-5px 0; background:#35a7ff; border-radius:6px; }
    QTreeWidget { border:0; background:transparent; }
    QGroupBox { border:0; }
    QStatusBar { border-top:1px solid #273241; color:#8996a7; }
    QScrollArea { border:0; }
  )");
}

void MainWindow::buildUi() {
  auto *root = new QWidget; auto *outer = new QVBoxLayout(root); outer->setContentsMargins(0,0,0,0); outer->setSpacing(0);
  auto *top = new QWidget; top->setObjectName("topbar"); top->setFixedHeight(54); auto *topLayout = new QHBoxLayout(top); topLayout->setContentsMargins(18,0,18,0); topLayout->setSpacing(16);
  auto *logo = new QLabel("SCAN<span style='color:#35a7ff'>FORGE</span>"); logo->setObjectName("logo"); logo->setTextFormat(Qt::RichText);
  topLayout->addWidget(logo); topLayout->addStretch();
  auto *open = button(QStringLiteral("打开 PCD")); auto *shot = button(QStringLiteral("保存截图"), "primary"); topLayout->addWidget(open); topLayout->addWidget(shot); outer->addWidget(top);

  auto *split = new QSplitter(Qt::Horizontal); split->setHandleWidth(1);
  auto *left = new QWidget; left->setObjectName("sidebar"); left->setMinimumWidth(210); left->setMaximumWidth(280); auto *ll = new QVBoxLayout(left); ll->setContentsMargins(14,14,14,14); ll->setSpacing(9);
  ll->addWidget(sectionTitle(QStringLiteral("文件树")));
  auto *tree = new QTreeWidget; tree->setHeaderHidden(true); tree->setIndentation(16); tree->setRootIsDecorated(true);
  fileTree_ = tree;
  connect(tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
    if (column != 0 || !view_ || !selectedPcdItem_) return;
    const bool visible = item->checkState(0) == Qt::Checked;
    if (item == selectedPcdItem_) {
      if (visible) {
        QString error; const QString path = item->data(0, Qt::UserRole).toString();
        if (!path.isEmpty() && data_.load(path, &error)) { view_->setBounds(data_.minBound(), data_.maxBound()); for (int i = 0; i < 3; ++i) cropSliders_[i]->setValues(0, 10000); updateCropRange(); view_->setPoints(data_.displaySample(), true); updateCloudLabels(); }
      }
      view_->setCloudVisible(visible);
      setStatus(visible ? QStringLiteral("已显示父点云") : QStringLiteral("已隐藏父点云"));
    } else if (item->parent() == selectedPcdItem_) {
      if (!visible) { view_->setCloudVisible(false); setStatus(QStringLiteral("已隐藏 %1").arg(item->text(0))); return; }
      const QString filePath = item->data(0, Qt::UserRole).toString();
      if (filePath.isEmpty()) { view_->setCloudVisible(true); setStatus(QStringLiteral("已显示当前裁剪结果 %1").arg(item->text(0))); return; }
      QString error;
      if (!data_.load(filePath, &error)) { item->setCheckState(0, Qt::Unchecked); QMessageBox::warning(this, QStringLiteral("加载失败"), error); return; }
      view_->setBounds(data_.minBound(), data_.maxBound()); view_->setPoints(data_.displaySample(), true); updateCloudLabels(); view_->setCloudVisible(true); setStatus(QStringLiteral("已显示 %1").arg(item->text(0)));
    }
  });
  ll->addWidget(tree, 1);
  auto *drop = new QLabel(QStringLiteral("打开 PCD\n支持拖拽点云文件到窗口")); drop->setAlignment(Qt::AlignCenter); drop->setStyleSheet("border:1px dashed #3b4b5d; border-radius:6px; padding:18px; color:#8392a3;"); ll->addWidget(drop); split->addWidget(left);

  view_ = new CloudView; view_->setObjectName("viewport"); split->addWidget(view_); split->setStretchFactor(1,1);

  auto *right = new QWidget; right->setObjectName("inspector"); right->setMinimumWidth(290); right->setMaximumWidth(340); auto *rl = new QVBoxLayout(right); rl->setContentsMargins(14,14,14,10); rl->setSpacing(8);
  rl->addWidget(sectionTitle(QStringLiteral("点云信息"))); pointLabel_ = new QLabel(QStringLiteral("Points: —")); boundsLabel_ = new QLabel(QStringLiteral("Bounds: —")); boundsLabel_->setObjectName("muted"); rl->addWidget(pointLabel_); rl->addWidget(boundsLabel_);
  showAllCheck_ = new QCheckBox(QStringLiteral("全部显示（可能影响性能）")); rl->addWidget(showAllCheck_);
  rl->addSpacing(10); rl->addWidget(sectionTitle(QStringLiteral("三轴裁剪 · XYZ")));
  const QStringList axisNames{QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")};
  for (int axis = 0; axis < 3; ++axis) {
    rl->addWidget(new QLabel(axisNames[axis] + QStringLiteral(" 范围")));
    auto *labels = new QHBoxLayout; auto *minLabel = new QLabel(QStringLiteral("最小 —")); auto *maxLabel = new QLabel(QStringLiteral("最大 —")); labels->addWidget(minLabel); labels->addStretch(); labels->addWidget(maxLabel); rl->addLayout(labels);
    auto *slider = new RangeSlider; rl->addWidget(slider); cropSliders_.append(slider); cropMinLabels_.append(minLabel); cropMaxLabels_.append(maxLabel);
    slider->onChange([this](int, int){ updateCropRange(); });
  }
  /* Legacy Z controls remain below for compatibility. */
  rl->addWidget(button(QStringLiteral("确定裁剪"), "primary")); auto *confirmCrop = qobject_cast<QPushButton*>(rl->itemAt(rl->count()-1)->widget());
  rl->addSpacing(10); rl->addWidget(sectionTitle(QStringLiteral("编辑和测量"))); auto *measure = button(QStringLiteral("⌖ 两点测量")); measure->setObjectName("primary"); rl->addWidget(measure); auto *fit = button(QStringLiteral("⊙ 适配视图")); rl->addWidget(fit);
  measureLabel_ = new QLabel(QStringLiteral("状态：未选择测量点\n距离 —\n水平距离 —\n高度差 —")); measureLabel_->setWordWrap(true); measureLabel_->setStyleSheet("background:#0c131b; border:1px solid #2a3949; border-radius:6px; padding:10px; line-height:1.5;"); rl->addWidget(measureLabel_); auto *csv = button(QStringLiteral("导出测量 CSV")); rl->addWidget(csv);
  rl->addSpacing(10); rl->addWidget(sectionTitle(QStringLiteral("成果"))); auto *exports = new QHBoxLayout; auto *ply = button(QStringLiteral("导出 PLY"), "primary"); auto *pcd = button(QStringLiteral("导出 PCD")); exports->addWidget(ply); exports->addWidget(pcd); rl->addLayout(exports); rl->addStretch();
  auto *inspectorScroll = new QScrollArea; inspectorScroll->setWidgetResizable(true); inspectorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); inspectorScroll->setFrameShape(QFrame::NoFrame); inspectorScroll->setWidget(right);
  auto *inspectorPane = new QWidget; auto *paneLayout = new QHBoxLayout(inspectorPane); paneLayout->setContentsMargins(0,0,0,0); paneLayout->setSpacing(0);
  auto *toggleInspector = button(QStringLiteral("›")); toggleInspector->setToolTip(QStringLiteral("收起右侧栏")); toggleInspector->setFixedWidth(24); toggleInspector->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
  paneLayout->addWidget(inspectorScroll); paneLayout->addWidget(toggleInspector);
  split->addWidget(inspectorPane); split->setCollapsible(2, false); inspectorPane->setMinimumWidth(24); inspectorPane->setMaximumWidth(340);
  connect(toggleInspector, &QPushButton::clicked, this, [inspectorScroll, toggleInspector, inspectorPane, split, savedWidth = 364]() mutable {
    if (inspectorScroll->isVisible()) {
      const auto sizes = split->sizes(); savedWidth = sizes[2]; inspectorScroll->hide(); toggleInspector->setText(QStringLiteral("‹")); toggleInspector->setToolTip(QStringLiteral("展开右侧栏"));
      split->setSizes({sizes[0], sizes[1] + sizes[2] - toggleInspector->width(), toggleInspector->width()});
    } else {
      inspectorScroll->show(); toggleInspector->setText(QStringLiteral("›")); toggleInspector->setToolTip(QStringLiteral("收起右侧栏"));
      const auto sizes = split->sizes(); split->setSizes({sizes[0], std::max(0, sizes[1] - savedWidth + toggleInspector->width()), savedWidth});
    }
  });
  outer->addWidget(split,1);
  auto *status = new QStatusBar; status->setObjectName("statusbar"); toolLabel_ = new QLabel(QStringLiteral("● Engine Ready    Tool: Orbit")); status->addWidget(toolLabel_); setStatusBar(status);

  connect(open, &QPushButton::clicked, this, &MainWindow::openFile); connect(shot, &QPushButton::clicked, this, &MainWindow::saveScreenshot); connect(fit, &QPushButton::clicked, view_, &CloudView::fitView); connect(measure, &QPushButton::clicked, this, &MainWindow::chooseMeasureMode); connect(csv, &QPushButton::clicked, this, &MainWindow::exportCsv); connect(ply, &QPushButton::clicked, this, &MainWindow::exportPly); connect(pcd, &QPushButton::clicked, this, &MainWindow::exportPcd); connect(view_, &CloudView::pointPicked, this, &MainWindow::receivePoint); connect(view_, &CloudView::measurementPreview, this, &MainWindow::receivePreview); connect(view_, &CloudView::measurementCanceled, this, &MainWindow::cancelMeasurement); connect(showAllCheck_, &QCheckBox::toggled, this, &MainWindow::updateDisplayMode);
  connect(confirmCrop, &QPushButton::clicked, this, [this] {
    if (!selectedPcdItem_) { setStatus(QStringLiteral("请先打开 PCD 文件")); return; }
    int maxNumber = 0; const QRegularExpression re(QStringLiteral("^(\\d+)\\.pcd$"));
    for (int i = 0; i < selectedPcdItem_->childCount(); ++i) { const auto match = re.match(selectedPcdItem_->child(i)->text(0)); if (match.hasMatch()) maxNumber = std::max(maxNumber, match.captured(1).toInt()); }
    const QString name = QStringLiteral("%1.pcd").arg(maxNumber + 1, 4, 10, QLatin1Char('0'));
    auto *item = new QTreeWidgetItem(selectedPcdItem_, {name}); item->setFlags(item->flags() | Qt::ItemIsUserCheckable); item->setCheckState(0, Qt::Checked); selectedPcdItem_->setCheckState(0, Qt::Unchecked); selectedPcdItem_->setExpanded(true); fileTree_->setCurrentItem(item); view_->setCloudVisible(true); setStatus(QStringLiteral("已显示最新裁剪结果：%1").arg(name));
  });
  setCentralWidget(root);
  QTimer::singleShot(0, this, [split] { split->setSizes({238, std::max(400, split->width() - 602), 364}); });
}

void MainWindow::updateCloudLabels() { pointLabel_->setText(QStringLiteral("Points: %1").arg(view_->pointCount())); boundsLabel_->setText(QStringLiteral("原始点数: %1\nBounds: X %2 ~ %3\nY %4 ~ %5\nZ %6 ~ %7 m").arg(data_.points().size()).arg(data_.minBound().x(),0,'f',2).arg(data_.maxBound().x(),0,'f',2).arg(data_.minBound().y(),0,'f',2).arg(data_.maxBound().y(),0,'f',2).arg(data_.minBound().z(),0,'f',2).arg(data_.maxBound().z(),0,'f',2)); }
void MainWindow::openFile() { const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开 PCD 点云"), QStringLiteral("../prev"), QStringLiteral("Point Cloud (*.pcd)")); if (!path.isEmpty()) loadFilePath(path); }
bool MainWindow::loadFilePath(const QString &path) { QApplication::setOverrideCursor(Qt::WaitCursor); QString error; const bool ok = data_.load(path, &error); QApplication::restoreOverrideCursor(); if (!ok) { QMessageBox::critical(this, QStringLiteral("打开失败"), error); return false; }
  fileTree_->clear(); auto *root = new QTreeWidgetItem(fileTree_, {QStringLiteral("选择的 pcd")}); root->setExpanded(true); selectedPcdItem_ = new QTreeWidgetItem(root, {QFileInfo(path).fileName()}); selectedPcdItem_->setData(0, Qt::UserRole, path); selectedPcdItem_->setFlags(selectedPcdItem_->flags() | Qt::ItemIsUserCheckable); selectedPcdItem_->setCheckState(0, Qt::Checked); selectedPcdItem_->setSelected(true); cropSequence_ = 1;
  view_->setBounds(data_.minBound(), data_.maxBound()); for(int i=0;i<3;++i) cropSliders_[i]->setValues(0, 10000); updateCropRange(); view_->setPoints(data_.displaySample(), true); updateCloudLabels(); measurePoints_.clear(); updateMeasureLabels(); qInfo().noquote() << QStringLiteral("Loaded %1 points; bounds X[%2,%3] Y[%4,%5] Z[%6,%7]").arg(data_.points().size()).arg(data_.minBound().x()).arg(data_.maxBound().x()).arg(data_.minBound().y()).arg(data_.maxBound().y()).arg(data_.minBound().z()).arg(data_.maxBound().z()); setWindowTitle(QStringLiteral("ScanForge · %1").arg(QFileInfo(path).fileName())); setStatus(QStringLiteral("已加载 %1 · %2 个点").arg(QFileInfo(path).fileName()).arg(data_.points().size())); return true; }
void MainWindow::updateCropRange() { if (data_.points().isEmpty() || cropSliders_.size()!=3) return; for(int i=0;i<3;++i) { const double lo=data_.minBound()[i], hi=data_.maxBound()[i]; cropMin_[i]=lo+(hi-lo)*cropSliders_[i]->minValue()/10000.0; cropMax_[i]=lo+(hi-lo)*cropSliders_[i]->maxValue()/10000.0; cropMinLabels_[i]->setText(QStringLiteral("最小 %1").arg(cropMin_[i],0,'f',2)); cropMaxLabels_[i]->setText(QStringLiteral("最大 %1").arg(cropMax_[i],0,'f',2)); } view_->setCropRange(cropMin_,cropMax_); }
void MainWindow::updateDisplayMode() { if (data_.points().isEmpty()) return; view_->setPoints(showAllCheck_->isChecked() ? data_.points() : data_.displaySample()); updateCloudLabels(); setStatus(showAllCheck_->isChecked() ? QStringLiteral("已切换为全部显示") : QStringLiteral("已切换为采样显示")); }
void MainWindow::toggleFullscreen() { isFullScreen() ? showNormal() : showFullScreen(); }
void MainWindow::chooseMeasureMode() { measurePoints_.clear(); updateMeasureLabels(); const bool enabled = !view_->measureMode(); view_->setMeasureMode(enabled); toolLabel_->setText(enabled ? QStringLiteral("● Engine Ready    Tool: Two Point Measure · 请点击两个点") : QStringLiteral("● Engine Ready    Tool: Orbit")); }
void MainWindow::receivePoint(const QVector3D &point) { if (measurePoints_.size() >= 2) measurePoints_.clear(); measurePoints_.append(point); updateMeasureLabels(); if (measurePoints_.size() == 2) { view_->setMeasureMode(false); toolLabel_->setText(QStringLiteral("● Engine Ready    Tool: Orbit · 测量完成")); } }
void MainWindow::receivePreview(const QVector3D &point) { if (measurePoints_.size() != 1) return; const double distance = (measurePoints_[0] - point).length(); measureLabel_->setText(QStringLiteral("状态：已选择 1 / 2 个点\n预览距离 %1 m (%2 cm)\n右键取消").arg(distance,0,'f',3).arg(distance * 100.0,0,'f',1)); }
void MainWindow::cancelMeasurement() { measurePoints_.clear(); updateMeasureLabels(); toolLabel_->setText(QStringLiteral("● Engine Ready    Tool: Two Point Measure · 已取消")); }
void MainWindow::updateMeasureLabels() { if (measurePoints_.size() < 2) { measureLabel_->setText(QStringLiteral("状态：已选择 %1 / 2 个点\n距离 —\n水平距离 —\n高度差 —").arg(measurePoints_.size())); return; } const auto &a = measurePoints_[0]; const auto &b = measurePoints_[1]; const double distance = (a-b).length(); const double horizontal = std::hypot(double(a.x()-b.x()), double(a.y()-b.y())); const double height = std::abs(double(a.z()-b.z())); measureLabel_->setText(QStringLiteral("状态：✓ 测量完成\n距离 %1 m\n水平距离 %2 m\n高度差 %3 m").arg(distance,0,'f',3).arg(horizontal,0,'f',3).arg(height,0,'f',3)); }
void MainWindow::exportPly() { if (data_.points().isEmpty()) return; const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出 PLY"), QStringLiteral("cropped_area.ply"), QStringLiteral("PLY (*.ply)")); if (path.isEmpty()) return; QString error; if (!data_.exportPly(path, data_.minBound().z(), data_.maxBound().z(), &error)) QMessageBox::critical(this, QStringLiteral("导出失败"), error); else setStatus(QStringLiteral("已导出 %1").arg(path)); }
void MainWindow::exportPcd() { if (data_.points().isEmpty()) return; const QString suggested = QStringLiteral("%1.pcd").arg(cropSequence_, 4, 10, QLatin1Char('0')); const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出 PCD"), suggested, QStringLiteral("PCD (*.pcd)")); if (path.isEmpty()) return; QString error; if (!data_.exportPcd(path, data_.minBound().z(), data_.maxBound().z(), &error)) QMessageBox::critical(this, QStringLiteral("导出失败"), error); else { if (selectedPcdItem_) { auto *item = new QTreeWidgetItem(selectedPcdItem_, {QFileInfo(path).fileName()}); item->setData(0, Qt::UserRole, path); item->setFlags(item->flags() | Qt::ItemIsUserCheckable); item->setCheckState(0, Qt::Unchecked); selectedPcdItem_->setExpanded(true); fileTree_->setCurrentItem(item); } ++cropSequence_; setStatus(QStringLiteral("已导出 %1，请勾选后浏览").arg(QFileInfo(path).fileName())); } }
void MainWindow::exportCsv() { if (measurePoints_.size() < 2) { QMessageBox::information(this, QStringLiteral("尚无测量"), QStringLiteral("请先点击“两点测量”，再在点云中选择两个点。")); return; } const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出测量 CSV"), QStringLiteral("measurements.csv"), QStringLiteral("CSV (*.csv)")); if (path.isEmpty()) return; QFile file(path); if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { QMessageBox::critical(this, QStringLiteral("导出失败"), file.errorString()); return; } QTextStream out(&file); const auto &a=measurePoints_[0]; const auto &b=measurePoints_[1]; out << "point_a_x,point_a_y,point_a_z,point_b_x,point_b_y,point_b_z,distance_m,horizontal_m,height_diff_m\n" << a.x() << ',' << a.y() << ',' << a.z() << ',' << b.x() << ',' << b.y() << ',' << b.z() << ',' << (a-b).length() << ',' << std::hypot(double(a.x()-b.x()), double(a.y()-b.y())) << ',' << std::abs(double(a.z()-b.z())) << '\n'; setStatus(QStringLiteral("已导出测量 CSV")); }
void MainWindow::saveScreenshot() { const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存截图"), QStringLiteral("scanforge_snapshot.png"), QStringLiteral("PNG (*.png)")); if (!path.isEmpty() && view_->grab().save(path)) setStatus(QStringLiteral("已保存截图 %1").arg(path)); }
void MainWindow::setStatus(const QString &message) { statusBar()->showMessage(message, 5000); }
