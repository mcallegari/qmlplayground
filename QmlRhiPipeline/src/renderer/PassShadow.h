#pragma once

#include "core/RenderGraph.h"
#include "scene/Mesh.h"
#include <QtCore/QVector>
#include <QtGui/QVector3D>

class Camera;
struct Light;
class QRhiRenderBuffer;

class PassShadow final : public RenderPass
{
public:
    void prepare(FrameContext &ctx) override;
    void execute(FrameContext &ctx) override;

private:
    struct Cascade {
        QRhiTexture *color = nullptr;
        QRhiRenderBuffer *depthStencil = nullptr;
        QRhiTextureRenderTarget *rt = nullptr;
        QRhiRenderPassDescriptor *rpDesc = nullptr;
        QMatrix4x4 lightViewProj;
    };

    void ensureResources(FrameContext &ctx);
    void renderCascade(FrameContext &ctx, Cascade &cascade, const QMatrix4x4 &lightViewProj);
    void renderSpot(FrameContext &ctx, const QMatrix4x4 &lightViewProj,
                    const QVector3D &lightPos, float nearPlane, float farPlane);
    QMatrix4x4 computeLightViewProj(const Camera &camera, const QVector3D &lightDir, float nearPlane, float farPlane);
    QMatrix4x4 computeSpotViewProj(const Light &light, float nearPlane, float farPlane);
    QRhiShaderResourceBindings *shadowSrbForMesh(FrameContext &ctx, Mesh &mesh);

    Cascade m_cascades[3];
    QRhiTexture *m_spotShadowMap = nullptr;
    QRhiRenderBuffer *m_spotDepthStencil = nullptr;
    QRhiTextureRenderTarget *m_spotRt = nullptr;
    QRhiRenderPassDescriptor *m_spotRpDesc = nullptr;
    QRhiGraphicsPipeline *m_pipeline = nullptr;
    QRhiGraphicsPipeline *m_spotPipeline = nullptr;
    QRhiShaderResourceBindings *m_srb = nullptr;
    QRhiBuffer *m_shadowUbo = nullptr;
    QRhiBuffer *m_modelUbo = nullptr;
    int m_shadowSize = 2048;
    int m_spotShadowSize = 256;
    int m_maxSpotShadows = kMaxLights;
    bool m_reverseZ = false;
    int m_spotShaderVersion = 0;
};
