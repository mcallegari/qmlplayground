import Qt3D.Core 2.0
import Qt3D.Render 2.0
import Qt3D.Extras 2.0

Entity
{
    function bindTransform(t)
    {
        transform.matrix = Qt.binding(function() { return t.matrix })
    }

    readonly property SpotLight light:
        SpotLight
        {
            id: eSpotLight
            localDirection: Qt.vector3d(0.0, -1.0, 0.0)
            color: "blue"
            cutOffAngle: 15
            constantAttenuation: 0.5
            intensity: 0.8
        }

    Transform
    {
        id: transform
    }

    components: [
        transform,
        light
    ]
}
