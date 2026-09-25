#pragma once

#include <QByteArray>
#include <QMetaObject>
#include <QPointer>
#include <QString>
#include <QVariantMap>
#include <Qt>
#include <QtQml/qqmlregistration.h>

#include <optional>

#include "builtins/media/open-camera-state.hpp"
#include "ui/quick/completion-model.hpp"
#include "ui/views/bridge-view.hpp"

class QCamera;
class QMediaCaptureSession;
class QMediaDevices;
class QWindow;

class OpenCameraViewHost : public ViewHostBase {
  Q_OBJECT
  QML_NAMED_ELEMENT(OpenCameraViewHost)
  QML_UNCREATABLE("")
  Q_PROPERTY(QString state READ state NOTIFY stateChanged)
  Q_PROPERTY(QString statusTitle READ statusTitle NOTIFY stateChanged)
  Q_PROPERTY(QString statusDescription READ statusDescription NOTIFY stateChanged)
  Q_PROPERTY(bool previewVisible READ previewVisible NOTIFY stateChanged)
  Q_PROPERTY(bool retryAvailable READ retryAvailable NOTIFY stateChanged)
  Q_PROPERTY(bool hasMultipleCameras READ hasMultipleCameras NOTIFY camerasChanged)
  Q_PROPERTY(CompletionModel *cameraModel READ cameraModel CONSTANT)
  Q_PROPERTY(QVariantMap selectedCamera READ selectedCamera NOTIFY camerasChanged)

public:
  Q_INVOKABLE void attachVideoOutput(QObject *output);
  Q_INVOKABLE void detachVideoOutput(QObject *output);
  Q_INVOKABLE void selectCamera(const QString &id);
  Q_INVOKABLE void retry();

signals:
  void stateChanged();
  void camerasChanged();

public:
  OpenCameraViewHost() = default;
  ~OpenCameraViewHost() override;

  QUrl qmlComponentUrl() const override;
  QVariantMap qmlProperties() override;
  void initialize() override;
  void beforePop() override;

  bool supportsSearch() const override { return false; }
  bool searchInteractive() const override { return false; }
  bool needsGlobalStatusBar() const override { return false; }

  QString state() const;
  QString statusTitle() const;
  QString statusDescription() const;
  bool previewVisible() const { return m_state == OpenCameraState::Ready; }
  bool retryAvailable() const;
  bool hasMultipleCameras() const { return m_cameraModel.rowCount() > 1; }
  CompletionModel *cameraModel() { return &m_cameraModel; }
  QVariantMap selectedCamera() const;

private:
  void requestPermission();
  void refreshCameras();
  void updateState();
  void synchronizeCamera();
  void stopCamera();
  void setCurrentView(bool current);
  void setLogicalWindowVisible(bool visible);
  void setNativeWindow(QWindow *window);
  OpenCameraState resolvedState() const;

  CompletionModel m_cameraModel{this};
  QMediaDevices *m_mediaDevices = nullptr;
  QCamera *m_camera = nullptr;
  QMediaCaptureSession *m_captureSession = nullptr;
  QPointer<QObject> m_videoOutput;
  QPointer<QWindow> m_window;
  QMetaObject::Connection m_outputDestroyedConnection;
  QMetaObject::Connection m_windowDestroyedConnection;
  QMetaObject::Connection m_windowVisibleConnection;
  std::optional<QByteArray> m_selectedDeviceId;
  std::optional<QByteArray> m_preferredDeviceId;
  Qt::PermissionStatus m_permissionStatus = Qt::PermissionStatus::Undetermined;
  OpenCameraState m_state = OpenCameraState::AwaitingPermission;
  bool m_hasCameraError = false;
  bool m_currentView = false;
  bool m_logicalWindowVisible = false;
  bool m_nativeWindowVisible = true;
};
