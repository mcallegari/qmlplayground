#pragma once

#include <QtGui/QVector3D>
#include <QtCore/QPointF>
#include <limits>

class Scene;
class QRhi;

namespace RhiQmlUtils
{

struct PickHit
{
    int meshIndex = -1;
    QVector3D worldPos;
    float distance = std::numeric_limits<float>::max();
};

enum class PickFilter
{
    All,
    SelectableOnly,
    GizmosOnly
};

bool computeRay(const Scene &scene, QRhi *rhi, const QPointF &normPos,
                QVector3D &origin, QVector3D &dir);
bool rayAabbIntersection(const QVector3D &origin, const QVector3D &dir,
                         const QVector3D &minV, const QVector3D &maxV,
                         float &tNear, float &tFar);
bool rayTriangleIntersection(const QVector3D &origin, const QVector3D &dir,
                             const QVector3D &v0, const QVector3D &v1, const QVector3D &v2,
                             float &tOut);
float closestAxisT(const QVector3D &rayOrigin, const QVector3D &rayDir,
                   const QVector3D &axisOrigin, const QVector3D &axisDir);
bool pickSceneMesh(const Scene &scene, QRhi *rhi, const QPointF &normPos,
                   PickFilter filter, PickHit &hit);
bool rayPlaneIntersection(const QVector3D &rayOrigin, const QVector3D &rayDir,
                          const QVector3D &planeOrigin, const QVector3D &planeNormal,
                          QVector3D &hit);

}
