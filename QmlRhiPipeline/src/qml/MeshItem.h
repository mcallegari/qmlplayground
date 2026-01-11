#pragma once

#include <QtCore/QObject>
#include <QtGui/QVector3D>

class MeshItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVector3D position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QVector3D rotationDegrees READ rotationDegrees WRITE setRotationDegrees NOTIFY rotationDegreesChanged)
    Q_PROPERTY(QVector3D scale READ scale WRITE setScale NOTIFY scaleChanged)
    Q_PROPERTY(bool isSelected READ isSelected WRITE setIsSelected NOTIFY isSelectedChanged)
    Q_PROPERTY(bool selectable READ selectable WRITE setSelectable NOTIFY selectableChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)

public:
    enum class MeshType
    {
        Model,
        StaticLight,
        MovingHead,
        Cube,
        Sphere,
        PixelBar,
        BeamBar
    };
    Q_ENUM(MeshType)

    explicit MeshItem(QObject *parent = nullptr);

    virtual MeshType type() const = 0;

    QVector3D position() const { return m_position; }
    void setPosition(const QVector3D &position);

    QVector3D rotationDegrees() const { return m_rotationDegrees; }
    void setRotationDegrees(const QVector3D &rotation);

    QVector3D scale() const { return m_scale; }
    void setScale(const QVector3D &scale);

    bool isSelected() const { return m_isSelected; }
    void setIsSelected(bool selected);

    bool selectable() const { return m_selectable; }
    void setSelectable(bool selectable);

    bool visible() const { return m_visible; }
    void setVisible(bool visible);

Q_SIGNALS:
    void positionChanged();
    void rotationDegreesChanged();
    void scaleChanged();
    void isSelectedChanged();
    void selectableChanged();
    void visibleChanged();

protected:
    void notifyParent();

    QVector3D m_position = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D m_rotationDegrees = QVector3D(0.0f, 0.0f, 0.0f);
    QVector3D m_scale = QVector3D(1.0f, 1.0f, 1.0f);
    bool m_isSelected = false;
    bool m_selectable = true;
    bool m_visible = true;
};
