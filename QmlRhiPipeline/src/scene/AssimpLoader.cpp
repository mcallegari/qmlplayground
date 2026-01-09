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
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QLatin1Char>
#include <QtCore/QUrl>
#include <QtGui/QColor>
#include <QtGui/QImage>
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

static QString resolveTexturePath(const QString &modelPath, const QString &texturePath)
{
    if (texturePath.isEmpty())
        return QString();
    const QUrl url(texturePath);
    if (url.isValid() && url.isLocalFile())
        return url.toLocalFile();
    const QFileInfo textureInfo(texturePath);
    if (textureInfo.isAbsolute())
        return texturePath;
    const QUrl modelUrl(modelPath);
    const QString modelFile = modelUrl.isValid() && modelUrl.isLocalFile()
            ? modelUrl.toLocalFile()
            : modelPath;
    const QFileInfo modelInfo(modelFile);
    const QString candidate = QDir(modelInfo.absolutePath()).filePath(texturePath);
    if (QFileInfo::exists(candidate))
        return candidate;
    return QDir::current().filePath(texturePath);
}

static QString resolveModelPath(const QString &path)
{
    const QUrl url(path);
    QString resolved = (url.isValid() && url.isLocalFile()) ? url.toLocalFile() : path;
    const QFileInfo info(resolved);
    if (info.exists())
        return info.absoluteFilePath();
    return resolved;
}

static QImage loadEmbeddedTexture(const aiTexture *texture)
{
    if (!texture)
        return QImage();
    if (texture->mHeight == 0)
    {
        const uchar *data = reinterpret_cast<const uchar *>(texture->pcData);
        QImage image = QImage::fromData(data, int(texture->mWidth));
        return image.isNull() ? QImage() : image.convertToFormat(QImage::Format_RGBA8888);
    }
    const int width = int(texture->mWidth);
    const int height = int(texture->mHeight);
    QImage image(reinterpret_cast<const uchar *>(texture->pcData), width, height, QImage::Format_RGBA8888);
    return image.copy();
}

static bool loadTextureFromPath(const aiScene *scene,
                                const QString &modelPath,
                                const aiString &texturePath,
                                QString &outPath,
                                QImage &outImage)
{
    if (texturePath.length == 0)
        return false;
    const QString texString = QString::fromUtf8(texturePath.C_Str());
    if (texString.startsWith(QLatin1Char('*')))
    {
        bool ok = false;
        const int index = texString.mid(1).toInt(&ok);
        if (ok && scene && index >= 0 && index < int(scene->mNumTextures))
        {
            outImage = loadEmbeddedTexture(scene->mTextures[index]);
            return !outImage.isNull();
        }
        return false;
    }
    const QString resolved = resolveTexturePath(modelPath, texString);
    outPath = resolved;
    QImage image(resolved);
    if (!image.isNull())
    {
        outImage = image.convertToFormat(QImage::Format_RGBA8888);
        return true;
    }
    return false;
}

static bool loadTextureForType(const aiScene *scene,
                               const QString &modelPath,
                               const aiMaterial *mat,
                               aiTextureType type,
                               QString &outPath,
                               QImage &outImage)
{
    aiString texturePath;
    if (aiGetMaterialTexture(mat, type, 0, &texturePath) != AI_SUCCESS)
        return false;
    return loadTextureFromPath(scene, modelPath, texturePath, outPath, outImage);
}

static QImage buildMetallicRoughnessMap(const QImage &metalness, const QImage &roughness)
{
    if (metalness.isNull() && roughness.isNull())
        return QImage();

    const QImage metal = metalness.isNull()
            ? QImage()
            : metalness.convertToFormat(QImage::Format_RGBA8888);
    const QImage rough = roughness.isNull()
            ? QImage()
            : roughness.convertToFormat(QImage::Format_RGBA8888);

    QSize size;
    if (!metal.isNull())
        size = metal.size();
    else
        size = rough.size();

    QImage metalScaled = metal;
    QImage roughScaled = rough;
    if (!metalScaled.isNull() && metalScaled.size() != size)
        metalScaled = metalScaled.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    if (!roughScaled.isNull() && roughScaled.size() != size)
        roughScaled = roughScaled.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QImage combined(size, QImage::Format_RGBA8888);
    for (int y = 0; y < size.height(); ++y)
    {
        const QRgb *metalLine = metalScaled.isNull() ? nullptr : reinterpret_cast<const QRgb *>(metalScaled.constScanLine(y));
        const QRgb *roughLine = roughScaled.isNull() ? nullptr : reinterpret_cast<const QRgb *>(roughScaled.constScanLine(y));
        QRgb *outLine = reinterpret_cast<QRgb *>(combined.scanLine(y));
        for (int x = 0; x < size.width(); ++x)
        {
            const int metalValue = metalLine ? qRed(metalLine[x]) : 0;
            const int roughValue = roughLine ? qRed(roughLine[x]) : 255;
            outLine[x] = qRgba(0, roughValue, metalValue, 255);
        }
    }
    return combined;
}

static void readMaterial(const aiScene *scene, const QString &modelPath, const aiMaterial *mat, Material &out)
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

    loadTextureForType(scene, modelPath, mat, aiTextureType_BASE_COLOR, out.baseColorMapPath, out.baseColorMap);
    if (out.baseColorMap.isNull())
        loadTextureForType(scene, modelPath, mat, aiTextureType_DIFFUSE, out.baseColorMapPath, out.baseColorMap);

    loadTextureForType(scene, modelPath, mat, aiTextureType_NORMALS, out.normalMapPath, out.normalMap);
    if (out.normalMap.isNull())
        loadTextureForType(scene, modelPath, mat, aiTextureType_HEIGHT, out.normalMapPath, out.normalMap);

    QString metalPath;
    QString roughPath;
    QImage metalImage;
    QImage roughImage;
    if (loadTextureForType(scene, modelPath, mat, aiTextureType_UNKNOWN,
                           out.metallicRoughnessMapPath, out.metallicRoughnessMap))
        return;
    const bool hasMetal = loadTextureForType(scene, modelPath, mat, aiTextureType_METALNESS, metalPath, metalImage);
    const bool hasRough = loadTextureForType(scene, modelPath, mat, aiTextureType_DIFFUSE_ROUGHNESS, roughPath, roughImage);

    if (hasMetal || hasRough)
    {
        out.metallicRoughnessMap = buildMetallicRoughnessMap(metalImage, roughImage);
        if (hasMetal)
            out.metallicRoughnessMapPath = metalPath;
        else
            out.metallicRoughnessMapPath = roughPath;
    }
    else
    {
        loadTextureForType(scene, modelPath, mat, aiTextureType_UNKNOWN,
                           out.metallicRoughnessMapPath, out.metallicRoughnessMap);
    }
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
    const QString resolvedPath = resolveModelPath(path);
    const aiScene *ai = importer.ReadFile(resolvedPath.toStdString(),
                                         aiProcess_Triangulate |
                                         aiProcess_GenNormals |
                                         aiProcess_CalcTangentSpace |
                                         aiProcess_JoinIdenticalVertices);
    if (!ai)
        return false;

    std::function<void(aiNode *, const QMatrix4x4 &)> visitNode;
    visitNode = [&](aiNode *node, const QMatrix4x4 &parent)
    {
        const QMatrix4x4 local = toQtMatrix(node->mTransformation);
        const QMatrix4x4 world = parent * local;

        for (unsigned int i = 0; i < node->mNumMeshes; ++i)
        {
            const aiMesh *m = ai->mMeshes[node->mMeshes[i]];
            Mesh mesh;
            mesh.modelMatrix = world;
            mesh.vertices.reserve(int(m->mNumVertices));
            mesh.indices.reserve(int(m->mNumFaces * 3));
            QVector3D minV;
            QVector3D maxV;
            bool haveBounds = false;

            for (unsigned int v = 0; v < m->mNumVertices; ++v)
            {
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

                if (!haveBounds)
                {
                    minV = QVector3D(pos.x, pos.y, pos.z);
                    maxV = minV;
                    haveBounds = true;
                }
                else
                {
                    minV.setX(qMin(minV.x(), pos.x));
                    minV.setY(qMin(minV.y(), pos.y));
                    minV.setZ(qMin(minV.z(), pos.z));
                    maxV.setX(qMax(maxV.x(), pos.x));
                    maxV.setY(qMax(maxV.y(), pos.y));
                    maxV.setZ(qMax(maxV.z(), pos.z));
                }
            }
            if (haveBounds)
            {
                mesh.boundsMin = minV;
                mesh.boundsMax = maxV;
                mesh.boundsValid = true;
            }

            for (unsigned int f = 0; f < m->mNumFaces; ++f)
            {
                const aiFace &face = m->mFaces[f];
                if (face.mNumIndices != 3)
                    continue;
                mesh.indices.push_back(face.mIndices[0]);
                mesh.indices.push_back(face.mIndices[1]);
                mesh.indices.push_back(face.mIndices[2]);
            }

            if (m->mMaterialIndex < ai->mNumMaterials)
            {
                const aiMaterial *mat = ai->mMaterials[m->mMaterialIndex];
                readMaterial(ai, resolvedPath, mat, mesh.material);
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
