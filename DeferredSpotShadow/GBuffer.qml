/*
  Q Light Controller Plus
  GBuffer.qml

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

import Qt3D.Core 2.0
import Qt3D.Render 2.0

RenderTarget
{
    property alias color: colorAttachment
    property alias position: positionAttachment
    property alias normal: normalAttachment
    property alias depth: depthAttachment

    attachments: [
        RenderTargetOutput
        {
            objectName: "color"
            attachmentPoint: RenderTargetOutput.Color0
            texture:
                Texture2D
                {
                    id: colorAttachment
                    width: 1024
                    height: 1024
                    format: Texture.RGBA32F
                    generateMipMaps: false
                    magnificationFilter: Texture.Linear
                    minificationFilter: Texture.Linear
                    wrapMode
                    {
                        x: WrapMode.ClampToEdge
                        y: WrapMode.ClampToEdge
                    }
                }
        },
        RenderTargetOutput
        {
            objectName: "position"
            attachmentPoint: RenderTargetOutput.Color1
            texture:
                Texture2D
                {
                    id: positionAttachment
                    width: 1024
                    height: 1024
                    // We use RGBA32F here instead of a more fitting format because
                    // OpenGL vendors might not support other formats
                    format: Texture.RGBA32F
                    generateMipMaps: false
                    magnificationFilter: Texture.Linear
                    minificationFilter: Texture.Linear
                    wrapMode
                    {
                        x: WrapMode.ClampToEdge
                        y: WrapMode.ClampToEdge
                    }
                }
        },
        RenderTargetOutput
        {
            objectName: "normal"
            attachmentPoint: RenderTargetOutput.Color2
            texture:
                Texture2D
                {
                    id: normalAttachment
                    width: 1024
                    height: 1024
                    // We use RGBA32F here instead of a more fitting format because
                    // OpenGL vendors might not support other formats
                    format: Texture.RGBA32F
                    generateMipMaps: false
                    magnificationFilter: Texture.Linear
                    minificationFilter: Texture.Linear
                    wrapMode
                    {
                        x: WrapMode.ClampToEdge
                        y: WrapMode.ClampToEdge
                    }
                }
        },
        RenderTargetOutput
        {
            objectName: "depth"
            attachmentPoint: RenderTargetOutput.Depth
            texture:
                Texture2D
                {
                    id: depthAttachment
                    width: 1024
                    height: 1024
                    format: Texture.D32F
                    generateMipMaps: false
                    magnificationFilter: Texture.Linear
                    minificationFilter: Texture.Linear
                    wrapMode
                    {
                        x: WrapMode.ClampToEdge
                        y: WrapMode.ClampToEdge
                    }
                }
        }
    ] // outputs
}
