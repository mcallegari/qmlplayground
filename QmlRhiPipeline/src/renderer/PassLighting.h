#pragma once

#include "core/RenderGraph.h"
#include <QtCore/QString>
#include <QtCore/QSize>

class QRhiBuffer;
class QRhiGraphicsPipeline;
class QRhiRenderTarget;
class QRhiRenderPassDescriptor;
class QRhiResourceUpdateBatch;
class QRhiSampler;
class QRhiShaderResourceBindings;
class QRhiTexture;

class PassLighting final : public RenderPass
{
public:
    void prepare(FrameContext &ctx) override;
    void execute(FrameContext &ctx) override;

private:
    void ensurePipeline(FrameContext &ctx);
    void ensureSelectionBoxesPipeline(FrameContext &ctx, QRhiRenderTarget *rt);
    void updateGoboTextures(FrameContext &ctx, QRhiResourceUpdateBatch *u);

    QRhiGraphicsPipeline *m_pipeline = nullptr;
    QRhiShaderResourceBindings *m_srb = nullptr;
    QRhiSampler *m_sampler = nullptr;
    QRhiSampler *m_shadowSampler = nullptr;
    QRhiSampler *m_spotShadowSampler = nullptr;
    QRhiSampler *m_goboSampler = nullptr;
    QRhiBuffer *m_lightsUbo = nullptr;
    QRhiBuffer *m_cameraUbo = nullptr;
    QRhiBuffer *m_shadowUbo = nullptr;
    QRhiBuffer *m_flipUbo = nullptr;
    QRhiRenderPassDescriptor *m_rpDesc = nullptr;
    QRhiTexture *m_shadowMapRefs[3] = { nullptr, nullptr, nullptr };
    QRhiTexture *m_gbufColor0 = nullptr;
    QRhiTexture *m_gbufColor1 = nullptr;
    QRhiTexture *m_gbufColor2 = nullptr;
    QRhiTexture *m_gbufDepth = nullptr;
    bool m_gbufWorldPosFloat = false;
    QRhiTexture *m_spotShadowMaps[kMaxSpotShadows] = { nullptr };
    QRhiTexture *m_spotGoboMap = nullptr;
    QString m_spotGoboPaths[kMaxLights];
    QSize m_spotGoboSize = QSize(256, 256);
    bool m_reverseZ = false;

    QRhiGraphicsPipeline *m_selectionPipeline = nullptr;
    QRhiShaderResourceBindings *m_selectionSrb = nullptr;
    QRhiBuffer *m_selectionUbo = nullptr;
    QRhiBuffer *m_selectionDepthUbo = nullptr;
    QRhiBuffer *m_selectionVbuf = nullptr;
    QRhiRenderPassDescriptor *m_selectionRpDesc = nullptr;
    int m_selectionMaxVertices = 0;

};
