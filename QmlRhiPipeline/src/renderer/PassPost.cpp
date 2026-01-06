#include "renderer/PassPost.h"

#include "core/RhiContext.h"
#include "core/ShaderManager.h"
#include "scene/Scene.h"
#include "core/RenderTargetCache.h"

#include <QtGui/QColor>
#include <QtGui/QVector4D>

void PassPost::prepare(FrameContext &ctx)
{
    if (!ctx.rhi || !ctx.shaders || !ctx.targets)
        return;
    QRhi *rhi = ctx.rhi->rhi();
    if (!rhi)
        return;

    QRhiRenderTarget *swapRt = ctx.rhi->swapchainRenderTarget();
    if (!swapRt)
        return;
    const QSize size = swapRt->pixelSize();
    if (size.isEmpty())
        return;

    if (m_lastSize != size)
    {
        delete m_bloomRpDesc;
        m_bloomRpDesc = nullptr;
        delete m_bloomRt;
        m_bloomRt = nullptr;
        delete m_bloomTex;
        m_bloomTex = nullptr;
        delete m_bloomBlurRpDesc;
        m_bloomBlurRpDesc = nullptr;
        delete m_bloomBlurRt;
        m_bloomBlurRt = nullptr;
        delete m_bloomBlurTex;
        m_bloomBlurTex = nullptr;
        delete m_bloomSrb;
        m_bloomSrb = nullptr;
        delete m_bloomUpsampleSrb;
        m_bloomUpsampleSrb = nullptr;
        delete m_combineSrb;
        m_combineSrb = nullptr;
        delete m_bloomPipeline;
        m_bloomPipeline = nullptr;
        delete m_bloomUpsamplePipeline;
        m_bloomUpsamplePipeline = nullptr;
        delete m_combinePipeline;
        m_combinePipeline = nullptr;
        m_swapRpDesc = nullptr;
        m_lastSize = size;
    }

    if (!m_sampler)
    {
        m_sampler = rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                    QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        if (!m_sampler->create())
            return;
    }

    if (!m_postUbo)
    {
        m_postUbo = rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QVector4D) * 2);
        if (!m_postUbo->create())
            return;
    }

    if (!m_bloomTex)
    {
        const QSize half = QSize(qMax(1, size.width() / 2), qMax(1, size.height() / 2));
        QRhiTexture::Format colorFormat = QRhiTexture::RGBA16F;
        if (!rhi->isTextureFormatSupported(colorFormat, QRhiTexture::RenderTarget))
            colorFormat = QRhiTexture::RGBA8;
        m_bloomTex = rhi->newTexture(colorFormat, half, 1, QRhiTexture::RenderTarget);
        if (!m_bloomTex->create())
            return;
        QRhiTextureRenderTargetDescription rtDesc;
        rtDesc.setColorAttachments({ QRhiColorAttachment(m_bloomTex) });
        m_bloomRt = rhi->newTextureRenderTarget(rtDesc);
        m_bloomRpDesc = m_bloomRt->newCompatibleRenderPassDescriptor();
        m_bloomRt->setRenderPassDescriptor(m_bloomRpDesc);
        if (!m_bloomRt->create())
            return;
    }
    if (!m_bloomBlurTex)
    {
        QRhiTexture::Format colorFormat = QRhiTexture::RGBA16F;
        if (!rhi->isTextureFormatSupported(colorFormat, QRhiTexture::RenderTarget))
            colorFormat = QRhiTexture::RGBA8;
        m_bloomBlurTex = rhi->newTexture(colorFormat, size, 1, QRhiTexture::RenderTarget);
        if (!m_bloomBlurTex->create())
            return;
        QRhiTextureRenderTargetDescription rtDesc;
        rtDesc.setColorAttachments({ QRhiColorAttachment(m_bloomBlurTex) });
        m_bloomBlurRt = rhi->newTextureRenderTarget(rtDesc);
        m_bloomBlurRpDesc = m_bloomBlurRt->newCompatibleRenderPassDescriptor();
        m_bloomBlurRt->setRenderPassDescriptor(m_bloomBlurRpDesc);
        if (!m_bloomBlurRt->create())
            return;
    }

    if (m_swapRpDesc != swapRt->renderPassDescriptor())
    {
        delete m_combinePipeline;
        m_combinePipeline = nullptr;
        m_swapRpDesc = swapRt->renderPassDescriptor();
    }

    if (!m_bloomSrb || !m_bloomUpsampleSrb || !m_combineSrb
            || !m_bloomPipeline || !m_bloomUpsamplePipeline || !m_combinePipeline)
    {
        const RenderTargetCache::GBufferTargets gbuf = ctx.targets->getOrCreateGBuffer(size, 1);
        const RenderTargetCache::LightingTargets lighting = ctx.targets->getOrCreateLightingTarget(size, 1);
        if (!gbuf.color3 || !lighting.color)
            return;

        delete m_bloomSrb;
        m_bloomSrb = nullptr;
        delete m_bloomUpsampleSrb;
        m_bloomUpsampleSrb = nullptr;
        delete m_combineSrb;
        m_combineSrb = nullptr;
        delete m_bloomPipeline;
        m_bloomPipeline = nullptr;
        delete m_bloomUpsamplePipeline;
        m_bloomUpsamplePipeline = nullptr;
        delete m_combinePipeline;
        m_combinePipeline = nullptr;

        m_bloomSrb = rhi->newShaderResourceBindings();
        m_bloomSrb->setBindings({
            QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, gbuf.color3, m_sampler),
            QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, m_postUbo)
        });
        if (!m_bloomSrb->create())
            return;

        m_bloomUpsampleSrb = rhi->newShaderResourceBindings();
        m_bloomUpsampleSrb->setBindings({
            QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, m_bloomTex, m_sampler),
            QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, m_postUbo)
        });
        if (!m_bloomUpsampleSrb->create())
            return;

        m_combineSrb = rhi->newShaderResourceBindings();
        m_combineSrb->setBindings({
            QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, lighting.color, m_sampler),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, m_bloomBlurTex, m_sampler),
            QRhiShaderResourceBinding::uniformBuffer(2, QRhiShaderResourceBinding::FragmentStage, m_postUbo)
        });
        if (!m_combineSrb->create())
            return;

        const QRhiShaderStage vs = ctx.shaders->loadStage(QRhiShaderStage::Vertex, QStringLiteral(":/shaders/lighting.vert.qsb"));
        const QRhiShaderStage fsBloom = ctx.shaders->loadStage(QRhiShaderStage::Fragment, QStringLiteral(":/shaders/post_bloom_downsample.frag.qsb"));
        const QRhiShaderStage fsUpsample = ctx.shaders->loadStage(QRhiShaderStage::Fragment, QStringLiteral(":/shaders/post_bloom_upsample.frag.qsb"));
        const QRhiShaderStage fsCombine = ctx.shaders->loadStage(QRhiShaderStage::Fragment, QStringLiteral(":/shaders/post_combine.frag.qsb"));
        if (!vs.shader().isValid() || !fsBloom.shader().isValid() || !fsUpsample.shader().isValid()
                || !fsCombine.shader().isValid())
            return;

        QRhiGraphicsPipeline *bloomPipeline = rhi->newGraphicsPipeline();
        bloomPipeline->setShaderStages({ vs, fsBloom });
        bloomPipeline->setCullMode(QRhiGraphicsPipeline::None);
        bloomPipeline->setDepthTest(false);
        bloomPipeline->setDepthWrite(false);
        bloomPipeline->setShaderResourceBindings(m_bloomSrb);
        bloomPipeline->setRenderPassDescriptor(m_bloomRpDesc);
        if (!bloomPipeline->create())
            return;
        m_bloomPipeline = bloomPipeline;

        QRhiGraphicsPipeline *upsamplePipeline = rhi->newGraphicsPipeline();
        upsamplePipeline->setShaderStages({ vs, fsUpsample });
        upsamplePipeline->setCullMode(QRhiGraphicsPipeline::None);
        upsamplePipeline->setDepthTest(false);
        upsamplePipeline->setDepthWrite(false);
        upsamplePipeline->setShaderResourceBindings(m_bloomUpsampleSrb);
        upsamplePipeline->setRenderPassDescriptor(m_bloomBlurRpDesc);
        if (!upsamplePipeline->create())
            return;
        m_bloomUpsamplePipeline = upsamplePipeline;

        QRhiGraphicsPipeline *combinePipeline = rhi->newGraphicsPipeline();
        combinePipeline->setShaderStages({ vs, fsCombine });
        combinePipeline->setCullMode(QRhiGraphicsPipeline::None);
        combinePipeline->setDepthTest(false);
        combinePipeline->setDepthWrite(false);
        combinePipeline->setShaderResourceBindings(m_combineSrb);
        combinePipeline->setRenderPassDescriptor(m_swapRpDesc);
        if (!combinePipeline->create())
            return;
        m_combinePipeline = combinePipeline;
    }
}

void PassPost::execute(FrameContext &ctx)
{
    if (!ctx.rhi || !ctx.targets || !m_bloomRt || !m_bloomBlurRt
            || !m_bloomPipeline || !m_bloomUpsamplePipeline || !m_combinePipeline)
        return;
    QRhiCommandBuffer *cb = ctx.rhi->commandBuffer();
    QRhiRenderTarget *swapRt = ctx.rhi->swapchainRenderTarget();
    if (!cb || !swapRt)
        return;

    const QSize size = swapRt->pixelSize();
    struct PostParams
    {
        QVector4D pixelSize;
        QVector4D intensity;
    } params;

    const float bloomIntensity = ctx.scene ? ctx.scene->bloomIntensity() : 0.0f;
    const float bloomRadius = ctx.scene ? ctx.scene->bloomRadius() : 0.0f;
    const QColor clear(0, 0, 0);
    const QSize half = QSize(qMax(1, size.width() / 2), qMax(1, size.height() / 2));
    QRhiResourceUpdateBatch *u = ctx.rhi->rhi()->nextResourceUpdateBatch();
    params.pixelSize = QVector4D(1.0f / float(size.width()), 1.0f / float(size.height()), 0.0f, 0.0f);
    params.intensity = QVector4D(bloomIntensity, bloomRadius, 0.0f, 0.0f);
    u->updateDynamicBuffer(m_postUbo, 0, sizeof(PostParams), &params);
    cb->resourceUpdate(u);
    cb->beginPass(m_bloomRt, clear, {});
    cb->setGraphicsPipeline(m_bloomPipeline);
    cb->setViewport(QRhiViewport(0, 0, half.width(), half.height()));
    cb->setShaderResources(m_bloomSrb);
    cb->draw(3);
    cb->endPass();

    u = ctx.rhi->rhi()->nextResourceUpdateBatch();
    params.pixelSize = QVector4D(1.0f / float(half.width()), 1.0f / float(half.height()), 0.0f, 0.0f);
    params.intensity = QVector4D(bloomIntensity, bloomRadius, 0.0f, 0.0f);
    u->updateDynamicBuffer(m_postUbo, 0, sizeof(PostParams), &params);
    cb->resourceUpdate(u);
    cb->beginPass(m_bloomBlurRt, clear, {});
    cb->setGraphicsPipeline(m_bloomUpsamplePipeline);
    cb->setViewport(QRhiViewport(0, 0, size.width(), size.height()));
    cb->setShaderResources(m_bloomUpsampleSrb);
    cb->draw(3);
    cb->endPass();

    cb->beginPass(swapRt, clear, {});
    cb->setGraphicsPipeline(m_combinePipeline);
    cb->setViewport(QRhiViewport(0, 0, size.width(), size.height()));
    cb->setShaderResources(m_combineSrb);
    cb->draw(3);
    cb->endPass();
}
