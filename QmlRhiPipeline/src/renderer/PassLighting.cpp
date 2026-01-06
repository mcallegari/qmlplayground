#include "renderer/PassLighting.h"

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtGui/QImage>
#include <rhi/qrhi.h>
#include <vector>
#include <cstring>
#include <QtCore/QVector>

#include "core/RhiContext.h"
#include "core/RenderTargetCache.h"
#include "core/ShaderManager.h"
#include "scene/Scene.h"

static QImage loadGoboImage(const QString &path, const QSize &size)
{
    QImage image;
    if (!path.isEmpty()) {
        QFileInfo info(path);
        const QString resolved = info.isRelative() ? QDir::current().filePath(path) : path;
        image = QImage(resolved);
    }
    if (image.isNull()) {
        image = QImage(size, QImage::Format_RGBA8888);
        image.fill(Qt::white);
    } else {
        image = image.convertToFormat(QImage::Format_RGBA8888);
        if (image.size() != size)
            image = image.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

void PassLighting::prepare(FrameContext &ctx)
{
    if (qEnvironmentVariableIsSet("RHIPIPELINE_SKIP_LIGHTING")) {
        qWarning() << "PassLighting: skipping lighting (RHIPIPELINE_SKIP_LIGHTING)";
        return;
    }
    ensurePipeline(ctx);
}

void PassLighting::updateGoboTextures(FrameContext &ctx, QRhiResourceUpdateBatch *u)
{
    if (!ctx.scene || !m_spotGoboMap || !u)
        return;
    QVector<QRhiTextureUploadEntry> entries;
    entries.reserve(kMaxLights);
    const auto &lights = ctx.scene->lights();
    for (int i = 0; i < kMaxLights; ++i) {
        const QString path = i < lights.size() ? lights[i].goboPath : QString();
        if (!m_spotGoboPaths[i].isNull() && path == m_spotGoboPaths[i])
            continue;
        m_spotGoboPaths[i] = path;
        const QImage image = loadGoboImage(path, m_spotGoboSize);
        entries.push_back(QRhiTextureUploadEntry(i, 0, QRhiTextureSubresourceUploadDescription(image)));
    }
    if (entries.isEmpty())
        return;
    QRhiTextureUploadDescription desc;
    desc.setEntries(entries.begin(), entries.end());
    u->uploadTexture(m_spotGoboMap, desc);
}

void PassLighting::execute(FrameContext &ctx)
{
    if (!ctx.rhi)
        return;
    if (!ctx.scene || !m_pipeline || !m_srb)
        return;
    QRhiCommandBuffer *cb = ctx.rhi->commandBuffer();
    QRhiRenderTarget *rt = ctx.rhi->swapchainRenderTarget();
    if (!cb || !rt)
        return;

    struct LightsData {
        QVector4D lightCount;
        QVector4D posRange[kMaxLights];
        QVector4D colorIntensity[kMaxLights];
        QVector4D dirInner[kMaxLights];
        QVector4D other[kMaxLights];
    } lightData;
    memset(&lightData, 0, sizeof(LightsData));

    const int maxLights = qMin(kMaxLights, ctx.scene->lights().size());
    const QVector3D ambient = ctx.scene->ambientLight() * ctx.scene->ambientIntensity();
    lightData.lightCount = QVector4D(float(maxLights), ambient.x(), ambient.y(), ambient.z());
    for (int i = 0; i < maxLights; ++i) {
        const Light &l = ctx.scene->lights()[i];
        const QVector3D dir = l.direction.normalized();
        lightData.posRange[i] = QVector4D(l.position, l.range);
        lightData.colorIntensity[i] = QVector4D(l.color, l.intensity);
        lightData.dirInner[i] = QVector4D(dir, qCos(l.innerCone));
        float extraZ = 0.0f;
        float extraW = 0.0f;
        if (l.type == Light::Type::Area) {
            extraZ = l.areaSize.x();
            extraW = l.areaSize.y();
        } else if (l.type == Light::Type::Spot) {
            extraZ = l.goboPath.isEmpty() ? -1.0f : float(i);
            extraW = float(l.qualitySteps);
        }
        lightData.other[i] = QVector4D(qCos(l.outerCone),
                                       float(l.type),
                                       extraZ,
                                       extraW);
    }

    struct CameraData {
        float view[16];
        float invViewProj[16];
        QVector4D cameraPos;
    } camData;
    const QMatrix4x4 view = ctx.scene->camera().viewMatrix();
    const QMatrix4x4 viewProj = ctx.rhi->rhi()->clipSpaceCorrMatrix()
            * ctx.scene->camera().projectionMatrix()
            * ctx.scene->camera().viewMatrix();
    const QMatrix4x4 invViewProj = viewProj.inverted();
    std::memcpy(camData.view, view.constData(), sizeof(camData.view));
    std::memcpy(camData.invViewProj, invViewProj.constData(), sizeof(camData.invViewProj));
    camData.cameraPos = QVector4D(ctx.scene->camera().position(), 1.0f);

    struct ShadowDataGpu {
        float lightViewProj[3][16];
        float splits[4];
        float dirLightDir[4];
        float dirLightColorIntensity[4];
        float spotLightViewProj[kMaxLights][16];
        float spotShadowParams[kMaxLights][4];
        float shadowDepthParams[4];
    } shadowData;
    memset(&shadowData, 0, sizeof(ShadowDataGpu));
    if (ctx.shadows && ctx.shadows->cascadeCount > 0) {
        for (int i = 0; i < ctx.shadows->cascadeCount; ++i)
            std::memcpy(shadowData.lightViewProj[i], ctx.shadows->lightViewProj[i].constData(), sizeof(shadowData.lightViewProj[i]));
        shadowData.splits[0] = ctx.shadows->splits.x();
        shadowData.splits[1] = ctx.shadows->splits.y();
        shadowData.splits[2] = ctx.shadows->splits.z();
        shadowData.splits[3] = ctx.shadows->splits.w();
        shadowData.dirLightDir[0] = ctx.shadows->dirLightDir.x();
        shadowData.dirLightDir[1] = ctx.shadows->dirLightDir.y();
        shadowData.dirLightDir[2] = ctx.shadows->dirLightDir.z();
        shadowData.dirLightDir[3] = ctx.shadows->dirLightDir.w();
        shadowData.dirLightColorIntensity[0] = ctx.shadows->dirLightColorIntensity.x();
        shadowData.dirLightColorIntensity[1] = ctx.shadows->dirLightColorIntensity.y();
        shadowData.dirLightColorIntensity[2] = ctx.shadows->dirLightColorIntensity.z();
        shadowData.dirLightColorIntensity[3] = ctx.shadows->dirLightColorIntensity.w();
    }
    if (ctx.shadows) {
        for (int i = 0; i < kMaxLights; ++i) {
            std::memcpy(shadowData.spotLightViewProj[i], ctx.shadows->spotLightViewProj[i].constData(), sizeof(shadowData.spotLightViewProj[i]));
            shadowData.spotShadowParams[i][0] = ctx.shadows->spotShadowParams[i].x();
            shadowData.spotShadowParams[i][1] = ctx.shadows->spotShadowParams[i].y();
            shadowData.spotShadowParams[i][2] = ctx.shadows->spotShadowParams[i].z();
            shadowData.spotShadowParams[i][3] = ctx.shadows->spotShadowParams[i].w();
        }
        shadowData.shadowDepthParams[0] = ctx.shadows->shadowDepthParams.x();
        shadowData.shadowDepthParams[1] = ctx.shadows->shadowDepthParams.y();
        shadowData.shadowDepthParams[2] = ctx.shadows->shadowDepthParams.z();
        shadowData.shadowDepthParams[3] = m_gbufWorldPosFloat ? 1.0f : 0.0f;
    }

    QRhiResourceUpdateBatch *u = ctx.rhi->rhi()->nextResourceUpdateBatch();
    u->updateDynamicBuffer(m_lightsUbo, 0, sizeof(LightsData), &lightData);
    u->updateDynamicBuffer(m_cameraUbo, 0, sizeof(CameraData), &camData);
    if (m_shadowUbo)
    u->updateDynamicBuffer(m_shadowUbo, 0, sizeof(ShadowDataGpu), &shadowData);
    const bool flipSampleY = !ctx.rhi->rhi()->isYUpInFramebuffer();
    const bool flipNdcY = !ctx.rhi->rhi()->isYUpInNDC();
    const QVector4D flipData(flipSampleY ? 1.0f : 0.0f,
                             flipNdcY ? 1.0f : 0.0f,
                             0.0f,
                             0.0f);
    u->updateDynamicBuffer(m_flipUbo, 0, sizeof(flipData), &flipData);
    updateGoboTextures(ctx, u);

    cb->resourceUpdate(u);

    const QColor clear(0, 0, 0);
    const QRhiDepthStencilClearValue dsClear(1.0f, 0);
    cb->beginPass(rt, clear, dsClear);
    cb->setGraphicsPipeline(m_pipeline);
    cb->setViewport(QRhiViewport(0, 0, rt->pixelSize().width(), rt->pixelSize().height()));
    cb->setShaderResources(m_srb);
    cb->draw(3);

    bool anySelected = false;
    for (const Mesh &mesh : ctx.scene->meshes()) {
        if (mesh.selected) {
            anySelected = true;
            break;
        }
    }

    if (anySelected)
        ensureSelectionBoxesPipeline(ctx, rt);

    if (anySelected && m_selectionPipeline && m_selectionSrb && m_selectionVbuf) {
        struct SelectionVertex {
            float x;
            float y;
            float z;
        };

        const auto &meshes = ctx.scene->meshes();
        const int maxVertices = m_selectionMaxVertices > 0 ? m_selectionMaxVertices : 0;
        std::vector<SelectionVertex> vertices;
        if (maxVertices > 0)
            vertices.reserve(qMin(int(meshes.size()) * 24, maxVertices));

        const int edges[12][2] = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0},
            {4, 5}, {5, 6}, {6, 7}, {7, 4},
            {0, 4}, {1, 5}, {2, 6}, {3, 7}
        };

        for (const Mesh &mesh : meshes) {
            if (!mesh.selected)
                continue;
            if (mesh.vertices.isEmpty())
                continue;

            QVector3D minV = mesh.boundsMin;
            QVector3D maxV = mesh.boundsMax;
            if (!mesh.boundsValid) {
                minV = QVector3D(mesh.vertices[0].px, mesh.vertices[0].py, mesh.vertices[0].pz);
                maxV = minV;
                for (const Vertex &v : mesh.vertices) {
                    minV.setX(qMin(minV.x(), v.px));
                    minV.setY(qMin(minV.y(), v.py));
                    minV.setZ(qMin(minV.z(), v.pz));
                    maxV.setX(qMax(maxV.x(), v.px));
                    maxV.setY(qMax(maxV.y(), v.py));
                    maxV.setZ(qMax(maxV.z(), v.pz));
                }
            }

            QVector3D corners[8] = {
                { minV.x(), minV.y(), minV.z() },
                { maxV.x(), minV.y(), minV.z() },
                { maxV.x(), maxV.y(), minV.z() },
                { minV.x(), maxV.y(), minV.z() },
                { minV.x(), minV.y(), maxV.z() },
                { maxV.x(), minV.y(), maxV.z() },
                { maxV.x(), maxV.y(), maxV.z() },
                { minV.x(), maxV.y(), maxV.z() }
            };

            const QMatrix4x4 model = mesh.modelMatrix;

            for (const auto &edge : edges) {
                if (maxVertices > 0 && int(vertices.size() + 2) > maxVertices)
                    break;
                const QVector3D a = (model * QVector4D(corners[edge[0]], 1.0f)).toVector3D();
                const QVector3D b = (model * QVector4D(corners[edge[1]], 1.0f)).toVector3D();
                vertices.push_back({ a.x(), a.y(), a.z() });
                vertices.push_back({ b.x(), b.y(), b.z() });
            }
        }

        if (!vertices.empty()) {
            QRhiResourceUpdateBatch *u = ctx.rhi->rhi()->nextResourceUpdateBatch();
            const QMatrix4x4 viewProj = ctx.rhi->rhi()->clipSpaceCorrMatrix()
                    * ctx.scene->camera().projectionMatrix()
                    * ctx.scene->camera().viewMatrix();
            const quint32 mat4Size = 16 * sizeof(float);
            const QVector4D depthParams(ctx.shadows ? ctx.shadows->shadowDepthParams.x() : 1.0f,
                                        ctx.shadows ? ctx.shadows->shadowDepthParams.y() : 0.0f,
                                        ctx.shadows ? ctx.shadows->shadowDepthParams.z() : 0.0f,
                                        (!ctx.rhi->rhi()->isYUpInFramebuffer()) ? 1.0f : 0.0f);
            u->updateDynamicBuffer(m_selectionUbo, 0, mat4Size, viewProj.constData());
            u->updateDynamicBuffer(m_selectionDepthUbo, 0, sizeof(depthParams), &depthParams);
            u->updateDynamicBuffer(m_selectionVbuf, 0, int(vertices.size() * sizeof(SelectionVertex)), vertices.data());
            cb->resourceUpdate(u);

            cb->setGraphicsPipeline(m_selectionPipeline);
            cb->setViewport(QRhiViewport(0, 0, rt->pixelSize().width(), rt->pixelSize().height()));
            cb->setShaderResources(m_selectionSrb);
            const QRhiCommandBuffer::VertexInput vbufBinding(m_selectionVbuf, 0);
            cb->setVertexInput(0, 1, &vbufBinding);
            cb->draw(quint32(vertices.size()));
        }
    }

    cb->endPass();
}

void PassLighting::ensurePipeline(FrameContext &ctx)
{
    if (!ctx.rhi || !ctx.targets || !ctx.shaders)
        return;

    QRhiRenderTarget *rt = ctx.rhi->swapchainRenderTarget();
    if (!rt)
        return;

    const QSize size = rt->pixelSize();
    const RenderTargetCache::GBufferTargets gbuf = ctx.targets->getOrCreateGBuffer(size, 1);
    const bool gbufChanged = gbuf.color0 != m_gbufColor0 ||
                             gbuf.color1 != m_gbufColor1 ||
                             gbuf.color2 != m_gbufColor2 ||
                             gbuf.depth != m_gbufDepth;
    const bool reverseZ = ctx.rhi->rhi()->clipSpaceCorrMatrix()(2, 2) < 0.0f;
    const QRhiTexture *spotShadow = ctx.shadows ? ctx.shadows->spotShadowMap : nullptr;
    const bool spotChanged = spotShadow != m_spotShadowMap;

    bool shadowsChanged = false;
    if (ctx.shadows) {
        for (int i = 0; i < 3; ++i) {
            if (m_shadowMapRefs[i] != ctx.shadows->shadowMaps[i])
                shadowsChanged = true;
        }
    }
    if (m_pipeline && m_rpDesc == rt->renderPassDescriptor() && !shadowsChanged && !gbufChanged
            && !spotChanged && m_reverseZ == reverseZ)
        return;

    delete m_pipeline;
    m_pipeline = nullptr;
    delete m_srb;
    m_srb = nullptr;
    delete m_sampler;
    m_sampler = nullptr;
    delete m_shadowSampler;
    m_shadowSampler = nullptr;
    delete m_spotShadowSampler;
    m_spotShadowSampler = nullptr;
    delete m_goboSampler;
    m_goboSampler = nullptr;
    delete m_lightsUbo;
    m_lightsUbo = nullptr;
    delete m_cameraUbo;
    m_cameraUbo = nullptr;
    delete m_shadowUbo;
    m_shadowUbo = nullptr;
    delete m_flipUbo;
    m_flipUbo = nullptr;
    delete m_spotGoboMap;
    m_spotGoboMap = nullptr;
    for (QString &path : m_spotGoboPaths)
        path = QString();

    m_sampler = ctx.rhi->rhi()->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                           QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    if (!m_sampler->create())
        return;

    m_shadowSampler = ctx.rhi->rhi()->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                                 QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    if (!m_shadowSampler->create())
        return;

    m_spotShadowSampler = ctx.rhi->rhi()->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                                     QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    if (!m_spotShadowSampler->create())
        return;
    m_goboSampler = ctx.rhi->rhi()->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                               QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    if (!m_goboSampler->create())
        return;
    m_reverseZ = reverseZ;

    struct LightsDataSize {
        QVector4D lightCount;
        QVector4D posRange[kMaxLights];
        QVector4D colorIntensity[kMaxLights];
        QVector4D dirInner[kMaxLights];
        QVector4D other[kMaxLights];
    };
    m_lightsUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(LightsDataSize));
    struct CameraDataSize {
        float view[16];
        float invViewProj[16];
        QVector4D cameraPos;
    };
    m_cameraUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                            sizeof(CameraDataSize));
    struct ShadowDataGpu {
        float lightViewProj[3][16];
        float splits[4];
        float dirLightDir[4];
        float dirLightColorIntensity[4];
        float spotLightViewProj[kMaxLights][16];
        float spotShadowParams[kMaxLights][4];
        float shadowDepthParams[4];
    };
    m_shadowUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ShadowDataGpu));
    m_flipUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QVector4D));
    if (!m_lightsUbo->create() || !m_cameraUbo->create() || !m_shadowUbo->create() || !m_flipUbo->create())
        return;

    if (!gbuf.color0 || !gbuf.color1 || !gbuf.color2) {
        qWarning() << "PassLighting: missing GBuffer textures";
        return;
    }
    m_gbufColor0 = gbuf.color0;
    m_gbufColor1 = gbuf.color1;
    m_gbufColor2 = gbuf.color2;
    m_gbufDepth = gbuf.depth;
    m_gbufWorldPosFloat = (gbuf.colorFormat == QRhiTexture::RGBA16F
                           || gbuf.colorFormat == QRhiTexture::RGBA32F);
    m_spotShadowMap = spotShadow;
    if (!m_spotGoboMap) {
        m_spotGoboMap = ctx.rhi->rhi()->newTextureArray(QRhiTexture::RGBA8, kMaxLights, m_spotGoboSize);
        if (!m_spotGoboMap->create())
            return;
    }

    m_srb = ctx.rhi->rhi()->newShaderResourceBindings();
    QVector<QRhiShaderResourceBinding> bindings = {
        QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, gbuf.color0, m_sampler),
        QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, gbuf.color1, m_sampler),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, gbuf.color2, m_sampler),
        QRhiShaderResourceBinding::uniformBuffer(3, QRhiShaderResourceBinding::FragmentStage, m_lightsUbo),
        QRhiShaderResourceBinding::uniformBuffer(4, QRhiShaderResourceBinding::FragmentStage, m_cameraUbo),
        QRhiShaderResourceBinding::uniformBuffer(5, QRhiShaderResourceBinding::FragmentStage, m_shadowUbo),
        QRhiShaderResourceBinding::uniformBuffer(12, QRhiShaderResourceBinding::FragmentStage, m_flipUbo)
    };

    if (ctx.shadows && ctx.shadows->shadowMaps[0]) {
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(6, QRhiShaderResourceBinding::FragmentStage, ctx.shadows->shadowMaps[0], m_shadowSampler));
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(7, QRhiShaderResourceBinding::FragmentStage, ctx.shadows->shadowMaps[1], m_shadowSampler));
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(8, QRhiShaderResourceBinding::FragmentStage, ctx.shadows->shadowMaps[2], m_shadowSampler));
        for (int i = 0; i < 3; ++i)
            m_shadowMapRefs[i] = ctx.shadows->shadowMaps[i];
    } else {
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(6, QRhiShaderResourceBinding::FragmentStage, gbuf.depth, m_shadowSampler));
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(7, QRhiShaderResourceBinding::FragmentStage, gbuf.depth, m_shadowSampler));
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(8, QRhiShaderResourceBinding::FragmentStage, gbuf.depth, m_shadowSampler));
        for (int i = 0; i < 3; ++i)
            m_shadowMapRefs[i] = nullptr;
    }
    if (spotShadow) {
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(9, QRhiShaderResourceBinding::FragmentStage,
                                                                     ctx.shadows->spotShadowMap,
                                                                     m_spotShadowSampler));
    } else {
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(9, QRhiShaderResourceBinding::FragmentStage,
                                                                     gbuf.depth, m_spotShadowSampler));
    }
    bindings.push_back(QRhiShaderResourceBinding::sampledTexture(10, QRhiShaderResourceBinding::FragmentStage,
                                                                 gbuf.depth, m_sampler));
    bindings.push_back(QRhiShaderResourceBinding::sampledTexture(11, QRhiShaderResourceBinding::FragmentStage,
                                                                 m_spotGoboMap, m_goboSampler));

    m_srb->setBindings(bindings.begin(), bindings.end());
    if (!m_srb->create()) {
        qWarning() << "PassLighting: failed to create SRB";
        return;
    }
    const QRhiShaderStage vs = ctx.shaders->loadStage(QRhiShaderStage::Vertex, QStringLiteral(":/shaders/lighting.vert.qsb"));
    const QRhiShaderStage fs = ctx.shaders->loadStage(QRhiShaderStage::Fragment, QStringLiteral(":/shaders/lighting.frag.qsb"));
    if (!vs.shader().isValid() || !fs.shader().isValid())
        return;

    QRhiGraphicsPipeline *pipeline = ctx.rhi->rhi()->newGraphicsPipeline();
    pipeline->setShaderStages({ vs, fs });
    pipeline->setSampleCount(1);
    pipeline->setCullMode(QRhiGraphicsPipeline::None);
    pipeline->setDepthTest(false);
    pipeline->setDepthWrite(false);
    pipeline->setShaderResourceBindings(m_srb);
    pipeline->setRenderPassDescriptor(rt->renderPassDescriptor());

    if (!pipeline->create()) {
        qWarning() << "PassLighting: failed to create pipeline";
        return;
    }

    m_pipeline = pipeline;
    m_rpDesc = rt->renderPassDescriptor();
}

void PassLighting::ensureSelectionBoxesPipeline(FrameContext &ctx, QRhiRenderTarget *rt)
{
    if (!ctx.rhi || !ctx.shaders || !rt || !ctx.targets)
        return;

    if (m_selectionPipeline && m_selectionRpDesc == rt->renderPassDescriptor())
        return;

    delete m_selectionPipeline;
    m_selectionPipeline = nullptr;
    delete m_selectionSrb;
    m_selectionSrb = nullptr;
    delete m_selectionUbo;
    m_selectionUbo = nullptr;
    delete m_selectionVbuf;
    m_selectionVbuf = nullptr;
    delete m_selectionDepthUbo;
    m_selectionDepthUbo = nullptr;

    const quint32 mat4Size = 16 * sizeof(float);
    m_selectionUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, mat4Size);
    if (!m_selectionUbo->create())
        return;

    m_selectionDepthUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QVector4D));
    if (!m_selectionDepthUbo->create())
        return;

    const int maxMeshes = 256;
    m_selectionMaxVertices = maxMeshes * 24;
    m_selectionVbuf = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                            m_selectionMaxVertices * int(sizeof(float) * 3));
    if (!m_selectionVbuf->create())
        return;

    m_selectionSrb = ctx.rhi->rhi()->newShaderResourceBindings();
    m_selectionSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_selectionUbo),
        QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, m_selectionDepthUbo),
        QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage,
                                                  ctx.targets->getOrCreateGBuffer(rt->pixelSize(), 1).depth,
                                                  m_sampler)
    });
    if (!m_selectionSrb->create())
        return;

    const QRhiShaderStage vs = ctx.shaders->loadStage(QRhiShaderStage::Vertex, QStringLiteral(":/shaders/selection_box.vert.qsb"));
    const QRhiShaderStage fs = ctx.shaders->loadStage(QRhiShaderStage::Fragment, QStringLiteral(":/shaders/selection_box.frag.qsb"));
    if (!vs.shader().isValid() || !fs.shader().isValid())
        return;

    QRhiGraphicsPipeline *pipeline = ctx.rhi->rhi()->newGraphicsPipeline();
    pipeline->setShaderStages({ vs, fs });
    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({ QRhiVertexInputBinding(sizeof(float) * 3) });
    inputLayout.setAttributes({ QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, 0) });
    pipeline->setVertexInputLayout(inputLayout);
    pipeline->setTopology(QRhiGraphicsPipeline::Lines);
    pipeline->setCullMode(QRhiGraphicsPipeline::None);
    pipeline->setDepthTest(false);
    pipeline->setDepthWrite(false);
    pipeline->setShaderResourceBindings(m_selectionSrb);
    pipeline->setRenderPassDescriptor(rt->renderPassDescriptor());

    if (!pipeline->create())
        return;

    m_selectionPipeline = pipeline;
    m_selectionRpDesc = rt->renderPassDescriptor();
}

