import QtQuick 2.15

Item {
    id: viewCube
    property var renderer: null
    property var snapCamera: null
    width: 120
    height: 120
    z: 10

    property vector3d camForward: Qt.vector3d(0, 0, -1)
    property vector3d camRight: Qt.vector3d(1, 0, 0)
    property vector3d camUp: Qt.vector3d(0, 1, 0)

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

    function updateBasis() {
        if (!renderer)
            return
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
            var lightDir = Qt.vector3d(0.4, 0.6, 0.7)
            var lightLen = Math.sqrt(lightDir.x * lightDir.x + lightDir.y * lightDir.y + lightDir.z * lightDir.z)
            if (lightLen > 0.0001)
                lightDir = Qt.vector3d(lightDir.x / lightLen, lightDir.y / lightLen, lightDir.z / lightLen)

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
            var faces = [
                [0, 1, 2, 3], // -Z
                [4, 5, 6, 7], // +Z
                [0, 1, 5, 4], // -Y
                [3, 2, 6, 7], // +Y
                [0, 3, 7, 4], // -X
                [1, 2, 6, 5]  // +X
            ]

            var rverts = []
            for (var i = 0; i < verts.length; ++i)
                rverts.push(rot(verts[i]))

            function faceDepth(face) {
                return (rverts[face[0]].z + rverts[face[1]].z +
                        rverts[face[2]].z + rverts[face[3]].z) * 0.25
            }

            function faceNormal(face) {
                var a = rverts[face[0]]
                var b = rverts[face[1]]
                var c = rverts[face[2]]
                var ab = Qt.vector3d(b.x - a.x, b.y - a.y, b.z - a.z)
                var ac = Qt.vector3d(c.x - a.x, c.y - a.y, c.z - a.z)
                var n = vcross(ab, ac)
                return vnorm(n)
            }

            var faceOrder = faces.slice()
            faceOrder.sort(function(a, b) { return faceDepth(a) - faceDepth(b) })

            for (var f = 0; f < faceOrder.length; ++f) {
                var face = faceOrder[f]
                var n = faceNormal(face)
                var lit = Math.max(0.1, vdot(n, lightDir))
                var shade = Math.floor(90 + 140 * lit)
                ctx.fillStyle = "rgb(" + shade + "," + shade + "," + shade + ")"
                ctx.beginPath()
                var p0 = proj(rverts[face[0]])
                ctx.moveTo(p0.x, p0.y)
                for (var k = 1; k < 4; ++k) {
                    var pk = proj(rverts[face[k]])
                    ctx.lineTo(pk.x, pk.y)
                }
                ctx.closePath()
                ctx.fill()
            }

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
            if (!pressed || !renderer)
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
            if (dragging || !snapCamera)
                return
            updateBasis()
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
                snapCamera(best.axis, true)
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
