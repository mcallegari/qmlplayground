import QtQuick 2.15
import QtQuick.Window 2.15
import RhiQmlItem 1.0

Window {
    width: 1280
    height: 720
    visible: true
    title: "Deferred Rendering with QRhi"

    property real meshStep: 0.25
    property vector3d cubePos: Qt.vector3d(0, 0, 0)
    RhiQmlItem {
        id: renderer
        anchors.fill: parent
        focus: true
        freeCameraEnabled: true
        moveSpeed: 5.0
        lookSensitivity: 0.2

        Keys.onPressed: function(event) {
            var step = meshStep
            if (event.modifiers & Qt.ShiftModifier)
                step = meshStep * 0.25
            if (event.modifiers & Qt.ControlModifier)
                step = meshStep * 4.0
            if (event.key === Qt.Key_Left) {
                cubePos = Qt.vector3d(cubePos.x - step, cubePos.y, cubePos.z)
                event.accepted = true
            } else if (event.key === Qt.Key_Right) {
                cubePos = Qt.vector3d(cubePos.x + step, cubePos.y, cubePos.z)
                event.accepted = true
            } else if (event.key === Qt.Key_Up) {
                cubePos = Qt.vector3d(cubePos.x, cubePos.y, cubePos.z - step)
                event.accepted = true
            } else if (event.key === Qt.Key_Down) {
                cubePos = Qt.vector3d(cubePos.x, cubePos.y, cubePos.z + step)
                event.accepted = true
            } else if (event.key === Qt.Key_PageUp) {
                cubePos = Qt.vector3d(cubePos.x, cubePos.y + step, cubePos.z)
                event.accepted = true
            } else if (event.key === Qt.Key_PageDown) {
                cubePos = Qt.vector3d(cubePos.x, cubePos.y - step, cubePos.z)
                event.accepted = true
            }
        }

        Camera {
            id: camera
            position: Qt.vector3d(0, 1, 5)
            target: Qt.vector3d(0, 0, 0)
            fov: 60
            nearPlane: 0.1
            farPlane: 200
        }

        Light {
            type: Light.Ambient
            color: Qt.vector3d(0.1, 0.1, 0.1)
            intensity: 1.0
            castShadows: false
        }

        Light {
            type: Light.Spotlight
            position: Qt.vector3d(0.5, 5, 0)
            direction: Qt.vector3d(0, -1, 0)
            color: Qt.vector3d(1, 0.1, 0.1)
            intensity: 3.0
            range: 150.0
            coneAngle: 25.0
            qualitySteps: 40
            castShadows: true
            goboPath: "/home/massimo/projects/qmlplayground/QmlRhiPipeline/gobos/gobo00024.svg"
        }

        Light {
            type: Light.Spotlight
            position: Qt.vector3d(-1, 5, 0)
            direction: Qt.vector3d(0, -1, 0)
            color: Qt.vector3d(0.1, 0.1, 0.8)
            intensity: 3.0
            range: 150.0
            coneAngle: 25.0
            qualitySteps: 40
            castShadows: true
            goboPath: "/home/massimo/projects/qmlplayground/QmlRhiPipeline/gobos/gobo00013.svg"
        }

        Cube {
            position: cubePos
            rotationDegrees: Qt.vector3d(0, 0, 0)
            scale: Qt.vector3d(1, 1, 1)
            isSelected: true
        }

        Cube {
            position: Qt.vector3d(0, -2.5, 0)
            rotationDegrees: Qt.vector3d(0, 0, 0)
            scale: Qt.vector3d(8, 0.2, 8)
            //isSelected: true
        }

        Model {
            path: "/home/massimo/projects/qmlplayground/QmlRhiPipeline/models/suzanne.obj"
            position: Qt.vector3d(1.5, -1.2, 0)
            rotationDegrees: Qt.vector3d(0, -30, 0)
            scale: Qt.vector3d(1, 1, 1)
            isSelected: true
        }

    }

}
