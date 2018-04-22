/*
  Q Light Controller Plus
  SceneEntity.qml

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

import QtQuick 2.0

import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Input 2.0
import Qt3D.Extras 2.0

Entity
{
    id: sceneRootEntity
    objectName: "sceneRootEntity"

    property size viewSize
    readonly property Camera camera: camera
    readonly property Layer deferredLayer: Layer { objectName: "sceneDeferredLayer" }
    readonly property Layer selectionLayer: Layer { objectName: "selectionLayer" }
    readonly property GeometryRenderer selectionMesh: SelectionGeometry { }
    readonly property Effect geometryPassEffect: GeometryPassEffect { }

    // Global elements
    Camera
    {
        id: camera
        projectionType: CameraLens.PerspectiveProjection
        fieldOfView: 45
        aspectRatio: viewSize.width / viewSize.height
        nearPlane : 1.0
        farPlane : 1000.0
        position: Qt.vector3d(0.0, 3.0, 7.5)
        upVector: Qt.vector3d(0.0, 1.0, 0.0)
        viewCenter: Qt.vector3d(0.0, 1.0, 0.0)
    }

    Entity
    {
        id: stage

        property vector3d size: Qt.vector3d(10, 0.2, 10)
        property Layer sceneLayer: deferredLayer
        property Effect effect: geometryPassEffect

        property Material material:
            Material
            {
                effect: stage.effect
                parameters: Parameter { name: "meshColor"; value: "lightgray" }
            }

        property Transform transform:
            Transform
            {
                translation: Qt.vector3d(0, -1, 0)
            }

        CuboidMesh
        {
            id: stageMesh
            xExtent: stage.size.x
            yExtent: stage.size.y
            zExtent: stage.size.z
        }

        components: [
            stageMesh,
            stage.material,
            stage.transform,
            stage.sceneLayer
        ]
    }
}
