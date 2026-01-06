#pragma once

#include <QtCore/QString>

#include "scene/Scene.h"

class AssimpLoader
{
public:
    bool loadModel(const QString &path, Scene &scene);
    bool loadModel(const QString &path, Scene &scene, bool append);
};
