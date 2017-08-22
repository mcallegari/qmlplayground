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

import QtQuick 2.0

import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Input 2.0
import Qt3D.Extras 2.0

Entity {
    id : root
    readonly property Camera camera: camera
    readonly property Layer layer: sceneLayer

    property int itemRows: 10
    property int itemColumns: 10

    property var lightsPos: []

    signal sceneCreated()

    // Global elements
    Camera {
        id: camera
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        aspectRatio: 16/9
        nearPlane : 1.0
        farPlane : 1000.0
        position: Qt.vector3d( 0.0, 10.0, -25.0 )
        upVector: Qt.vector3d( 0.0, 1.0, 0.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 10.0 )
    }

    ConeMesh {
        id: coneMesh
        length: 1.5
        bottomRadius: 0.5
        topRadius: 0
    }

    SceneEffect { id : sceneMaterialEffect }

    Layer { id: sceneLayer }

    Component.onCompleted:
    {
        var xPos = -itemColumns
        var zPos = -itemRows

        var tmp = lightsPos

        // if you change the mesh number, change also the maximum
        // number of lights in the final fragment shader
        for (var z = 0; z < itemRows; z++)
        {
            xPos = -itemColumns
            for (var x = 0; x < itemColumns; x++)
            {
                var component = Qt.createComponent("AnimatedEntity.qml");
                var mesh = component.createObject(root,
                                                  {"xPos": xPos, "zPos": zPos, "mesh": coneMesh,
                                                   "geomPassEffect": sceneMaterialEffect, "targetLayer": sceneLayer });
                tmp.push(mesh)
                xPos += 2
            }
            zPos += 2
        }

        lightsPos = tmp
        sceneCreated()
    }

    Entity
    {
        id: plane
        property Material material : Material {
            effect : sceneMaterialEffect
            parameters : Parameter { name : "meshColor"; value : "lightgray" }
        }

        PlaneMesh {
            id: groundMesh
            width: 50
            height: width
            meshResolution: Qt.size(2, 2)
        }
        
        property Transform transform: Transform {
            translation: Qt.vector3d(0, -2, 0)
        }

        components: [
            groundMesh,
            plane.material,
            plane.transform,
            sceneLayer
        ]
    }
}
