.import QtQuick 2.0 as QQ2InJS

var component;
var meshObj;
var meshURL;

function createGenericMesh(source) {
    meshURL = source;
    console.log("Loading mesh: " + meshURL)
    component = Qt.createComponent("qrc:/GenericMesh.qml");
    checkComponent();
}

function createCubeMesh() {
    component = Qt.createComponent("qrc:/CubeMesh.qml");
    checkComponent();
}

function checkComponent() {
    if (component.status === QQ2InJS.Component.Ready)
        finishCreation();
    else
        component.statusChanged.connect(finishCreation);
}

function finishCreation() {
    if (component.status === QQ2InJS.Component.Ready) {
        meshObj = component.createObject(sceneEntity, {"sourceMesh": meshURL, "position": Qt.vector3d(5, -2, 0 ), "sceneEffect": sceneMaterialEffect });
        if (meshObj ===  null) {
            // Error Handling
            console.log("Error creating mesh object");
        }
    } else if (component.status === QQ2InJS.Component.Error) {
        // Error Handling
        console.log("Error loading component:", component.errorString());
    }
}
