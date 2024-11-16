 
#include "rhiqmlitem.h"
#include <QFile>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>

QQuickRhiItemRenderer *RhiQmlItem::createRenderer()
{
    return new RhiQmlItemRenderer;
}

void RhiQmlItem::setRotation(float rotation)
{
    if (m_rotation == rotation)
        return;

    m_rotation = rotation;
    emit rotationChanged();
    update();
}

void RhiQmlItem::setLightIntensity(float intensity)
{
    if (m_lightIntensity == intensity)
        return;

    m_lightIntensity = intensity;
    emit lightIntensityChanged();
    update();
}

void RhiQmlItemRenderer::synchronize(QQuickRhiItem *rhiItem)
{
    RhiQmlItem *item = static_cast<RhiQmlItem *>(rhiItem);
    if (item->rotation() != m_rot)
        m_rot = item->rotation();

    if (item->lightIntensity() != m_int)
        m_int = item->lightIntensity();
}

QShader RhiQmlItemRenderer::getShader(const QString &name)
{
    QFile f(name);
    return f.open(QIODevice::ReadOnly) ? QShader::fromSerialized(f.readAll()) : QShader();
}

void RhiQmlItemRenderer::processMesh(aiMesh *mesh)
{
    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        m_positions.append(QVector3D(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z));

        // Ensure normals are loaded (Assimp should generate them if they are missing)
        if (mesh->HasNormals())
            m_normals.append(QVector3D(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z));
        else
            m_normals.append(QVector3D(0.0f, 0.0f, 1.0f));  // Default normal if not present

        // Load texture coordinates if they exist, otherwise default to (0,0)
        m_texCoords.append(mesh->mTextureCoords[0]
                            ? QVector2D(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
                            : QVector2D(0.0f, 0.0f));
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            m_indices.push_back(face.mIndices[j]);
    }
}

void RhiQmlItemRenderer::processNode(aiNode *node)
{
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh *mesh = m_scene->mMeshes[node->mMeshes[i]];
        processMesh(mesh);
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++)
        processNode(node->mChildren[i]);
}

void RhiQmlItemRenderer::load3DModel(const QString &path)
{
    Assimp::Importer importer;
    m_scene = importer.ReadFile(path.toStdString(), aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals);

    if (!m_scene || m_scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !m_scene->mRootNode)
    {
        qWarning() << "Error loading model: " << importer.GetErrorString();
        return;
    }

    processNode(m_scene->mRootNode);
    m_positions.append(m_normals);
}


void RhiQmlItemRenderer::initialize(QRhiCommandBuffer *cb)
{
    if (m_rhi != rhi()) {
        m_rhi = rhi();
        m_pipeline.reset();
    }

    if (m_sampleCount != renderTarget()->sampleCount()) {
        m_sampleCount = renderTarget()->sampleCount();
        m_pipeline.reset();
    }

    //![1]
    //![2]
    if (!m_pipeline)
    {
        load3DModel("/home/massimo/projects/qmlplayground/QmlRhiPipeline/meshes/suzanne.obj");
        qDebug() << "VERTICES LOADED" << (m_positions.length() / 2);

        m_vbuf.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, m_positions.length() * sizeof(QVector3D)));
        m_vbuf->create();

        m_mvpUniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 3 * sizeof(QMatrix4x4)));
        m_mvpUniformBuffer->create();

        m_lightUniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 7 * sizeof(float)));
        m_lightUniformBuffer->create();

        m_srb.reset(m_rhi->newShaderResourceBindings());
        m_srb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_mvpUniformBuffer.get()),
            QRhiShaderResourceBinding::uniformBuffer(1, QRhiShaderResourceBinding::FragmentStage, m_lightUniformBuffer.get()),
        });
        m_srb->create();

        m_pipeline.reset(m_rhi->newGraphicsPipeline());
        m_pipeline->setDepthTest(true);
        m_pipeline->setDepthWrite(true);
        m_pipeline->setCullMode(QRhiGraphicsPipeline::Back);
        m_pipeline->setShaderStages({
            { QRhiShaderStage::Vertex, getShader(QLatin1String(":/scene_pass.vert.qsb")) },
            { QRhiShaderStage::Fragment, getShader(QLatin1String(":/scene_pass.frag.qsb")) }
        });
        QRhiVertexInputLayout inputLayout;
        // The cube is provided as non-interleaved sets of positions, UVs, normals.
        inputLayout.setBindings({
            { 3 * sizeof(float) },
            //{ 2 * sizeof(float) },
            { 3 * sizeof(float) }
        });
        inputLayout.setAttributes({
            { 0, 0, QRhiVertexInputAttribute::Float3, 0 },
            //{ 0, 1, QRhiVertexInputAttribute::Float2, 0 },
            { 0, 1, QRhiVertexInputAttribute::Float3, 0 }
        });
        QRhiResourceUpdateBatch *resourceUpdates = m_rhi->nextResourceUpdateBatch();
        resourceUpdates->uploadStaticBuffer(m_vbuf.get(), m_positions.constData());
        cb->resourceUpdate(resourceUpdates);

        m_pipeline->setSampleCount(m_sampleCount);
        m_pipeline->setVertexInputLayout(inputLayout);
        m_pipeline->setShaderResourceBindings(m_srb.get());
        m_pipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        m_pipeline->create();
    }

    const QSize outputSize = renderTarget()->pixelSize();
    m_viewMatrix.setToIdentity();
    m_viewMatrix.translate(0.0f, 0.0f, -5.0f); // Basic camera view

    m_projMatrix = m_rhi->clipSpaceCorrMatrix();
    m_projMatrix.perspective(45.0f, outputSize.width() / (float) outputSize.height(), 0.1f, 1000.0f);
    //![2]
}

void RhiQmlItemRenderer::render(QRhiCommandBuffer *cb)
{
    QRhiResourceUpdateBatch *resourceUpdates = m_rhi->nextResourceUpdateBatch();

    m_modelMatrix.setToIdentity();
    m_modelMatrix.rotate(m_rot, 0.0f, 1.0f, 0.0f);
    //m_modelMatrix.rotate(m_rot, 1.0f, 0.0f, 0.0f);

    //qDebug() << "RHI matrix" << m_projMatrix * m_viewMatrix * m_modelMatrix;

    float mvpValues[16 * 3];
    memcpy(&mvpValues[0], m_modelMatrix.data(), 16 * sizeof(float));
    memcpy(&mvpValues[16], m_viewMatrix.data(), 16 * sizeof(float));
    memcpy(&mvpValues[32], m_projMatrix.data(), 16 * sizeof(float));

    resourceUpdates->updateDynamicBuffer(m_mvpUniformBuffer.get(), 0, 3 * sizeof(QMatrix4x4), mvpValues);

    // apply light: green color and medium intensity
    float lightData[7] = {
        0.3, 0.3, 0.3,  // ambientColor
        0.5,            // ambientIntensity
        0.0, 4.0, 0.0   // lightPosition
    };
    resourceUpdates->updateDynamicBuffer(m_lightUniformBuffer.get(), 0, 7 * sizeof(float), &lightData);

    cb->beginPass(renderTarget(), Qt::black, { 1.0f, 0 }, resourceUpdates);

    cb->setGraphicsPipeline(m_pipeline.get());
    const QSize outputSize = renderTarget()->pixelSize();
    cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));
    cb->setShaderResources();

    const QRhiCommandBuffer::VertexInput vbufBindings[] = {
        { m_vbuf.get(), 0 },
        { m_vbuf.get(), quint32((m_positions.length() / 2) * 3 * sizeof(float)) }
    };
    cb->setVertexInput(0, 2, vbufBindings);

    cb->draw(m_positions.length() / 2);

    cb->endPass();
}
