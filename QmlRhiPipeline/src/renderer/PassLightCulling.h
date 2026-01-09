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
    QRhiBuffer *m_lightIndexBuffer = nullptr;
    QSize m_lastSize;
    int m_tileSize = 16;
    int m_lastTileCountX = 0;
    int m_lastTileCountY = 0;
};
