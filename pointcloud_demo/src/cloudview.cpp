#include "cloudview.h"

#include <QMouseEvent>
#include <QPainter>
#include <QOpenGLShaderProgram>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>

CloudView::CloudView(QWidget *parent) : QOpenGLWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setMinimumSize(500, 400);
}

void CloudView::setPoints(const QVector<CloudPoint> &points, bool resetView) { points_ = points; upload(); if (resetView) fitView(); else update(); }
void CloudView::setBounds(const QVector3D &minBound, const QVector3D &maxBound) {
  minBound_ = minBound; maxBound_ = maxBound;
  center_ = (minBound_ + maxBound_) * 0.5f;
  radius_ = std::max(0.001f, (maxBound_ - minBound_).length() * 0.5f);
}
void CloudView::setCropRange(const QVector3D &minRange, const QVector3D &maxRange) { cropMin_ = minRange; cropMax_ = maxRange; update(); }
void CloudView::setPointSize(float size) { pointSize_ = size; update(); }
void CloudView::fitView() { distance_ = radius_ * 2.5f; yaw_ = 225.0f; pitch_ = 28.0f; update(); }
void CloudView::setMeasureMode(bool enabled) { measureMode_ = enabled; if (!enabled) { hasMeasureAnchor_ = false; hasMeasurePreview_ = false; } setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor); update(); }

void CloudView::initializeGL() {
  if (!initializeOpenGLFunctions()) { qWarning("OpenGL 3.3 core functions are unavailable"); return; }
  qInfo() << "OpenGL renderer:" << reinterpret_cast<const char *>(glGetString(GL_RENDERER))
          << "version:" << reinterpret_cast<const char *>(glGetString(GL_VERSION));
  glEnable(GL_PROGRAM_POINT_SIZE); glEnable(GL_DEPTH_TEST);
  const char *vs = "#version 330 core\nlayout(location=0) in vec3 position; layout(location=1) in vec3 color; out vec3 vColor; uniform mat4 mvp; uniform vec3 cropMin; uniform vec3 cropMax; uniform float pointSize; void main(){ if(any(lessThan(position,cropMin)) || any(greaterThan(position,cropMax))){ gl_Position=vec4(2.0,2.0,2.0,1.0); gl_PointSize=0.0; } else { gl_Position=mvp*vec4(position,1.0); gl_PointSize=pointSize; } vColor=color; }";
  const char *fs = "#version 330 core\nin vec3 vColor; out vec4 fragColor; void main(){ vec2 p=gl_PointCoord*2.0-1.0; if(dot(p,p)>1.0) discard; fragColor=vec4(vColor,1.0); }";
  shader_ = glCreateProgram(); const GLuint v = glCreateShader(GL_VERTEX_SHADER), f = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(v, 1, &vs, nullptr); glShaderSource(f, 1, &fs, nullptr); glCompileShader(v); glCompileShader(f);
  GLint vertexOk = GL_FALSE, fragmentOk = GL_FALSE;
  glGetShaderiv(v, GL_COMPILE_STATUS, &vertexOk); glGetShaderiv(f, GL_COMPILE_STATUS, &fragmentOk);
  if (vertexOk != GL_TRUE || fragmentOk != GL_TRUE) {
    char log[2048] = {}; GLsizei length = 0;
    if (vertexOk != GL_TRUE) glGetShaderInfoLog(v, sizeof(log)-1, &length, log); else glGetShaderInfoLog(f, sizeof(log)-1, &length, log);
    qWarning() << "Point shader compile failed:" << log;
  }
  glAttachShader(shader_, v); glAttachShader(shader_, f); glLinkProgram(shader_);
  GLint linkOk = GL_FALSE; glGetProgramiv(shader_, GL_LINK_STATUS, &linkOk);
  qInfo() << "Point shader program:" << shader_ << "vertex:" << v << "fragment:" << f << "linked:" << linkOk;
  if (linkOk != GL_TRUE) { char log[2048] = {}; glGetProgramInfoLog(shader_, sizeof(log)-1, nullptr, log); qWarning() << "Point shader link failed:" << log; }
  glDeleteShader(v); glDeleteShader(f);
  glGenVertexArrays(1, &vao_); glGenBuffers(1, &vbo_);
  uMvp_ = glGetUniformLocation(shader_, "mvp"); uCropMin_ = glGetUniformLocation(shader_, "cropMin"); uCropMax_ = glGetUniformLocation(shader_, "cropMax"); uPointSize_ = glGetUniformLocation(shader_, "pointSize");
  upload();
}

void CloudView::upload() {
  if (!isValid() || !context()) return;
  makeCurrent();
  struct Vertex { float x, y, z, r, g, b; };
  QVector<Vertex> vertices; vertices.reserve(points_.size());
  for (const auto &p : points_) vertices.append({p.x, p.y, p.z, p.r, p.g, p.b});
  glBindVertexArray(vao_); glBindBuffer(GL_ARRAY_BUFFER, vbo_); glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.constData(), GL_STATIC_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr); glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void *>(3 * sizeof(float))); glEnableVertexAttribArray(1);
  doneCurrent();
}

void CloudView::paintGL() {
  glClearColor(0.03f, 0.047f, 0.067f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (!cloudVisible_ || points_.isEmpty() || !shader_) return;
  QMatrix4x4 view, projection; const float yaw = qDegreesToRadians(yaw_), pitch = qDegreesToRadians(pitch_);
  const QVector3D eye(center_.x() + distance_ * std::cos(pitch) * std::cos(yaw), center_.y() + distance_ * std::cos(pitch) * std::sin(yaw), center_.z() + distance_ * std::sin(pitch));
  view.lookAt(eye, center_, QVector3D(0, 0, 1)); projection.perspective(55.0f, float(width()) / std::max(1, height()), std::max(0.001f, radius_ / 1000.0f), radius_ * 100.0f);
  glUseProgram(shader_); glUniformMatrix4fv(uMvp_, 1, GL_FALSE, (projection * view).constData()); glUniform3f(uCropMin_, cropMin_.x(), cropMin_.y(), cropMin_.z()); glUniform3f(uCropMax_, cropMax_.x(), cropMax_.y(), cropMax_.z()); glUniform1f(uPointSize_, pointSize_);
  GLenum error = glGetError(); if (error != GL_NO_ERROR) qWarning() << "OpenGL uniform error:" << error << uMvp_ << uPointSize_;
  glBindVertexArray(vao_); error = glGetError(); if (error != GL_NO_ERROR) qWarning() << "OpenGL VAO error:" << error;
  glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(points_.size()));
  error = glGetError(); if (error != GL_NO_ERROR) qWarning() << "OpenGL draw error:" << error;
  auto project = [&](const QVector3D &p) { const QVector4D clip = (projection * view) * QVector4D(p, 1.0f); if (clip.w() <= 0) return QPointF(-10000, -10000); const QVector3D ndc = clip.toVector3DAffine(); return QPointF((ndc.x()+1.0)*0.5*width(), (1.0-ndc.y())*0.5*height()); };
  if (measureMode_ && hasMeasureAnchor_) {
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    const QPointF a = project(measureAnchor_); painter.setPen(Qt::NoPen); painter.setBrush(QColor(255,45,45)); painter.drawEllipse(a, 7, 7);
    if (hasMeasurePreview_) { const QPointF b = project(measurePreview_); painter.setPen(QPen(QColor(255,80,80), 2)); painter.drawLine(a,b); painter.setBrush(QColor(255,80,80)); painter.drawEllipse(b,5,5); }
  }
}

void CloudView::resizeGL(int w, int h) { glViewport(0, 0, w, h); }
void CloudView::mousePressEvent(QMouseEvent *event) {
  lastMouse_ = event->pos();
  if (measureMode_ && event->button() == Qt::RightButton && hasMeasureAnchor_) { hasMeasureAnchor_ = false; hasMeasurePreview_ = false; emit measurementCanceled(); update(); return; }
  if (measureMode_ && event->button() == Qt::LeftButton) { QVector3D point; if (pickPoint(event->pos(), &point)) { if (!hasMeasureAnchor_) { measureAnchor_ = point; hasMeasureAnchor_ = true; emit pointPicked(point); } else { emit pointPicked(point); hasMeasureAnchor_ = false; hasMeasurePreview_ = false; } update(); } return; }
  dragButton_ = event->button();
  if (dragButton_ == Qt::LeftButton || dragButton_ == Qt::MiddleButton || dragButton_ == Qt::RightButton) grabMouse();
}
void CloudView::mouseMoveEvent(QMouseEvent *event) {
  const Qt::MouseButtons buttons = event->buttons();
  if (buttons == Qt::NoButton) return;
  const QPoint delta = event->pos() - lastMouse_; lastMouse_ = event->pos();
  if (measureMode_ && hasMeasureAnchor_) { QVector3D point; if (pickPoint(event->pos(), &point)) { measurePreview_ = point; hasMeasurePreview_ = true; emit measurementPreview(point); } update(); return; }
  if (buttons.testFlag(Qt::MiddleButton)) {
    const float s = distance_ / 500.0f;
    const float yaw = qDegreesToRadians(yaw_), pitch = qDegreesToRadians(pitch_);
    const QVector3D eye(center_.x() + distance_ * std::cos(pitch) * std::cos(yaw),
                        center_.y() + distance_ * std::cos(pitch) * std::sin(yaw),
                        center_.z() + distance_ * std::sin(pitch));
    const QVector3D forward = (center_ - eye).normalized();
    const QVector3D right = QVector3D::crossProduct(forward, QVector3D(0, 0, 1)).normalized();
    const QVector3D cameraUp = QVector3D::crossProduct(right, forward).normalized();
    center_ -= right * (delta.x() * s);
    center_ += cameraUp * (delta.y() * s);
  }
  else if (buttons.testFlag(Qt::RightButton)) { distance_ *= std::exp(delta.y() * 0.01f); distance_ = std::clamp(distance_, radius_ * 0.02f, radius_ * 100.0f); }
  else if (buttons.testFlag(Qt::LeftButton)) { yaw_ += delta.x() * 0.45f; pitch_ = std::clamp(pitch_ - delta.y() * 0.35f, -89.0f, 89.0f); }
  update();
}
void CloudView::mouseReleaseEvent(QMouseEvent *event) { if (event->button() == dragButton_) { dragButton_ = Qt::NoButton; releaseMouse(); } }
void CloudView::wheelEvent(QWheelEvent *event) { distance_ *= std::pow(0.85f, event->angleDelta().y() / 120.0f); distance_ = std::clamp(distance_, radius_ * 0.02f, radius_ * 100.0f); update(); }
void CloudView::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) { fitView(); event->accept(); return; }
  QOpenGLWidget::mouseDoubleClickEvent(event);
}

bool CloudView::pickPoint(const QPoint &screen, QVector3D *point) const {
  if (points_.isEmpty()) return false;
  QMatrix4x4 view, projection; const float yaw = qDegreesToRadians(yaw_), pitch = qDegreesToRadians(pitch_);
  const QVector3D eye(center_.x() + distance_ * std::cos(pitch) * std::cos(yaw), center_.y() + distance_ * std::cos(pitch) * std::sin(yaw), center_.z() + distance_ * std::sin(pitch));
  view.lookAt(eye, center_, QVector3D(0, 0, 1)); projection.perspective(55.0f, float(width()) / std::max(1, height()), std::max(0.001f, radius_ / 1000.0f), radius_ * 100.0f);
  const QMatrix4x4 mvp = projection * view; float best = 14.0f * 14.0f; bool found = false;
  for (const auto &p : points_) { if (p.x < cropMin_.x() || p.x > cropMax_.x() || p.y < cropMin_.y() || p.y > cropMax_.y() || p.z < cropMin_.z() || p.z > cropMax_.z()) continue; const QVector4D clip = mvp * QVector4D(p.x, p.y, p.z, 1); if (clip.w() <= 0) continue; const QVector3D ndc = clip.toVector3DAffine(); const QPointF q((ndc.x() + 1) * 0.5 * width(), (1 - ndc.y()) * 0.5 * height()); const float d = std::pow(float(q.x() - screen.x()), 2) + std::pow(float(q.y() - screen.y()), 2); if (d < best) { best = d; *point = QVector3D(p.x, p.y, p.z); found = true; } }
  return found;
}
