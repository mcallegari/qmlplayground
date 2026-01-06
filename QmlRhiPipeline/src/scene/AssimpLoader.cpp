#include "scene/AssimpLoader.h"

#ifdef RHIPIPELINE_NO_ASSIMP
bool AssimpLoader::loadModel(const QString &path, Scene &scene)
{
    Q_UNUSED(path);
    Q_UNUSED(scene);
    return false;
}

bool AssimpLoader::loadModel(const QString &path, Scene &scene, bool append)
{
    Q_UNUSED(path);
    Q_UNUSED(scene);
    Q_UNUSED(append);
    return false;
}
#else
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <functional>
#include <QtGui/QMatrix4x4>
#include <QtGui/QVector3D>
#include <QtGui/QVector2D>

static QMatrix4x4 toQtMatrix(const aiMatrix4x4 &m)
{
    QMatrix4x4 out;
    out.setRow(0, QVector4D(m.a1, m.a2, m.a3, m.a4));
    out.setRow(1, QVector4D(m.b1, m.b2, m.b3, m.b4));
    out.setRow(2, QVector4D(m.c1, m.c2, m.c3, m.c4));
    out.setRow(3, QVector4D(m.d1, m.d2, m.d3, m.d4));
    return out;
}

static void readMaterial(const aiMaterial *mat, Material &out)
{
    aiColor4D color;
    if (aiGetMaterialColor(mat, AI_MATKEY_BASE_COLOR, &color) == AI_SUCCESS)
        out.baseColor = QVector3D(color.r, color.g, color.b);
    else if (aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS)
        out.baseColor = QVector3D(color.r, color.g, color.b);

    float metallic = 0.0f;
    if (aiGetMaterialFloat(mat, AI_MATKEY_METALLIC_FACTOR, &metallic) == AI_SUCCESS)
        out.metalness = metallic;

    float roughness = 0.5f;
    if (aiGetMaterialFloat(mat, AI_MATKEY_ROUGHNESS_FACTOR, &roughness) == AI_SUCCESS)
        out.roughness = roughness;

    if (aiGetMaterialColor(mat, AI_MATKEY_COLOR_EMISSIVE, &color) == AI_SUCCESS)
        out.emissive = QVector3D(color.r, color.g, color.b);
}

bool AssimpLoader::loadModel(const QString &path, Scene &scene)
{
    return loadModel(path, scene, false);
}

bool AssimpLoader::loadModel(const QString &path, Scene &scene, bool append)
{
    if (!append)
        scene.meshes().clear();

    Assimp::Importer importer;
    const aiScene *ai = importer.ReadFile(path.toStdString(),
                                         aiProcess_Triangulate |
                                         aiProcess_GenNormals |
                                         aiProcess_CalcTangentSpace |
                                         aiProcess_JoinIdenticalVertices);
    if (!ai)
        return false;

    std::function<void(aiNode *, const QMatrix4x4 &)> visitNode;
    visitNode = [&](aiNode *node, const QMatrix4x4 &parent) {
        const QMatrix4x4 local = toQtMatrix(node->mTransformation);
        const QMatrix4x4 world = parent * local;

        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            const aiMesh *m = ai->mMeshes[node->mMeshes[i]];
            Mesh mesh;
            mesh.modelMatrix = world;
            mesh.vertices.reserve(int(m->mNumVertices));
            mesh.indices.reserve(int(m->mNumFaces * 3));
            QVector3D minV;
            QVector3D maxV;
            bool haveBounds = false;

            for (unsigned int v = 0; v < m->mNumVertices; ++v) {
                const aiVector3D pos = m->mVertices[v];
                const aiVector3D nrm = m->HasNormals() ? m->mNormals[v] : aiVector3D(0.0f, 1.0f, 0.0f);
                const aiVector3D uv = m->HasTextureCoords(0) ? m->mTextureCoords[0][v] : aiVector3D(0.0f, 0.0f, 0.0f);
                Vertex vert;
                vert.px = pos.x;
                vert.py = pos.y;
                vert.pz = pos.z;
                vert.nx = nrm.x;
                vert.ny = nrm.y;
                vert.nz = nrm.z;
                vert.u = uv.x;
                vert.v = uv.y;
                mesh.vertices.push_back(vert);

                if (!haveBounds) {
                    minV = QVector3D(pos.x, pos.y, pos.z);
                    maxV = minV;
                    haveBounds = true;
                } else {
                    minV.setX(qMin(minV.x(), pos.x));
                    minV.setY(qMin(minV.y(), pos.y));
                    minV.setZ(qMin(minV.z(), pos.z));
                    maxV.setX(qMax(maxV.x(), pos.x));
                    maxV.setY(qMax(maxV.y(), pos.y));
                    maxV.setZ(qMax(maxV.z(), pos.z));
                }
            }
            if (haveBounds) {
                mesh.boundsMin = minV;
                mesh.boundsMax = maxV;
                mesh.boundsValid = true;
            }

            for (unsigned int f = 0; f < m->mNumFaces; ++f) {
                const aiFace &face = m->mFaces[f];
                if (face.mNumIndices != 3)
                    continue;
                mesh.indices.push_back(face.mIndices[0]);
                mesh.indices.push_back(face.mIndices[1]);
                mesh.indices.push_back(face.mIndices[2]);
            }

            if (m->mMaterialIndex < ai->mNumMaterials) {
                const aiMaterial *mat = ai->mMaterials[m->mMaterialIndex];
                readMaterial(mat, mesh.material);
            }

            scene.meshes().push_back(mesh);
        }

        for (unsigned int c = 0; c < node->mNumChildren; ++c)
            visitNode(node->mChildren[c], world);
    };

    visitNode(ai->mRootNode, QMatrix4x4());
    return !scene.meshes().isEmpty();
}
#endif
