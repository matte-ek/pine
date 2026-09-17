#pragma once

#include "Pine/Core/Math/Math.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace Pine
{
    class Camera;
    class Terrain;
    class TerrainRendererComponent;
}

// The height-field brush behind the editor's terrain tools and the debug server's /terrain/sculpt.
//
// The brush is editor-side rather than part of the Terrain asset on purpose: the asset is the data
// model, and what a raise brush is - its falloff curve, how a stroke becomes one undo step - is an
// authoring decision that a running game has no use for. What the asset does provide is the
// rectangle accessors this builds on, which read and write a region and dirty it once.
//
// Layer painting is the same tool writing weights instead of heights: same brush shape, same
// stroke-is-one-undo-step rule, same rectangle accessors on the asset. It is a brush mode rather
// than a second tool because a stroke only ever writes one of the two fields, and a mode is
// already how a stroke says which way it moves the ground.
namespace Editor::TerrainSculpting
{
    enum class BrushMode
    {
        Raise,
        Lower,

        // Pulls each sample towards the average of the ones around it.
        Smooth,

        // Pulls each sample towards one reference height, which is what cuts a level plateau or a
        // road bed out of uneven ground.
        Flatten,

        // Writes layer weights rather than heights: the samples under the brush give more of their
        // share to Brush::Layer and less to the others. The ground does not move.
        Paint
    };

    struct Brush
    {
        BrushMode Mode = BrushMode::Raise;

        // World units. Samples further than this from the stroke point are left alone.
        float Radius = 8.f;

        // How fast the surface changes under the centre of the brush, per second.
        //
        // For the four height modes that is world units, and it means the same thing in all of
        // them: Smooth and Flatten move a sample towards their target by at most this much, rather
        // than blending by some separate rate. For Paint it is the share of the layer added per
        // second, which approaches full coverage rather than reaching it - so the two are different
        // enough in magnitude that a caller holding both wants to remember them separately.
        float Strength = 8.f;

        // How much of the radius is soft edge, from 0 for a hard-edged stamp to 1 for a dome that
        // starts falling away at the centre.
        float Falloff = 0.5f;

        // The height Flatten pulls towards, in terrain-local units. Left empty, a stroke takes the
        // height of the ground under the point it started at, which is what "level this off from
        // here" means when the author clicks somewhere and drags.
        std::optional<float> FlattenHeight;

        // The splat channel Paint gives its samples to. Ignored by the height modes.
        int Layer = 0;
    };

    struct ViewportRay
    {
        Pine::Vector3f Origin{};

        // Normalized.
        Pine::Vector3f Direction{};
    };

    // The world-space ray through a point on a viewport image, or empty when the camera cannot
    // produce one.
    //
    // 'cursor' is measured from the top-left corner of the image in the same pixels as 'size',
    // which is how every mouse position in the editor arrives. Unprojected through the camera's own
    // matrices rather than rebuilt from its transform and field of view, so the ray goes through
    // what the viewport is actually showing - the two drift apart the moment anything overrides the
    // aspect ratio or the projection.
    //
    // Separate from the panel that calls it because this is where a picking bug lives: a sign or a
    // transpose wrong here puts the brush somewhere other than under the cursor, which nothing but
    // a hand on a mouse would notice. Here it can be checked against a camera aimed at a known
    // point.
    std::optional<ViewportRay> BuildCursorRay(const Pine::Camera* camera, Pine::Vector2f cursor, Pine::Vector2f size);

    // What a ray from the cursor met.
    struct TerrainPick
    {
        Pine::TerrainRendererComponent* Component = nullptr;
        Pine::Terrain* Terrain = nullptr;

        // The same point in both spaces. The brush works in terrain-local coordinates and the
        // overlay is expressed in them; anything drawn in the scene wants the world one.
        Pine::Vector3f LocalPosition{};
        Pine::Vector3f WorldPosition{};

        float Distance = 0.f;
    };

    // The nearest terrain a world-space ray meets, or empty when it meets none.
    //
    // Only the entity's position is taken into account, not its rotation or scale - which is the
    // same thing the renderer and the collision cooker do, because a height field has one surface
    // per column and cannot describe a rotated one. All three agreeing to ignore it is what keeps
    // the ground you see, the ground you walk on and the ground you sculpt the same ground.
    std::optional<TerrainPick> PickTerrain(const Pine::Vector3f& origin, const Pine::Vector3f& direction);

    // The spelling the debug server and the panel share, so a mode reads the same in a JSON request
    // and in the UI.
    std::string BrushModeToString(BrushMode mode);
    std::optional<BrushMode> BrushModeFromString(std::string_view name);

    // Applies one step of the brush at a terrain-local point, and returns whether it changed
    // anything - false when the point is off the terrain, or the brush covers no samples.
    //
    // A stroke starts on the first call and continues until EndStroke(), so a drag is one undo step
    // however many frames it spans. deltaTime is what turns Strength into an amount, so a drag
    // moves the ground at the same rate whatever the frame rate is.
    bool Apply(Pine::Terrain* terrain, const Brush& brush, Pine::Vector2f point, float deltaTime);

    // Ends the current stroke and writes it to the undo history as a single step. Does nothing when
    // no stroke is running, so a caller can end on mouse-up without tracking whether the drag ever
    // reached the ground.
    void EndStroke();

    bool IsStrokeActive();
}
