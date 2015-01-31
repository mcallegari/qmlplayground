import QtQuick 2.0

ListView {
    id: treeModelView
    width: 350
    height: 600
    model: myApp.treeModel
    delegate:
        Component {
            Loader {
                width: parent.width
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
            }
    }
}


