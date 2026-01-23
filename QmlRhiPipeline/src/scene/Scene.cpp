#include "scene/Scene.h"

namespace {

bool lightEquals(const Light &a, const Light &b)
{
    return a.type == b.type
            && a.color == b.color
            && a.intensity == b.intensity
            && a.position == b.position
            && a.direction == b.direction
            && a.range == b.range
            && a.innerCone == b.innerCone
            && a.outerCone == b.outerCone
            && a.areaSize == b.areaSize
            && a.castShadows == b.castShadows
            && a.qualitySteps == b.qualitySteps
            && a.goboPath == b.goboPath
            && a.beamRadius == b.beamRadius
            && a.beamShape == b.beamShape;
}

} // namespace

bool Scene::setLights(const QVector<Light> &lights)
{
    if (m_lights.size() == lights.size())
    {
        bool same = true;
        for (int i = 0; i < lights.size(); ++i)
        {
            if (!lightEquals(m_lights[i], lights[i]))
            {
                same = false;
                break;
            }
        }
        if (same)
            return false;
    }
    m_lights = lights;
    m_lightsDirty = true;
    return true;
}
