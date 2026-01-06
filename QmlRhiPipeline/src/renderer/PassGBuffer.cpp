#include "renderer/PassGBuffer.h"

#include <QtGui/QMatrix4x4>
#include <cstring>
#include <rhi/qrhi.h>

#include "core/RhiContext.h"
#include "core/ShaderManager.h"
#include "scene/Scene.h"

void PassGBuffer::prepare(FrameContext &ctx)
{
    if (!ctx.targets || !ctx.rhi)
        return;
    const QSize size = ctx.rhi->swapchainRenderTarget()->pixelSize();
    m_gbuffer = ctx.targets->getOrCreateGBuffer(size, 1);
    if (!m_gbuffer.rt)
        qWarning() << "PassGBuffer: failed to acquire GBuffer render target";

    ensurePipeline(ctx);
}

void PassGBuffer::execute(FrameContext &ctx)
{
    if (!ctx.rhi || !m_gbuffer.rt)
        return;
    if (!ctx.scene || !m_pipeline || !m_srb)
        return;
    QRhiCommandBuffer *cb = ctx.rhi->commandBuffer();
    if (!cb)
        return;

    const QColor clear0(0, 0, 0, 0);
    const QRhiDepthStencilClearValue dsClear(1.0f, 0);

    struct CameraData {
        float viewProj[16];
        QVector4D cameraPos;
    } camData;

    const QMatrix4x4 viewProj = ctx.rhi->rhi()->clipSpaceCorrMatrix()
            * ctx.scene->camera().projectionMatrix()
            * ctx.scene->camera().viewMatrix();
    std::memcpy(camData.viewProj, viewProj.constData(), sizeof(camData.viewProj));
    camData.cameraPos = QVector4D(ctx.scene->camera().position(), 1.0f);

    static bool s_dumped = false;
    if (!s_dumped) {
        //qDebug() << "PassGBuffer: mesh count" << ctx.scene->meshes().size();
        int idx = 0;
        for (const Mesh &mesh : ctx.scene->meshes()) {
            const QVector3D modelPos = mesh.modelMatrix.column(3).toVector3D();
            QVector3D localMin;
            QVector3D localMax;
            if (!mesh.vertices.isEmpty()) {
                localMin = QVector3D(mesh.vertices[0].px, mesh.vertices[0].py, mesh.vertices[0].pz);
                localMax = localMin;
                for (const Vertex &v : mesh.vertices) {
                    localMin.setX(qMin(localMin.x(), v.px));
                    localMin.setY(qMin(localMin.y(), v.py));
                    localMin.setZ(qMin(localMin.z(), v.pz));
                    localMax.setX(qMax(localMax.x(), v.px));
                    localMax.setY(qMax(localMax.y(), v.py));
                    localMax.setZ(qMax(localMax.z(), v.pz));
                }
            }
            const QVector3D localCenter = (localMin + localMax) * 0.5f;
            const QVector3D worldCenter = (mesh.modelMatrix * QVector4D(localCenter, 1.0f)).toVector3D();
            //qDebug() << "PassGBuffer: mesh" << idx << "modelPos" << modelPos
            //         << "localCenter" << localCenter << "worldCenter" << worldCenter;
            ++idx;
        }
        s_dumped = true;
    }

    struct ModelData {
        float model[16];
        float normalMatrix[16];
    };

    struct MaterialData {
        QVector4D baseColorMetal;
        QVector4D roughnessOcclusion;
        QVector4D emissive;
    };

    QRhiResourceUpdateBatch *u = ctx.rhi->rhi()->nextResourceUpdateBatch();
    u->updateDynamicBuffer(m_cameraUbo, 0, sizeof(CameraData), &camData);

    for (Mesh &mesh : ctx.scene->meshes()) {
        ensureMeshBuffers(ctx, mesh, u);
        if (!mesh.modelUbo || !mesh.materialUbo)
            continue;
        const QMatrix4x4 model = mesh.modelMatrix;
        QMatrix4x4 normalMatrix = model.inverted();
        normalMatrix = normalMatrix.transposed();

        ModelData modelData;
        std::memcpy(modelData.model, model.constData(), sizeof(modelData.model));
        std::memcpy(modelData.normalMatrix, normalMatrix.constData(), sizeof(modelData.normalMatrix));

        MaterialData matData;
        matData.baseColorMetal = QVector4D(mesh.material.baseColor, mesh.material.metalness);
        matData.roughnessOcclusion = QVector4D(mesh.material.roughness, mesh.material.occlusion, 0.0f, 0.0f);
        matData.emissive = QVector4D(mesh.material.emissive, 0.0f);

        u->updateDynamicBuffer(mesh.modelUbo, 0, sizeof(ModelData), &modelData);
        u->updateDynamicBuffer(mesh.materialUbo, 0, sizeof(MaterialData), &matData);
    }

    cb->resourceUpdate(u);

    cb->beginPass(m_gbuffer.rt, clear0, dsClear);
    cb->setGraphicsPipeline(m_pipeline);
    cb->setViewport(QRhiViewport(0, 0, m_gbuffer.rt->pixelSize().width(), m_gbuffer.rt->pixelSize().height()));

    for (Mesh &mesh : ctx.scene->meshes()) {
        if (!mesh.vertexBuffer || !mesh.indexBuffer || mesh.indexCount == 0)
            continue;
        if (!mesh.srb)
            continue;
        cb->setShaderResources(mesh.srb);
        const QRhiCommandBuffer::VertexInput vbufBinding(mesh.vertexBuffer, 0);
        cb->setVertexInput(0, 1, &vbufBinding, mesh.indexBuffer, 0, QRhiCommandBuffer::IndexUInt32);
        cb->drawIndexed(mesh.indexCount);
    }

    cb->endPass();
}

void PassGBuffer::ensurePipeline(FrameContext &ctx)
{
    if (!ctx.rhi || !ctx.shaders || !m_gbuffer.rpDesc)
        return;
    if (m_pipeline && m_rpDesc == m_gbuffer.rpDesc)
        return;

    delete m_pipeline;
    m_pipeline = nullptr;
    delete m_srb;
    m_srb = nullptr;
    delete m_cameraUbo;
    delete m_modelUbo;
    delete m_materialUbo;

    if (ctx.scene) {
        for (Mesh &mesh : ctx.scene->meshes()) {
            delete mesh.srb;
            mesh.srb = nullptr;
        }
    }

    const quint32 mat4Size = 16 * sizeof(float);
    m_cameraUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, mat4Size + sizeof(QVector4D));
    m_modelUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, mat4Size * 2);
    m_materialUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QVector4D) * 3);

    if (!m_cameraUbo->create() || !m_modelUbo->create() || !m_materialUbo->create())
        return;

    m_srb = ctx.rhi->rhi()->newShaderResourceBindings();
    m_srb->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_cameraUbo),
        QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, m_modelUbo),
        QRhiShaderResourceBinding::uniformBuffer(2, QRhiShaderResourceBinding::FragmentStage, m_materialUbo)
    });
    if (!m_srb->create())
        return;

    const QRhiShaderStage vs = ctx.shaders->loadStage(QRhiShaderStage::Vertex, QStringLiteral(":/shaders/gbuffer.vert.qsb"));
    const QRhiShaderStage fs = ctx.shaders->loadStage(QRhiShaderStage::Fragment, QStringLiteral(":/shaders/gbuffer.frag.qsb"));
    if (!vs.shader().isValid() || !fs.shader().isValid())
        return;

    QRhiGraphicsPipeline *pipeline = ctx.rhi->rhi()->newGraphicsPipeline();
    pipeline->setShaderStages({ vs, fs });

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings({
        QRhiVertexInputBinding(sizeof(Vertex))
    });
    inputLayout.setAttributes({
        QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float3, 0),
        QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float3, 12),
        QRhiVertexInputAttribute(0, 2, QRhiVertexInputAttribute::Float2, 24)
    });
    pipeline->setVertexInputLayout(inputLayout);
    pipeline->setSampleCount(1);
    pipeline->setCullMode(QRhiGraphicsPipeline::Back);
    pipeline->setDepthTest(true);
    pipeline->setDepthWrite(true);
    pipeline->setShaderResourceBindings(m_srb);
    pipeline->setRenderPassDescriptor(m_gbuffer.rpDesc);

    if (!pipeline->create()) {
        qWarning() << "PassGBuffer: failed to create pipeline";
        return;
    }

    m_pipeline = pipeline;
    m_rpDesc = m_gbuffer.rpDesc;
}

void PassGBuffer::ensureMeshBuffers(FrameContext &ctx, Mesh &mesh, QRhiResourceUpdateBatch *u)
{
    if (mesh.vertices.isEmpty() || mesh.indices.isEmpty())
        return;

    if (!mesh.vertexBuffer || !mesh.indexBuffer) {
        mesh.vertexBuffer = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, mesh.vertices.size() * sizeof(Vertex));
        mesh.indexBuffer = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, mesh.indices.size() * sizeof(quint32));
        if (!mesh.vertexBuffer->create() || !mesh.indexBuffer->create())
            return;

        u->uploadStaticBuffer(mesh.vertexBuffer, mesh.vertices.constData());
        u->uploadStaticBuffer(mesh.indexBuffer, mesh.indices.constData());
        mesh.indexCount = mesh.indices.size();
    }
    if (mesh.indexCount == 0 && !mesh.indices.isEmpty())
        mesh.indexCount = mesh.indices.size();

    if (!mesh.modelUbo) {
        const quint32 mat4Size = 16 * sizeof(float);
        mesh.modelUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, mat4Size * 2);
        if (!mesh.modelUbo->create())
            return;
    }
    if (!mesh.materialUbo) {
        mesh.materialUbo = ctx.rhi->rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(QVector4D) * 3);
        if (!mesh.materialUbo->create())
            return;
    }
    if (!mesh.srb) {
        mesh.srb = ctx.rhi->rhi()->newShaderResourceBindings();
        mesh.srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_cameraUbo),
            QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::VertexStage, mesh.modelUbo),
            QRhiShaderResourceBinding::uniformBuffer(2, QRhiShaderResourceBinding::FragmentStage, mesh.materialUbo)
        });
        if (!mesh.srb->create())
            return;
    }
}
