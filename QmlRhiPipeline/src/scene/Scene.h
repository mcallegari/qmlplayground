#pragma once

#include <QtCore/QVector>
#include <QtCore/QString>
#include <QtGui/QVector3D>
#include <QtGui/QVector2D>

#include "scene/Camera.h"
#include "scene/Mesh.h"

struct Light
{
    enum class Type
    {
        Directional,
        Point,
        Spot,
        Area
    };

    Type type = Type::Point;
    QVector3D color = QVector3D(1.0f, 1.0f, 1.0f);
    float intensity = 1.0f;
    QVector3D position;
    QVector3D direction;
    float range = 10.0f;
    float innerCone = 0.5f;
    float outerCone = 0.7f;
    QVector2D areaSize = QVector2D(1.0f, 1.0f);
    bool castShadows = true;
    int qualitySteps = 8;
    QString goboPath;
};

class Scene
{
public:
    Camera &camera()
    {
        return m_camera;
    }
    const Camera &camera() const
    {
        return m_camera;
    }

    QVector<Mesh> &meshes()
    {
        return m_meshes;
    }
    const QVector<Mesh> &meshes() const
    {
        return m_meshes;
    }

    QVector<Light> &lights()
    {
        return m_lights;
    }
    const QVector<Light> &lights() const
    {
        return m_lights;
    }

    QVector3D ambientLight() const
    {
        return m_ambientLight;
    }
    void setAmbientLight(const QVector3D &ambient)
    {
        m_ambientLight = ambient;
    }
    float ambientIntensity() const
    {
        return m_ambientIntensity;
    }
    void setAmbientIntensity(float intensity)
    {
        m_ambientIntensity = intensity;
    }

private:
    Camera m_camera;
    QVector<Mesh> m_meshes;
    QVector<Light> m_lights;
    QVector3D m_ambientLight = QVector3D(0.0f, 0.0f, 0.0f);
    float m_ambientIntensity = 1.0f;
};
