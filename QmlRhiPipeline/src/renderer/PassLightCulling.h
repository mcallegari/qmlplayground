#pragma once

#include "core/RenderGraph.h"
#include <QtCore/QSize>
#include <rhi/qrhi.h>

class PassLightCulling final : public RenderPass
{
public:
    void prepare(FrameContext &ctx) override;
    void execute(FrameContext &ctx) override;

private:
    void ensurePipeline(FrameContext &ctx);
    void ensureBuffers(FrameContext &ctx, const QSize &size);

    QRhiComputePipeline *m_pipeline = nullptr;
    QRhiShaderResourceBindings *m_srb = nullptr;
    QRhiBuffer *m_lightUbo = nullptr;
    QRhiBuffer *m_cullUbo = nullptr;
    QRhiTexture *m_lightIndexTexture = nullptr;
    QSize m_lastSize;
    int m_clusterSize = 120;
    int m_clusterCountZ = 24;
    int m_lastClusterCountX = 0;
    int m_lastClusterCountY = 0;
};
