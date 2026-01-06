#pragma once

#include <QtGui/QVector3D>

struct Material
{
    QVector3D baseColor = QVector3D(1.0f, 1.0f, 1.0f);
    float metalness = 0.0f;
    float roughness = 0.5f;
    float occlusion = 1.0f;
    QVector3D emissive = QVector3D(0.0f, 0.0f, 0.0f);
};
