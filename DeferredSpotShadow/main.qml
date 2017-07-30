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

    MouseArea
    {
        width: parent.width * 0.75
        height: parent.height
        onClicked: scene3d.forceActiveFocus()
        preventStealing: true
        onWheel:
        {
            if (wheel.angleDelta.y > 0)
                sceneEntity.camera.position.z += 0.3
            else
                sceneEntity.camera.position.z -= 0.3
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

            RenderSettings
            {
                id : renderSettings
                activeFrameGraph:
                    DeferredRenderer
                    {
                        id: frameGraph
                        camera : sceneEntity.camera
                        gBuffer: GBuffer {}
                        sceneLayer: sceneEntity.layer
                        screenQuadLayer: screenQuadEntity.layer
                        //debugLayer: debugEntity.layer
                    }
                renderPolicy: RenderSettings.Always
            }

            // Event Source will be set by the Qt3DQuickWindow
            InputSettings { id: inputSettings  }

            components: [ renderSettings, inputSettings ]

            FirstPersonCameraController { camera: sceneEntity.camera }
            ScreenQuadEntity { id: screenQuadEntity }
            SceneEntity { id: sceneEntity }
            //GBufferDebugger { id: debugEntity }
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
                onValueChanged: View3D.setPanTilt(0, value, tiltSlider.value)
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
                onValueChanged:  View3D.setPanTilt(0, panSlider.value, value)
            }

            Button
            {
                text: "Add mesh"
                onClicked: sceneEntity.createMesh()
            }
        }
    }
}
