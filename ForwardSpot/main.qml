/*
  Q Light Controller Plus
  3DView.qml

  Copyright (c) Massimo Callegari

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

import QtQuick 2.3
import QtQuick.Controls 2.0

import QtQuick.Scene3D 2.0
import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Input 2.0
import Qt3D.Extras 2.0

Rectangle
{
    anchors.fill: parent
    color: "black"

    function setZoom(amount)
    {
        if (cameraZ - amount < 1.0)
            cameraZ = 1.0
        else
            cameraZ -= amount
    }

    MouseArea
    {
        anchors.fill: parent
        onClicked: scene3d.forceActiveFocus()
        preventStealing: true
        onWheel:
        {
            if (wheel.angleDelta.y > 0)
                camera.position.z += 0.3
            else
                camera.position.z -= 0.3
        }
    }

    Scene3D
    {
        id: scene3d
        objectName: "scene3DItem"
        z: 1
        width: parent.width * 0.75
        height: parent.height
        aspects: ["input", "logic"]
        cameraAspectRatioMode: Scene3D.AutomaticAspectRatio

        Entity
        {
            id: sceneRoot
            objectName: "sceneRootEntity"

            Camera
            {
                id: camera
                projectionType: CameraLens.PerspectiveProjection
                fieldOfView: 40
                aspectRatio: scene3d.width / scene3d.height
                nearPlane : 0.1
                farPlane : 1000.0
                position: Qt.vector3d(0.0, 2.5, -7.0)
                upVector: Qt.vector3d(0.0, 1.0, 0.0)
                viewCenter: Qt.vector3d(0.0, 0.0, 0.0)
            }

            OrbitCameraController
            {
                id: camController
                camera: camera
                lookSpeed: 800.0
            }

/*
            FirstPersonCameraController
            {
                camera: camera
                linearSpeed: 500.0
                acceleration: 0.1
                deceleration: 1.0
            }
*/
            RenderSettings
            {
                id : renderSettings
                activeFrameGraph :
                    ForwardRenderer
                    {
                        camera: camera
                        clearColor: "black"
                    }
            }

            // Event Source will be set by the Qt3DQuickWindow
            InputSettings {  }

            components: [ renderSettings ]

            Entity
            {
                id: ambientLight
                components: [
                    DirectionalLight
                    {
                        color: Qt.rgba(1.0, 1.0, 1.0, 1.0)
                        worldDirection: Qt.vector3d(0, -1, 0)
                    },
                    Transform
                    {
                        translation: Qt.vector3d(0, 5, 0)
                    }
                ]
            }

            Entity
            {
                id: ground

                property real xSize: 5.0
                property real zSize: 5.0

                CuboidMesh
                {
                    id: ceilingMesh
                    yzMeshResolution: Qt.size(2, 2)
                    xzMeshResolution: Qt.size(2, 2)
                    xyMeshResolution: Qt.size(2, 2)
                    xExtent: ground.xSize
                    zExtent: ground.zSize
                    yExtent: 0.2
                }

                Transform
                {
                    id: groundTransform
                    translation: Qt.vector3d(0, -1, 0)
                }

                PhongMaterial
                {
                    id: ceilingMaterial
                    ambient: "gray"
                    diffuse: "gray"
                }

                components: [ ceilingMaterial, groundTransform, ceilingMesh ]
            }

            Fixture3DItem
            {
                id: movinghead
                itemSource: "qrc:/moving_head.dae"
            }
        } // sceneRoot
    } // scene3d

    Rectangle
    {
        // scene controls
        color: "white"
        width: parent.width * 0.25
        height: parent.height
        anchors.right: parent.right
        z: 2

        Column
        {
            anchors.fill: parent

            Text
            {
                text: "Pan rotation"
            }

            Slider
            {
                id: panSlider
                from: 0
                to: 360
                onValueChanged: movinghead.setPosition(value, tiltSlider.value)
            }

            Text
            {
                text: "Tilt rotation"
            }

            Slider
            {
                id: tiltSlider
                from: 0
                to: 270
                onValueChanged:  movinghead.setPosition(panSlider.value, value)
            }
        }
    }
}
