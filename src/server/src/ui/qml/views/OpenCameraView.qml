pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import QtMultimedia
import Vicinae

Item {
    id: root

    required property OpenCameraViewHost host
    readonly property real contentRadius: Math.max(0, Config.borderRounding - Config.borderWidth)
    readonly property real controlInset: Math.max(12, contentRadius)

    function updateOutputAttachment() {
        if (root.visible)
            root.host.attachVideoOutput(videoOutput);
        else
            root.host.detachVideoOutput(videoOutput);
    }

    Rectangle {
        id: previewSurface
        anchors.fill: parent
        color: "black"
        clip: true
        layer.enabled: root.contentRadius > 0
        layer.effect: MultiEffect {
            autoPaddingEnabled: false
            maskEnabled: true
            maskSource: previewMask
        }

        VideoOutput {
            id: videoOutput
            anchors.fill: parent
            fillMode: VideoOutput.PreserveAspectCrop
            mirrored: true
        }

        Rectangle {
            anchors.fill: parent
            color: Config.withAlpha(Theme.background, 0.94)
            visible: !root.host.previewVisible

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(420, parent.width - 48)
                spacing: 10

                BusyIndicator {
                    id: progress
                    Layout.alignment: Qt.AlignHCenter
                    running: root.host.state === "awaitingPermission" || root.host.state === "starting"
                    visible: running
                }

                ViciImage {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    Layout.alignment: Qt.AlignHCenter
                    source: Img.icon(BuiltinIcon.Camera).withFillColor(Theme.foreground)
                    visible: !progress.visible
                }

                Text {
                    Layout.fillWidth: true
                    text: root.host.statusTitle
                    color: Theme.foreground
                    font.pointSize: Theme.regularFontSize
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: root.host.statusDescription
                    color: Theme.textMuted
                    font.pointSize: Theme.regularFontSize
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    visible: text !== ""
                }

                ViciButton {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 4
                    text: qsTr("Try Again")
                    variant: "secondary"
                    bordered: true
                    visible: root.host.retryAvailable
                    onClicked: root.host.retry()
                }
            }
        }
    }

    Rectangle {
        id: previewMask
        anchors.fill: previewSurface
        radius: root.contentRadius
        color: "white"
        visible: false
        layer.enabled: true
    }

    ViciButton {
        z: 2
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: root.controlInset
        width: 28
        height: 28
        radius: 6
        iconSource: Img.icon(BuiltinIcon.ArrowLeft)
        accessibleName: qsTr("Back")
        variant: "tinted"
        onClicked: root.host.goBack()
    }

    SearchableDropdown {
        id: cameraSelector
        z: 2
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: root.controlInset
        width: Math.min(preferredWidth, Math.max(0, parent.width - 2 * root.controlInset))
        compact: true
        preserveBackdrop: true
        model: root.host.cameraModel
        currentItem: root.host.selectedCamera
        visible: root.host.hasMultipleCameras
        onActivated: item => root.host.selectCamera(item.id)
    }

    Shortcut {
        sequence: "Ctrl+Up"
        enabled: root.visible && root.host.hasMultipleCameras
        onActivated: root.host.selectPreviousCamera()
    }

    Shortcut {
        sequence: "Ctrl+Down"
        enabled: root.visible && root.host.hasMultipleCameras
        onActivated: root.host.selectNextCamera()
    }

    Component.onCompleted: root.updateOutputAttachment()
    Component.onDestruction: {
        if (root.host)
            root.host.detachVideoOutput(videoOutput);
    }
    onVisibleChanged: root.updateOutputAttachment()
}
