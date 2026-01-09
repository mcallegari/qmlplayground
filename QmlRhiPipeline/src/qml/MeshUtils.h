#pragma once

#include "scene/Mesh.h"

namespace RhiQmlUtils
{

Mesh createUnitCubeMesh();
Mesh createSphereMesh(float radius = 0.5f, int rings = 16, int sectors = 24);
Mesh createArcMesh(float majorRadius, float tubeRadius,
                   float startAngle, float endAngle,
                   int segments, int sides);

}
