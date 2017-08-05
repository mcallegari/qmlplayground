/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
import QtQuick 2.7

import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Extras 2.0

Entity
{
    id: root

    readonly property Layer layer: screenQuadLayer
    property var lightsArray: []
    property bool sceneReady: false
    property bool quadReady: false

    function sceneCreated()
    {
        sceneReady = true
        if (sceneReady && quadReady)
            updateUniforms()
    }

    //property vector3d l1pos: lightsArray[0].transform.translation
    //onL1posChanged: console.log("Light 1 position changed: " + l1pos)

    function updateUniforms()
    {
        var parameters = [].slice.apply(lightPassMaterial.parameters)

        for (var i = 0; i < lightsArray.length; i++)
        {
            parameters.push(uniformComp.createObject(lightPassMaterial,
                            { name: "lightsArray[%1].type".arg(i), value: 0 }))
            parameters.push(uniformComp.createObject(lightPassMaterial,
                            { name: "lightsArray[%1].position".arg(i), value: Qt.binding(function() { return lightsArray[i].transform.translation }) }))
            parameters.push(uniformComp.createObject(lightPassMaterial,
                            { name: "lightsArray[%1].color".arg(i), value: lightsArray[i].meshColor }))
            parameters.push(uniformComp.createObject(lightPassMaterial,
                            { name: "lightsArray[%1].intensity".arg(i), value: 0.10 }))
            parameters.push(uniformComp.createObject(lightPassMaterial,
                            { name: "lightsArray[%1].constantAttenuation".arg(i), value: 1.0 }))
            parameters.push(uniformComp.createObject(lightPassMaterial,
                            { name: "lightsArray[%1].linearAttenuation".arg(i), value: 0.0 }))
            parameters.push(uniformComp.createObject(lightPassMaterial,
                            { name: "lightsArray[%1].linearAttenuation".arg(i), value: 0.0 }))
        }

        parameters.push(uniformComp.createObject(lightPassMaterial,
                        { name: "lightsNumber", value: lightsArray.length }))

        // dump the uniform list
        parameters.forEach(function (p) { console.log(p.name, '=', p.value); })

        lightPassMaterial.parameters = parameters
    }

    // dummy component to create a uniform
    Component
    {
        id: uniformComp
        Parameter {}
    }

    // We need to have the actual screen quad entity separate from the lights.
    // If the lights were sub-entities of this screen quad entity, they would
    // be affected by the rotation matrix, and their world positions would thus
    // be changed.
    Entity {
        components : [
            Layer { id: screenQuadLayer },

            PlaneMesh {
                width: 2.0
                height: 2.0
                meshResolution: Qt.size(2, 2)
            },

            Transform { // We rotate the plane so that it faces us
                rotation: fromAxisAndAngle(Qt.vector3d(1, 0, 0), 90)
            },

            Material {
                id: lightPassMaterial
                effect : FinalEffect {
                    Component.onCompleted: {
                        quadReady = true
                        if (sceneReady && quadReady)
                            updateUniforms()
                        }
                }
                /*
                parameters: [
                    Parameter { name: "lightsNumber"; value : 1 },
                    Parameter { name: "lightsArray[0].type"; value : 0 }, // Point Light
                    Parameter { name: "lightsArray[0].position"; value : Qt.vector3d(0, 0, 10) },
                    Parameter { name: "lightsArray[0].color"; value : "white" },
                    Parameter { name: "lightsArray[0].intensity"; value : 0.8 },
                    Parameter { name: "lightsArray[0].constantAttenuation"; value: 1.0 },
                    Parameter { name: "lightsArray[0].linearAttenuation"; value: 0.0 },
                    Parameter { name: "lightsArray[0].quadraticAttenuation"; value: 0.0 }
                ]
                */
            }
        ]
    }
}
