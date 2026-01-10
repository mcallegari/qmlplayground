#pragma once

#include <QtGui/QMatrix4x4>
#include <QtGui/QVector4D>
#include <memory>
#include <vector>

class RhiContext;
class RenderTargetCache;
class ShaderManager;
class Scene;
class QRhiTexture;
class QRhiBuffer;

inline constexpr int kMaxLights = 32;
inline constexpr int kMaxSpotShadows = 7;

struct ShadowData
{
    int cascadeCount = 0;
    QVector4D splits;
    QVector4D dirLightDir;
    QVector4D dirLightColorIntensity;
    QMatrix4x4 lightViewProj[3];
    QRhiTexture *shadowMaps[3] = { nullptr, nullptr, nullptr };
    QMatrix4x4 spotLightViewProj[kMaxLights];
    QVector4D spotShadowParams[kMaxLights];
    int spotShadowCount = 0;
    QRhiTexture *spotShadowMaps[kMaxSpotShadows] = { nullptr };
    QVector4D shadowDepthParams;
};

struct LightCullingData
{
    QRhiBuffer *tileLightIndexBuffer = nullptr;
    int tileCountX = 0;
    int tileCountY = 0;
    int tileSize = 16;
    bool enabled = false;
};

struct FrameContext
{
    RhiContext *rhi = nullptr;
    RenderTargetCache *targets = nullptr;
    ShaderManager *shaders = nullptr;
    Scene *scene = nullptr;
    ShadowData *shadows = nullptr;
    LightCullingData *lightCulling = nullptr;
    bool lightingEnabled = true;
};

class RenderPass
{
public:
    virtual ~RenderPass() = default;
    virtual void prepare(FrameContext &ctx) = 0;
    virtual void execute(FrameContext &ctx) = 0;
};

class RenderGraph
{
public:
    void addPass(std::unique_ptr<RenderPass> pass);
    void clear();
    void run(FrameContext &ctx);

private:
    std::vector<std::unique_ptr<RenderPass>> m_passes;
};
