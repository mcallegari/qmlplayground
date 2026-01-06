#include "renderer/DeferredRenderer.h"

#include "core/RhiContext.h"
#include "core/RenderTargetCache.h"
#include "core/ShaderManager.h"
#include "renderer/PassDepth.h"
#include "renderer/PassGBuffer.h"
#include "renderer/PassLightCulling.h"
#include "renderer/PassLighting.h"
#include "renderer/PassPost.h"
#include "renderer/PassShadow.h"
#include "scene/Scene.h"
#include <QtCore/QDebug>

DeferredRenderer::DeferredRenderer() = default;

void DeferredRenderer::initialize(RhiContext *rhi, RenderTargetCache *targets, ShaderManager *shaders)
{
    m_frameCtx.rhi = rhi;
    m_frameCtx.targets = targets;
    m_frameCtx.shaders = shaders;
    m_frameCtx.shadows = &m_shadowData;

    m_graph.clear();
    m_graph.addPass(std::make_unique<PassDepth>());
    m_graph.addPass(std::make_unique<PassGBuffer>());
    m_graph.addPass(std::make_unique<PassShadow>());
    m_graph.addPass(std::make_unique<PassLightCulling>());
    m_graph.addPass(std::make_unique<PassLighting>());
    m_graph.addPass(std::make_unique<PassPost>());
    qDebug() << "DeferredRenderer: initialized passes with shadows";
}

void DeferredRenderer::resize(const QSize &size)
{
    Q_UNUSED(size);
    // TODO: notify passes and target cache.
}

void DeferredRenderer::render(Scene *scene)
{
    m_frameCtx.scene = scene;
    m_graph.run(m_frameCtx);
}
