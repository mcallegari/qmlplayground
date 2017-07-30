/*
  Q Light Controller Plus
  Fixture3DItem.qml

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

import QtQuick 2.7

import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Extras 2.0

import "."

Entity
{
    id: fixtureEntity

    property int fixtureID: 0
    property alias itemSource: eSceneLoader.source

    property int panMaxDegrees: 0
    property int tiltMaxDegrees: 0

    function setPosition(pan, tilt)
    {
        console.log("[3Ditem] set position " + pan + ", " + tilt)
        if (panMaxDegrees)
        {
            panAnim.stop()
            panAnim.from = eSceneLoader.panRotation
            panAnim.to = pan
            panAnim.start()
        }

        if (tiltMaxDegrees)
        {
            tiltAnim.stop()
            tiltAnim.from = eSceneLoader.tiltRotation
            var degTo = tilt - (tiltMaxDegrees / 2)
            //console.log("Tilt to " + degTo + ", max: " + tiltMaxDegrees)
            tiltAnim.to = degTo
            tiltAnim.start()
        }
    }

    SceneLoader
    {
        id: eSceneLoader

        property real panRotation: 0
        property real tiltRotation: 0

        function bindPanTransform(t, maxDegrees)
        {
            console.log("Binding pan ----")
            fixtureEntity.panMaxDegrees = maxDegrees
            t.rotationZ = Qt.binding(function() { return panRotation })
        }

        function bindTiltTransform(t, maxDegrees)
        {
            console.log("Binding tilt ----")
            fixtureEntity.tiltMaxDegrees = maxDegrees
            tiltRotation = maxDegrees / 2
            t.rotationX = Qt.binding(function() { return tiltRotation })
        }

        onStatusChanged:
        {
            if (status == SceneLoader.Ready)
                View3D.initializeFixture(fixtureID, eObjectPicker, eSceneLoader, eSpotLight)
        }

        NumberAnimation on panRotation
        {
            id: panAnim
            duration: 2000
            easing.type: Easing.Linear
        }

        NumberAnimation on tiltRotation
        {
            id: tiltAnim
            duration: 2000
            easing.type: Easing.Linear
        }
    }

    Transform
    {
        id: eTransform
        translation: Qt.vector3d(0, 0, 0)
    }

    components: [ eTransform, eSceneLoader ]

    SpotLight
    {
        id: eSpotLight
        localDirection: Qt.vector3d(0.0, 0.0, -1.0)
        color: "blue"
        cutOffAngle: 15
        constantAttenuation: 1
        intensity: 0.8
    }

    /* This gets re-parented and activated on initializeFixture */
    ObjectPicker
    {
        id: eObjectPicker
        objectName: "ePicker"
        //hoverEnabled: true
        dragEnabled: true

        property var lastPos

        onClicked: console.log("Item clicked")
        onPressed: lastPos = pick.worldIntersection
        //onReleased: console.log("Item release")
        //onEntered: console.log("Item entered")
        //onExited: console.log("Item exited")
    }
}


