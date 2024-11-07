
#ifndef RHIQMLITEM_H
#define RHIQMLITEM_H

#include <QQuickRhiItem>
#include <rhi/qrhi.h>

class RhiQmlItemRenderer : public QQuickRhiItemRenderer
{
public:
    void initialize(QRhiCommandBuffer *cb) override;
    void synchronize(QQuickRhiItem *item) override;
    void render(QRhiCommandBuffer *cb) override;

private:
    QRhi *m_rhi = nullptr;
    int m_sampleCount = 1;

    std::unique_ptr<QRhiBuffer> m_vbuf;
    std::unique_ptr<QRhiBuffer> m_mvpUniformBuffer;
    std::unique_ptr<QRhiBuffer> m_lightUniformBuffer;
    std::unique_ptr<QRhiShaderResourceBindings> m_srb;
    std::unique_ptr<QRhiGraphicsPipeline> m_pipeline;

    QMatrix4x4 m_modelMatrix;
    QMatrix4x4 m_viewMatrix;
    QMatrix4x4 m_projMatrix;

    float m_rot = 0.0f;
    float m_int = 1.0f;
};

class RhiQmlItem : public QQuickRhiItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(RhiQmlItem)
    Q_PROPERTY(float rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(float lightIntensity READ lightIntensity WRITE setLightIntensity NOTIFY lightIntensityChanged)

public:
    QQuickRhiItemRenderer *createRenderer() override;

    float rotation() const { return m_rotation; }
    void setRotation(float rotation);

    float lightIntensity() const { return m_lightIntensity; }
    void setLightIntensity(float intensity);

signals:
    void rotationChanged();
    void lightIntensityChanged();

private:
    float m_rotation = 0.0f;
    float m_lightIntensity = 1.0f;
};

#endif 
