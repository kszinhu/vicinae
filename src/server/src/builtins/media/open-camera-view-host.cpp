#include <QCamera>
#include <QCameraDevice>
#include <QCoreApplication>
#include <QDebug>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QPermissions>
#include <QWindow>

#include <ranges>
#include <vector>

#include "builtins/media/open-camera-view-host.hpp"
#include "common/context.hpp"
#include "navigation-controller.hpp"
#include "ui/views/view-utils.hpp"

namespace {
constexpr auto CAMERA_STORAGE_KEY = "cameraDevice";

QString cameraItemId(const QByteArray &deviceId) { return cameraDeviceStorageId(deviceId); }
} // namespace

OpenCameraViewHost::~OpenCameraViewHost() {
  stopCamera();
  if (m_captureSession) m_captureSession->setVideoOutput(nullptr);
}

QUrl OpenCameraViewHost::qmlComponentUrl() const { return qml::componentUrl(u"OpenCameraView"); }

QVariantMap OpenCameraViewHost::qmlProperties() {
  return {{QStringLiteral("host"), QVariant::fromValue(this)}};
}

void OpenCameraViewHost::initialize() {
  BaseView::initialize();

  m_mediaDevices = new QMediaDevices(this);
  m_camera = new QCamera(this);
  m_captureSession = new QMediaCaptureSession(this);
  m_captureSession->setCamera(m_camera);

  const auto storedCamera = command()->storage().getItem(CAMERA_STORAGE_KEY);
  if (!storedCamera.isUndefined() && !storedCamera.isNull()) {
    m_preferredDeviceId = cameraDeviceIdFromStorage(storedCamera.toString());
  }

  auto *navigation = context()->navigation.get();
  m_currentView = true;
  m_logicalWindowVisible = navigation->isWindowOpened();
  setNativeWindow(navigation->window());

  connect(navigation, &NavigationController::currentViewChanged, this,
          [this](const NavigationController::ViewState &view) { setCurrentView(view.sender == this); });
  connect(navigation, &NavigationController::windowVisiblityChanged, this,
          &OpenCameraViewHost::setLogicalWindowVisible);
  connect(m_mediaDevices, &QMediaDevices::videoInputsChanged, this, &OpenCameraViewHost::refreshCameras);
  connect(m_camera, &QCamera::activeChanged, this, [this]() { updateState(); });
  connect(m_camera, &QCamera::errorOccurred, this, [this](QCamera::Error error, const QString &errorString) {
    if (error == QCamera::NoError) return;
    m_hasCameraError = true;
    qWarning() << "Failed to start camera preview:" << errorString;
    updateState();
    synchronizeCamera();
  });

  refreshCameras();
  requestPermission();
}

void OpenCameraViewHost::beforePop() {
  m_currentView = false;
  stopCamera();
}

void OpenCameraViewHost::attachVideoOutput(QObject *output) {
  if (!output || output == m_videoOutput) return;

  detachVideoOutput(nullptr);
  m_videoOutput = output;
  m_captureSession->setVideoOutput(output);
  m_outputDestroyedConnection = connect(output, &QObject::destroyed, this, [this]() {
    m_videoOutput = nullptr;
    if (m_captureSession) m_captureSession->setVideoOutput(nullptr);
    synchronizeCamera();
  });
  synchronizeCamera();
}

void OpenCameraViewHost::detachVideoOutput(QObject *output) {
  if (output && output != m_videoOutput) return;

  disconnect(m_outputDestroyedConnection);
  m_outputDestroyedConnection = {};
  if (m_captureSession) m_captureSession->setVideoOutput(nullptr);
  m_videoOutput = nullptr;
  synchronizeCamera();
}

void OpenCameraViewHost::selectCamera(const QString &id) {
  const auto deviceId = cameraDeviceIdFromStorage(id);
  if (!deviceId) return;

  const auto devices = QMediaDevices::videoInputs();
  const auto selected = std::ranges::find(devices, *deviceId, &QCameraDevice::id);
  if (selected == devices.end()) return;

  const bool deviceChanged = m_selectedDeviceId != deviceId;
  const bool preferenceChanged = m_preferredDeviceId != deviceId;
  if (!deviceChanged && !preferenceChanged) return;

  m_preferredDeviceId = deviceId;
  command()->storage().setItem(CAMERA_STORAGE_KEY, cameraDeviceStorageId(*deviceId));
  if (!deviceChanged) return;

  stopCamera();
  m_selectedDeviceId = deviceId;
  m_hasCameraError = false;
  m_camera->setCameraDevice(*selected);
  emit camerasChanged();
  updateState();
  synchronizeCamera();
}

void OpenCameraViewHost::retry() {
  m_hasCameraError = false;
  requestPermission();
  refreshCameras();
  synchronizeCamera();
}

QString OpenCameraViewHost::state() const {
  switch (m_state) {
  case OpenCameraState::AwaitingPermission:
    return QStringLiteral("awaitingPermission");
  case OpenCameraState::Starting:
    return QStringLiteral("starting");
  case OpenCameraState::Ready:
    return QStringLiteral("ready");
  case OpenCameraState::PermissionDenied:
    return QStringLiteral("permissionDenied");
  case OpenCameraState::NoCamera:
    return QStringLiteral("noCamera");
  case OpenCameraState::Error:
    return QStringLiteral("error");
  }
  return QStringLiteral("error");
}

QString OpenCameraViewHost::statusTitle() const {
  switch (m_state) {
  case OpenCameraState::AwaitingPermission:
    return tr("Waiting for camera permission");
  case OpenCameraState::Starting:
    return tr("Starting camera...");
  case OpenCameraState::Ready:
    return {};
  case OpenCameraState::PermissionDenied:
    return tr("Camera access is denied");
  case OpenCameraState::NoCamera:
    return tr("No camera found");
  case OpenCameraState::Error:
    return tr("The camera could not be started.");
  }
  return {};
}

QString OpenCameraViewHost::statusDescription() const {
  switch (m_state) {
  case OpenCameraState::AwaitingPermission:
    return tr("Allow camera access to show the preview.");
  case OpenCameraState::PermissionDenied:
    return tr("Allow camera access in your system settings, then try again.");
  case OpenCameraState::NoCamera:
    return tr("Connect a camera to show a preview.");
  case OpenCameraState::Error:
    return tr("Make sure the camera is connected and not in use by another application.");
  case OpenCameraState::Starting:
  case OpenCameraState::Ready:
    return {};
  }
  return {};
}

bool OpenCameraViewHost::retryAvailable() const {
  return m_state == OpenCameraState::PermissionDenied || m_state == OpenCameraState::Error;
}

QVariantMap OpenCameraViewHost::selectedCamera() const {
  if (!m_selectedDeviceId) return {};
  return m_cameraModel.itemDataById(cameraItemId(*m_selectedDeviceId));
}

void OpenCameraViewHost::requestPermission() {
  auto *app = QCoreApplication::instance();
  const QCameraPermission permission;
  m_permissionStatus = app->checkPermission(permission);
  updateState();

  if (m_permissionStatus != Qt::PermissionStatus::Undetermined) {
    synchronizeCamera();
    return;
  }

  app->requestPermission(permission, this, [this](const QPermission &result) {
    m_permissionStatus = result.status();
    refreshCameras();
    updateState();
    synchronizeCamera();
  });
}

void OpenCameraViewHost::refreshCameras() {
  const auto devices = QMediaDevices::videoInputs();
  std::vector<QByteArray> deviceIds;
  deviceIds.reserve(devices.size());

  QVariantList items;
  items.reserve(devices.size());
  for (const auto &device : devices) {
    deviceIds.emplace_back(device.id());
    items.emplace_back(QVariantMap{
        {QStringLiteral("id"), cameraItemId(device.id())},
        {QStringLiteral("displayName"), device.description()},
    });
  }
  m_cameraModel.setItems(items);

  const auto selectedId =
      selectCameraDeviceId(deviceIds, QMediaDevices::defaultVideoInput().id(), m_preferredDeviceId);
  m_selectedDeviceId = selectedId;

  if (!selectedId) {
    stopCamera();
    m_camera->setCameraDevice({});
  } else {
    const auto selected = std::ranges::find(devices, *selectedId, &QCameraDevice::id);
    if (selected != devices.end() && m_camera->cameraDevice().id() != *selectedId) {
      stopCamera();
      m_hasCameraError = false;
      m_camera->setCameraDevice(*selected);
    }
  }

  emit camerasChanged();
  updateState();
  synchronizeCamera();
}

void OpenCameraViewHost::updateState() {
  const auto next = resolvedState();
  if (m_state == next) return;
  m_state = next;
  emit stateChanged();
}

void OpenCameraViewHost::synchronizeCamera() {
  if (!m_camera) return;

  const bool visible = m_logicalWindowVisible && m_nativeWindowVisible;
  const bool shouldBeActive =
      shouldCameraBeActive(m_currentView, visible, !m_videoOutput.isNull(), resolvedState());
  if (m_camera->isActive() != shouldBeActive) m_camera->setActive(shouldBeActive);
  updateState();
}

void OpenCameraViewHost::stopCamera() {
  if (m_camera && m_camera->isActive()) m_camera->stop();
}

void OpenCameraViewHost::setCurrentView(bool current) {
  if (m_currentView == current) return;
  m_currentView = current;
  synchronizeCamera();
}

void OpenCameraViewHost::setLogicalWindowVisible(bool visible) {
  if (m_logicalWindowVisible == visible) return;
  m_logicalWindowVisible = visible;
  synchronizeCamera();
}

void OpenCameraViewHost::setNativeWindow(QWindow *window) {
  disconnect(m_windowDestroyedConnection);
  disconnect(m_windowVisibleConnection);
  m_window = window;
  m_nativeWindowVisible = !window || window->isVisible();

  if (!window) return;

  m_windowVisibleConnection = connect(window, &QWindow::visibleChanged, this, [this](bool visible) {
    m_nativeWindowVisible = visible;
    synchronizeCamera();
  });
  m_windowDestroyedConnection = connect(window, &QObject::destroyed, this, [this]() {
    m_window = nullptr;
    m_nativeWindowVisible = false;
    synchronizeCamera();
  });
}

OpenCameraState OpenCameraViewHost::resolvedState() const {
  const auto deviceCount = static_cast<std::size_t>(QMediaDevices::videoInputs().size());
  return resolveOpenCameraState(m_permissionStatus, deviceCount, m_hasCameraError,
                                m_camera && m_camera->isActive());
}
