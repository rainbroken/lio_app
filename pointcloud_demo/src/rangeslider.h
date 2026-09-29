#pragma once

#include <QMouseEvent>
#include <QPainter>
#include <QWidget>
#include <algorithm>
#include <functional>

class RangeSlider : public QWidget {
public:
  explicit RangeSlider(QWidget *parent = nullptr) : QWidget(parent) {
    setFixedHeight(26);
    setMinimumWidth(120);
    setCursor(Qt::PointingHandCursor);
  }

  void setValues(int minValue, int maxValue) {
    minValue_ = std::clamp(minValue, 0, 10000);
    maxValue_ = std::clamp(maxValue, minValue_, 10000);
    update();
  }
  int minValue() const { return minValue_; }
  int maxValue() const { return maxValue_; }
  void onChange(std::function<void(int, int)> callback) { onChange_ = std::move(callback); }

protected:
  void paintEvent(QPaintEvent *) override {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const int left = 8, right = width() - 8, y = height() / 2;
    auto xFor = [&](int value) { return left + (right - left) * value / 10000.0; };
    painter.setPen(QPen(QColor("#334253"), 5, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(left, y), QPointF(right, y));
    painter.setPen(QPen(QColor("#35a7ff"), 5, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(xFor(minValue_), y), QPointF(xFor(maxValue_), y));
    painter.setPen(QPen(QColor("#0b1017"), 1));
    painter.setBrush(QColor("#e7edf5"));
    painter.drawEllipse(QPointF(xFor(minValue_), y), 7, 7);
    painter.drawEllipse(QPointF(xFor(maxValue_), y), 7, 7);
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() != Qt::LeftButton) return;
    const int value = valueAt(event->position().x());
    activeMin_ = std::abs(value - minValue_) <= std::abs(value - maxValue_);
    moveHandle(value);
  }
  void mouseMoveEvent(QMouseEvent *event) override {
    if (event->buttons() & Qt::LeftButton) moveHandle(valueAt(event->position().x()));
  }
  void mouseReleaseEvent(QMouseEvent *) override { activeMin_ = false; }

private:
  int valueAt(double x) const {
    return std::clamp(int((x - 8) * 10000 / std::max(1, width() - 16)), 0, 10000);
  }
  void moveHandle(int value) {
    if (activeMin_) minValue_ = std::min(value, maxValue_);
    else maxValue_ = std::max(value, minValue_);
    update();
    if (onChange_) onChange_(minValue_, maxValue_);
  }

  int minValue_ = 0;
  int maxValue_ = 10000;
  bool activeMin_ = false;
  std::function<void(int, int)> onChange_;
};
