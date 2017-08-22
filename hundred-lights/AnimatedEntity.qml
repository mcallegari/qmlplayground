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

import QtQuick 2.0 as QQ2

import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Input 2.0
import Qt3D.Extras 2.0

Entity
{
    id: meshRoot
    property real xPos: 0.0
    property real zPos: 0.0

    property ConeMesh mesh
    property Effect geomPassEffect
    property Layer targetLayer
    property color meshColor: '#'+Math.random().toString(16).substr(-6);

    property Material material: Material {
        effect: geomPassEffect
        parameters : Parameter { name : "meshColor"; value : meshColor }
    }

    property Transform transform: Transform {
        id: meshTransform
        property real yPos: 3.0
        translation: Qt.vector3d(xPos, yPos, zPos)
        rotationZ: -35
    }

    QQ2.SequentialAnimation {
        loops: QQ2.Animation.Infinite
        running: true
        //QQ2.NumberAnimation { target: meshTransform; property: "yPos"; to: 0; duration: Math.random() * 1000 + 3000 }
        //QQ2.NumberAnimation { target: meshTransform; property: "yPos"; to: 7; duration: Math.random() * 1000 + 3000 }
        QQ2.NumberAnimation { target: meshTransform; property: "rotationZ"; to: 35; duration: Math.random() * 1000 + 1000 }
        QQ2.NumberAnimation { target: meshTransform; property: "rotationZ"; to: -35; duration: Math.random() * 1000 + 1000 }
    }

    components: [
        mesh,
        meshRoot.material,
        meshTransform,
        meshRoot.targetLayer
    ]
}
