#include "renderer/PassLightCulling.h"

void PassLightCulling::prepare(FrameContext &ctx)
{
    Q_UNUSED(ctx);
    // TODO: create compute pipeline and buffers for tiled/clustered lists.
}

void PassLightCulling::execute(FrameContext &ctx)
{
    Q_UNUSED(ctx);
    // TODO: dispatch light culling compute pass.
}
