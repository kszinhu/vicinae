#pragma once

#include <QByteArray>
#include <QString>
#include <Qt>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>

inline QString cameraDeviceStorageId(const QByteArray &deviceId) {
  return QString::fromLatin1(
      deviceId.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

inline std::optional<QByteArray> cameraDeviceIdFromStorage(const QString &storageId) {
  if (storageId.isEmpty()) return std::nullopt;

  const auto result = QByteArray::fromBase64Encoding(
      storageId.toLatin1(), QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
  if (!result) return std::nullopt;
  return result.decoded;
}

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

inline std::optional<QByteArray> selectCameraDeviceId(std::span<const QByteArray> deviceIds,
                                                      const QByteArray &defaultDeviceId,
                                                      const std::optional<QByteArray> &preferredDeviceId) {
  if (preferredDeviceId && std::ranges::contains(deviceIds, *preferredDeviceId)) return preferredDeviceId;
  if (std::ranges::contains(deviceIds, defaultDeviceId)) return defaultDeviceId;
  if (!deviceIds.empty()) return deviceIds.front();
  return std::nullopt;
}

inline std::optional<QByteArray> cycleCameraDeviceId(std::span<const QByteArray> deviceIds,
                                                     const QByteArray &currentDeviceId, int offset) {
  if (deviceIds.empty()) return std::nullopt;

  const auto current = std::ranges::find(deviceIds, currentDeviceId);
  if (current == deviceIds.end()) return deviceIds.front();

  const auto count = static_cast<std::ptrdiff_t>(deviceIds.size());
  const auto currentIndex = current - deviceIds.begin();
  const auto nextIndex = (currentIndex + offset % count + count) % count;
  return deviceIds[static_cast<std::size_t>(nextIndex)];
}

constexpr bool shouldCameraBeActive(bool currentView, bool windowVisible, bool outputAttached,
                                    OpenCameraState state) {
  const bool cameraUsable = state == OpenCameraState::Starting || state == OpenCameraState::Ready;
  return currentView && windowVisible && outputAttached && cameraUsable;
}
