#pragma once

#include <QtCore/QVector>
#include <QtCore/QString>
#include <QtGui/QMatrix4x4>
#include <QtGui/QVector2D>
#include <QtGui/QVector3D>
#include <rhi/qrhi.h>

#include "scene/Material.h"

struct Vertex
{
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

struct Mesh
{
    QString name;
    QVector<Vertex> vertices;
    QVector<quint32> indices;
    QRhiBuffer *vertexBuffer = nullptr;
    QRhiBuffer *indexBuffer = nullptr;
    QRhiTexture *baseColorTexture = nullptr;
    QRhiTexture *normalTexture = nullptr;
    QRhiTexture *metallicRoughnessTexture = nullptr;
    QRhiSampler *baseColorSampler = nullptr;
    QRhiSampler *normalSampler = nullptr;
    QRhiSampler *metallicRoughnessSampler = nullptr;
    QRhiBuffer *modelUbo = nullptr;
    QRhiBuffer *materialUbo = nullptr;
    QRhiShaderResourceBindings *srb = nullptr;
    QRhiShaderResourceBindings *gizmoSrb = nullptr;
    QRhiShaderResourceBindings *shadowSrb = nullptr;
    QVector<QRhiShaderResourceBindings *> spotShadowSrbs;
    int indexCount = 0;
    QMatrix4x4 baseModelMatrix;
    QMatrix4x4 modelMatrix;
    QVector3D userOffset = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D boundsMin = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D boundsMax = QVector3D(0.0f, 0.0f, 0.0f);
    bool boundsValid = false;
    bool selected = false;
    bool selectable = true;
    bool visible = true;
    int gizmoAxis = -1;
    int gizmoType = 0;
    Material material;
};
