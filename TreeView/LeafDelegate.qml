import QtQuick 2.0

Rectangle {
    id: leafDelegate
    width: 350
    height: 35

    color: "transparent"
    //color: "steelblue"

    property string textLabel
    property string lCapital
    property string lCurrency

    signal clicked

    Text {
        width: 150
        height: parent.height
        text: textLabel
        verticalAlignment: Text.AlignVCenter
    }
    Text {
        x: 150
        width: 100
        height: parent.height
        text: lCapital
        verticalAlignment: Text.AlignVCenter
    }
    Text {
        x: 250
        width: 50
        height: parent.height
        text: lCurrency
        verticalAlignment: Text.AlignVCenter
    }
    Rectangle {
        height: 1
        width: parent.width
        y: parent.height - 1
        color: "black"
    }

    MouseArea {
        anchors.fill: parent
        onClicked: leafDelegate.clicked()
    }
}

