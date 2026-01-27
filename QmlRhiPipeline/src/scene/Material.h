#pragma once

#include <QtCore/QString>
#include <QtGui/QImage>
#include <QtGui/QVector3D>

struct Material
{
    QVector3D baseColor = QVector3D(1.0f, 1.0f, 1.0f);
    float baseAlpha = 1.0f;
    float metalness = 0.0f;
    float roughness = 0.5f;
    float occlusion = 1.0f;
    QVector3D emissive = QVector3D(0.0f, 0.0f, 0.0f);
    float alphaCutoff = 0.5f;
    enum class AlphaMode
    {
        Opaque = 0,
        Mask = 1,
        Blend = 2
    };
    AlphaMode alphaMode = AlphaMode::Opaque;
    bool doubleSided = false;
    QString baseColorMapPath;
    QImage baseColorMap;
    QString normalMapPath;
    QImage normalMap;
    QString metallicRoughnessMapPath;
    QImage metallicRoughnessMap;
    QString occlusionMapPath;
    QImage occlusionMap;
    QString emissiveMapPath;
    QImage emissiveMap;
};
