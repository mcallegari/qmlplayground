import Qt3D 2.0
import Qt3D.Render 2.0
import QtQuick 2.1

Entity {
    id: spotlight
    property Effect sceneEffect
    property vector3d pos3D: Qt.vector3d(0, 0, 0)

    readonly property Camera lightCamera: lightCamera
    readonly property matrix4x4 lightViewProjection: lightCamera.projectionMatrix.times(lightCamera.matrix)

    Mesh {
        id: lightMesh
        source: "qrc:/assets/cone.obj"
    }
/*
    Transform {
        id: lightTransform

        Translate {
            dx: 1
            dy: 1
            dz: -1
        }
    }
*/
    property Material material : Material {
        effect : sceneEffect
        parameters : Parameter { name : "meshColor"; value : "gray" }
    }
    property SpotLight light : SpotLight {
        color : "green"
        intensity : 2.0
        direction: Qt.vector3d(0, -1, 0)
        cutOffAngle: 30.0
    }

    Camera {
        id: lightCamera
        objectName: "lightCameraLens"
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        aspectRatio: 1
        nearPlane : 0.1
        farPlane : 200.0
        position: spotlight.lightPosition
        viewCenter: Qt.vector3d(0.0, 0.0, 0.0)
        upVector: Qt.vector3d(0.0, 1.0, 0.0)
    }

    components : [
        lightMesh,
        //lightTransform,
        material,
        light,
        sceneLayer
    ]
}

