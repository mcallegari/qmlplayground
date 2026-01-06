#include "core/RhiContext.h"

#include <QtCore/QDebug>
#include <QtCore/QByteArray>
#include <QtCore/QVector>
#include <QtGui/QOpenGLFunctions>

RhiContext::RhiContext() = default;

bool RhiContext::initialize(QWindow *window)
{
    m_window = window;
    if (!m_window)
        return false;

    struct BackendAttempt {
        QRhi::Implementation impl;
        const char *name;
    };

#if defined(Q_OS_MACOS)
    const BackendAttempt attempts[] = {
        { QRhi::Metal, "Metal" },
        { QRhi::OpenGLES2, "OpenGLES2" }
    };
#elif defined(Q_OS_WIN)
    const BackendAttempt attempts[] = {
        { QRhi::D3D11, "D3D11" },
        { QRhi::Vulkan, "Vulkan" },
        { QRhi::OpenGLES2, "OpenGLES2" }
    };
#else
    const BackendAttempt defaultAttempts[] = {
        { QRhi::Vulkan, "Vulkan" },
        { QRhi::OpenGLES2, "OpenGLES2" }
    };
#endif

    QVector<BackendAttempt> attempts;
    const QByteArray backendOverride = qgetenv("RHIPIPELINE_BACKEND").toLower();
    if (backendOverride == "vulkan") {
        attempts.push_back({ QRhi::Vulkan, "Vulkan" });
    } else if (backendOverride == "opengl" || backendOverride == "gles2") {
        attempts.push_back({ QRhi::OpenGLES2, "OpenGLES2" });
    } else {
#if defined(Q_OS_MACOS)
        attempts = { { QRhi::Metal, "Metal" }, { QRhi::OpenGLES2, "OpenGLES2" } };
#elif defined(Q_OS_WIN)
        attempts = { { QRhi::D3D11, "D3D11" }, { QRhi::Vulkan, "Vulkan" }, { QRhi::OpenGLES2, "OpenGLES2" } };
#else
        for (const BackendAttempt &attempt : defaultAttempts)
            attempts.push_back(attempt);
#endif
    }

    for (const BackendAttempt &attempt : attempts) {
        qDebug() << "RhiContext: trying backend" << attempt.name;
        if (attempt.impl == QRhi::Vulkan) {
#if QT_CONFIG(vulkan)
            m_window->setSurfaceType(QSurface::VulkanSurface);
            m_vkInstance.setApiVersion(QVersionNumber(1, 1, 0));
            if (!m_vkInstance.create())
                continue;
            m_window->setVulkanInstance(&m_vkInstance);
            QRhiVulkanInitParams vkParams;
            vkParams.inst = &m_vkInstance;
            vkParams.window = m_window;
            m_rhi = QRhi::create(attempt.impl, &vkParams, QRhi::Flags());
#else
            continue;
#endif
        } else if (attempt.impl == QRhi::Metal) {
#if QT_CONFIG(metal)
            m_window->setSurfaceType(QSurface::MetalSurface);
            QRhiMetalInitParams metalParams;
            m_rhi = QRhi::create(attempt.impl, &metalParams, QRhi::Flags());
#else
            continue;
#endif
        } else if (attempt.impl == QRhi::D3D11) {
#if defined(Q_OS_WIN)
            m_window->setSurfaceType(QSurface::RasterSurface);
            QRhiD3D11InitParams d3dParams;
            m_rhi = QRhi::create(attempt.impl, &d3dParams, QRhi::Flags());
#else
            continue;
#endif
        } else if (attempt.impl == QRhi::OpenGLES2) {
#if QT_CONFIG(opengl)
            m_window->setSurfaceType(QSurface::OpenGLSurface);
            if (!m_glContext) {
                m_glContext = new QOpenGLContext();
                QSurfaceFormat fmt;
                fmt.setMajorVersion(3);
                fmt.setMinorVersion(3);
                fmt.setProfile(QSurfaceFormat::CoreProfile);
                m_glContext->setFormat(fmt);
                m_window->setFormat(fmt);
                if (!m_glContext->create()) {
                    delete m_glContext;
                    m_glContext = nullptr;
                    continue;
                }
                m_glOffscreenSurface = new QOffscreenSurface();
                m_glOffscreenSurface->setFormat(m_glContext->format());
                m_glOffscreenSurface->create();
            }
            QRhiGles2InitParams glParams;
            glParams.window = m_window;
            glParams.shareContext = m_glContext;
            glParams.fallbackSurface = m_glOffscreenSurface;
            glParams.format = m_glContext->format();
            m_rhi = QRhi::create(attempt.impl, &glParams, QRhi::Flags());
#else
            continue;
#endif
        }

        if (!m_rhi)
            continue;

        m_backend = attempt.impl;
        qDebug() << "RhiContext: backend created" << attempt.name;
        if (m_backend == QRhi::OpenGLES2)
            qDebug() << "RhiContext: OpenGLES2 backend uses OpenGL (desktop core) when available";

        const QRhiDriverInfo info = m_rhi->driverInfo();
        qDebug() << "RhiContext: driver device"
                 << info.deviceName
                 << "vendorId" << QString::number(info.vendorId, 16)
                 << "deviceId" << QString::number(info.deviceId, 16)
                 << "type" << info.deviceType;

        if (m_backend == QRhi::OpenGLES2 && m_glContext && m_glOffscreenSurface) {
            if (m_glContext->makeCurrent(m_glOffscreenSurface)) {
                QOpenGLFunctions *f = m_glContext->functions();
                const char *vendor = reinterpret_cast<const char *>(f->glGetString(GL_VENDOR));
                const char *renderer = reinterpret_cast<const char *>(f->glGetString(GL_RENDERER));
                const char *version = reinterpret_cast<const char *>(f->glGetString(GL_VERSION));
                qDebug() << "RhiContext: OpenGL vendor" << (vendor ? vendor : "unknown");
                qDebug() << "RhiContext: OpenGL renderer" << (renderer ? renderer : "unknown");
                qDebug() << "RhiContext: OpenGL version" << (version ? version : "unknown");
                m_glContext->doneCurrent();
            } else {
                qWarning() << "RhiContext: failed to make GL context current for driver info";
            }
        }
        break;
    }

    if (!m_rhi)
        return false;

    m_ownsRhi = true;
    m_swapChain = m_rhi->newSwapChain();
    m_swapChain->setWindow(m_window);
    m_swapChain->setSampleCount(1);

    const QSize surfaceSize = m_swapChain->surfacePixelSize();
    m_swapChainDepthStencil = m_rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil,
                                                     surfaceSize,
                                                     m_swapChain->sampleCount());
    if (!m_swapChainDepthStencil->create())
        return false;

    m_swapChain->setDepthStencil(m_swapChainDepthStencil);
    m_swapChainRpDesc = m_swapChain->newCompatibleRenderPassDescriptor();
    m_swapChain->setRenderPassDescriptor(m_swapChainRpDesc);
    if (!m_swapChain->createOrResize())
        return false;

    m_swapChainSize = m_swapChain->currentPixelSize();
    return true;
}

bool RhiContext::initializeExternal(QRhi *rhi)
{
    if (!rhi)
        return false;
    m_rhi = rhi;
    m_backend = rhi->backend();
    m_ownsRhi = false;
    return true;
}

void RhiContext::shutdown()
{
    delete m_swapChainDepthStencil;
    m_swapChainDepthStencil = nullptr;
    delete m_swapChainRpDesc;
    m_swapChainRpDesc = nullptr;
    delete m_swapChain;
    m_swapChain = nullptr;
    if (m_ownsRhi) {
        delete m_rhi;
        m_rhi = nullptr;
    }
    delete m_glOffscreenSurface;
    m_glOffscreenSurface = nullptr;
    delete m_glContext;
    m_glContext = nullptr;
    m_externalCb = nullptr;
    m_externalRt = nullptr;
}

void RhiContext::beginFrame()
{
    if (!m_rhi || !m_swapChain)
        return;

    const QRhi::FrameOpResult res = m_rhi->beginFrame(m_swapChain);
    if (res != QRhi::FrameOpSuccess) {
        qWarning() << "RhiContext: beginFrame failed with" << res;
        return;
    }

    m_cb = m_swapChain->currentFrameCommandBuffer();
}

void RhiContext::endFrame()
{
    if (!m_rhi || !m_swapChain)
        return;
    if (!m_cb)
        return;

    m_rhi->endFrame(m_swapChain);
    m_cb = nullptr;
}

void RhiContext::resize(const QSize &size)
{
    Q_UNUSED(size);
    if (!m_swapChain || !m_swapChainDepthStencil)
        return;

    const QSize surfaceSize = m_swapChain->surfacePixelSize();
    if (surfaceSize == m_swapChainSize)
        return;

    m_swapChainDepthStencil->setPixelSize(surfaceSize);
    m_swapChainDepthStencil->create();
    m_swapChain->createOrResize();
    m_swapChainSize = m_swapChain->currentPixelSize();
}

QRhiRenderTarget *RhiContext::swapchainRenderTarget() const
{
    if (m_externalRt)
        return m_externalRt;
    return m_swapChain ? m_swapChain->currentFrameRenderTarget() : nullptr;
}

void RhiContext::setExternalFrame(QRhiCommandBuffer *cb, QRhiRenderTarget *rt)
{
    m_externalCb = cb;
    m_externalRt = rt;
}

void RhiContext::clearExternalFrame()
{
    m_externalCb = nullptr;
    m_externalRt = nullptr;
}
