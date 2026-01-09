#pragma once

#include "core/RenderGraph.h"
#include <rhi/qrhi.h>

struct Mesh;

class PassPost final : public RenderPass
{
public:
    void prepare(FrameContext &ctx) override;
    void execute(FrameContext &ctx) override;

private:
    void ensureGizmoPipeline(FrameContext &ctx);
    void ensureGizmoMeshBuffers(FrameContext &ctx, Mesh &mesh, QRhiResourceUpdateBatch *u);

    QRhiTexture *m_bloomTex = nullptr;
    QRhiTextureRenderTarget *m_bloomRt = nullptr;
    QRhiRenderPassDescriptor *m_bloomRpDesc = nullptr;
    QRhiTexture *m_bloomBlurTex = nullptr;
    QRhiTextureRenderTarget *m_bloomBlurRt = nullptr;
    QRhiRenderPassDescriptor *m_bloomBlurRpDesc = nullptr;
    QRhiBuffer *m_postUbo = nullptr;
    QRhiShaderResourceBindings *m_bloomSrb = nullptr;
    QRhiShaderResourceBindings *m_bloomUpsampleSrb = nullptr;
    QRhiShaderResourceBindings *m_combineSrb = nullptr;
    QRhiGraphicsPipeline *m_bloomPipeline = nullptr;
    QRhiGraphicsPipeline *m_bloomUpsamplePipeline = nullptr;
    QRhiGraphicsPipeline *m_combinePipeline = nullptr;
    QRhiGraphicsPipeline *m_gizmoPipeline = nullptr;
    QRhiShaderResourceBindings *m_gizmoLayoutSrb = nullptr;
    QRhiBuffer *m_gizmoModelUbo = nullptr;
    QRhiBuffer *m_gizmoMaterialUbo = nullptr;
    QRhiSampler *m_sampler = nullptr;
    QRhiRenderPassDescriptor *m_swapRpDesc = nullptr;
    QRhiBuffer *m_gizmoCameraUbo = nullptr;
    QSize m_lastSize;
};
