#include "renderer/PassLightCulling.h"

#include <QtCore/QVector>
#include <QtCore/QtGlobal>
#include <QtGui/QMatrix4x4>
#include <cstring>

#include "core/RhiContext.h"
#include "core/ShaderManager.h"
#include "scene/Scene.h"

namespace {

struct LightsData
{
    QVector4D lightCount;
    QVector4D lightParams;
    QVector4D lightFlags;
    QVector4D lightBeam[kMaxLights];
    QVector4D posRange[kMaxLights];
    QVector4D colorIntensity[kMaxLights];
    QVector4D dirInner[kMaxLights];
    QVector4D other[kMaxLights];
};

struct CullParams
{
    float view[16];
    float proj[16];
    QVector4D screen; // x=width y=height z=invW w=invH
    QVector4D tile;   // x=tileCountX y=tileCountY z=tileSize w=enabled
};

} // namespace

void PassLightCulling::prepare(FrameContext &ctx)
{
    if (!ctx.rhi || !ctx.shaders || !ctx.lightCulling)
        return;
    QRhi *rhi = ctx.rhi->rhi();
    if (!rhi)
        return;
    if (rhi->backend() == QRhi::Metal)
    {
        // Metal path: avoid storage-buffer based light culling until validated.
        ctx.lightCulling->enabled = false;
        ctx.lightCulling->tileLightIndexBuffer = nullptr;
        return;
    }

    const bool supported = rhi->isFeatureSupported(QRhi::Compute);
    ctx.lightCulling->enabled = supported;
    if (!supported)
        ctx.lightCulling->tileLightIndexBuffer = nullptr;
    ctx.lightCulling->tileSize = m_tileSize;

    if (!supported)
        return;

    QRhiRenderTarget *swapRt = ctx.rhi->swapchainRenderTarget();
    if (!swapRt)
        return;
    const QSize size = swapRt->pixelSize();
    if (size.isEmpty())
    {
        ctx.lightCulling->tileLightIndexBuffer = nullptr;
        return;
    }

    ensureBuffers(ctx, size);
    ensurePipeline(ctx);
    if (m_lightIndexBuffer)
    {
        ctx.lightCulling->tileLightIndexBuffer = m_lightIndexBuffer;
        ctx.lightCulling->tileCountX = m_lastTileCountX;
        ctx.lightCulling->tileCountY = m_lastTileCountY;
    }
    if (!m_pipeline || !m_srb)
    {
        ctx.lightCulling->enabled = false;
        ctx.lightCulling->tileLightIndexBuffer = nullptr;
    }
}

void PassLightCulling::execute(FrameContext &ctx)
{
    if (!ctx.rhi || !ctx.scene || !ctx.lightCulling || !ctx.lightCulling->enabled)
        return;
    if (!m_pipeline || !m_srb || !m_lightUbo || !m_cullUbo || !m_lightIndexBuffer)
        return;

    QRhiCommandBuffer *cb = ctx.rhi->commandBuffer();
    if (!cb)
        return;

    const int maxLights = qMin(kMaxLights, ctx.scene->lights().size());
    LightsData lightData = {};
    const QVector3D ambient = ctx.scene->ambientLight() * ctx.scene->ambientIntensity();
    lightData.lightCount = QVector4D(float(maxLights), ambient.x(), ambient.y(), ambient.z());
    lightData.lightParams = QVector4D(ctx.scene->smokeAmount(),
                                      float(static_cast<int>(ctx.scene->beamModel())),
                                      ctx.scene->bloomIntensity(),
                                      ctx.scene->bloomRadius());
    lightData.lightFlags = QVector4D(ctx.scene->volumetricEnabled() ? 1.0f : 0.0f,
                                     ctx.scene->smokeNoiseEnabled() ? 1.0f : 0.0f,
                                     ctx.scene->shadowsEnabled() ? 1.0f : 0.0f,
                                     0.0f);
    for (int i = 0; i < maxLights; ++i)
    {
        const Light &l = ctx.scene->lights()[i];
        const QVector3D dir = l.direction.normalized();
        lightData.posRange[i] = QVector4D(l.position, l.range);
        lightData.colorIntensity[i] = QVector4D(l.color, l.intensity);
        lightData.dirInner[i] = QVector4D(dir, qCos(l.innerCone));
        lightData.lightBeam[i] = QVector4D(l.beamRadius, float(l.beamShape), 0.0f, 0.0f);
        float extraZ = 0.0f;
        float extraW = 0.0f;
        if (l.type == Light::Type::Area)
        {
            extraZ = l.areaSize.x();
            extraW = l.areaSize.y();
        }
        else if (l.type == Light::Type::Spot)
        {
            extraZ = l.goboPath.isEmpty() ? -1.0f : float(i);
            extraW = float(l.qualitySteps);
        }
        lightData.other[i] = QVector4D(qCos(l.outerCone),
                                       float(l.type),
                                       extraZ,
                                       extraW);
    }

    const QSize size = m_lastSize;
    const int tileCountX = m_lastTileCountX;
    const int tileCountY = m_lastTileCountY;

    CullParams params = {};
    const QMatrix4x4 view = ctx.scene->camera().viewMatrix();
    const QMatrix4x4 proj = ctx.rhi->rhi()->clipSpaceCorrMatrix()
            * ctx.scene->camera().projectionMatrix();
    std::memcpy(params.view, view.constData(), sizeof(params.view));
    std::memcpy(params.proj, proj.constData(), sizeof(params.proj));
    params.screen = QVector4D(float(size.width()), float(size.height()),
                              1.0f / float(size.width()), 1.0f / float(size.height()));
    params.tile = QVector4D(float(tileCountX), float(tileCountY), float(m_tileSize), 1.0f);

    QRhiResourceUpdateBatch *u = ctx.rhi->rhi()->nextResourceUpdateBatch();
    u->updateDynamicBuffer(m_lightUbo, 0, sizeof(LightsData), &lightData);
    u->updateDynamicBuffer(m_cullUbo, 0, sizeof(CullParams), &params);
    cb->resourceUpdate(u);

    cb->beginComputePass();
    cb->setComputePipeline(m_pipeline);
    cb->setShaderResources(m_srb);
    cb->dispatch(tileCountX, tileCountY, 1);
    cb->endComputePass();

    ctx.lightCulling->tileLightIndexBuffer = m_lightIndexBuffer;
    ctx.lightCulling->tileCountX = tileCountX;
    ctx.lightCulling->tileCountY = tileCountY;
}

void PassLightCulling::ensurePipeline(FrameContext &ctx)
{
    if (!ctx.rhi || !ctx.shaders || !m_lightIndexBuffer)
        return;
    if (m_pipeline && m_srb)
        return;

    delete m_pipeline;
    m_pipeline = nullptr;
    delete m_srb;
    m_srb = nullptr;

    const QRhiShaderStage cs = ctx.shaders->loadStage(QRhiShaderStage::Compute,
                                                      QStringLiteral(":/shaders/light_cull.comp.qsb"));
    if (!cs.shader().isValid())
        return;

    if (!m_lightUbo)
    {
        m_lightUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(LightsData));
        if (!m_lightUbo->create())
        {
            delete m_lightUbo;
            m_lightUbo = nullptr;
            return;
        }
    }
    if (!m_cullUbo)
    {
        m_cullUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(CullParams));
        if (!m_cullUbo->create())
        {
            delete m_cullUbo;
            m_cullUbo = nullptr;
            return;
        }
    }

    m_srb = ctx.rhi->rhi()->newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::ComputeStage, m_lightUbo),
        QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::ComputeStage, m_cullUbo),
        QRhiShaderResourceBinding::bufferLoadStore(2, QRhiShaderResourceBinding::ComputeStage, m_lightIndexBuffer)
    });
    if (!m_srb->create())
    {
        delete m_srb;
        m_srb = nullptr;
        return;
    }

    m_pipeline = ctx.rhi->rhi()->newComputePipeline();
    m_pipeline->setShaderStage(cs);
    m_pipeline->setShaderResourceBindings(m_srb);
    if (!m_pipeline->create())
    {
        delete m_pipeline;
        m_pipeline = nullptr;
        return;
    }
}

void PassLightCulling::ensureBuffers(FrameContext &ctx, const QSize &size)
{
    const int tileCountX = (size.width() + m_tileSize - 1) / m_tileSize;
    const int tileCountY = (size.height() + m_tileSize - 1) / m_tileSize;
    if (size == m_lastSize && tileCountX == m_lastTileCountX && tileCountY == m_lastTileCountY && m_lightIndexBuffer)
        return;

    m_lastSize = size;
    m_lastTileCountX = tileCountX;
    m_lastTileCountY = tileCountY;

    delete m_lightIndexBuffer;
    m_lightIndexBuffer = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer,
                                                   quint32(tileCountX * tileCountY * (kMaxLights + 1) * sizeof(quint32)));
    if (!m_lightIndexBuffer->create())
    {
        delete m_lightIndexBuffer;
        m_lightIndexBuffer = nullptr;
        ctx.lightCulling->enabled = false;
        ctx.lightCulling->tileLightIndexBuffer = nullptr;
        return;
    }

    if (m_srb)
    {
        delete m_srb;
        m_srb = nullptr;
    }
    if (m_pipeline)
    {
        delete m_pipeline;
        m_pipeline = nullptr;
    }
}
