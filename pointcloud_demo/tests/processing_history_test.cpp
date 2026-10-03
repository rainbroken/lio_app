#include "mainwindow.h"

#include <QApplication>
#include <QLabel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QElapsedTimer>
#include <QThread>
#include <cstdio>

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  if (argc > 1) {
    MainWindow window;
    QElapsedTimer timer; timer.start();
    if (!window.loadFilePath(QString::fromLocal8Bit(argv[1]))) return 20;
    const qint64 loadMs=timer.restart();
    QComboBox *mode=nullptr;
    QPushButton *preview=nullptr, *apply=nullptr;
    for (auto *combo : window.findChildren<QComboBox*>()) if (combo->findText(QStringLiteral("体素降采样"))>=0) mode=combo;
    for (auto *button : window.findChildren<QPushButton*>()) {
      if (button->text()==QStringLiteral("预览处理")) preview=button;
      if (button->text()==QStringLiteral("应用处理")) apply=button;
    }
    if (!mode || !preview || !apply) return 21;
    mode->setCurrentText(QStringLiteral("体素降采样"));
    for (auto *spin : window.findChildren<QDoubleSpinBox*>()) if (spin->value()==0.02) spin->setValue(0.03);
    preview->click();
    while (!apply->isEnabled() && timer.elapsed()<60000) { app.processEvents(); QThread::msleep(2); }
    if (!apply->isEnabled()) return 22;
    const qint64 previewMs=timer.restart();
    apply->click();
    const qint64 applyMs=timer.restart();
    QMetaObject::invokeMethod(&window,"undo");
    const qint64 undoMs=timer.elapsed();
    std::printf("window headless: load %lld ms, preview %lld ms, apply %lld ms, undo %lld ms\n",
      static_cast<long long>(loadMs),static_cast<long long>(previewMs),static_cast<long long>(applyMs),static_cast<long long>(undoMs));
    return 0;
  }
  QTemporaryDir dir;
  if (!dir.isValid()) return 1;
  PointCloudData source;
  source.setPoints({{0, 0, 0, 1, 0, 0}, {10, 0, 0, 0, 1, 0}, {20, 0, 0, 0, 0, 1}});
  const QString path = dir.filePath(QStringLiteral("source.pcd"));
  if (!source.exportPcd(path)) return 2;
  MainWindow window;
  if (!window.loadFilePath(path)) return 3;
  auto currentCount = [&window]() {
    for (auto *label : window.findChildren<QLabel*>())
      if (label->text().startsWith(QStringLiteral("当前点数:"))) return label->text();
    return QString();
  };
  if (!currentCount().startsWith(QStringLiteral("当前点数: 3"))) return 4;
  QVector<RangeSlider*> sliders;
  for (auto *widget : window.findChildren<QWidget*>())
    if (auto *slider = dynamic_cast<RangeSlider*>(widget)) sliders.append(slider);
  if (sliders.size() != 3) return 5;
  sliders[0]->setValues(0, 5000);
  QMetaObject::invokeMethod(&window, "updateCropRange");
  if (!currentCount().startsWith(QStringLiteral("当前点数: 3"))) return 6;
  QPushButton *apply = nullptr;
  for (auto *button : window.findChildren<QPushButton*>())
    if (button->text() == QStringLiteral("应用裁剪")) apply = button;
  if (!apply) return 7;
  apply->click();
  if (!currentCount().startsWith(QStringLiteral("当前点数: 2"))) return 8;
  QMetaObject::invokeMethod(&window, "undo");
  if (!currentCount().startsWith(QStringLiteral("当前点数: 3"))) return 9;
  QMetaObject::invokeMethod(&window, "redo");
  if (!currentCount().startsWith(QStringLiteral("当前点数: 2"))) return 10;
  auto *frame = window.findChild<QLineEdit*>();
  if (!frame) return 11;
  frame->setText(QStringLiteral("map_test"));
  QComboBox *units = nullptr;
  for (auto *combo : window.findChildren<QComboBox*>()) if (combo->findText(QStringLiteral("m")) >= 0) units = combo;
  if (!units) return 12;
  units->setCurrentText(QStringLiteral("m"));
  for (int i=0; i<2; ++i) {
    QMetaObject::invokeMethod(&window, "receivePoint", Q_ARG(QVector3D, QVector3D(0,0,0)));
    QMetaObject::invokeMethod(&window, "receivePoint", Q_ARG(QVector3D, QVector3D(float(i+1),0,0)));
  }
  QTreeWidget *records = nullptr;
  for (auto *tree : window.findChildren<QTreeWidget*>())
    if (tree->headerItem() && tree->headerItem()->text(0)==QStringLiteral("记录")) records=tree;
  if (!records || records->topLevelItemCount()!=2 || records->topLevelItem(1)->text(1)!=QStringLiteral("2.000 m")) return 13;
  QPushButton *remove=nullptr, *preview=nullptr, *applyProcess=nullptr;
  for (auto *button : window.findChildren<QPushButton*>()) {
    if (button->text()==QStringLiteral("删除选中")) remove=button;
    if (button->text()==QStringLiteral("预览处理")) preview=button;
    if (button->text()==QStringLiteral("应用处理")) applyProcess=button;
  }
  if (!remove || !preview || !applyProcess) return 14;
  records->setCurrentItem(records->topLevelItem(0)); remove->click();
  if (records->topLevelItemCount()!=1) return 15;
  QComboBox *mode=nullptr;
  for (auto *combo : window.findChildren<QComboBox*>()) if (combo->findText(QStringLiteral("体素降采样"))>=0) mode=combo;
  if (!mode) return 16;
  mode->setCurrentText(QStringLiteral("体素降采样"));
  for (auto *spin : window.findChildren<QDoubleSpinBox*>()) if (spin->value()==0.02) spin->setValue(30.0);
  preview->click();
  QElapsedTimer timer; timer.start();
  while (!applyProcess->isEnabled() && timer.elapsed()<3000) { app.processEvents(); QThread::msleep(1); }
  if (!applyProcess->isEnabled()) return 17;
  applyProcess->click();
  if (!currentCount().startsWith(QStringLiteral("当前点数: 1"))) return 18;
  QMetaObject::invokeMethod(&window, "undo");
  if (!currentCount().startsWith(QStringLiteral("当前点数: 2"))) return 19;
  return 0;
}
