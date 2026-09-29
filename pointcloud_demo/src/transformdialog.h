#pragma once

#include <QDialog>
#include <QMatrix4x4>

class QDoubleSpinBox;
class QPlainTextEdit;
class QPushButton;

class TransformDialog : public QDialog {
  Q_OBJECT
public:
  explicit TransformDialog(QWidget *parent = nullptr);
  QMatrix4x4 transform() const { return transform_; }

private:
  void updateMatrixText();
  void updateParameters();
  void updateFromParameters();
  bool parseMatrix(QMatrix4x4 *result) const;
  void updateFromMatrix();

  QPlainTextEdit *matrixEdit_ = nullptr;
  QDoubleSpinBox *axis_[3]{};
  QDoubleSpinBox *angle_ = nullptr;
  QDoubleSpinBox *translation_[3]{};
  QPushButton *apply_ = nullptr;
  QMatrix4x4 transform_;
  bool updating_ = false;
};
