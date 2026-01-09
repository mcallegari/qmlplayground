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
    enum class BeamShapeType
    {
        ConeShape,
        BeamShape
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
    float beamRadius = 0.15f;
    BeamShapeType beamShape = BeamShapeType::ConeShape;
};

class Scene
{
public:
    enum class BeamModel
    {
        SoftHaze,
        Physical
    };
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
    float smokeAmount() const
    {
        return m_smokeAmount;
    }
    void setSmokeAmount(float amount)
    {
        m_smokeAmount = amount;
    }
    BeamModel beamModel() const
    {
        return m_beamModel;
    }
    void setBeamModel(BeamModel mode)
    {
        m_beamModel = mode;
    }
    QVector3D hazePosition() const
    {
        return m_hazePosition;
    }
    void setHazePosition(const QVector3D &position)
    {
        m_hazePosition = position;
    }
    QVector3D hazeDirection() const
    {
        return m_hazeDirection;
    }
    void setHazeDirection(const QVector3D &direction)
    {
        m_hazeDirection = direction;
    }
    float hazeLength() const
    {
        return m_hazeLength;
    }
    void setHazeLength(float length)
    {
        m_hazeLength = length;
    }
    float hazeRadius() const
    {
        return m_hazeRadius;
    }
    void setHazeRadius(float radius)
    {
        m_hazeRadius = radius;
    }
    float hazeDensity() const
    {
        return m_hazeDensity;
    }
    void setHazeDensity(float density)
    {
        m_hazeDensity = density;
    }
    bool hazeEnabled() const
    {
        return m_hazeEnabled;
    }
    void setHazeEnabled(bool enabled)
    {
        m_hazeEnabled = enabled;
    }
    float bloomIntensity() const
    {
        return m_bloomIntensity;
    }
    void setBloomIntensity(float intensity)
    {
        m_bloomIntensity = intensity;
    }
    float bloomRadius() const
    {
        return m_bloomRadius;
    }
    void setBloomRadius(float radius)
    {
        m_bloomRadius = radius;
    }
    bool volumetricEnabled() const
    {
        return m_volumetricEnabled;
    }
    void setVolumetricEnabled(bool enabled)
    {
        m_volumetricEnabled = enabled;
    }
    bool shadowsEnabled() const
    {
        return m_shadowsEnabled;
    }
    void setShadowsEnabled(bool enabled)
    {
        m_shadowsEnabled = enabled;
    }
    bool smokeNoiseEnabled() const
    {
        return m_smokeNoiseEnabled;
    }
    void setSmokeNoiseEnabled(bool enabled)
    {
        m_smokeNoiseEnabled = enabled;
    }
    float timeSeconds() const
    {
        return m_timeSeconds;
    }
    void setTimeSeconds(float seconds)
    {
        m_timeSeconds = seconds;
    }

private:
    Camera m_camera;
    QVector<Mesh> m_meshes;
    QVector<Light> m_lights;
    QVector3D m_ambientLight = QVector3D(0.0f, 0.0f, 0.0f);
    float m_ambientIntensity = 1.0f;
    float m_smokeAmount = 0.0f;
    BeamModel m_beamModel = BeamModel::SoftHaze;
    float m_bloomIntensity = 0.0f;
    float m_bloomRadius = 4.0f;
    float m_timeSeconds = 0.0f;
    bool m_volumetricEnabled = true;
    bool m_shadowsEnabled = true;
    bool m_smokeNoiseEnabled = true;
    QVector3D m_hazePosition = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D m_hazeDirection = QVector3D(0.0f, 1.0f, 0.0f);
    float m_hazeLength = 3.0f;
    float m_hazeRadius = 1.0f;
    float m_hazeDensity = 0.0f;
    bool m_hazeEnabled = false;
};
