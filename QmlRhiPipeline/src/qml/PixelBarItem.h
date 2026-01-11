#pragma once

#include "qml/MeshItem.h"

#include <QtCore/QVariant>

class PixelBarItem : public MeshItem
{
    Q_OBJECT
    Q_PROPERTY(int emitterCount READ emitterCount WRITE setEmitterCount NOTIFY emitterCountChanged)
    Q_PROPERTY(QVector3D baseColor READ baseColor WRITE setBaseColor NOTIFY baseColorChanged)
    Q_PROPERTY(QVector3D emissiveColor READ emissiveColor WRITE setEmissiveColor NOTIFY emissiveColorChanged)
    Q_PROPERTY(QVariantList emitterColors READ emitterColors WRITE setEmitterColors NOTIFY emitterColorsChanged)
    Q_PROPERTY(QVariantList emitterIntensities READ emitterIntensities WRITE setEmitterIntensities NOTIFY emitterIntensitiesChanged)

public:
    explicit PixelBarItem(QObject *parent = nullptr);

    MeshType type() const override { return MeshType::PixelBar; }

    int emitterCount() const { return m_emitterCount; }
    void setEmitterCount(int count);

    QVector3D baseColor() const { return m_baseColor; }
    void setBaseColor(const QVector3D &color);

    QVector3D emissiveColor() const { return m_emissiveColor; }
    void setEmissiveColor(const QVector3D &color);

    QVariantList emitterColors() const;
    void setEmitterColors(const QVariantList &colors);
    const QVector<QVector3D> &emitterColorsVector() const { return m_emitterColors; }

    QVariantList emitterIntensities() const;
    void setEmitterIntensities(const QVariantList &intensities);
    const QVector<float> &emitterIntensitiesVector() const { return m_emitterIntensities; }

Q_SIGNALS:
    void emitterCountChanged();
    void baseColorChanged();
    void emissiveColorChanged();
    void emitterColorsChanged();
    void emitterIntensitiesChanged();

private:
    int m_emitterCount = 1;
    QVector3D m_baseColor = QVector3D(0.08f, 0.08f, 0.08f);
    QVector3D m_emissiveColor = QVector3D(3.0f, 0.6f, 0.2f);
    QVector<QVector3D> m_emitterColors;
    QVector<float> m_emitterIntensities;
};
