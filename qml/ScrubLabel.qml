import QtQuick
import QtQuick.Controls
import PhotoCrap.Native 1.0

Label {
    id: root
    required property Item control
    readonly property bool scrubbing: scrubber.pressed
    signal scrubStarted()
    signal scrubFinished(bool cancelled)

    ValueScrubber {
        id: scrubber
        objectName: "valueScrubber"
        anchors.fill: parent
        target: root.control
        enabled: root.control && root.control.enabled
        onStarted: root.scrubStarted()
        onFinished: function(cancelled) { root.scrubFinished(cancelled) }
    }
}
