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
    struct Cascade
    {
        QRhiTexture *color = nullptr;
        QRhiRenderBuffer *depthStencil = nullptr;
        QRhiTextureRenderTarget *rt = nullptr;
        QRhiRenderPassDescriptor *rpDesc = nullptr;
        QMatrix4x4 lightViewProj;
    };

    void ensureResources(FrameContext &ctx);
    void renderCascade(FrameContext &ctx, Cascade &cascade, const QMatrix4x4 &lightViewProj);
    void renderSpot(FrameContext &ctx,
                    QRhiTextureRenderTarget *rt,
                    const QMatrix4x4 &lightViewProj,
                    const QVector3D &lightPos, float nearPlane, float farPlane,
                    int slot);
    QMatrix4x4 computeLightViewProj(const Camera &camera, const QVector3D &lightDir, float nearPlane, float farPlane);
    QMatrix4x4 computeSpotViewProj(const Light &light, float nearPlane, float farPlane);
    QRhiShaderResourceBindings *shadowSrbForMesh(FrameContext &ctx, Mesh &mesh);
    QRhiShaderResourceBindings *spotShadowSrbForMesh(FrameContext &ctx, Mesh &mesh, int slot);

    Cascade m_cascades[3];
    QRhiTexture *m_spotShadowMapArray = nullptr;
    QVector<QRhiRenderBuffer *> m_spotDepthStencils;
    QVector<QRhiTextureRenderTarget *> m_spotRts;
    QRhiRenderPassDescriptor *m_spotRpDesc = nullptr;
    QRhiGraphicsPipeline *m_pipeline = nullptr;
    QRhiGraphicsPipeline *m_spotPipeline = nullptr;
    QRhiShaderResourceBindings *m_srb = nullptr;
    QRhiBuffer *m_shadowUbo = nullptr;
    QVector<QRhiBuffer *> m_spotShadowUbos;
    QRhiBuffer *m_modelUbo = nullptr;
    int m_shadowSize = 2048;
    int m_spotShadowSize = 512;
    int m_maxSpotShadows = kMaxSpotShadows;
    int m_spotShadowSlots = kMaxSpotShadows;
    bool m_reverseZ = false;
    int m_spotShaderVersion = 0;
};
