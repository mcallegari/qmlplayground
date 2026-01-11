import QtQuick 2.15
import QtQuick.Window 2.15
import QtQml 2.15
import QtQuick.Controls 2.15
import RhiQmlItem 1.0

Window {
    id: root
    width: 1280
    height: 720
    visible: true
    title: "Deferred Rendering with QRhi"

    property vector3d lastPickPos: Qt.vector3d(0, 0, 0)
    property bool showFpsOverlay: false
    property real fpsCurrent: 0.0
    property real fpsMin: 0.0
    property real fpsMax: 0.0
    property real fpsAvg: 0.0
    property int fpsSamples: 0
    property double fpsLastStamp: 0.0
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
    function vdot(a, b) { return a.x * b.x + a.y * b.y + a.z * b.z }
    function vcross(a, b) {
        return Qt.vector3d(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        )
    }
    function vlen(a) { return Math.sqrt(a.x * a.x + a.y * a.y + a.z * a.z) }
    function vnorm(a) {
        var l = vlen(a)
        if (l <= 0.00001)
            return Qt.vector3d(0, 0, -1)
        return Qt.vector3d(a.x / l, a.y / l, a.z / l)
    }
    function resetFps() {
        fpsCurrent = 0.0
        fpsMin = 0.0
        fpsMax = 0.0
        fpsAvg = 0.0
        fpsSamples = 0
        fpsLastStamp = 0.0
    }
    function updateFps() {
        if (!showFpsOverlay) {
            fpsLastStamp = 0.0
            return
        }
        var now = Date.now()
        if (fpsLastStamp === 0.0) {
            fpsLastStamp = now
            return
        }
        var dt = now - fpsLastStamp
        if (dt <= 0.0)
            return
        var fps = 1000.0 / dt
        fpsCurrent = fps
        if (fpsSamples === 0) {
            fpsMin = fps
            fpsMax = fps
            fpsAvg = fps
        } else {
            fpsMin = Math.min(fpsMin, fps)
            fpsMax = Math.max(fpsMax, fps)
            fpsAvg = fpsAvg + (fps - fpsAvg) / (fpsSamples + 1)
        }
        fpsSamples += 1
        fpsLastStamp = now
    }
    function snapCamera(axis, useOrigin) {
        var target = renderer.cameraTarget
        var pos = renderer.cameraPosition
        var dx = pos.x - target.x
        var dy = pos.y - target.y
        var dz = pos.z - target.z
        var dist = Math.sqrt(dx * dx + dy * dy + dz * dz)
        if (dist < 0.001)
            dist = 1.0
        if (useOrigin)
            target = Qt.vector3d(0, 0, 0)
        if (useOrigin)
            dist = Math.sqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z)
        var newPos = Qt.vector3d(target.x + axis.x * dist,
                                 target.y + axis.y * dist,
                                 target.z + axis.z * dist)
        renderer.cameraPosition = newPos
        renderer.cameraTarget = target
        renderer.setCameraDirection(Qt.vector3d(target.x - newPos.x,
                                                target.y - newPos.y,
                                                target.z - newPos.z))
    }
    RhiQmlItem {
        id: renderer
        objectName: "sceneRenderer"
        anchors.fill: parent
        focus: true
        freeCameraEnabled: true
        smokeAmount: 0.8 // 0-1 range
        volumetricEnabled: true
        shadowsEnabled: true
        smokeNoiseEnabled: true
        beamModel: Scene.SoftHaze // soft haze (fast fade), physically‑motivated (inverse‑square + exponential
        bloomIntensity: 2.0
        bloomRadius: 2.0
        moveSpeed: 5.0
        lookSensitivity: 0.2

        onMeshPicked: function(item, worldPos, hit, modifiers) {
            if (hit)
                root.lastPickPos = worldPos
            renderer.handlePick(item, hit, modifiers)
        }

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
            if (event.key === Qt.Key_F) {
                root.showFpsOverlay = !root.showFpsOverlay
                root.resetFps()
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_V) {
                renderer.volumetricEnabled = !renderer.volumetricEnabled
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_H) {
                renderer.shadowsEnabled = !renderer.shadowsEnabled
                event.accepted = true
                return
            }
            if (event.key === Qt.Key_N) {
                renderer.smokeNoiseEnabled = !renderer.smokeNoiseEnabled
                event.accepted = true
                return
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
            color: Qt.vector3d(0.5, 0.5, 0.5)
            intensity: 1.0
            castShadows: false
        }

        Light {
            type: Light.Directional
            direction: Qt.vector3d(0, -1.0, 0.3)
            color: Qt.vector3d(1.0, 1.0, 1.0)
            intensity: 1.0
            castShadows: true
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
            coneAngle: 20.0
            qualitySteps: 40
            castShadows: true
            beamShape: Light.ConeShape
            beamRadius: 0.15
            //goboPath: "/home/massimo/projects/qmlplayground/QmlRhiPipeline/gobos/gobo00013.svg"
        }
/*
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
            //goboPath: "C:/projects/qmlplayground/QmlRhiPipeline/gobos/gobo00024.svg"
            //goboPath: "/Users/massimo/projects/qmlplayground/QmlRhiPipeline/gobos/gobo00024.svg"
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
*/

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
/*
        StaticLight {
            position: Qt.vector3d(-3, 0, 0)
            rotationDegrees: Qt.vector3d(-90, 0, 0)
            intensity: 3.5
            color: Qt.vector3d(1.0, 0.9, 0.8)
            zoom: 20
            //goboPath: "gobos/open.svg"
        }
*/
/*
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
*/
/*
        Cube {
            id: mainCube
            position: Qt.vector3d(-2, 0, 0)
            rotationDegrees: Qt.vector3d(0, 0, 0)
            scale: Qt.vector3d(1, 1, 1)
            baseColor: Qt.vector3d(1, 0, 0)
        }
*/
        // ground plane
        Cube {
            id: groundPlane
            selectable: false
            position: Qt.vector3d(0, -2.5, 0)
            rotationDegrees: Qt.vector3d(0, 0, 0)
            scale: Qt.vector3d(10, 0.2, 10)
        }
/*
        Model {
            id: suzanne
            //path: "C:/projects/qmlplayground/QmlRhiPipeline/models/suzanne.obj"
            //path: "/Users/massimo/projects/qmlplayground/QmlRhiPipeline/models/suzanne.obj"
            path: "/home/massimo/projects/qmlplayground/QmlRhiPipeline/models/suzanne.obj"
            position: Qt.vector3d(1.5, -1.2, 0)
            rotationDegrees: Qt.vector3d(0, -30, 0)
            scale: Qt.vector3d(1, 1, 1)
        }
*/
/*
        Sphere {
            id: metalSphere
            visible: true
            position: Qt.vector3d(-1.6, -1.2, 0)
            scale: Qt.vector3d(1, 1, 1)
            baseColor: Qt.vector3d(0.9, 0.9, 0.95)
            emissiveColor: Qt.vector3d(0, 0, 0)
            metalness: 1.0
            roughness: 0.15
        }
*/
    }

    ViewGizmo {
        anchors.top: parent.top
        anchors.right: sidePanel.left
        anchors.margins: 12
        renderer: renderer
        snapCamera: root.snapCamera
    }

    SidePanel {
        id: sidePanel
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        renderer: renderer
        selectedItem: renderer.selectedItem
        spawnPosition: root.lastPickPos
    }

    Rectangle {
        id: fpsOverlay
        visible: root.showFpsOverlay
        color: "#1a1a1a"
        opacity: 0.8
        radius: 6
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 12
        width: 220
        height: 120

        Text {
            anchors.fill: parent
            anchors.margins: 10
            color: "#e8e8e8"
            font.family: "monospace"
            font.pixelSize: 14
            text: "FPS\n" +
                  "  cur: " + root.fpsCurrent.toFixed(1) + "\n" +
                  "  min: " + root.fpsMin.toFixed(1) +
                  "  max: " + root.fpsMax.toFixed(1) + "\n" +
                  "  avg: " + root.fpsAvg.toFixed(1) + "\n" +
                  "  vol: " + (renderer.volumetricEnabled ? "on" : "off") +
                  "  shd: " + (renderer.shadowsEnabled ? "on" : "off") +
                  "  noise: " + (renderer.smokeNoiseEnabled ? "on" : "off")
        }
    }

    onFrameSwapped: updateFps()
}
