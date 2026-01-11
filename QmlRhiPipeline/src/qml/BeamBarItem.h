#pragma once

#include "qml/MeshItem.h"
#include "scene/Scene.h"

#include <QtCore/QVariant>

class BeamBarItem : public MeshItem
{
    Q_OBJECT
    Q_PROPERTY(int emitterCount READ emitterCount WRITE setEmitterCount NOTIFY emitterCountChanged)
    Q_PROPERTY(QVector3D baseColor READ baseColor WRITE setBaseColor NOTIFY baseColorChanged)
    Q_PROPERTY(QVector3D color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(float intensity READ intensity WRITE setIntensity NOTIFY intensityChanged)
    Q_PROPERTY(float range READ range WRITE setRange NOTIFY rangeChanged)
    Q_PROPERTY(float beamRadius READ beamRadius WRITE setBeamRadius NOTIFY beamRadiusChanged)
    Q_PROPERTY(bool castShadows READ castShadows WRITE setCastShadows NOTIFY castShadowsChanged)
    Q_PROPERTY(QVariantList emitterColors READ emitterColors WRITE setEmitterColors NOTIFY emitterColorsChanged)
    Q_PROPERTY(QVariantList emitterIntensities READ emitterIntensities WRITE setEmitterIntensities NOTIFY emitterIntensitiesChanged)

public:
    explicit BeamBarItem(QObject *parent = nullptr);

    MeshType type() const override { return MeshType::BeamBar; }

    int emitterCount() const { return m_emitterCount; }
    void setEmitterCount(int count);

    QVector3D baseColor() const { return m_baseColor; }
    void setBaseColor(const QVector3D &color);

    QVector3D color() const { return m_color; }
    void setColor(const QVector3D &color);

    float intensity() const { return m_intensity; }
    void setIntensity(float intensity);

    float range() const { return m_range; }
    void setRange(float range);

    float beamRadius() const { return m_beamRadius; }
    void setBeamRadius(float radius);

    bool castShadows() const { return m_castShadows; }
    void setCastShadows(bool cast);

    QVariantList emitterColors() const;
    void setEmitterColors(const QVariantList &colors);
    const QVector<QVector3D> &emitterColorsVector() const { return m_emitterColors; }

    QVariantList emitterIntensities() const;
    void setEmitterIntensities(const QVariantList &intensities);
    const QVector<float> &emitterIntensitiesVector() const { return m_emitterIntensities; }

    Light toLight() const;

Q_SIGNALS:
    void emitterCountChanged();
    void baseColorChanged();
    void colorChanged();
    void intensityChanged();
    void rangeChanged();
    void beamRadiusChanged();
    void castShadowsChanged();
    void emitterColorsChanged();
    void emitterIntensitiesChanged();

private:
    int m_emitterCount = 1;
    QVector3D m_baseColor = QVector3D(0.06f, 0.06f, 0.06f);
    QVector3D m_color = QVector3D(0, 1.0f, 1.0f);
    float m_intensity = 1.0f;
    float m_range = 30.0f;
    float m_beamRadius = 0.05f;
    bool m_castShadows = false;
    QVector<QVector3D> m_emitterColors;
    QVector<float> m_emitterIntensities;
};
