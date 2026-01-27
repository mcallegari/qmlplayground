#pragma once

#include "core/RenderGraph.h"
#include "core/RenderTargetCache.h"

class Mesh;

class PassGBuffer final : public RenderPass
{
public:
    void prepare(FrameContext &ctx) override;
    void execute(FrameContext &ctx) override;

private:
    void ensurePipeline(FrameContext &ctx);
    void ensureMeshBuffers(FrameContext &ctx, Mesh &mesh, QRhiResourceUpdateBatch *u);
    QRhiGraphicsPipeline *createPipeline(FrameContext &ctx, QRhiGraphicsPipeline::CullMode cullMode);

    RenderTargetCache::GBufferTargets m_gbuffer;
    QRhiGraphicsPipeline *m_pipeline = nullptr;
    QRhiGraphicsPipeline *m_pipelineTwoSided = nullptr;
    QRhiShaderResourceBindings *m_srb = nullptr;
    QRhiBuffer *m_cameraUbo = nullptr;
    QRhiBuffer *m_modelUbo = nullptr;
    QRhiBuffer *m_materialUbo = nullptr;
    QRhiTexture *m_defaultBaseColor = nullptr;
    QRhiTexture *m_defaultNormal = nullptr;
    QRhiTexture *m_defaultMetallicRoughness = nullptr;
    QRhiTexture *m_defaultOcclusion = nullptr;
    QRhiTexture *m_defaultEmissive = nullptr;
    QRhiSampler *m_linearSampler = nullptr;
    QRhiSampler *m_videoSampler = nullptr;
    bool m_defaultBaseColorUploaded = false;
    bool m_defaultNormalUploaded = false;
    bool m_defaultMetallicRoughnessUploaded = false;
    bool m_defaultOcclusionUploaded = false;
    bool m_defaultEmissiveUploaded = false;
    QRhiRenderPassDescriptor *m_rpDesc = nullptr;
};
