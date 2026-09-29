#include "mainwindow.h"
#include <QApplication>
#include <QSurfaceFormat>
#include <QTimer>

int main(int argc, char **argv) {
  QSurfaceFormat format;
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setVersion(3, 3);
  format.setProfile(QSurfaceFormat::CoreProfile);
  format.setDepthBufferSize(24);
  QSurfaceFormat::setDefaultFormat(format);
  QApplication app(argc, argv);
  app.setApplicationName(QStringLiteral("ScanForge Point Cloud Demo"));
  MainWindow window;
  window.setWindowState(Qt::WindowNoState);
  window.showNormal();
  window.resize(1440, 900);
  window.show();
  if (argc > 1) QTimer::singleShot(0, &window, [&window, path = QString::fromLocal8Bit(argv[1])] { window.loadFilePath(path); });
  return app.exec();
}
