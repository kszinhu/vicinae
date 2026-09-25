#pragma once

#include <Qt>
#include <cstddef>

enum class OpenCameraState {
  AwaitingPermission,
  Starting,
  Ready,
  PermissionDenied,
  NoCamera,
  Error,
};

constexpr OpenCameraState resolveOpenCameraState(Qt::PermissionStatus permission, std::size_t deviceCount,
                                                 bool hasCameraError, bool cameraActive) {
  if (permission == Qt::PermissionStatus::Undetermined) return OpenCameraState::AwaitingPermission;
  if (permission == Qt::PermissionStatus::Denied) return OpenCameraState::PermissionDenied;
  if (deviceCount == 0) return OpenCameraState::NoCamera;
  if (hasCameraError) return OpenCameraState::Error;
  return cameraActive ? OpenCameraState::Ready : OpenCameraState::Starting;
}

constexpr bool shouldCameraBeActive(bool currentView, bool windowVisible, bool outputAttached,
                                    OpenCameraState state) {
  const bool cameraUsable = state == OpenCameraState::Starting || state == OpenCameraState::Ready;
  return currentView && windowVisible && outputAttached && cameraUsable;
}
