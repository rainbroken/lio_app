#include "mainwindow.h"
#include "transformdialog.h"

#include <QApplication>
#include <QActionGroup>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMenuBar>
#include <QMenu>
#include <QKeySequence>
#include <QSettings>
#include <QPushButton>
#include <QSlider>
#include <QScrollArea>
#include <QSplitter>
#include <QStatusBar>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QToolButton>
#include <QSignalBlocker>
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
  auto *fileMenu = menuBar()->addMenu(QStringLiteral("文件"));
  auto *openAction = fileMenu->addAction(QStringLiteral("打开 PCD"), this, &MainWindow::openFile);
  openAction->setShortcut(QKeySequence::Open);
  auto *saveAction = fileMenu->addAction(QStringLiteral("保存当前点云为 PCD"), this, &MainWindow::exportPcd);
  saveAction->setShortcut(QKeySequence::Save);
  auto *shotAction = fileMenu->addAction(QStringLiteral("保存截图"), this, &MainWindow::saveScreenshot);
  shotAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
  fileMenu->addSeparator(); fileMenu->addAction(QStringLiteral("退出"), this, &QWidget::close);
  auto *editMenu = menuBar()->addMenu(QStringLiteral("编辑")); editMenu->addAction(QStringLiteral("旋转"), this, &MainWindow::rotateCloud);
  auto *viewMenu = menuBar()->addMenu(QStringLiteral("视图"));
  auto *fitAction = viewMenu->addAction(QStringLiteral("适配视图"), view_, &CloudView::fitView);
  fitAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
  auto *zoomInAction = viewMenu->addAction(QStringLiteral("放大"), view_, &CloudView::zoomIn);
  zoomInAction->setShortcuts({QKeySequence(Qt::CTRL | Qt::Key_Plus), QKeySequence(Qt::CTRL | Qt::Key_Equal)});
  auto *zoomOutAction = viewMenu->addAction(QStringLiteral("缩小"), view_, &CloudView::zoomOut);
  zoomOutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
  auto *full = viewMenu->addAction(QStringLiteral("全屏"), this, &MainWindow::toggleFullscreen); full->setShortcut(QKeySequence(Qt::Key_F11));
  auto *settingsMenu = menuBar()->addMenu(QStringLiteral("设置"));
  auto *pointSizeMenu = settingsMenu->addMenu(QStringLiteral("点大小"));
  auto *pointSizeGroup = new QActionGroup(pointSizeMenu);
  QSettings settings(QStringLiteral("ScanForge"), QStringLiteral("PointCloudDemo"));
  const int savedPointSize = settings.value(QStringLiteral("display/pointSize"), 2).toInt();
  for (int size : {1, 2, 3, 4, 6}) {
    auto *action = pointSizeMenu->addAction(QStringLiteral("%1 px").arg(size));
    action->setCheckable(true); action->setChecked(size == savedPointSize);
    pointSizeGroup->addAction(action);
    connect(action, &QAction::triggered, this, [this, size] {
      view_->setPointSize(size);
      QSettings(QStringLiteral("ScanForge"), QStringLiteral("PointCloudDemo")).setValue(QStringLiteral("display/pointSize"), size);
    });
  }
  if (!pointSizeGroup->checkedAction()) pointSizeGroup->actions().at(1)->setChecked(true);
  view_->setPointSize(pointSizeGroup->checkedAction()->text().split(QLatin1Char(' ')).first().toFloat());
  auto *showAllAction = settingsMenu->addAction(QStringLiteral("全部显示（可能影响性能）"));
  showAllAction->setCheckable(true);
  connect(showAllAction, &QAction::toggled, showAllCheck_, &QCheckBox::setChecked);
  connect(showAllCheck_, &QCheckBox::toggled, showAllAction, &QAction::setChecked);
  auto *shortcutsMenu = settingsMenu->addMenu(QStringLiteral("快捷键"));
  for (auto *action : {openAction, saveAction, shotAction, zoomInAction, zoomOutAction, fitAction, full}) {
    auto *entry = shortcutsMenu->addAction(action->text());
    entry->setShortcut(action->shortcut());
    entry->setEnabled(false);
  }
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
    #viewRail { background:#0e141c; border-right:1px solid #273241; }
    QToolButton#axisView { background:#18212d; color:#dce5ef; border:1px solid #304052; border-radius:5px; font-weight:700; }
    QToolButton#axisView:hover { background:#1d2936; border-color:#4c6680; }
    QToolButton#axisView:checked { background:#1768a2; border-color:#35a7ff; color:white; }
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
  tree->setContextMenuPolicy(Qt::CustomContextMenu);
  connect(tree, &QWidget::customContextMenuRequested, this, [this, tree](const QPoint &pos) {
    auto *item = tree->itemAt(pos);
    if (!item) return;
    QMenu menu(tree);
    auto *remove = menu.addAction(QStringLiteral("删除"));
    if (menu.exec(tree->viewport()->mapToGlobal(pos)) != remove) return;

    const QString name = item->text(0);
    const QString detail = item->childCount() > 0
        ? QStringLiteral("将从文件树中移除“%1”及其所有子点云。磁盘上的 PCD 文件不会删除。").arg(name)
        : QStringLiteral("将从文件树中移除点云“%1”。磁盘上的 PCD 文件不会删除。").arg(name);
    if (QMessageBox::question(this, QStringLiteral("确认删除"), detail,
                              QMessageBox::Yes | QMessageBox::Cancel,
                              QMessageBox::Cancel) != QMessageBox::Yes) return;
    bool removesActive = false;
    for (auto *node = activePcdItem_; node; node = node->parent()) {
      if (node == item) { removesActive = true; break; }
    }
    if (removesActive) {
      activePcdItem_ = nullptr;
      data_.setPoints({});
      view_->setPoints({});
      pointLabel_->setText(QStringLiteral("Points: —"));
      boundsLabel_->setText(QStringLiteral("Bounds: —"));
      measurePoints_.clear();
      updateMeasureLabels();
    }
    for (auto *node = selectedPcdItem_; node; node = node->parent()) {
      if (node == item) { selectedPcdItem_ = nullptr; break; }
    }
    auto forgetCrops = [this](auto &&self, QTreeWidgetItem *node) -> void {
      cropClouds_.remove(node);
      transformedClouds_.remove(node);
      for (int i = 0; i < node->childCount(); ++i) self(self, node->child(i));
    };
    forgetCrops(forgetCrops, item);
    {
      QSignalBlocker blocker(tree);
      delete item;
    }
    if (removesActive) setWindowTitle(QStringLiteral("ScanForge · 点云后处理 Demo"));
    setStatus(QStringLiteral("已从文件树删除 %1").arg(name));
  });
  connect(tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
    if (column != 0 || !view_ || !selectedPcdItem_ || !(item->flags() & Qt::ItemIsUserCheckable)) return;
    const bool visible = item->checkState(0) == Qt::Checked;
    if (!visible) { if (item == activePcdItem_) view_->setCloudVisible(false); return; }
    QString error;
    const QString path = item->data(0, Qt::UserRole).toString();
    if (transformedClouds_.contains(item)) data_.setPoints(transformedClouds_.value(item));
    else if (!path.isEmpty()) { if (!data_.load(path, &error)) { QSignalBlocker blocker(fileTree_); item->setCheckState(0, Qt::Unchecked); QMessageBox::warning(this, QStringLiteral("加载失败"), error); return; } }
    else if (cropClouds_.contains(item)) data_.setPoints(cropClouds_.value(item));
    else return;
    {
      QSignalBlocker blocker(fileTree_);
      if (activePcdItem_ && activePcdItem_ != item) activePcdItem_->setCheckState(0, Qt::Unchecked);
    }
    activePcdItem_ = item;
    view_->setBounds(data_.minBound(), data_.maxBound(), true);
    for (auto *slider : cropSliders_) slider->setValues(0, 10000);
    updateCropRange(); view_->setPoints(showAllCheck_->isChecked() ? data_.points() : data_.displaySample());
    updateCloudLabels(); view_->setCloudVisible(true); setStatus(QStringLiteral("已显示 %1").arg(item->text(0)));
  });
  ll->addWidget(tree, 1);
  auto *drop = new QLabel(QStringLiteral("打开 PCD\n支持拖拽点云文件到窗口")); drop->setAlignment(Qt::AlignCenter); drop->setStyleSheet("border:1px dashed #3b4b5d; border-radius:6px; padding:18px; color:#8392a3;"); ll->addWidget(drop); split->addWidget(left);

  auto *viewRail = new QWidget; viewRail->setObjectName("viewRail"); viewRail->setFixedWidth(58);
  auto *railLayout = new QVBoxLayout(viewRail); railLayout->setContentsMargins(7,12,7,12); railLayout->setSpacing(8);
  auto *axisGroup = new QButtonGroup(viewRail); axisGroup->setExclusive(true);
  const struct { const char *label; const char *tip; CloudView::AxisView direction; } views[] = {
    {"+X", "从 X 正方向观察", CloudView::AxisView::PositiveX},
    {"-X", "从 X 负方向观察", CloudView::AxisView::NegativeX},
    {"+Y", "从 Y 正方向观察", CloudView::AxisView::PositiveY},
    {"-Y", "从 Y 负方向观察", CloudView::AxisView::NegativeY},
    {"+Z", "从上方观察（Z 正方向）", CloudView::AxisView::PositiveZ},
    {"-Z", "从下方观察（Z 负方向）", CloudView::AxisView::NegativeZ},
  };
  for (const auto &preset : views) {
    auto *axis = new QToolButton(viewRail); axis->setObjectName("axisView");
    axis->setText(QLatin1String(preset.label)); axis->setToolTip(QString::fromUtf8(preset.tip));
    axis->setFixedSize(42, 42); axis->setCheckable(true); axisGroup->addButton(axis);
    connect(axis, &QToolButton::clicked, this, [this, direction = preset.direction] { view_->setAxisView(direction); });
    railLayout->addWidget(axis);
  }
  railLayout->addStretch(); split->insertWidget(0, viewRail); split->setCollapsible(0, false);
  view_ = new CloudView; view_->setObjectName("viewport"); split->addWidget(view_); split->setStretchFactor(2,1);
  connect(view_, &CloudView::viewRotated, this, [axisGroup] {
    axisGroup->setExclusive(false);
    for (auto *axis : axisGroup->buttons()) axis->setChecked(false);
    axisGroup->setExclusive(true);
  });

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
  split->addWidget(inspectorPane); split->setCollapsible(3, false); inspectorPane->setMinimumWidth(24); inspectorPane->setMaximumWidth(340);
  connect(toggleInspector, &QPushButton::clicked, this, [inspectorScroll, toggleInspector, inspectorPane, split, savedWidth = 364]() mutable {
    if (inspectorScroll->isVisible()) {
      const auto sizes = split->sizes(); savedWidth = sizes[3]; inspectorScroll->hide(); toggleInspector->setText(QStringLiteral("‹")); toggleInspector->setToolTip(QStringLiteral("展开右侧栏"));
      split->setSizes({sizes[0], sizes[1], sizes[2] + sizes[3] - toggleInspector->width(), toggleInspector->width()});
    } else {
      inspectorScroll->show(); toggleInspector->setText(QStringLiteral("›")); toggleInspector->setToolTip(QStringLiteral("收起右侧栏"));
      const auto sizes = split->sizes(); split->setSizes({sizes[0], sizes[1], std::max(0, sizes[2] - savedWidth + toggleInspector->width()), savedWidth});
    }
  });
  outer->addWidget(split,1);
  auto *status = new QStatusBar; status->setObjectName("statusbar"); toolLabel_ = new QLabel(QStringLiteral("● Engine Ready    Tool: Orbit")); status->addWidget(toolLabel_); setStatusBar(status);

  connect(open, &QPushButton::clicked, this, &MainWindow::openFile); connect(shot, &QPushButton::clicked, this, &MainWindow::saveScreenshot); connect(fit, &QPushButton::clicked, view_, &CloudView::fitView); connect(measure, &QPushButton::clicked, this, &MainWindow::chooseMeasureMode); connect(csv, &QPushButton::clicked, this, &MainWindow::exportCsv); connect(ply, &QPushButton::clicked, this, &MainWindow::exportPly); connect(pcd, &QPushButton::clicked, this, &MainWindow::exportPcd); connect(view_, &CloudView::pointPicked, this, &MainWindow::receivePoint); connect(view_, &CloudView::measurementPreview, this, &MainWindow::receivePreview); connect(view_, &CloudView::measurementCanceled, this, &MainWindow::cancelMeasurement); connect(showAllCheck_, &QCheckBox::toggled, this, &MainWindow::updateDisplayMode);
  connect(confirmCrop, &QPushButton::clicked, this, [this] {
    if (!activePcdItem_ || data_.points().isEmpty()) { setStatus(QStringLiteral("请先选择点云")); return; }
    QVector<CloudPoint> cropped;
    for (const auto &p : data_.points()) if (p.x >= cropMin_.x() && p.x <= cropMax_.x() && p.y >= cropMin_.y() && p.y <= cropMax_.y() && p.z >= cropMin_.z() && p.z <= cropMax_.z()) cropped.append(p);
    if (cropped.isEmpty()) { setStatus(QStringLiteral("裁剪范围内没有点")); return; }
    const QString name = QStringLiteral("%1.pcd").arg(cropSequence_++, 4, 10, QLatin1Char('0'));
    auto *parent = activePcdItem_;
    auto *item = new QTreeWidgetItem(parent, {name}); cropClouds_.insert(item, std::move(cropped));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable); item->setCheckState(0, Qt::Checked);
    parent->setExpanded(true); fileTree_->setCurrentItem(item);
  });
  setCentralWidget(root);
  QTimer::singleShot(0, this, [split] { split->setSizes({58, 238, std::max(400, split->width() - 660), 364}); });
}

void MainWindow::updateCloudLabels() { pointLabel_->setText(QStringLiteral("Points: %1").arg(view_->pointCount())); boundsLabel_->setText(QStringLiteral("原始点数: %1\nBounds: X %2 ~ %3\nY %4 ~ %5\nZ %6 ~ %7 m").arg(data_.points().size()).arg(data_.minBound().x(),0,'f',2).arg(data_.maxBound().x(),0,'f',2).arg(data_.minBound().y(),0,'f',2).arg(data_.maxBound().y(),0,'f',2).arg(data_.minBound().z(),0,'f',2).arg(data_.maxBound().z(),0,'f',2)); }
void MainWindow::openFile() { const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开 PCD 点云"), QStringLiteral("../prev"), QStringLiteral("Point Cloud (*.pcd)")); if (!path.isEmpty()) loadFilePath(path); }
bool MainWindow::loadFilePath(const QString &path) { QApplication::setOverrideCursor(Qt::WaitCursor); QString error; const bool ok = data_.load(path, &error); QApplication::restoreOverrideCursor(); if (!ok) { QMessageBox::critical(this, QStringLiteral("打开失败"), error); return false; }
  activePcdItem_ = nullptr; selectedPcdItem_ = nullptr; cropClouds_.clear(); transformedClouds_.clear(); fileTree_->clear(); auto *root = new QTreeWidgetItem(fileTree_, {QFileInfo(path).completeBaseName()}); root->setExpanded(true); selectedPcdItem_ = new QTreeWidgetItem(root, {QFileInfo(path).fileName()}); selectedPcdItem_->setData(0, Qt::UserRole, path); selectedPcdItem_->setFlags(selectedPcdItem_->flags() | Qt::ItemIsUserCheckable); selectedPcdItem_->setCheckState(0, Qt::Checked); selectedPcdItem_->setSelected(true); activePcdItem_ = selectedPcdItem_; cropSequence_ = 1;
  view_->setBounds(data_.minBound(), data_.maxBound()); for(int i=0;i<3;++i) cropSliders_[i]->setValues(0, 10000); updateCropRange(); view_->setPoints(data_.displaySample(), true); updateCloudLabels(); measurePoints_.clear(); updateMeasureLabels(); qInfo().noquote() << QStringLiteral("Loaded %1 points; bounds X[%2,%3] Y[%4,%5] Z[%6,%7]").arg(data_.points().size()).arg(data_.minBound().x()).arg(data_.maxBound().x()).arg(data_.minBound().y()).arg(data_.maxBound().y()).arg(data_.minBound().z()).arg(data_.maxBound().z()); setWindowTitle(QStringLiteral("ScanForge · %1").arg(QFileInfo(path).fileName())); setStatus(QStringLiteral("已加载 %1 · %2 个点").arg(QFileInfo(path).fileName()).arg(data_.points().size())); return true; }
void MainWindow::updateCropRange() { if (data_.points().isEmpty() || cropSliders_.size()!=3) return; for(int i=0;i<3;++i) { const double lo=data_.minBound()[i], hi=data_.maxBound()[i]; cropMin_[i]=lo+(hi-lo)*cropSliders_[i]->minValue()/10000.0; cropMax_[i]=lo+(hi-lo)*cropSliders_[i]->maxValue()/10000.0; cropMinLabels_[i]->setText(QStringLiteral("最小 %1").arg(cropMin_[i],0,'f',2)); cropMaxLabels_[i]->setText(QStringLiteral("最大 %1").arg(cropMax_[i],0,'f',2)); } view_->setCropRange(cropMin_,cropMax_); }
void MainWindow::updateDisplayMode() { if (data_.points().isEmpty()) return; view_->setPoints(showAllCheck_->isChecked() ? data_.points() : data_.displaySample()); updateCloudLabels(); setStatus(showAllCheck_->isChecked() ? QStringLiteral("已切换为全部显示") : QStringLiteral("已切换为采样显示")); }
void MainWindow::toggleFullscreen() { isFullScreen() ? showNormal() : showFullScreen(); }
void MainWindow::rotateCloud() {
  if (!activePcdItem_ || data_.points().isEmpty() || activePcdItem_->checkState(0) != Qt::Checked) {
    QMessageBox::information(this, QStringLiteral("没有选中点云"), QStringLiteral("请先在文件树中勾选要旋转的点云。"));
    return;
  }
  TransformDialog dialog(this);
  if (dialog.exec() != QDialog::Accepted) return;
  QVector<CloudPoint> points = data_.points();
  const QMatrix4x4 transform = dialog.transform();
  for (auto &point : points) {
    const QVector3D result = transform.map(QVector3D(point.x, point.y, point.z));
    point.x = result.x(); point.y = result.y(); point.z = result.z();
  }
  if (cropClouds_.contains(activePcdItem_)) cropClouds_.insert(activePcdItem_, points);
  else transformedClouds_.insert(activePcdItem_, points);
  data_.setPoints(points);
  view_->setBounds(data_.minBound(), data_.maxBound());
  for (auto *slider : cropSliders_) slider->setValues(0, 10000);
  updateCropRange();
  view_->setPoints(showAllCheck_->isChecked() ? data_.points() : data_.displaySample(), true);
  updateCloudLabels();
  measurePoints_.clear(); updateMeasureLabels();
  setStatus(QStringLiteral("已变换 %1 个点；可通过导出 PCD 或 PLY 保存").arg(points.size()));
}
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
