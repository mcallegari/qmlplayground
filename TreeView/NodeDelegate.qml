import QtQuick 2.0

Rectangle {
    id: nodeContainer
    width: 350
    height: nodeLabel.height + nodeChildrenView.height

    color: "transparent"

    property string textLabel
    property var folderChildren
    property bool isExpanded: false
    property int childrenHeight: 0
    property int variableHeight: 0

    signal toggled(bool expanded, int newHeight)

    Image {
        width: 40
        height: 35
        source: "qrc:/folder.svg"
    }

    Text {
        id: nodeLabel
        x: 45
        width: parent.width
        height: 35
        text: textLabel
        verticalAlignment: Text.AlignVCenter
    }

    MouseArea {
        width: parent.width
        height: 35
        onClicked: {
            isExpanded = !isExpanded
            nodeContainer.toggled(isExpanded, childrenHeight)
        }
    }

    Rectangle {
        height: 1
        width: parent.width
        y: 34
        color: "black"
    }

    function childToggled(expanded, height)
    {
        if (expanded)
            variableHeight += height;
        else
            variableHeight -= height;
    }

    ListView {
        id: nodeChildrenView
        visible: isExpanded
        x: 30
        y: nodeLabel.height
        height: isExpanded ? (childrenHeight + variableHeight) : 0
        model: folderChildren
        delegate:
            Component {
                Loader {
                    width: 350 //parent.width
                    //height: 35
                    source: hasChildren ? "NodeDelegate.qml" : "LeafDelegate.qml"
                    onLoaded: {
                        item.textLabel = label
                        if (hasChildren)
                        {
                            item.folderChildren = childrenModel
                            item.childrenHeight = (childrenModel.rowCount() * 35)
                        }
                        else
                        {
                            item.lCapital = capital
                            item.lCurrency = currency
                        }
                    }
                    Connections {
                         target: item
                         onToggled: childToggled(item.isExpanded, item.childrenHeight)
                    }
                }
        }
    }
}

