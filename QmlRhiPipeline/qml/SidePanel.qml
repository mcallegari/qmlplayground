import QtQuick 
import QtQuick.Layouts 
import QtQuick.Controls 
import QtQuick.Dialogs 
import QtQml
import RhiQmlItem 1.0

Rectangle {
    id: panel
    property var renderer
    property var selectedItem
    property vector3d spawnPosition: Qt.vector3d(0, 0, 0)
    property string modelPath: ""
    property int emitterCountToAdd: 1
    property bool expanded: true

    color: "#151515"
    opacity: 0.95
    width: expanded ? 360 : 44
    clip: true

    Behavior on width {
        NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
    }

    function hasProp(obj, name) {
        return obj && (obj[name] !== undefined)
    }

    function itemKind(obj) {
        if (!obj)
            return "None"
        if (hasProp(obj, "pan") && hasProp(obj, "tilt"))
            return "MovingHeadLight"
        if (hasProp(obj, "zoom"))
            return "StaticLight"
        if (hasProp(obj, "emitterCount") && hasProp(obj, "beamRadius"))
            return "BeamBar"
        if (hasProp(obj, "emitterCount") && hasProp(obj, "emissiveColor"))
            return "PixelBar"
        if (hasProp(obj, "coneAngle"))
            return "Light"
        if (hasProp(obj, "path"))
            return "Model"
        return "Mesh"
    }

    function addItem(kind, explicitPath, explicitEmitterCount) {
        if (!renderer)
            return
        var typeName = kind
        var qml = 'import RhiQmlItem 1.0; ' + typeName + ' {}'
        var obj = Qt.createQmlObject(qml, renderer, typeName.toLowerCase() + "Factory")
        if (typeName === "Model") {
            var path = explicitPath && explicitPath.length ? explicitPath : modelPath
            if (path && path.length > 0)
                obj.path = path
        }
        var count = explicitEmitterCount !== undefined ? explicitEmitterCount : panel.emitterCountToAdd
        if (count && count > 0 && hasProp(obj, "emitterCount"))
            obj.emitterCount = count
        obj.position = spawnPosition
        if (typeName === "StaticLight")
            obj.rotationDegrees = Qt.vector3d(0, 0, 0)
    }

    function fileUrlToPath(url) {
        if (!url || typeof url !== "string")
            return ""
        if (url.indexOf("file://") !== 0)
            return url
        var local = url.slice(7)
        if (local.indexOf("localhost/") === 0)
            local = local.slice(9)
        if (local.length === 0)
            return ""
        if (local[0] !== "/")
            local = "/" + local
        local = decodeURIComponent(local)
        if (local.length >= 3 && local[0] === "/" && local[2] === ":" && /[A-Za-z]/.test(local[1]))
            local = local.slice(1)
        return local
    }

    function dialogToPath(fileValue) {
        if (!fileValue)
            return ""
        if (fileValue.toLocalFile)
            return fileValue.toLocalFile()
        if (typeof fileValue === "string") {
            if (fileValue.indexOf("file:") === 0)
                return fileUrlToPath(fileValue)
            return fileValue
        }
        if (fileValue.toString) {
            var asString = fileValue.toString()
            if (asString.indexOf("file:") === 0)
                return fileUrlToPath(asString)
            return asString
        }
        return ""
    }

    function removeSelected() {
        if (renderer && renderer.removeSelectedItems)
            renderer.removeSelectedItems()
    }

    component FloatSpinBox: SpinBox {
        id: control
        property real realValue: 0.0
        property int decimals: 2
        property real minValue: -10000
        property real maxValue: 10000
        property real stepValue: 0.1
        readonly property real factor: Math.pow(10, decimals)
        signal valueEdited(real value)

        editable: true
        from: Math.round(minValue * factor)
        to: Math.round(maxValue * factor)
        stepSize: Math.max(1, Math.round(stepValue * factor))
        validator: DoubleValidator {
            bottom: control.minValue
            top: control.maxValue
            decimals: control.decimals
        }

        textFromValue: function(value) {
            return (value / factor).toFixed(decimals)
        }
        valueFromText: function(text) {
            var num = Number(text)
            if (isNaN(num))
                return control.value
            return Math.round(num * factor)
        }

        onValueModified: valueEdited(value / factor)

        Component.onCompleted: {
            value = Math.round(realValue * factor)
        }
        onRealValueChanged: {
            value = Math.round(realValue * factor)
        }
    }

    component Vec3Editor: RowLayout {
        id: editor
        property vector3d value: Qt.vector3d(0, 0, 0)
        property real stepValue: 0.1
        property int decimals: 2
        property real minValue: -10000
        property real maxValue: 10000
        signal valueEdited(vector3d value)

        spacing: 6

        FloatSpinBox {
            Layout.preferredWidth: 80
            realValue: editor.value.x
            stepValue: editor.stepValue
            decimals: editor.decimals
            minValue: editor.minValue
            maxValue: editor.maxValue
            onValueEdited: function(v) {
                editor.valueEdited(Qt.vector3d(v, editor.value.y, editor.value.z))
            }
        }
        FloatSpinBox {
            Layout.preferredWidth: 80
            realValue: editor.value.y
            stepValue: editor.stepValue
            decimals: editor.decimals
            minValue: editor.minValue
            maxValue: editor.maxValue
            onValueEdited: function(v) {
                editor.valueEdited(Qt.vector3d(editor.value.x, v, editor.value.z))
            }
        }
        FloatSpinBox {
            Layout.preferredWidth: 80
            realValue: editor.value.z
            stepValue: editor.stepValue
            decimals: editor.decimals
            minValue: editor.minValue
            maxValue: editor.maxValue
            onValueEdited: function(v) {
                editor.valueEdited(Qt.vector3d(editor.value.x, editor.value.y, v))
            }
        }
    }

    FileDialog {
        id: modelDialog
        title: "Select model"
        nameFilters: ["3D Models (*.obj *.gltf *.glb *.dae)", "All Files (*)"]
        onAccepted: {
            var path = dialogToPath(selectedFile)
            if (path && path.length > 0) {
                panel.modelPath = path
                panel.addItem("Model", path)
            }
        }
    }

    FileDialog {
        id: goboDialog
        title: "Select gobo"
        nameFilters: ["Gobo SVG (*.svg)", "All Files (*)"]
        onAccepted: {
            var path = dialogToPath(selectedFile)
            if (path && path.length > 0 && panel.selectedItem) {
                panel.selectedItem.goboPath = path
                goboField.text = path
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: panel.expanded ? ">>" : "<<"
                Layout.preferredWidth: 34
                onClicked: panel.expanded = !panel.expanded
            }

            Text {
                text: "Scene"
                color: "#f2f2f2"
                font.pixelSize: 18
                visible: panel.expanded
            }
        }

        Rectangle {
            height: 1
            color: "#2a2a2a"
            Layout.fillWidth: true
            visible: panel.expanded
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: panel.expanded

            Text {
                text: "Add Items"
                color: "#cfcfcf"
                font.pixelSize: 14
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                ComboBox {
                    id: addTypeCombo
                    Layout.fillWidth: true
                    model: ["Model", "StaticLight", "MovingHeadLight", "Light", "Cube", "Sphere", "PixelBar", "BeamBar"]
                    currentIndex: 0
                }

                Button {
                    text: "Add"
                    onClicked: {
                        if (addTypeCombo.currentText === "Model")
                            modelDialog.open()
                        else if (addTypeCombo.currentText === "PixelBar" || addTypeCombo.currentText === "BeamBar")
                            panel.addItem(addTypeCombo.currentText, "", panel.emitterCountToAdd)
                        else
                            panel.addItem(addTypeCombo.currentText)
                    }
                }

                Button {
                    text: "Remove"
                    enabled: panel.selectedItem !== null
                    onClicked: panel.removeSelected()
                }
            }

            TextField {
                Layout.fillWidth: true
                placeholderText: "Model path (optional)"
                visible: addTypeCombo.currentText === "Model"
                text: panel.modelPath
                onTextEdited: panel.modelPath = text
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                visible: addTypeCombo.currentText === "PixelBar" || addTypeCombo.currentText === "BeamBar"
                Text { text: "Emitters"; color: "#9a9a9a"; Layout.preferredWidth: 70 }
                SpinBox {
                    Layout.fillWidth: true
                    from: 1
                    to: 256
                    value: panel.emitterCountToAdd
                    onValueChanged: {
                        panel.emitterCountToAdd = value
                    }
                }
            }

            Rectangle { height: 1; color: "#2a2a2a"; Layout.fillWidth: true }

            Text {
                text: "Selected: " + itemKind(panel.selectedItem)
                color: "#cfcfcf"
                font.pixelSize: 14
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    visible: hasProp(panel.selectedItem, "position")
                    Text { text: "Position"; color: "#9a9a9a"; Layout.preferredWidth: 70 }
                    Vec3Editor {
                        Layout.fillWidth: true
                        value: hasProp(panel.selectedItem, "position") ? panel.selectedItem.position : Qt.vector3d(0, 0, 0)
                        onValueEdited: function(v) {
                            if (panel.selectedItem)
                                panel.selectedItem.position = v
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: hasProp(panel.selectedItem, "rotationDegrees")
                    Text { text: "Rotation"; color: "#9a9a9a"; Layout.preferredWidth: 70 }
                    Vec3Editor {
                        Layout.fillWidth: true
                        value: hasProp(panel.selectedItem, "rotationDegrees") ? panel.selectedItem.rotationDegrees : Qt.vector3d(0, 0, 0)
                        onValueEdited: function(v) {
                            if (panel.selectedItem)
                                panel.selectedItem.rotationDegrees = v
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: hasProp(panel.selectedItem, "direction")
                    Text { text: "Direction"; color: "#9a9a9a"; Layout.preferredWidth: 70 }
                    Vec3Editor {
                        Layout.fillWidth: true
                        value: hasProp(panel.selectedItem, "direction") ? panel.selectedItem.direction : Qt.vector3d(0, 0, 0)
                        onValueEdited: function(v) {
                            if (panel.selectedItem)
                                panel.selectedItem.direction = v
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: hasProp(panel.selectedItem, "scale")
                    Text { text: "Scale"; color: "#9a9a9a"; Layout.preferredWidth: 70 }
                    Vec3Editor {
                        Layout.fillWidth: true
                        value: hasProp(panel.selectedItem, "scale") ? panel.selectedItem.scale : Qt.vector3d(1, 1, 1)
                        minValue: 0.0
                        onValueEdited: function(v) {
                            if (panel.selectedItem)
                                panel.selectedItem.scale = v
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: hasProp(panel.selectedItem, "emitterCount")
                    Text { text: "Emitters"; color: "#9a9a9a"; Layout.preferredWidth: 70 }
                    SpinBox {
                        Layout.fillWidth: true
                        from: 1
                        to: 256
                        value: hasProp(panel.selectedItem, "emitterCount") ? panel.selectedItem.emitterCount : 0
                        onValueChanged: function() {
                            if (panel.selectedItem && panel.selectedItem.emitterCount !== value)
                                panel.selectedItem.emitterCount = value
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    visible: hasProp(panel.selectedItem, "baseColor")
                    Text { text: "Base"; color: "#9a9a9a"; Layout.preferredWidth: 70 }
                    Vec3Editor {
                        Layout.fillWidth: true
                        value: hasProp(panel.selectedItem, "baseColor") ? panel.selectedItem.baseColor : Qt.vector3d(0.5, 0.5, 0.5)
                        minValue: 0.0
                        maxValue: 10.0
                        stepValue: 0.05
                        onValueEdited: function(v) {
                            if (panel.selectedItem)
                                panel.selectedItem.baseColor = v
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: hasProp(panel.selectedItem, "color")
                    Text { text: "Color"; color: "#9a9a9a"; Layout.preferredWidth: 70 }
                    Vec3Editor {
                        Layout.fillWidth: true
                        value: hasProp(panel.selectedItem, "color") ? panel.selectedItem.color : Qt.vector3d(1, 1, 1)
                        minValue: 0.0
                        maxValue: 10.0
                        stepValue: 0.05
                        onValueEdited: function(v) {
                            if (panel.selectedItem)
                                panel.selectedItem.color = v
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: hasProp(panel.selectedItem, "emissiveColor")
                    Text { text: "Emissive"; color: "#9a9a9a"; Layout.preferredWidth: 70 }
                    Vec3Editor {
                        Layout.fillWidth: true
                        value: hasProp(panel.selectedItem, "emissiveColor") ? panel.selectedItem.emissiveColor : Qt.vector3d(0.0, 0.0, 0.0)
                        minValue: 0.0
                        maxValue: 20.0
                        stepValue: 0.05
                        onValueEdited: function(v) {
                            if (panel.selectedItem)
                                panel.selectedItem.emissiveColor = v
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: hasProp(panel.selectedItem, "intensity")
                    Text { text: "Intensity"; color: "#9a9a9a"; Layout.preferredWidth: 70 }
                    FloatSpinBox {
                        Layout.fillWidth: true
                        realValue: hasProp(panel.selectedItem, "intensity") ? panel.selectedItem.intensity : 0.0
                        minValue: 0.0
                        maxValue: 100.0
                        stepValue: 0.1
                        onValueEdited: function(v) {
                            if (panel.selectedItem)
                                panel.selectedItem.intensity = v
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: hasProp(panel.selectedItem, "range")
                    Text { text: "Range"; color: "#9a9a9a"; Layout.preferredWidth: 70 }
                    FloatSpinBox {
                        Layout.fillWidth: true
                        realValue: hasProp(panel.selectedItem, "range") ? panel.selectedItem.range : 0.0
                        minValue: 0.0
                        maxValue: 500.0
                        stepValue: 0.5
                        onValueEdited: function(v) {
                            if (panel.selectedItem)
                                panel.selectedItem.range = v
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: hasProp(panel.selectedItem, "coneAngle")
                    Text { text: "Cone"; color: "#9a9a9a"; Layout.preferredWidth: 70 }
                    FloatSpinBox {
                        Layout.fillWidth: true
                        realValue: hasProp(panel.selectedItem, "coneAngle") ? panel.selectedItem.coneAngle : 0.0
                        minValue: 0.1
                        maxValue: 120.0
                        stepValue: 0.5
                        onValueEdited: function(v) {
                            if (panel.selectedItem)
                                panel.selectedItem.coneAngle = v
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: hasProp(panel.selectedItem, "zoom")
                    Text { text: "Zoom"; color: "#9a9a9a"; Layout.preferredWidth: 70 }
                    FloatSpinBox {
                        Layout.fillWidth: true
                        realValue: hasProp(panel.selectedItem, "zoom") ? panel.selectedItem.zoom : 0.0
                        minValue: 0.1
                        maxValue: 120.0
                        stepValue: 0.5
                        onValueEdited: function(v) {
                            if (panel.selectedItem)
                                panel.selectedItem.zoom = v
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                visible: hasProp(panel.selectedItem, "goboPath")

                Text { text: "Gobo"; color: "#cfcfcf"; font.pixelSize: 14 }
                RowLayout {
                    Layout.fillWidth: true
                    TextField {
                        id: goboField
                        Layout.fillWidth: true
                        placeholderText: "path/to/gobo.svg"
                        text: hasProp(panel.selectedItem, "goboPath") ? panel.selectedItem.goboPath : ""
                        onEditingFinished: {
                            if (panel.selectedItem)
                                panel.selectedItem.goboPath = text
                        }
                    }
                    Button {
                        text: "..."
                        onClicked: goboDialog.open()
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6
                visible: hasProp(panel.selectedItem, "pan") && hasProp(panel.selectedItem, "tilt")

                Text { text: "Pan / Tilt"; color: "#cfcfcf"; font.pixelSize: 14 }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Pan"; color: "#9a9a9a"; Layout.preferredWidth: 70 }
                    FloatSpinBox {
                        Layout.fillWidth: true
                        realValue: hasProp(panel.selectedItem, "pan") ? panel.selectedItem.pan : 0.0
                        minValue: -540.0
                        maxValue: 540.0
                        stepValue: 1.0
                        onValueEdited: function(v) {
                            if (panel.selectedItem)
                                panel.selectedItem.pan = v
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: "Tilt"; color: "#9a9a9a"; Layout.preferredWidth: 70 }
                    FloatSpinBox {
                        Layout.fillWidth: true
                        realValue: hasProp(panel.selectedItem, "tilt") ? panel.selectedItem.tilt : 0.0
                        minValue: -180.0
                        maxValue: 180.0
                        stepValue: 1.0
                        onValueEdited: function(v) {
                            if (panel.selectedItem)
                                panel.selectedItem.tilt = v
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }
}
