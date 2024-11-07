import QtQuick

Rectangle
{
    width: 800
    height: 600
    color: "transparent"

    RhiQmlItem
    {
        anchors.fill: parent

        NumberAnimation on rotation { to: 360; duration: 5000; easing.type: Easing.Linear; loops: Animation.Infinite}
    }

    Rectangle {
        color: Qt.rgba(1, 1, 1, 0.7)
        radius: 10
        border.width: 1
        border.color: "white"
        anchors.fill: label
        anchors.margins: -10
    }

    Text
    {
        id: label
        color: "black"
        wrapMode: Text.WordWrap
        text: qsTr("This is a QML overlay message on top of a RHI viewport")
        anchors.right: parent.right
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 20
    }
}
