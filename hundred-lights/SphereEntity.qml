import QtQuick 2.0 as QQ2

import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Input 2.0
import Qt3D.Extras 2.0

Entity
{
    id: sphere
    property real xPos: 0.0
    property real zPos: 0.0

    property Effect geomPassEffect
    property Layer targetLayer
    property color meshColor: '#'+Math.random().toString(16).substr(-6);

    SphereMesh
    {
        id : sphereMesh
        rings: 50
        slices: 100
        radius: 0.5
    }

    property Material material : Material {
        effect : geomPassEffect
        parameters : Parameter { name : "meshColor"; value : meshColor }
    }

    property Transform transform: Transform {
        id: sphereTransform
        property real yPos: 0.0
        translation: Qt.vector3d(xPos, yPos, zPos)
    }

    QQ2.SequentialAnimation
    {
        loops: QQ2.Animation.Infinite
        running: true
        QQ2.NumberAnimation { target: sphereTransform; property: "yPos"; to: 0; duration: Math.random() * 1000 + 1000 }
        QQ2.NumberAnimation { target: sphereTransform; property: "yPos"; to: 5; duration: Math.random() * 1000 + 1000 }
    }

    components : [
        sphereMesh,
        sphere.material,
        sphereTransform,
        sphere.targetLayer
    ]
}
