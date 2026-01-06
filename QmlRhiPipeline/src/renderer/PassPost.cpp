#include "renderer/PassPost.h"

void PassPost::prepare(FrameContext &ctx)
{
    Q_UNUSED(ctx);
    // TODO: setup tonemapping pipeline.
}

void PassPost::execute(FrameContext &ctx)
{
    Q_UNUSED(ctx);
    // TODO: draw fullscreen tonemapping to swapchain.
}
