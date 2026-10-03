#include "mainwindow.h"
#include "transformdialog.h"

#include <QApplication>
#include <QActionGroup>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QFutureWatcher>
#include <QLineEdit>
#include <QSpinBox>
#include <QtConcurrent>
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
#include <QSaveFile>
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
  processWatcher_ = new QFutureWatcher<ProcessingResult>(this);
  connect(processWatcher_, &QFutureWatcher<ProcessingResult>::finished, this, [this] {
    processPreviewButton_->setEnabled(true);
    const auto result = processWatcher_->result();
    if (processGeneration_ != processWatcher_->property("generation").toInt() || !activePcdItem_ || activePcdItem_->checkState(0) != Qt::Checked) return;
    if (!result.ok()) { processInfo_->setText(result.error); setStatus(result.error); return; }
    processPreview_ = result;
    processPreviewActive_ = true;
    processApplyButton_->setEnabled(true);
    PointCloudData preview; preview.setPoints(result.points);
    view_->setBounds(preview.minBound(), preview.maxBound());
    view_->setCropRange(preview.minBound(), preview.maxBound());
    view_->setPoints(showAllCheck_->isChecked() ? result.points : preview.displaySample(), true);
    updateCloudLabels();
    processInfo_->setText(QStringLiteral("前 %1 → 后 %2 点（变化 %3）\n%4")
        .arg(data_.points().size()).arg(result.points.size()).arg(qint64(result.points.size()) - data_.points().size()).arg(result.detail));
    previewLabel_->setText(QStringLiteral("正在显示处理预览；PCD/PLY 导出取预览"));
  });
  auto *fileMenu = menuBar()->addMenu(QStringLiteral("文件"));
  auto *openAction = fileMenu->addAction(QStringLiteral("打开 PCD"), this, &MainWindow::openFile);
  openAction->setShortcut(QKeySequence::Open);
  auto *saveAction = fileMenu->addAction(QStringLiteral("保存当前点云为 PCD"), this, &MainWindow::exportPcd);
  saveAction->setShortcut(QKeySequence::Save);
  auto *shotAction = fileMenu->addAction(QStringLiteral("保存截图"), this, &MainWindow::saveScreenshot);
  shotAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
  fileMenu->addSeparator(); fileMenu->addAction(QStringLiteral("退出"), this, &QWidget::close);
  auto *editMenu = menuBar()->addMenu(QStringLiteral("编辑"));
  undoAction_ = editMenu->addAction(QStringLiteral("撤销"), this, &MainWindow::undo); undoAction_->setShortcut(QKeySequence::Undo); undoAction_->setEnabled(false);
  redoAction_ = editMenu->addAction(QStringLiteral("重做"), this, &MainWindow::redo); redoAction_->setShortcut(QKeySequence::Redo); redoAction_->setEnabled(false);
  editMenu->addSeparator(); editMenu->addAction(QStringLiteral("变换"), this, &MainWindow::rotateCloud);
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
  for (auto *action : {openAction, saveAction, shotAction, undoAction_, redoAction_, zoomInAction, zoomOutAction, fitAction, full}) {
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
      previewLabel_->setText(QStringLiteral("未加载点云"));
    }
    for (auto *node = selectedPcdItem_; node; node = node->parent()) {
      if (node == item) { selectedPcdItem_ = nullptr; break; }
    }
    auto forgetCrops = [this](auto &&self, QTreeWidgetItem *node) -> void {
      histories_.remove(node);
      for (int i = 0; i < node->childCount(); ++i) self(self, node->child(i));
    };
    forgetCrops(forgetCrops, item);
    {
      QSignalBlocker blocker(tree);
      delete item;
    }
    if (removesActive) setWindowTitle(QStringLiteral("ScanForge · 点云后处理 Demo"));
    if (activePcdItem_) { const auto &history = histories_.value(activePcdItem_); undoAction_->setEnabled(history.position > 0); redoAction_->setEnabled(history.position < history.operations.size()); }
    else { undoAction_->setEnabled(false); redoAction_->setEnabled(false); }
    setStatus(QStringLiteral("已从文件树删除 %1").arg(name));
  });
  connect(tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
    if (column != 0 || !view_ || !selectedPcdItem_ || !(item->flags() & Qt::ItemIsUserCheckable)) return;
    const bool visible = item->checkState(0) == Qt::Checked;
    if (item == activePcdItem_ || visible) clearProcessingPreview();
    if (!visible) { if (item == activePcdItem_) { view_->setCloudVisible(false); previewLabel_->setText(QStringLiteral("点云已隐藏；勾选后可处理或导出")); undoAction_->setEnabled(false); redoAction_->setEnabled(false); } return; }
    QString error;
    const QString path = item->data(0, Qt::UserRole).toString();
    if (!histories_.contains(item)) {
      if (path.isEmpty()) return;
      PointCloudData loaded;
      if (!loaded.load(path, &error)) { QSignalBlocker blocker(fileTree_); item->setCheckState(0, Qt::Unchecked); QMessageBox::warning(this, QStringLiteral("加载失败"), error); return; }
      History history; history.original = loaded.points(); histories_.insert(item, std::move(history));
    }
    {
      QSignalBlocker blocker(fileTree_);
      if (activePcdItem_ && activePcdItem_ != item) activePcdItem_->setCheckState(0, Qt::Unchecked);
    }
    activePcdItem_ = item;
    restoreHistory();
    for (auto *slider : cropSliders_) slider->setValues(0, 10000);
    showCurrentCloud(); view_->setCloudVisible(true); setStatus(QStringLiteral("已显示 %1").arg(item->text(0)));
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
  previewLabel_ = new QLabel(QStringLiteral("未加载点云")); previewLabel_->setObjectName("muted"); previewLabel_->setWordWrap(true); rl->addWidget(previewLabel_);
  rl->addSpacing(10); rl->addWidget(sectionTitle(QStringLiteral("三轴裁剪 · XYZ")));
  const QStringList axisNames{QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")};
  for (int axis = 0; axis < 3; ++axis) {
    rl->addWidget(new QLabel(axisNames[axis] + QStringLiteral(" 范围")));
    auto *labels = new QHBoxLayout; auto *minLabel = new QLabel(QStringLiteral("最小 —")); auto *maxLabel = new QLabel(QStringLiteral("最大 —")); labels->addWidget(minLabel); labels->addStretch(); labels->addWidget(maxLabel); rl->addLayout(labels);
    auto *slider = new RangeSlider; rl->addWidget(slider); cropSliders_.append(slider); cropMinLabels_.append(minLabel); cropMaxLabels_.append(maxLabel);
    slider->onChange([this](int, int){ if (processPreviewActive_) clearProcessingPreview(); updateCropRange(); });
  }
  rl->addWidget(button(QStringLiteral("应用裁剪"), "primary")); auto *confirmCrop = qobject_cast<QPushButton*>(rl->itemAt(rl->count()-1)->widget());
  rl->addSpacing(10); rl->addWidget(sectionTitle(QStringLiteral("清理与调平")));
  processMode_ = new QComboBox;
  processMode_->addItems({QStringLiteral("区域删除"), QStringLiteral("半径离群过滤"), QStringLiteral("体素降采样"), QStringLiteral("地面拟合调平")});
  rl->addWidget(processMode_);
  auto *processForm = new QFormLayout;
  radiusInput_ = new QDoubleSpinBox; radiusInput_->setRange(0.001, 1000); radiusInput_->setDecimals(3); radiusInput_->setValue(0.05);
  neighborsInput_ = new QSpinBox; neighborsInput_->setRange(1, 100); neighborsInput_->setValue(3);
  voxelInput_ = new QDoubleSpinBox; voxelInput_->setRange(0.001, 1000); voxelInput_->setDecimals(3); voxelInput_->setValue(0.02);
  groundInput_ = new QDoubleSpinBox; groundInput_->setRange(0.001, 1000); groundInput_->setDecimals(3); groundInput_->setValue(0.03);
  processForm->addRow(QStringLiteral("搜索半径（坐标单位）"), radiusInput_); processForm->addRow(QStringLiteral("最少邻点"), neighborsInput_);
  processForm->addRow(QStringLiteral("体素边长（坐标单位）"), voxelInput_); processForm->addRow(QStringLiteral("平面容差（坐标单位）"), groundInput_); rl->addLayout(processForm);
  groundInput_->setToolTip(QStringLiteral("假定 Z 轴向上；可用 XYZ 范围选择拟合区域，变换仍作用于整云。"));
  auto *processButtons = new QHBoxLayout;
  processPreviewButton_ = button(QStringLiteral("预览处理")); processApplyButton_ = button(QStringLiteral("应用处理"), "primary"); processApplyButton_->setEnabled(false);
  auto *cancelProcessPreview = button(QStringLiteral("取消预览"));
  processButtons->addWidget(processPreviewButton_); processButtons->addWidget(processApplyButton_); processButtons->addWidget(cancelProcessPreview); rl->addLayout(processButtons);
  processInfo_ = new QLabel(QStringLiteral("待预览")); processInfo_->setObjectName("muted"); processInfo_->setWordWrap(true); rl->addWidget(processInfo_);
  connect(processPreviewButton_, &QPushButton::clicked, this, &MainWindow::previewProcessing);
  connect(processApplyButton_, &QPushButton::clicked, this, &MainWindow::applyProcessing);
  connect(cancelProcessPreview, &QPushButton::clicked, this, &MainWindow::clearProcessingPreview);
  connect(processMode_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int mode) {
    radiusInput_->setEnabled(mode == 1); neighborsInput_->setEnabled(mode == 1);
    voxelInput_->setEnabled(mode == 2); groundInput_->setEnabled(mode == 3);
    clearProcessingPreview();
  });
  for (auto *spin : {radiusInput_, voxelInput_, groundInput_}) connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double) { clearProcessingPreview(); });
  connect(neighborsInput_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) { clearProcessingPreview(); });
  radiusInput_->setEnabled(false); neighborsInput_->setEnabled(false); voxelInput_->setEnabled(false); groundInput_->setEnabled(false);
  rl->addSpacing(10); rl->addWidget(sectionTitle(QStringLiteral("编辑和测量"))); auto *measure = button(QStringLiteral("⌖ 两点测量")); measure->setObjectName("primary"); rl->addWidget(measure); auto *fit = button(QStringLiteral("⊙ 适配视图")); rl->addWidget(fit);
  auto *metadata = new QFormLayout;
  frameEdit_ = new QLineEdit; frameEdit_->setPlaceholderText(QStringLiteral("未指定"));
  unitBox_ = new QComboBox; unitBox_->addItems({QStringLiteral("未指定"), QStringLiteral("m"), QStringLiteral("cm"), QStringLiteral("mm")});
  metadata->addRow(QStringLiteral("坐标系"), frameEdit_); metadata->addRow(QStringLiteral("坐标单位"), unitBox_); rl->addLayout(metadata);
  boundsLabel_->setWordWrap(true);
  connect(frameEdit_, &QLineEdit::textChanged, this, [this] { if (!data_.points().isEmpty()) updateCloudLabels(); updateMeasureLabels(); });
  connect(unitBox_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { if (!data_.points().isEmpty()) updateCloudLabels(); updateMeasureLabels(); });
  measureLabel_ = new QLabel(QStringLiteral("状态：未选择测量点\n距离 —\n水平距离 —\n高度差 —")); measureLabel_->setWordWrap(true); measureLabel_->setStyleSheet("background:#0c131b; border:1px solid #2a3949; border-radius:6px; padding:10px; line-height:1.5;"); rl->addWidget(measureLabel_);
  measurementTree_ = new QTreeWidget; measurementTree_->setColumnCount(2); measurementTree_->setHeaderLabels({QStringLiteral("记录"), QStringLiteral("距离")}); measurementTree_->setFixedHeight(115); rl->addWidget(measurementTree_);
  auto *measurementActions = new QHBoxLayout;
  auto *removeMeasurement = button(QStringLiteral("删除选中")); auto *csv = button(QStringLiteral("导出全部测量 CSV"));
  measurementActions->addWidget(removeMeasurement); measurementActions->addWidget(csv); rl->addLayout(measurementActions);
  connect(removeMeasurement, &QPushButton::clicked, this, [this] {
    const int index = measurementTree_->indexOfTopLevelItem(measurementTree_->currentItem());
    if (index < 0) return;
    delete measurementTree_->takeTopLevelItem(index);
    measurements_.removeAt(index);
    for (int i=0; i<measurementTree_->topLevelItemCount(); ++i)
      measurementTree_->topLevelItem(i)->setText(0, QStringLiteral("#%1 %2").arg(i+1).arg(measurements_[i].frame));
    setStatus(QStringLiteral("已删除测量记录"));
  });
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
    if (processPreviewActive_) { setStatus(QStringLiteral("请先应用或取消处理预览")); return; }
    if (!activePcdItem_ || activePcdItem_->checkState(0) != Qt::Checked || data_.points().isEmpty()) { setStatus(QStringLiteral("请先勾选点云")); return; }
    if (!hasPreview()) { setStatus(QStringLiteral("请先调整裁剪范围")); return; }
    const PointCloudData preview = previewData();
    if (preview.points().isEmpty()) { setStatus(QStringLiteral("裁剪范围内没有点")); return; }
    CloudOperation operation; operation.kind = CloudOperation::Crop; operation.min = cropMin_; operation.max = cropMax_;
    applyOperation(operation);
    setStatus(QStringLiteral("已应用裁剪，当前 %1 点；可撤销").arg(data_.points().size()));
  });
  setCentralWidget(root);
  QTimer::singleShot(0, this, [split] { split->setSizes({58, 238, std::max(400, split->width() - 660), 364}); });
}

void MainWindow::updateCloudLabels() { pointLabel_->setText(QStringLiteral("当前点数: %1（显示采样 %2）").arg(data_.points().size()).arg(view_->pointCount())); boundsLabel_->setText(QStringLiteral("原始点数: %1\nX %2 ~ %3\nY %4 ~ %5\nZ %6 ~ %7\n单位 %8 · 坐标系 %9").arg(activePcdItem_ ? histories_.value(activePcdItem_).original.size() : 0).arg(data_.minBound().x(),0,'f',2).arg(data_.maxBound().x(),0,'f',2).arg(data_.minBound().y(),0,'f',2).arg(data_.maxBound().y(),0,'f',2).arg(data_.minBound().z(),0,'f',2).arg(data_.maxBound().z(),0,'f',2).arg(unitBox_ ? unitBox_->currentText() : QStringLiteral("未指定")).arg(frameEdit_ && !frameEdit_->text().trimmed().isEmpty() ? frameEdit_->text().trimmed() : QStringLiteral("未指定"))); }
void MainWindow::openFile() { const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开 PCD 点云"), QStringLiteral("../prev"), QStringLiteral("Point Cloud (*.pcd)")); if (!path.isEmpty()) loadFilePath(path); }
bool MainWindow::loadFilePath(const QString &path) { QApplication::setOverrideCursor(Qt::WaitCursor); QString error; PointCloudData loaded; const bool ok = loaded.load(path, &error); QApplication::restoreOverrideCursor(); if (!ok) { QMessageBox::critical(this, QStringLiteral("打开失败"), error); return false; }
  clearProcessingPreview();
  frameEdit_->clear(); unitBox_->setCurrentIndex(0);
  QSignalBlocker blocker(fileTree_);
  activePcdItem_ = nullptr; selectedPcdItem_ = nullptr; histories_.clear(); fileTree_->clear(); auto *root = new QTreeWidgetItem(fileTree_, {QFileInfo(path).completeBaseName()}); root->setExpanded(true); selectedPcdItem_ = new QTreeWidgetItem(root, {QFileInfo(path).fileName()}); selectedPcdItem_->setData(0, Qt::UserRole, path); selectedPcdItem_->setFlags(selectedPcdItem_->flags() | Qt::ItemIsUserCheckable); selectedPcdItem_->setCheckState(0, Qt::Checked); selectedPcdItem_->setSelected(true); activePcdItem_ = selectedPcdItem_;
  History history; history.original = loaded.points(); histories_.insert(activePcdItem_, std::move(history)); data_ = std::move(loaded);
  for(auto *slider : cropSliders_) slider->setValues(0, 10000);
  showCurrentCloud(true); measurePoints_.clear(); updateMeasureLabels(); qInfo().noquote() << QStringLiteral("Loaded %1 points; bounds X[%2,%3] Y[%4,%5] Z[%6,%7]").arg(data_.points().size()).arg(data_.minBound().x()).arg(data_.maxBound().x()).arg(data_.minBound().y()).arg(data_.maxBound().y()).arg(data_.minBound().z()).arg(data_.maxBound().z()); setWindowTitle(QStringLiteral("ScanForge · %1").arg(QFileInfo(path).fileName())); setStatus(QStringLiteral("已加载 %1 · %2 个点").arg(QFileInfo(path).fileName()).arg(data_.points().size())); return true; }
bool MainWindow::hasPreview() const { for (auto *slider : cropSliders_) if (slider->minValue() != 0 || slider->maxValue() != 10000) return true; return false; }
PointCloudData MainWindow::previewData() const { PointCloudData result; if (processPreviewActive_) { result.setPoints(processPreview_.points); return result; } if (!hasPreview()) { result.setPoints(data_.points()); return result; } QVector<CloudPoint> points; for (const auto &p : data_.points()) if (p.x >= cropMin_.x() && p.x <= cropMax_.x() && p.y >= cropMin_.y() && p.y <= cropMax_.y() && p.z >= cropMin_.z() && p.z <= cropMax_.z()) points.append(p); result.setPoints(points); return result; }
void MainWindow::updateCropRange() { if (data_.points().isEmpty() || cropSliders_.size()!=3) return; for(int i=0;i<3;++i) { const float lo=data_.minBound()[i], hi=data_.maxBound()[i]; cropMin_[i]=cropSliders_[i]->minValue() == 0 ? lo : lo+(hi-lo)*cropSliders_[i]->minValue()/10000.0; cropMax_[i]=cropSliders_[i]->maxValue() == 10000 ? hi : lo+(hi-lo)*cropSliders_[i]->maxValue()/10000.0; cropMinLabels_[i]->setText(QStringLiteral("最小 %1").arg(cropMin_[i],0,'f',2)); cropMaxLabels_[i]->setText(QStringLiteral("最大 %1").arg(cropMax_[i],0,'f',2)); } view_->setCropRange(cropMin_,cropMax_); if (hasPreview()) { qsizetype count=0; for (const auto &p : data_.points()) if (p.x >= cropMin_.x() && p.x <= cropMax_.x() && p.y >= cropMin_.y() && p.y <= cropMax_.y() && p.z >= cropMin_.z() && p.z <= cropMax_.z()) ++count; previewLabel_->setText(QStringLiteral("预览 %1 / 当前 %2 点；导出取预览，应用后可撤销").arg(count).arg(data_.points().size())); } else previewLabel_->setText(QStringLiteral("当前 %1 点；导出取当前点云").arg(data_.points().size())); }
void MainWindow::showCurrentCloud(bool resetView) { if (data_.points().isEmpty()) return; view_->setBounds(data_.minBound(), data_.maxBound(), !resetView); updateCropRange(); view_->setPoints(showAllCheck_->isChecked() ? data_.points() : data_.displaySample(), resetView); updateCloudLabels(); const auto &history = histories_.value(activePcdItem_); undoAction_->setEnabled(history.position > 0); redoAction_->setEnabled(history.position < history.operations.size()); }
void MainWindow::restoreHistory() { const auto &history = histories_.value(activePcdItem_); QVector<CloudPoint> points = history.original; for (int i=0; i<history.position; ++i) { ProcessingResult result = processCloud(points, history.operations[i]); if (!result.ok()) { qWarning().noquote() << result.error; return; } points = std::move(result.points); } data_.setPoints(points); }
void MainWindow::applyOperation(const CloudOperation &operation) { clearProcessingPreview(); auto &history=histories_[activePcdItem_]; history.operations.resize(history.position); history.operations.append(operation); ++history.position; restoreHistory(); if (operation.kind == CloudOperation::Transform || operation.kind == CloudOperation::LevelGround) frameEdit_->clear(); for (auto *slider : cropSliders_) slider->setValues(0,10000); showCurrentCloud(true); measurePoints_.clear(); updateMeasureLabels(); }
void MainWindow::undo() { if (!activePcdItem_ || histories_[activePcdItem_].position == 0) return; clearProcessingPreview(); --histories_[activePcdItem_].position; restoreHistory(); for (auto *slider : cropSliders_) slider->setValues(0,10000); showCurrentCloud(true); measurePoints_.clear(); updateMeasureLabels(); setStatus(QStringLiteral("已撤销，当前 %1 点").arg(data_.points().size())); }
void MainWindow::redo() { if (!activePcdItem_ || histories_[activePcdItem_].position >= histories_[activePcdItem_].operations.size()) return; clearProcessingPreview(); ++histories_[activePcdItem_].position; restoreHistory(); for (auto *slider : cropSliders_) slider->setValues(0,10000); showCurrentCloud(true); measurePoints_.clear(); updateMeasureLabels(); setStatus(QStringLiteral("已重做，当前 %1 点").arg(data_.points().size())); }
void MainWindow::updateDisplayMode() { if (data_.points().isEmpty()) return; if (processPreviewActive_) { PointCloudData preview; preview.setPoints(processPreview_.points); view_->setPoints(showAllCheck_->isChecked() ? preview.points() : preview.displaySample()); } else view_->setPoints(showAllCheck_->isChecked() ? data_.points() : data_.displaySample()); updateCloudLabels(); setStatus(showAllCheck_->isChecked() ? QStringLiteral("已切换为全部显示") : QStringLiteral("已切换为采样显示")); }
void MainWindow::toggleFullscreen() { isFullScreen() ? showNormal() : showFullScreen(); }
void MainWindow::rotateCloud() {
  if (processPreviewActive_) { setStatus(QStringLiteral("请先应用或取消处理预览")); return; }
  if (!activePcdItem_ || data_.points().isEmpty() || activePcdItem_->checkState(0) != Qt::Checked) {
    QMessageBox::information(this, QStringLiteral("没有选中点云"), QStringLiteral("请先在文件树中勾选要旋转的点云。"));
    return;
  }
  TransformDialog dialog(this);
  if (dialog.exec() != QDialog::Accepted) return;
  if (hasPreview()) {
    if (previewData().points().isEmpty()) { setStatus(QStringLiteral("裁剪范围内没有点")); return; }
    CloudOperation crop; crop.kind = CloudOperation::Crop; crop.min = cropMin_; crop.max = cropMax_; applyOperation(crop);
  }
  CloudOperation operation; operation.kind = CloudOperation::Transform; operation.matrix = dialog.transform();
  applyOperation(operation);
  setStatus(QStringLiteral("已变换 %1 个点；可撤销").arg(data_.points().size()));
}
void MainWindow::clearProcessingPreview() {
  ++processGeneration_;
  const bool wasActive = processPreviewActive_;
  processPreviewActive_ = false;
  processPreview_.points.clear();
  if (processApplyButton_) processApplyButton_->setEnabled(false);
  if (processInfo_) processInfo_->setText(QStringLiteral("待预览"));
  if (wasActive && activePcdItem_ && !data_.points().isEmpty()) showCurrentCloud();
}
void MainWindow::previewProcessing() {
  if (!activePcdItem_ || activePcdItem_->checkState(0) != Qt::Checked || data_.points().isEmpty() || processWatcher_->isRunning()) return;
  CloudOperation operation;
  switch (processMode_->currentIndex()) {
    case 0:
      if (!hasPreview()) { setStatus(QStringLiteral("先用 XYZ 范围选择要删除的区域")); return; }
      operation.kind = CloudOperation::DeleteRegion; operation.min = cropMin_; operation.max = cropMax_; break;
    case 1: operation.kind = CloudOperation::RadiusOutlier; operation.radius = float(radiusInput_->value()); operation.minNeighbors = neighborsInput_->value(); break;
    case 2: operation.kind = CloudOperation::VoxelDownsample; operation.voxel = float(voxelInput_->value()); break;
    default: operation.kind = CloudOperation::LevelGround; operation.groundTolerance = float(groundInput_->value()); operation.useFitRegion = hasPreview(); operation.min = cropMin_; operation.max = cropMax_; break;
  }
  clearProcessingPreview();
  if (operation.kind == CloudOperation::RadiusOutlier || operation.kind == CloudOperation::VoxelDownsample) { for (auto *slider : cropSliders_) slider->setValues(0, 10000); updateCropRange(); }
  processPreviewButton_->setEnabled(false);
  processInfo_->setText(QStringLiteral("正在计算预览… 前 %1 点").arg(data_.points().size()));
  const int generation = ++processGeneration_;
  processWatcher_->setProperty("generation", generation);
  processWatcher_->setFuture(QtConcurrent::run([points = data_.points(), operation] { return processCloud(points, operation); }));
}
void MainWindow::applyProcessing() {
  if (!processPreviewActive_ || !activePcdItem_ || activePcdItem_->checkState(0) != Qt::Checked) return;
  const auto result = processPreview_;
  auto &history = histories_[activePcdItem_];
  history.operations.resize(history.position);
  history.operations.append(result.operation);
  ++history.position;
  const qsizetype before = data_.points().size();
  data_.setPoints(result.points);
  clearProcessingPreview();
  if (result.operation.kind == CloudOperation::LevelGround) frameEdit_->clear();
  for (auto *slider : cropSliders_) slider->setValues(0, 10000);
  showCurrentCloud(true);
  processInfo_->setText(QStringLiteral("已应用：前 %1 → 后 %2 点\n%3").arg(before).arg(data_.points().size()).arg(result.detail));
  measurePoints_.clear(); updateMeasureLabels();
  setStatus(QStringLiteral("已应用处理，当前 %1 点；可撤销").arg(data_.points().size()));
}
void MainWindow::chooseMeasureMode() { measurePoints_.clear(); updateMeasureLabels(); const bool enabled = !view_->measureMode(); view_->setMeasureMode(enabled); toolLabel_->setText(enabled ? QStringLiteral("● Engine Ready    Tool: Two Point Measure · 请点击两个点") : QStringLiteral("● Engine Ready    Tool: Orbit")); }
void MainWindow::receivePoint(const QVector3D &point) { if (measurePoints_.size() >= 2) measurePoints_.clear(); measurePoints_.append(point); if (measurePoints_.size() == 2) { const auto &a=measurePoints_[0], &b=measurePoints_[1]; Measurement record{a,b,frameEdit_->text().trimmed().isEmpty() ? QStringLiteral("未指定") : frameEdit_->text().trimmed(),unitBox_->currentText(),activePcdItem_ ? QStringLiteral("%1 / 步骤 %2").arg(activePcdItem_->text(0)).arg(histories_.value(activePcdItem_).position) : QStringLiteral("未知")}; measurements_.append(record); auto *item = new QTreeWidgetItem(measurementTree_, {QStringLiteral("#%1 %2").arg(measurements_.size()).arg(record.frame), QStringLiteral("%1 %2").arg((a-b).length(),0,'f',3).arg(record.unit)}); item->setToolTip(0, record.source); view_->setMeasureMode(false); toolLabel_->setText(QStringLiteral("● Engine Ready    Tool: Orbit · 测量完成")); } updateMeasureLabels(); }
void MainWindow::receivePreview(const QVector3D &point) { if (measurePoints_.size() != 1) return; const double distance = (measurePoints_[0] - point).length(); measureLabel_->setText(QStringLiteral("状态：已选择 1 / 2 个点\n预览距离 %1 %2\n坐标系 %3").arg(distance,0,'f',3).arg(unitBox_->currentText()).arg(frameEdit_->text().trimmed().isEmpty() ? QStringLiteral("未指定") : frameEdit_->text().trimmed())); }
void MainWindow::cancelMeasurement() { measurePoints_.clear(); updateMeasureLabels(); toolLabel_->setText(QStringLiteral("● Engine Ready    Tool: Two Point Measure · 已取消")); }
void MainWindow::updateMeasureLabels() { QString unit=unitBox_ ? unitBox_->currentText() : QStringLiteral("未指定"); QString frame=frameEdit_ && !frameEdit_->text().trimmed().isEmpty() ? frameEdit_->text().trimmed() : QStringLiteral("未指定"); if (measurePoints_.size() < 2) { measureLabel_->setText(QStringLiteral("状态：已选择 %1 / 2 个点\n距离 —\n水平距离 —\n高度差 —\n单位 %2 · 坐标系 %3").arg(measurePoints_.size()).arg(unit).arg(frame)); return; } if (!measurements_.isEmpty()) { unit=measurements_.last().unit; frame=measurements_.last().frame; } const auto &a = measurePoints_[0]; const auto &b = measurePoints_[1]; const double distance = (a-b).length(); const double horizontal = std::hypot(double(a.x()-b.x()), double(a.y()-b.y())); const double height = std::abs(double(a.z()-b.z())); measureLabel_->setText(QStringLiteral("状态：测量完成\n距离 %1 %4\n水平距离 %2 %4\n高度差 %3 %4\n坐标系 %5").arg(distance,0,'f',3).arg(horizontal,0,'f',3).arg(height,0,'f',3).arg(unit).arg(frame)); }
void MainWindow::exportPly() { if (!activePcdItem_ || activePcdItem_->checkState(0) != Qt::Checked || data_.points().isEmpty()) { setStatus(QStringLiteral("请先勾选点云")); return; } const PointCloudData output=previewData(); if (output.points().isEmpty()) { setStatus(QStringLiteral("预览范围内没有点")); return; } const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出 PLY"), QStringLiteral("current_cloud.ply"), QStringLiteral("PLY (*.ply)")); if (path.isEmpty()) return; QString error; if (!output.exportPly(path, &error)) QMessageBox::critical(this, QStringLiteral("导出失败"), error); else setStatus(QStringLiteral("已导出 %1 点到 %2").arg(output.points().size()).arg(path)); }
void MainWindow::exportPcd() { if (!activePcdItem_ || activePcdItem_->checkState(0) != Qt::Checked || data_.points().isEmpty()) { setStatus(QStringLiteral("请先勾选点云")); return; } const PointCloudData output=previewData(); if (output.points().isEmpty()) { setStatus(QStringLiteral("预览范围内没有点")); return; } const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出 PCD"), QStringLiteral("current_cloud.pcd"), QStringLiteral("PCD (*.pcd)")); if (path.isEmpty()) return; if (selectedPcdItem_ && QFileInfo(path).absoluteFilePath() == QFileInfo(selectedPcdItem_->data(0, Qt::UserRole).toString()).absoluteFilePath()) { QMessageBox::warning(this, QStringLiteral("不能覆盖原件"), QStringLiteral("请选择其他文件名，保留最初导入的 PCD。")); return; } QString error; if (!output.exportPcd(path, &error)) QMessageBox::critical(this, QStringLiteral("导出失败"), error); else { if (selectedPcdItem_) { QSignalBlocker blocker(fileTree_); auto *item = new QTreeWidgetItem(selectedPcdItem_, {QFileInfo(path).fileName()}); item->setData(0, Qt::UserRole, path); item->setFlags(item->flags() | Qt::ItemIsUserCheckable); item->setCheckState(0, Qt::Unchecked); selectedPcdItem_->setExpanded(true); } setStatus(QStringLiteral("已导出 %1 点到 %2；可在文件树勾选重开").arg(output.points().size()).arg(path)); } }
void MainWindow::exportCsv() {
  if (measurements_.isEmpty()) { QMessageBox::information(this, QStringLiteral("尚无测量"), QStringLiteral("请先完成至少一条两点测量。")); return; }
  const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出测量 CSV"), QStringLiteral("measurements.csv"), QStringLiteral("CSV (*.csv)"));
  if (path.isEmpty()) return;
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) { QMessageBox::critical(this, QStringLiteral("导出失败"), file.errorString()); return; }
  QTextStream out(&file); out.setLocale(QLocale::c()); out.setRealNumberPrecision(9);
  auto quoted = [](QString value) { value.replace(QLatin1Char('"'), QStringLiteral("\"\"")); return QStringLiteral("\"") + value + QStringLiteral("\""); };
  out << "id,source,frame,coordinate_unit,point_a_x,point_a_y,point_a_z,point_b_x,point_b_y,point_b_z,distance,horizontal_distance,height_difference\n";
  for (int i=0; i<measurements_.size(); ++i) {
    const auto &m=measurements_[i]; const auto &a=m.a, &b=m.b;
    out << i+1 << ',' << quoted(m.source) << ',' << quoted(m.frame) << ',' << quoted(m.unit) << ','
        << a.x() << ',' << a.y() << ',' << a.z() << ',' << b.x() << ',' << b.y() << ',' << b.z() << ','
        << (a-b).length() << ',' << std::hypot(double(a.x()-b.x()),double(a.y()-b.y())) << ',' << std::abs(double(a.z()-b.z())) << '\n';
  }
  out.flush();
  if (out.status() != QTextStream::Ok || !file.commit()) QMessageBox::critical(this, QStringLiteral("导出失败"), file.errorString());
  else setStatus(QStringLiteral("已导出 %1 条测量记录").arg(measurements_.size()));
}
void MainWindow::saveScreenshot() { const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存截图"), QStringLiteral("scanforge_snapshot.png"), QStringLiteral("PNG (*.png)")); if (!path.isEmpty() && view_->grab().save(path)) setStatus(QStringLiteral("已保存截图 %1").arg(path)); }
void MainWindow::setStatus(const QString &message) { statusBar()->showMessage(message, 5000); }
