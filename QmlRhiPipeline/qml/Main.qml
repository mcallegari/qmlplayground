import QtQuick 2.15
import QtQuick.Window 2.15
import QtQml 2.15
import RhiQmlItem 1.0

Window {
    id: root
    width: 1280
    height: 720
    visible: true
    title: "Deferred Rendering with QRhi"

    property real meshStep: 0.25
    property vector3d cubePos: Qt.vector3d(0, 0, 0)
    property var ledColors: [
        Qt.vector3d(1.0, 0.1, 0.1),
        Qt.vector3d(1.0, 0.4, 0.1),
        Qt.vector3d(1.0, 0.8, 0.1),
        Qt.vector3d(0.8, 1.0, 0.1),
        Qt.vector3d(0.1, 1.0, 0.1),
        Qt.vector3d(0.1, 1.0, 0.8),
        Qt.vector3d(0.1, 0.6, 1.0),
        Qt.vector3d(0.1, 0.2, 1.0),
        Qt.vector3d(0.4, 0.1, 1.0),
        Qt.vector3d(0.8, 0.1, 1.0),
        Qt.vector3d(1.0, 0.1, 0.6),
        Qt.vector3d(1.0, 0.1, 0.3)
    ]

    function scaleColor(c, s) {
        return Qt.vector3d(c.x * s, c.y * s, c.z * s)
    }
    RhiQmlItem {
        id: renderer
        anchors.fill: parent
        focus: true
        freeCameraEnabled: true
        smokeAmount: 0.8 // 0-1 range
        beamModel: Scene.SoftHaze // soft haze (fast fade), physically‑motivated (inverse‑square + exponential
        bloomIntensity: 2.0
        bloomRadius: 2.0
        moveSpeed: 5.0
        lookSensitivity: 0.2

        WheelHandler {
            target: renderer
            onWheel: function(wheel) {
                var step = wheel.angleDelta.y / 120.0
                var dx = renderer.cameraTarget.x - renderer.cameraPosition.x
                var dy = renderer.cameraTarget.y - renderer.cameraPosition.y
                var dz = renderer.cameraTarget.z - renderer.cameraPosition.z
                var dist = Math.sqrt(dx * dx + dy * dy + dz * dz)
                dist = Math.max(dist, 0.5)
                var factor = Math.pow(1.12, step)
                renderer.zoomAlongView((factor - 1.0) * dist)
            }
        }

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
            nearPlane: 0.01
            farPlane: 300
        }

        Light {
            type: Light.Ambient
            color: Qt.vector3d(0.1, 0.1, 0.1)
            intensity: 1.0
            castShadows: false
        }
/*
        Light {
            type: Light.Area
            position: Qt.vector3d(0, 0.1, 3)
            direction: Qt.vector3d(0, 0, 1)
            color: Qt.vector3d(0.9, 0.9, 0.8)
            intensity: 2.0
            range: 30.0
            size: Qt.size(3.0, 2.0)
            castShadows: false
        }
*/
        Light {
            type: Light.Spotlight
            position: Qt.vector3d(-1, 5, 0)
            direction: Qt.vector3d(0, -1, 0)
            color: Qt.vector3d(0.1, 0.1, 0.8)
            intensity: 5.0
            range: 100.0
            coneAngle: 25.0
            qualitySteps: 40
            castShadows: true
            beamShape: Light.ConeShape
            beamRadius: 0.15
            //goboPath: "/home/massimo/projects/qmlplayground/QmlRhiPipeline/gobos/gobo00013.svg"
        }

        Light {
            type: Light.Spotlight
            position: Qt.vector3d(0.5, 5, 0)
            direction: Qt.vector3d(0, -1, 0)
            color: Qt.vector3d(1, 0.1, 0.1)
            intensity: 3.0
            range: 100.0
            coneAngle: 25.0
            qualitySteps: 40
            castShadows: true
            beamShape: Light.ConeShape
            beamRadius: 0.2
            goboPath: "/home/massimo/projects/qmlplayground/QmlRhiPipeline/gobos/gobo00024.svg"
        }

        Light {
            type: Light.Spotlight
            position: Qt.vector3d(3, 5, 0)
            direction: Qt.vector3d(0, -1, 0)
            color: Qt.vector3d(0.1, 1, 0.1)
            intensity: 3.0
            range: 100.0
            coneAngle: 2
            qualitySteps: 40
            castShadows: true
            beamShape: Light.BeamShape
            beamRadius: 0.3
        }
/*
        Hazer {
            position: Qt.vector3d(0, 1.5, -2.0)
            direction: Qt.vector3d(1, 0, 0)
            length: 4.0
            radius: 1.5
            density: 0.8
            enabled: true
        }
*/
        Instantiator {
            model: 12
            delegate: Cube {
                property vector3d ledColor: root.ledColors[index % root.ledColors.length]
                position: Qt.vector3d(0 + index * 0.42, -2.2, 2.5)
                rotationDegrees: Qt.vector3d(90, 0, 0)
                scale: Qt.vector3d(0.4, 0.02, 0.2)
                baseColor: ledColor
                emissiveColor: root.scaleColor(ledColor, 3.0)
            }
        }

        Cube {
            position: cubePos
            rotationDegrees: Qt.vector3d(0, 0, 0)
            scale: Qt.vector3d(1, 1, 1)
            isSelected: true
        }

        // ground plane
        Cube {
            position: Qt.vector3d(0, -2.5, 0)
            rotationDegrees: Qt.vector3d(0, 0, 0)
            scale: Qt.vector3d(10, 0.2, 10)
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
