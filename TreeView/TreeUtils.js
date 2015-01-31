
function getSource(hasChildren)
{
    if (hasChildren)
        return "NodeDelegate.qml";
    else
        return "LeafDelegate.qml";
}

function createItem(container, label, capital, currency, bChildren, chModel)
{
    console.log("Model data items: " + label + ", hasChildren: " + bChildren);

    if (bChildren === true)
    {
        console.log("Children count: " + chModel.rowCount());
        var chHeight = chModel.rowCount() * 35;
        var nodeComp = Qt.createComponent("NodeDelegate.qml");
        var folderObj = nodeComp.createObject(container, { "childrenHeight": chHeight, "textLabel": label, "folderChildren": chModel } );
        if (folderObj === null)
            console.log("Error creating node delegate object !");
        return nodeComp;
    }
    else
    {
        var leafComp = Qt.createComponent("LeafDelegate.qml");
        var funcObj = leafComp.createObject(container, { "height": 35, "textLabel": label, "lCapital": capital, "lCurrency" : currency } );
        if (funcObj === null)
            console.log("Error creating leaf delegate object !");
        return leafComp;
    }
}

