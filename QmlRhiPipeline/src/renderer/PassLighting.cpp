#include "renderer/PassLighting.h"

#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtSvg/QSvgRenderer>
#include <rhi/qrhi.h>
#include <vector>
#include <cstring>
#include <QtCore/QVector>

#include "core/RhiContext.h"
#include "core/RenderTargetCache.h"
#include "core/ShaderManager.h"
#include "scene/Scene.h"

static QString resolveGoboPath(const QString &path)
{
    if (path.isEmpty())
        return QString();
    const QUrl url(path);
    if (url.isValid() && url.isLocalFile())
        return url.toLocalFile();
    const QFileInfo info(path);
    if (info.isAbsolute())
        return path;
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString candidateCwd = QDir::current().filePath(path);
    if (QFileInfo::exists(candidateCwd))
        return candidateCwd;
    const QString candidateApp = QDir(appDir).filePath(path);
    if (QFileInfo::exists(candidateApp))
        return candidateApp;
    const QString candidateAppParent = QDir(appDir).filePath(QStringLiteral("../") + path);
    if (QFileInfo::exists(candidateAppParent))
        return candidateAppParent;
    return candidateCwd;
}

static QImage loadGoboImage(const QString &path, const QSize &size)
{
    QImage image;
    if (!path.isEmpty())
    {
        const QString resolved = resolveGoboPath(path);
        if (resolved.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive))
        {
            QSvgRenderer renderer(resolved);
            if (renderer.isValid())
            {
                const int w = size.width();
                const int h = size.height();
                image = QImage(size, QImage::Format_RGBA8888);
                // Match existing gobo pipeline: black background + circular mask + inset render.
                image.fill(Qt::black);
                QPainter painter(&image);
                painter.setBrush(QBrush(Qt::white));
                painter.setPen(Qt::NoPen);
                // Mask out a 2px border to avoid edge bleed from the rasterized SVG.
                painter.drawEllipse(2, 2, qMax(0, w - 4), qMax(0, h - 4));
                // Render SVG 1px inset to keep it inside the mask.
                renderer.render(&painter, QRect(1, 1, qMax(0, w - 2), qMax(0, h - 2)));
            }
        }
        else
        {
            const int w = size.width();
            const int h = size.height();
            image = QImage(size, QImage::Format_RGBA8888);
            // Apply the same mask/inset behavior for non-SVG gobos.
            image.fill(Qt::black);
            QPainter painter(&image);
            painter.setBrush(QBrush(Qt::white));
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(2, 2, qMax(0, w - 4), qMax(0, h - 4));
            QIcon goboFile(resolved);
            painter.drawPixmap(1, 1, qMax(0, w - 2), qMax(0, h - 2),
                               goboFile.pixmap(QSize(qMax(0, w - 2), qMax(0, h - 2))));
        }
    }
    if (image.isNull())
    {
        image = QImage(size, QImage::Format_RGBA8888);
        image.fill(Qt::black);
    }
    else
    {
        image = image.convertToFormat(QImage::Format_RGBA8888);
        if (image.size() != size)
            image = image.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

void PassLighting::prepare(FrameContext &ctx)
{
    if (qEnvironmentVariableIsSet("RHIPIPELINE_SKIP_LIGHTING"))
    {
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
    for (int i = 0; i < kMaxLights; ++i)
    {
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
    QRhiRenderTarget *swapRt = ctx.rhi->swapchainRenderTarget();
    if (!cb || !swapRt)
        return;
    const RenderTargetCache::LightingTargets lighting = ctx.targets->getOrCreateLightingTarget(swapRt->pixelSize(), 1);
    QRhiRenderTarget *rt = lighting.rt;
    if (!rt)
        return;

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
    } lightData;
    memset(&lightData, 0, sizeof(LightsData));

    const int maxLights = qMin(kMaxLights, ctx.scene->lights().size());
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
        } else if (l.type == Light::Type::Spot)
        {
            extraZ = l.goboPath.isEmpty() ? -1.0f : float(i);
            extraW = float(l.qualitySteps);
        }
        lightData.other[i] = QVector4D(qCos(l.outerCone),
                                       float(l.type),
                                       extraZ,
                                       extraW);
    }

    struct CameraData
    {
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
    camData.cameraPos = QVector4D(ctx.scene->camera().position(), ctx.scene->timeSeconds());

    struct ShadowDataGpu
    {
        float lightViewProj[3][16];
        float splits[4];
        float dirLightDir[4];
        float dirLightColorIntensity[4];
        float spotLightViewProj[kMaxLights][16];
        float spotShadowParams[kMaxLights][4];
        float shadowDepthParams[4];
    } shadowData;
    memset(&shadowData, 0, sizeof(ShadowDataGpu));
    if (ctx.shadows && ctx.shadows->cascadeCount > 0)
    {
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
    if (ctx.shadows)
    {
        for (int i = 0; i < kMaxLights; ++i)
        {
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
    if (m_lightsUbo)
        u->updateDynamicBuffer(m_lightsUbo, 0, sizeof(LightsData), &lightData);
    if (m_cameraUbo)
        u->updateDynamicBuffer(m_cameraUbo, 0, sizeof(CameraData), &camData);
    if (m_shadowUbo)
    u->updateDynamicBuffer(m_shadowUbo, 0, sizeof(ShadowDataGpu), &shadowData);
    bool flipSampleY = !ctx.rhi->rhi()->isYUpInFramebuffer();
    bool flipNdcY = !ctx.rhi->rhi()->isYUpInNDC();
    if (ctx.rhi->rhi()->backend() == QRhi::D3D11)
    {
        // D3D path already matches the default quad orientation for this pass.
        flipSampleY = false;
        flipNdcY = false;
    }
    const QVector4D flipData(flipSampleY ? 1.0f : 0.0f,
                             flipNdcY ? 1.0f : 0.0f,
                             0.0f,
                             0.0f);
    u->updateDynamicBuffer(m_flipUbo, 0, sizeof(flipData), &flipData);
    if (m_useLightCulling && m_lightCullUbo && ctx.lightCulling)
    {
        struct LightCullParams
        {
            QVector4D screen;
            QVector4D tile;
        } params;
        const QSize size = rt->pixelSize();
        params.screen = QVector4D(float(size.width()), float(size.height()),
                                  1.0f / float(size.width()), 1.0f / float(size.height()));
        params.tile = QVector4D(float(ctx.lightCulling->tileCountX),
                                float(ctx.lightCulling->tileCountY),
                                float(ctx.lightCulling->tileSize),
                                ctx.lightCulling->enabled ? 1.0f : 0.0f);
        u->updateDynamicBuffer(m_lightCullUbo, 0, sizeof(LightCullParams), &params);
    }
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
    for (const Mesh &mesh : ctx.scene->meshes())
    {
        if (mesh.selected && mesh.visible)
        {
            anySelected = true;
            break;
        }
    }

    if (anySelected)
        ensureSelectionBoxesPipeline(ctx, rt);

    if (anySelected && m_selectionPipeline && m_selectionSrb && m_selectionVbuf)
    {
        struct SelectionVertex
        {
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

        for (const Mesh &mesh : meshes)
        {
            if (!mesh.selected)
                continue;
            if (!mesh.visible)
                continue;
            if (mesh.vertices.isEmpty())
                continue;

            QVector3D minV = mesh.boundsMin;
            QVector3D maxV = mesh.boundsMax;
            if (!mesh.boundsValid)
            {
                minV = QVector3D(mesh.vertices[0].px, mesh.vertices[0].py, mesh.vertices[0].pz);
                maxV = minV;
                for (const Vertex &v : mesh.vertices)
                {
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

            for (const auto &edge : edges)
            {
                if (maxVertices > 0 && int(vertices.size() + 2) > maxVertices)
                    break;
                const QVector3D a = (model * QVector4D(corners[edge[0]], 1.0f)).toVector3D();
                const QVector3D b = (model * QVector4D(corners[edge[1]], 1.0f)).toVector3D();
                vertices.push_back({ a.x(), a.y(), a.z() });
                vertices.push_back({ b.x(), b.y(), b.z() });
            }
        }

        if (!vertices.empty())
        {
            QRhiResourceUpdateBatch *u = ctx.rhi->rhi()->nextResourceUpdateBatch();
            QMatrix4x4 viewProj = ctx.rhi->rhi()->clipSpaceCorrMatrix()
                    * ctx.scene->camera().projectionMatrix()
                    * ctx.scene->camera().viewMatrix();
            if (ctx.rhi->rhi()->backend() == QRhi::D3D11)
            {
                QMatrix4x4 flipY;
                flipY.scale(1.0f, -1.0f, 1.0f);
                viewProj = flipY * viewProj;
            }
            const quint32 mat4Size = 16 * sizeof(float);
            bool flipSampleY = !ctx.rhi->rhi()->isYUpInFramebuffer();
            const QVector4D depthParams(ctx.shadows ? ctx.shadows->shadowDepthParams.x() : 1.0f,
                                        ctx.shadows ? ctx.shadows->shadowDepthParams.y() : 0.0f,
                                        ctx.shadows ? ctx.shadows->shadowDepthParams.z() : 0.0f,
                                        flipSampleY ? 1.0f : 0.0f);
            const QSize rtSize = rt->pixelSize();
            const QVector4D screenParams(float(rtSize.width()),
                                         float(rtSize.height()),
                                         1.0f / float(rtSize.width()),
                                         1.0f / float(rtSize.height()));
            u->updateDynamicBuffer(m_selectionUbo, 0, mat4Size, viewProj.constData());
            u->updateDynamicBuffer(m_selectionDepthUbo, 0, sizeof(depthParams), &depthParams);
            u->updateDynamicBuffer(m_selectionScreenUbo, 0, sizeof(screenParams), &screenParams);
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

    QRhiRenderTarget *swapRt = ctx.rhi->swapchainRenderTarget();
    if (!swapRt)
        return;

    const QSize size = swapRt->pixelSize();
    const RenderTargetCache::LightingTargets lighting = ctx.targets->getOrCreateLightingTarget(size, 1);
    QRhiRenderTarget *rt = lighting.rt;
    if (!rt)
        return;
    const RenderTargetCache::GBufferTargets gbuf = ctx.targets->getOrCreateGBuffer(size, 1);
    const bool gbufChanged = gbuf.color0 != m_gbufColor0 ||
                             gbuf.color1 != m_gbufColor1 ||
                             gbuf.color2 != m_gbufColor2 ||
                             gbuf.color3 != m_gbufColor3 ||
                             gbuf.depth != m_gbufDepth;
    const bool reverseZ = ctx.rhi->rhi()->clipSpaceCorrMatrix()(2, 2) < 0.0f;
    const bool d3d11 = ctx.rhi->rhi()->backend() == QRhi::D3D11;
    const bool metal = ctx.rhi->rhi()->backend() == QRhi::Metal;
    const bool useLightCulling = ctx.lightCulling
            && ctx.lightCulling->enabled
            && ctx.lightCulling->tileLightIndexBuffer;
    const bool effectiveLightCulling = useLightCulling && !d3d11 && !metal;
    bool spotChanged = false;
    if (ctx.shadows)
    {
        for (int i = 0; i < kMaxSpotShadows; ++i)
        {
            if (m_spotShadowMaps[i] != ctx.shadows->spotShadowMaps[i])
            {
                spotChanged = true;
                break;
            }
        }
    }

    bool shadowsChanged = false;
    if (ctx.shadows)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (m_shadowMapRefs[i] != ctx.shadows->shadowMaps[i])
                shadowsChanged = true;
        }
    }
    if (m_pipeline && m_rpDesc == rt->renderPassDescriptor() && !shadowsChanged && !gbufChanged
            && !spotChanged && m_reverseZ == reverseZ && m_useLightCulling == effectiveLightCulling
            && (!effectiveLightCulling || m_lightIndexBuffer == ctx.lightCulling->tileLightIndexBuffer))
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
    delete m_selectionPipeline;
    m_selectionPipeline = nullptr;
    delete m_selectionSrb;
    m_selectionSrb = nullptr;
    m_selectionRpDesc = nullptr;
    delete m_lightsUbo;
    m_lightsUbo = nullptr;
    delete m_cameraUbo;
    m_cameraUbo = nullptr;
    delete m_shadowUbo;
    m_shadowUbo = nullptr;
    delete m_flipUbo;
    m_flipUbo = nullptr;
    delete m_lightCullUbo;
    m_lightCullUbo = nullptr;
    m_lightIndexBuffer = nullptr;
    delete m_spotGoboMap;
    m_spotGoboMap = nullptr;
    for (QString &path : m_spotGoboPaths)
        path = QString();

    m_sampler = ctx.rhi->rhi()->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                           QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    if (!m_sampler->create())
        return;

    if (!metal)
    {
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
    }
    else
    {
        m_spotShadowSampler = ctx.rhi->rhi()->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                                                         QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        if (!m_spotShadowSampler->create())
            return;
        m_goboSampler = ctx.rhi->rhi()->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                                   QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        if (!m_goboSampler->create())
            return;
    }
    m_reverseZ = reverseZ;
    m_useLightCulling = effectiveLightCulling;

    if (metal)
    {
        struct LightsDataSize
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
        m_lightsUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(LightsDataSize));
        struct CameraDataSize
        {
            float view[16];
            float invViewProj[16];
            QVector4D cameraPos;
        };
        m_cameraUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                                sizeof(CameraDataSize));
        struct ShadowDataGpu
        {
            float lightViewProj[3][16];
            float splits[4];
            float dirLightDir[4];
            float dirLightColorIntensity[4];
            float spotLightViewProj[kMaxLights][16];
            float spotShadowParams[kMaxLights][4];
            float shadowDepthParams[4];
        };
        m_shadowUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ShadowDataGpu));
    }
    else
    {
        struct LightsDataSize
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
        m_lightsUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(LightsDataSize));
        struct CameraDataSize
        {
            float view[16];
            float invViewProj[16];
            QVector4D cameraPos;
        };
        m_cameraUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                                sizeof(CameraDataSize));
        struct ShadowDataGpu
        {
            float lightViewProj[3][16];
            float splits[4];
            float dirLightDir[4];
            float dirLightColorIntensity[4];
            float spotLightViewProj[kMaxLights][16];
            float spotShadowParams[kMaxLights][4];
            float shadowDepthParams[4];
        };
        m_shadowUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ShadowDataGpu));
    }
    m_flipUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QVector4D));
    if (m_useLightCulling)
    {
        m_lightCullUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
                                                   sizeof(QVector4D) * 2);
        if (!m_lightCullUbo->create())
            return;
        m_lightIndexBuffer = ctx.lightCulling ? ctx.lightCulling->tileLightIndexBuffer : nullptr;
        if (!m_lightIndexBuffer)
            return;
    }
    if (!m_flipUbo->create())
        return;
    if (metal)
    {
        if (!m_lightsUbo->create() || !m_cameraUbo->create() || !m_shadowUbo->create())
            return;
    }
    else
    {
        if (!m_lightsUbo->create() || !m_cameraUbo->create() || !m_shadowUbo->create())
            return;
    }

    if (!gbuf.color0 || !gbuf.color1 || !gbuf.color2 || !gbuf.color3)
    {
        qWarning() << "PassLighting: missing GBuffer textures";
        return;
    }
    m_gbufColor0 = gbuf.color0;
    m_gbufColor1 = gbuf.color1;
    m_gbufColor2 = gbuf.color2;
    m_gbufColor3 = gbuf.color3;
    m_gbufDepth = gbuf.depth;
    m_gbufWorldPosFloat = (gbuf.colorFormat == QRhiTexture::RGBA16F
                           || gbuf.colorFormat == QRhiTexture::RGBA32F);
    if (ctx.shadows)
    {
        for (int i = 0; i < kMaxSpotShadows; ++i)
            m_spotShadowMaps[i] = ctx.shadows->spotShadowMaps[i];
    }
    else
    {
        for (int i = 0; i < kMaxSpotShadows; ++i)
            m_spotShadowMaps[i] = nullptr;
    }
    if (!m_spotGoboMap)
    {
        m_spotGoboMap = ctx.rhi->rhi()->newTextureArray(QRhiTexture::RGBA8, kMaxLights, m_spotGoboSize);
        if (!m_spotGoboMap->create())
            return;
    }

    m_srb = ctx.rhi->rhi()->newShaderResourceBindings();
    QVector<QRhiShaderResourceBinding> bindings;
    if (metal)
    {
        bindings = {
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::FragmentStage, m_flipUbo),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, gbuf.color0, m_sampler),
            QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, gbuf.color3, m_sampler),
            QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, gbuf.color1, m_sampler),
            QRhiShaderResourceBinding::sampledTexture(4, QRhiShaderResourceBinding::FragmentStage, gbuf.color2, m_sampler),
            QRhiShaderResourceBinding::sampledTexture(5, QRhiShaderResourceBinding::FragmentStage, gbuf.depth, m_sampler),
            QRhiShaderResourceBinding::uniformBuffer(6, QRhiShaderResourceBinding::FragmentStage, m_lightsUbo),
            QRhiShaderResourceBinding::uniformBuffer(7, QRhiShaderResourceBinding::FragmentStage, m_cameraUbo),
            QRhiShaderResourceBinding::uniformBuffer(8, QRhiShaderResourceBinding::FragmentStage, m_shadowUbo)
        };
        for (int i = 0; i < kMaxSpotShadows; ++i)
        {
            QRhiTexture *spotTex = ctx.shadows ? ctx.shadows->spotShadowMaps[i] : nullptr;
            if (!spotTex)
                spotTex = gbuf.depth;
            bindings.push_back(QRhiShaderResourceBinding::sampledTexture(8 + i, QRhiShaderResourceBinding::FragmentStage,
                                                                         spotTex, m_spotShadowSampler));
        }
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(16, QRhiShaderResourceBinding::FragmentStage,
                                                                     m_spotGoboMap, m_goboSampler));
    }
    else if (d3d11)
    {
        bindings = {
            QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, gbuf.color0, m_sampler),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, gbuf.color1, m_sampler),
            QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, gbuf.color2, m_sampler),
            QRhiShaderResourceBinding::uniformBuffer(20, QRhiShaderResourceBinding::FragmentStage, m_lightsUbo),
            QRhiShaderResourceBinding::uniformBuffer(21, QRhiShaderResourceBinding::FragmentStage, m_cameraUbo),
            QRhiShaderResourceBinding::uniformBuffer(22, QRhiShaderResourceBinding::FragmentStage, m_shadowUbo),
            QRhiShaderResourceBinding::uniformBuffer(23, QRhiShaderResourceBinding::FragmentStage, m_flipUbo)
        };
    }
    else
    {
        bindings = {
            QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, gbuf.color0, m_sampler),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, gbuf.color1, m_sampler),
            QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, gbuf.color2, m_sampler),
            QRhiShaderResourceBinding::sampledTexture(20, QRhiShaderResourceBinding::FragmentStage, gbuf.color3, m_sampler),
            QRhiShaderResourceBinding::uniformBuffer(3, QRhiShaderResourceBinding::FragmentStage, m_lightsUbo),
            QRhiShaderResourceBinding::uniformBuffer(4, QRhiShaderResourceBinding::FragmentStage, m_cameraUbo),
            QRhiShaderResourceBinding::uniformBuffer(5, QRhiShaderResourceBinding::FragmentStage, m_shadowUbo),
            QRhiShaderResourceBinding::uniformBuffer(19, QRhiShaderResourceBinding::FragmentStage, m_flipUbo)
        };
        if (m_useLightCulling)
        {
            bindings.push_back(QRhiShaderResourceBinding::uniformBuffer(21, QRhiShaderResourceBinding::FragmentStage,
                                                                        m_lightCullUbo));
            bindings.push_back(QRhiShaderResourceBinding::bufferLoad(22, QRhiShaderResourceBinding::FragmentStage,
                                                                     m_lightIndexBuffer));
        }
    }

    if (!metal && ctx.shadows && ctx.shadows->shadowMaps[0])
    {
        const int base = d3d11 ? 3 : 6;
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(base + 0, QRhiShaderResourceBinding::FragmentStage, ctx.shadows->shadowMaps[0], m_shadowSampler));
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(base + 1, QRhiShaderResourceBinding::FragmentStage, ctx.shadows->shadowMaps[1], m_shadowSampler));
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(base + 2, QRhiShaderResourceBinding::FragmentStage, ctx.shadows->shadowMaps[2], m_shadowSampler));
        for (int i = 0; i < 3; ++i)
            m_shadowMapRefs[i] = ctx.shadows->shadowMaps[i];
    }
    else if (!metal)
    {
        const int base = d3d11 ? 3 : 6;
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(base + 0, QRhiShaderResourceBinding::FragmentStage, gbuf.depth, m_shadowSampler));
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(base + 1, QRhiShaderResourceBinding::FragmentStage, gbuf.depth, m_shadowSampler));
        bindings.push_back(QRhiShaderResourceBinding::sampledTexture(base + 2, QRhiShaderResourceBinding::FragmentStage, gbuf.depth, m_shadowSampler));
        for (int i = 0; i < 3; ++i)
            m_shadowMapRefs[i] = nullptr;
    }
    if (!metal)
    {
        const int spotShadowBindings = d3d11 ? 7 : kMaxSpotShadows;
        for (int i = 0; i < spotShadowBindings; ++i)
        {
            QRhiTexture *spotTex = ctx.shadows ? ctx.shadows->spotShadowMaps[i] : nullptr;
            if (!spotTex)
                spotTex = gbuf.depth;
            const int base = d3d11 ? 6 : 9;
            bindings.push_back(QRhiShaderResourceBinding::sampledTexture(base + i, QRhiShaderResourceBinding::FragmentStage,
                                                                         spotTex, m_spotShadowSampler));
        }
        if (d3d11)
        {
            bindings.push_back(QRhiShaderResourceBinding::sampledTexture(13, QRhiShaderResourceBinding::FragmentStage,
                                                                         gbuf.depth, m_sampler));
            bindings.push_back(QRhiShaderResourceBinding::sampledTexture(14, QRhiShaderResourceBinding::FragmentStage,
                                                                         m_spotGoboMap, m_goboSampler));
        }
        else
        {
            bindings.push_back(QRhiShaderResourceBinding::sampledTexture(17, QRhiShaderResourceBinding::FragmentStage,
                                                                         gbuf.depth, m_sampler));
            bindings.push_back(QRhiShaderResourceBinding::sampledTexture(18, QRhiShaderResourceBinding::FragmentStage,
                                                                         m_spotGoboMap, m_goboSampler));
        }
    }

    m_srb->setBindings(bindings.begin(), bindings.end());
    if (!m_srb->create())
    {
        qWarning() << "PassLighting: failed to create SRB";
        return;
    }
    const QRhiShaderStage vs = ctx.shaders->loadStage(QRhiShaderStage::Vertex, QStringLiteral(":/shaders/lighting.vert.qsb"));
    QString fragPath;
    if (d3d11)
        fragPath = QStringLiteral(":/shaders/lighting_d3d.frag.qsb");
    else if (metal)
        fragPath = QStringLiteral(":/shaders/lighting_metal.frag.qsb");
    else
        fragPath = m_useLightCulling
                ? QStringLiteral(":/shaders/lighting_cull.frag.qsb")
                : QStringLiteral(":/shaders/lighting.frag.qsb");
    const QRhiShaderStage fs = ctx.shaders->loadStage(QRhiShaderStage::Fragment, fragPath);
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

    if (!pipeline->create())
    {
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
    if (!m_sampler)
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
    delete m_selectionScreenUbo;
    m_selectionScreenUbo = nullptr;

    const quint32 mat4Size = 16 * sizeof(float);
    m_selectionUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, mat4Size);
    if (!m_selectionUbo->create())
        return;

    m_selectionDepthUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QVector4D));
    if (!m_selectionDepthUbo->create())
        return;
    m_selectionScreenUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QVector4D));
    if (!m_selectionScreenUbo->create())
        return;

    const int maxMeshes = 256;
    m_selectionMaxVertices = maxMeshes * 24;
    m_selectionVbuf = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                            m_selectionMaxVertices * int(sizeof(float) * 3));
    if (!m_selectionVbuf->create())
        return;

    QRhiTexture *gbufDepth = ctx.targets->getOrCreateGBuffer(rt->pixelSize(), 1).depth;
    if (!gbufDepth)
        return;

    m_selectionSrb = ctx.rhi->rhi()->newShaderResourceBindings();
    m_selectionSrb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_selectionUbo),
        QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, m_selectionDepthUbo),
        QRhiShaderResourceBinding::uniformBuffer(2, QRhiShaderResourceBinding::FragmentStage, m_selectionScreenUbo),
        QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage,
                                                  gbufDepth,
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
