#pragma once

#include <memory>

#include "core/RenderGraph.h"

class RhiContext;
class RenderTargetCache;
class ShaderManager;
class Scene;

class DeferredRenderer
{
public:
    DeferredRenderer();

    void initialize(RhiContext *rhi, RenderTargetCache *targets, ShaderManager *shaders);
    void resize(const QSize &size);
    void render(Scene *scene);

private:
    RenderGraph m_graph;
    FrameContext m_frameCtx;
    ShadowData m_shadowData;
};
