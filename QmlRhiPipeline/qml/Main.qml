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
    property vector3d lastPickPos: Qt.vector3d(0, 0, 0)
    property var selectableItems: []
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
    function snapCamera(axis) {
        var target = renderer.cameraTarget
        var pos = renderer.cameraPosition
        var dx = pos.x - target.x
        var dy = pos.y - target.y
        var dz = pos.z - target.z
        var dist = Math.sqrt(dx * dx + dy * dy + dz * dz)
        if (dist < 0.001)
            dist = 1.0
        var newPos = Qt.vector3d(target.x + axis.x * dist,
                                 target.y + axis.y * dist,
                                 target.z + axis.z * dist)
        renderer.cameraPosition = newPos
        renderer.cameraTarget = target
        renderer.setCameraDirection(Qt.vector3d(target.x - newPos.x,
                                                target.y - newPos.y,
                                                target.z - newPos.z))
    }
    function registerSelectable(item) {
        if (!item)
            return
        for (var i = 0; i < selectableItems.length; ++i) {
            if (selectableItems[i] === item)
                return
        }
        selectableItems.push(item)
    }
    function unregisterSelectable(item) {
        if (!item)
            return
        for (var i = selectableItems.length - 1; i >= 0; --i) {
            if (selectableItems[i] === item)
                selectableItems.splice(i, 1)
        }
    }
    function clearSelection() {
        for (var i = 0; i < selectableItems.length; ++i) {
            if (selectableItems[i])
                selectableItems[i].isSelected = false
        }
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
        onMeshPicked: function(item, worldPos, hit) {
            root.clearSelection()
            if (hit)
                root.lastPickPos = worldPos
            if (item && item.isSelected !== undefined && hit) {
                item.isSelected = true
                renderer.selectedItem = item
            } else {
                renderer.selectedItem = null
            }
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
            onObjectAdded: function(index, object) { root.registerSelectable(object) }
            onObjectRemoved: function(index, object) { root.unregisterSelectable(object) }
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
            id: mainCube
            position: cubePos
            rotationDegrees: Qt.vector3d(0, 0, 0)
            scale: Qt.vector3d(1, 1, 1)
            Component.onCompleted: root.registerSelectable(this)
            Component.onDestruction: root.unregisterSelectable(this)
        }

        // ground plane
        Cube {
            id: groundPlane
            position: Qt.vector3d(0, -2.5, 0)
            rotationDegrees: Qt.vector3d(0, 0, 0)
            scale: Qt.vector3d(10, 0.2, 10)
            Component.onCompleted: root.registerSelectable(this)
            Component.onDestruction: root.unregisterSelectable(this)
        }

        Model {
            id: suzanne
            path: "/home/massimo/projects/qmlplayground/QmlRhiPipeline/models/suzanne.obj"
            position: Qt.vector3d(1.5, -1.2, 0)
            rotationDegrees: Qt.vector3d(0, -30, 0)
            scale: Qt.vector3d(1, 1, 1)
            Component.onCompleted: root.registerSelectable(this)
            Component.onDestruction: root.unregisterSelectable(this)
        }

    }

    Item {
        id: viewCube
        width: 120
        height: 120
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 12
        z: 10

        property vector3d camForward: Qt.vector3d(0, 0, -1)
        property vector3d camRight: Qt.vector3d(1, 0, 0)
        property vector3d camUp: Qt.vector3d(0, 1, 0)

        function updateBasis() {
            var f = vnorm(Qt.vector3d(renderer.cameraTarget.x - renderer.cameraPosition.x,
                                      renderer.cameraTarget.y - renderer.cameraPosition.y,
                                      renderer.cameraTarget.z - renderer.cameraPosition.z))
            var upWorld = Qt.vector3d(0, 1, 0)
            var r = vcross(f, upWorld)
            if (vlen(r) < 0.0001)
                r = vcross(f, Qt.vector3d(0, 0, 1))
            r = vnorm(r)
            var u = vnorm(vcross(r, f))
            camForward = f
            camRight = r
            camUp = u
        }

        Canvas {
            id: cubeCanvas
            anchors.fill: parent
            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                var cx = width * 0.5
                var cy = height * 0.5
                var scale = width * 0.32

                function rot(v) {
                    return Qt.vector3d(
                        vdot(v, viewCube.camRight),
                        vdot(v, viewCube.camUp),
                        vdot(v, Qt.vector3d(-viewCube.camForward.x, -viewCube.camForward.y, -viewCube.camForward.z))
                    )
                }
                function proj(v) {
                    return Qt.point(cx + v.x * scale, cy - v.y * scale)
                }

                var verts = [
                    Qt.vector3d(-1, -1, -1),
                    Qt.vector3d(1, -1, -1),
                    Qt.vector3d(1, 1, -1),
                    Qt.vector3d(-1, 1, -1),
                    Qt.vector3d(-1, -1, 1),
                    Qt.vector3d(1, -1, 1),
                    Qt.vector3d(1, 1, 1),
                    Qt.vector3d(-1, 1, 1)
                ]
                var edges = [
                    [0, 1], [1, 2], [2, 3], [3, 0],
                    [4, 5], [5, 6], [6, 7], [7, 4],
                    [0, 4], [1, 5], [2, 6], [3, 7]
                ]

                var rverts = []
                for (var i = 0; i < verts.length; ++i)
                    rverts.push(rot(verts[i]))

                ctx.lineWidth = 2
                ctx.strokeStyle = "#f3e86b"
                ctx.beginPath()
                for (var e = 0; e < edges.length; ++e) {
                    var a = proj(rverts[edges[e][0]])
                    var b = proj(rverts[edges[e][1]])
                    ctx.moveTo(a.x, a.y)
                    ctx.lineTo(b.x, b.y)
                }
                ctx.stroke()
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            property point pressPos: Qt.point(0, 0)
            property bool dragging: false

            onPressed: function(mouse) {
                pressPos = Qt.point(mouse.x, mouse.y)
                dragging = false
            }
            onPositionChanged: function(mouse) {
                if (!pressed)
                    return
                var dx = mouse.x - pressPos.x
                var dy = mouse.y - pressPos.y
                if (!dragging && (Math.abs(dx) > 4 || Math.abs(dy) > 4))
                    dragging = true
                if (dragging) {
                    renderer.rotateFreeCamera(dx * 0.2, -dy * 0.2)
                    pressPos = Qt.point(mouse.x, mouse.y)
                }
            }
            onReleased: function(mouse) {
                if (dragging)
                    return
                viewCube.updateBasis()
                var cx = viewCube.width * 0.5
                var cy = viewCube.height * 0.5
                var scale = viewCube.width * 0.32
                var click = Qt.point(mouse.x, mouse.y)

                var faces = [
                    { axis: Qt.vector3d(1, 0, 0) },
                    { axis: Qt.vector3d(-1, 0, 0) },
                    { axis: Qt.vector3d(0, 1, 0) },
                    { axis: Qt.vector3d(0, -1, 0) },
                    { axis: Qt.vector3d(0, 0, 1) },
                    { axis: Qt.vector3d(0, 0, -1) }
                ]

                function rot(v) {
                    return Qt.vector3d(
                        vdot(v, viewCube.camRight),
                        vdot(v, viewCube.camUp),
                        vdot(v, Qt.vector3d(-viewCube.camForward.x, -viewCube.camForward.y, -viewCube.camForward.z))
                    )
                }

                var best = null
                var bestDist = 1e9
                for (var i = 0; i < faces.length; ++i) {
                    var n = faces[i].axis
                    var rn = rot(n)
                    if (rn.z <= 0.0)
                        continue
                    var p = Qt.point(cx + rn.x * scale * 0.9, cy - rn.y * scale * 0.9)
                    var dx = p.x - click.x
                    var dy = p.y - click.y
                    var d2 = dx * dx + dy * dy
                    if (d2 < bestDist) {
                        bestDist = d2
                        best = faces[i]
                    }
                }
                if (best && bestDist < (scale * scale * 0.5))
                    root.snapCamera(best.axis)
            }
        }

        Component.onCompleted: {
            updateBasis()
            cubeCanvas.requestPaint()
        }
        Connections {
            target: renderer
            function onCameraPositionChanged() { viewCube.updateBasis(); cubeCanvas.requestPaint() }
            function onCameraTargetChanged() { viewCube.updateBasis(); cubeCanvas.requestPaint() }
        }
    }
}
