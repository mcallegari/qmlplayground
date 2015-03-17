/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

import Qt3D 2.0
import Qt3D.Render 2.0

Effect {
    property alias spotLightsSD: spotLightsShaderData.values

    techniques : [
        // OpenGL 3.1
        Technique {
            openGLFilter {api : OpenGLFilter.Desktop; profile : OpenGLFilter.Core; minorVersion : 1; majorVersion : 3 }
            parameters: [
                Parameter {
                    name: "PointLightBlock"
                    value: ShaderData {
                        property ShaderDataArray pointLights: ShaderDataArray {
                            // hard coded lights until we have a way to filter
                            // ShaderData in a scene
                            values: [ sceneEntity.light, light3.light ]
                        }
                    }
                },
                Parameter {
                    name: "SpotLightBlock"
                    value: ShaderData {
                        property ShaderDataArray spotLights: ShaderDataArray {
                            id: spotLightsShaderData
                            // hard coded lights until we have a way to filter
                            // ShaderData in a scene
                            values: [ coneLight.light ]
                        }
                    }
                }
            ]
            renderPasses : RenderPass {
                annotations : Annotation { name : "pass"; value : "final" }
                shaderProgram : ShaderProgram {
                    id : finalShaderGL3
                    vertexShaderCode: loadSource("qrc:/shaders/deferred.vert")
                    fragmentShaderCode: loadSource("qrc:/shaders/deferred.frag")

                }
            }
        },
        // OpenGL 2.0 with FBO extension
        Technique {
            openGLFilter {api : OpenGLFilter.Desktop; profile : OpenGLFilter.None; minorVersion : 0; majorVersion : 2 }
            parameters: Parameter { name: "pointLights"; value: ShaderData {
                                property ShaderDataArray lights: ShaderDataArray {
                                    // hard coded lights until we have a way to filter
                                    // ShaderData in a scene
                                    values: [sceneEntity.light, light3.light]
                                }
                            }
                        }
            renderPasses : RenderPass {
                annotations : Annotation { name : "pass"; value : "final" }
                shaderProgram : ShaderProgram {
                    id : finalShaderGL2
                    vertexShaderCode: loadSource("qrc:/shaders/deferred_gl2.vert")
                    fragmentShaderCode: loadSource("qrc:/shaders/deferred_gl2.frag")
                }
            }
        }]
}
