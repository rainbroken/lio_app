#include "transformdialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QQuaternion>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <cmath>

TransformDialog::TransformDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(QStringLiteral("旋转点云"));
  resize(720, 440);
  auto *layout = new QVBoxLayout(this);
  auto *tabs = new QTabWidget(this);
  auto *matrixPage = new QWidget;
  auto *matrixLayout = new QVBoxLayout(matrixPage);
  matrixLayout->addWidget(new QLabel(QStringLiteral("4×4 变换矩阵（每行四个数，包含平移）")));
  matrixEdit_ = new QPlainTextEdit;
  matrixEdit_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  matrixLayout->addWidget(matrixEdit_);
  tabs->addTab(matrixPage, QStringLiteral("旋转矩阵 4×4"));

  auto *paramsPage = new QWidget;
  auto *paramsLayout = new QVBoxLayout(paramsPage);
  paramsLayout->setContentsMargins(16, 18, 16, 16);
  paramsLayout->setSpacing(12);
  auto makeRow = [](QWidget *parent, QDoubleSpinBox **spins, double defaultZ) {
    auto *row = new QHBoxLayout;
    const char *names[] = {"X", "Y", "Z"};
    for (int i = 0; i < 3; ++i) {
      row->addWidget(new QLabel(QLatin1String(names[i]), parent));
      spins[i] = new QDoubleSpinBox(parent);
      spins[i]->setDecimals(8);
      spins[i]->setRange(-100000000.0, 100000000.0);
      spins[i]->setValue(i == 2 ? defaultZ : 0.0);
      row->addWidget(spins[i], 1);
    }
    return row;
  };
  paramsLayout->addWidget(new QLabel(QStringLiteral("旋转轴")));
  paramsLayout->addLayout(makeRow(paramsPage, axis_, 1.0));
  paramsLayout->addSpacing(8);
  paramsLayout->addWidget(new QLabel(QStringLiteral("旋转角度（度）")));
  auto *angleLayout = new QHBoxLayout;
  angle_ = new QDoubleSpinBox(paramsPage);
  angle_->setDecimals(4); angle_->setRange(-360000.0, 360000.0); angle_->setSuffix(QStringLiteral("°"));
  angleLayout->addWidget(angle_); angleLayout->addStretch();
  paramsLayout->addLayout(angleLayout);
  paramsLayout->addSpacing(8);
  paramsLayout->addWidget(new QLabel(QStringLiteral("平移")));
  paramsLayout->addLayout(makeRow(paramsPage, translation_, 0.0));
  paramsLayout->addStretch();
  tabs->addTab(paramsPage, QStringLiteral("旋转平移参数"));
  layout->addWidget(tabs);

  auto *actions = new QHBoxLayout;
  auto *reset = new QPushButton(QStringLiteral("重置"));
  actions->addWidget(reset); actions->addStretch();
  auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  apply_ = buttons->button(QDialogButtonBox::Ok);
  actions->addWidget(buttons); layout->addLayout(actions);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(buttons, &QDialogButtonBox::accepted, this, [this] {
    QMatrix4x4 matrix;
    if (!parseMatrix(&matrix)) { QMessageBox::warning(this, QStringLiteral("矩阵无效"), QStringLiteral("请输入有效的刚体变换矩阵：3×3 旋转部分须正交，末行为 0 0 0 1。")); return; }
    transform_ = matrix;
    accept();
  });
  connect(matrixEdit_, &QPlainTextEdit::textChanged, this, &TransformDialog::updateFromMatrix);
  for (auto *spin : axis_) connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &TransformDialog::updateFromParameters);
  for (auto *spin : translation_) connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &TransformDialog::updateFromParameters);
  connect(angle_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &TransformDialog::updateFromParameters);
  connect(reset, &QPushButton::clicked, this, [this] { transform_.setToIdentity(); updateMatrixText(); updateParameters(); });
  updateMatrixText();
}

void TransformDialog::updateMatrixText() {
  updating_ = true;
  QString value;
  QTextStream stream(&value);
  stream.setRealNumberNotation(QTextStream::FixedNotation);
  stream.setRealNumberPrecision(8);
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) stream << (col ? "  " : "") << transform_(row, col);
    if (row != 3) stream << '\n';
  }
  matrixEdit_->setPlainText(value);
  apply_->setEnabled(true);
  updating_ = false;
}

bool TransformDialog::parseMatrix(QMatrix4x4 *result) const {
  const QStringList rows = matrixEdit_->toPlainText().trimmed().split('\n');
  if (rows.size() != 4) return false;
  QMatrix4x4 matrix;
  for (int row = 0; row < 4; ++row) {
    const QStringList values = rows[row].trimmed().split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (values.size() != 4) return false;
    for (int col = 0; col < 4; ++col) {
      bool ok = false;
      const double value = values[col].toDouble(&ok);
      if (!ok || !std::isfinite(value)) return false;
      matrix(row, col) = float(value);
    }
  }
  if (std::abs(matrix(3, 0)) > 1e-4 || std::abs(matrix(3, 1)) > 1e-4 ||
      std::abs(matrix(3, 2)) > 1e-4 || std::abs(matrix(3, 3) - 1.0f) > 1e-4) return false;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      double dot = 0;
      for (int k = 0; k < 3; ++k) dot += double(matrix(k, i)) * matrix(k, j);
      if (std::abs(dot - (i == j ? 1.0 : 0.0)) > 1e-3) return false;
    }
  }
  if (matrix.determinant() < 0.0f) return false;
  *result = matrix;
  return true;
}

void TransformDialog::updateParameters() {
  updating_ = true;
  QMatrix3x3 rotation = transform_.normalMatrix();
  const QQuaternion quaternion = QQuaternion::fromRotationMatrix(rotation);
  QVector3D axis;
  float angle;
  quaternion.getAxisAndAngle(&axis, &angle);
  if (angle < 1e-6f) axis = QVector3D(0, 0, 1);
  for (int i = 0; i < 3; ++i) {
    axis_[i]->setValue(axis[i]);
    translation_[i]->setValue(transform_(i, 3));
  }
  angle_->setValue(angle);
  updating_ = false;
}

void TransformDialog::updateFromParameters() {
  if (updating_) return;
  QVector3D axis(axis_[0]->value(), axis_[1]->value(), axis_[2]->value());
  if (axis.lengthSquared() < 1e-16f && std::abs(angle_->value()) > 1e-8) {
    apply_->setEnabled(false);
    return;
  }
  transform_.setToIdentity();
  if (axis.lengthSquared() > 1e-16f) transform_.rotate(float(angle_->value()), axis.normalized());
  for (int i = 0; i < 3; ++i) transform_(i, 3) = float(translation_[i]->value());
  updateMatrixText();
}

void TransformDialog::updateFromMatrix() {
  if (updating_) return;
  QMatrix4x4 matrix;
  const bool valid = parseMatrix(&matrix);
  apply_->setEnabled(valid);
  if (!valid) return;
  transform_ = matrix;
  updateParameters();
}
