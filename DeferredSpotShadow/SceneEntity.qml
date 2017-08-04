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

Entity
{
    id : root
    readonly property Camera camera: camera
    readonly property Layer layer: sceneLayer

    readonly property vector3d light1Pos : sphere1.transform.translation
    readonly property vector3d light2Pos : sphere2.transform.translation

    function createMesh(quadEntity)
    {
        View3D.createMesh(root, sceneLayer, sceneMaterialEffect, quadEntity)
    }
/*
    property PointLight light: PointLight {
        color : "white"
        intensity : 0.8

        property Transform transform: Transform {
            translation: Qt.vector3d(0, 0, 10)
        }

        //ColorAnimation on color { from: "white"; to: "blue"; duration: 4000; loops: 2 }
        SequentialAnimation on intensity {
            NumberAnimation {
                from: 0; to: 1.0; duration: 4000
            }
            NumberAnimation {
                from: 1.0; to: 0.0; duration: 4000
            }
            loops: Animation.Infinite
        }
    }
*/
    // Global elements
    Camera
    {
        id: camera
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        aspectRatio: 16/9
        nearPlane : 1.0
        farPlane : 1000.0
        position: Qt.vector3d( 0.0, 0.0, -25.0 )
        upVector: Qt.vector3d( 0.0, 1.0, 0.0 )
        viewCenter: Qt.vector3d( 0.0, 0.0, 10.0 )
    }

    SphereMesh
    {
        id : sphereMesh
        rings: 50
        slices: 100
    }

    SceneEffect { id : sceneMaterialEffect }

    Layer { id: sceneLayer }

    Entity
    {
        id : sphere1

        property Material material : Material {
            effect : sceneMaterialEffect
            parameters : Parameter { name : "meshColor"; value : "green" }
        }

        property Transform transform: Transform {
            translation: Qt.vector3d(5, 0, 5)
        }

        components : [
            sphereMesh,
            sphere1.transform,
            sphere1.material,
            sceneLayer
        ]
    }

    Entity
    {
        id: sphere2

        property Material material : Material {
            effect : sceneMaterialEffect
            parameters : Parameter { name : "meshColor"; value : "red" }
        }

        property Transform transform: Transform {
            id: light3Transform
            property real y: 2.0
            translation: Qt.vector3d(2, y, 7)
        }

        SequentialAnimation {
            loops: Animation.Infinite
            running: true
            NumberAnimation { target: light3Transform; property: "y"; to: 6; duration: 5000; easing.type: Easing.InOutQuad }
            NumberAnimation { target: light3Transform; property: "y"; to: -6; duration: 5000; easing.type: Easing.InOutQuint }
        }

        components: [
            sphereMesh,
            sphere2.material,
            sphere2.transform,
            sceneLayer
        ]
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
            translation: Qt.vector3d(0, -5, 0)
        }

        components: [
            groundMesh,
            plane.material,
            plane.transform,
            sceneLayer
        ]
    }
}
