#pragma once

#include "core/RenderGraph.h"

class PassPost final : public RenderPass
{
public:
    void prepare(FrameContext &ctx) override;
    void execute(FrameContext &ctx) override;
};
