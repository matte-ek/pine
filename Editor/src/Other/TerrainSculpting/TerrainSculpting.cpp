#include "TerrainSculpting.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include "Other/Actions/Actions.hpp"
#include "Pine/Assets/Assets.hpp"
#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Physics/Physics3D/TerrainCollision/TerrainCollision.hpp"
#include "Pine/World/Components/Camera/Camera.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/TerrainRenderer/TerrainRendererComponent.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/Entity/Entity.hpp"

namespace
{
    using namespace Editor::TerrainSculpting;

    // The two fields a stroke can write, each as a policy: what one sample of the field is, how
    // many of them a terrain sample carries, the rectangle accessors that read and write it, and
    // what has to follow once a stroke, undo or redo has finished writing it.
    //
    // Named once and used from three places - the stroke's snapshot, the undo command and the
    // brush - because everything around that accessor pair is the same story for both fields. The
    // whole difference between undoing a raise and undoing a paint is which two functions run.
    struct HeightField
    {
        using Sample = std::uint16_t;

        static constexpr int Channels = 1;

        static std::vector<Sample> Read(const Pine::Terrain* terrain, const Pine::TerrainSampleRect& rect)
        {
            return terrain->GetSampleHeightRect(rect);
        }

        static bool Write(Pine::Terrain* terrain, const Pine::TerrainSampleRect& rect, const std::vector<Sample>& samples)
        {
            return terrain->SetSampleHeightRect(rect, samples);
        }

        // Collision is cooked from the heights, too slowly to redo on every brush step.
        static void OnWritten(const Pine::Terrain* terrain)
        {
            Pine::Physics3D::TerrainCollision::RebuildColliders(*terrain);
        }
    };

    struct WeightField
    {
        using Sample = std::uint8_t;

        static constexpr int Channels = Pine::Terrain::MaximumLayerCount;

        static std::vector<Sample> Read(const Pine::Terrain* terrain, const Pine::TerrainSampleRect& rect)
        {
            return terrain->GetSampleWeightRect(rect);
        }

        static bool Write(Pine::Terrain* terrain, const Pine::TerrainSampleRect& rect, const std::vector<Sample>& samples)
        {
            return terrain->SetSampleWeightRect(rect, samples);
        }

        // Layer weights only change how the ground looks.
        static void OnWritten(const Pine::Terrain*)
        {
        }
    };

    // A stroke in progress over one field. The terrain is held as an id rather than a pointer
    // because a stroke spans frames, and an asset refresh between two of them would leave a
    // pointer dangling.
    template <typename Field>
    struct Stroke
    {
        Pine::UId Terrain;
        Pine::TerrainSampleRect Rect;

        // The field over Rect as it was before the stroke touched any of it. Grown, never re-read,
        // so undo restores the ground the author started from rather than what it looked like half
        // way through the drag.
        std::vector<typename Field::Sample> Before;
    };

    // At most one of these is running at a time: a stroke either moves the ground or paints it,
    // and Apply ends the other one before it starts.
    Stroke<HeightField> m_HeightStroke;
    Stroke<WeightField> m_WeightStroke;

    // Taken from the ground under the first point of a Flatten stroke, when the brush does not name
    // a height itself. Kept for the whole stroke so that dragging levels everything to where the
    // stroke began, instead of chasing the height under the cursor.
    std::optional<float> m_StrokeFlattenHeight;

    // Restores a rectangle of one field, which is all a stroke ever does to a terrain. Undo and
    // redo are the same operation against different contents.
    template <typename Field>
    class StrokeCommand final : public Editor::Actions::EditorCommand
    {
    private:
        Pine::UId m_Terrain;
        Pine::TerrainSampleRect m_Rect;

        std::vector<typename Field::Sample> m_Before;
        std::vector<typename Field::Sample> m_After;
    public:
        StrokeCommand(const Pine::UId terrain,
                      const Pine::TerrainSampleRect& rect,
                      std::vector<typename Field::Sample> before,
                      std::vector<typename Field::Sample> after)
            : m_Terrain(terrain),
              m_Rect(rect),
              m_Before(std::move(before)),
              m_After(std::move(after))
        {
        }

        void Apply(const Editor::Actions::CommandState commandState) override
        {
            const auto terrain = Pine::Assets::Get<Pine::Terrain>(m_Terrain);

            if (terrain == nullptr)
            {
                PWarning(fmt::format("StrokeCommand::Apply: terrain {} is no longer loaded, skipping.",
                                     m_Terrain.ToString()));
                return;
            }

            // Refused rather than clamped when the terrain has since been resized out from under
            // the rectangle. Sample coordinates survive a resize, so this only happens when the
            // rows the stroke touched are genuinely gone.
            const bool restored = Field::Write(terrain, m_Rect,
                commandState == Editor::Actions::CommandState::PreCommand ? m_Before : m_After);

            if (!restored)
            {
                PWarning("StrokeCommand::Apply: the terrain no longer covers the samples this stroke changed.");
                return;
            }

            terrain->MarkAsModified();

            Field::OnWritten(terrain);
        }
    };

    // The samples a brush at a point can reach, clamped to the terrain. Empty when the brush falls
    // entirely outside it, which is the normal answer for a stroke dragged off the edge.
    Pine::TerrainSampleRect GetBrushRect(const Pine::Terrain* terrain, const Pine::Vector2f point, const float radius)
    {
        const float spacing = terrain->GetSampleSpacing();

        // Outward on both sides: a sample just outside the circle is still inside the rectangle,
        // and the per-sample distance test below is what actually gives the brush its round shape.
        const Pine::TerrainSampleRect reached = {
            { static_cast<int>(std::floor((point.x - radius) / spacing)),
              static_cast<int>(std::floor((point.y - radius) / spacing)) },
            { static_cast<int>(std::ceil((point.x + radius) / spacing)),
              static_cast<int>(std::ceil((point.y + radius) / spacing)) }
        };

        return {
            glm::max(reached.Min, terrain->GetSampleMin()),
            glm::min(reached.Max, terrain->GetSampleMax())
        };
    }

    // How much of the brush a sample takes: 1 under the centre, 0 at the rim and beyond.
    //
    // The falloff is measured inwards from the rim rather than outwards from the centre, so that
    // raising it softens the edge and leaves the middle of the brush at full strength, instead of
    // making the whole brush weaker.
    float GetBrushWeight(const float distance, const float radius, const float falloff)
    {
        if (distance >= radius)
        {
            return 0.f;
        }

        const float softEdge = std::clamp(falloff, 0.f, 1.f) * radius;
        const float distanceFromRim = radius - distance;

        if (softEdge <= 0.f || distanceFromRim >= softEdge)
        {
            return 1.f;
        }

        const float t = distanceFromRim / softEdge;

        // Smoothstep, so the brush meets untouched ground with a flat tangent. A linear falloff
        // leaves a visible crease in the surface at the rim of every stroke.
        return t * t * (3.f - 2.f * t);
    }

    // The mean of a sample and the eight around it, which is what Smooth pulls towards. Samples off
    // the terrain are left out rather than substituted, so the rim averages what is actually there
    // instead of being dragged towards a made-up value.
    float GetSmoothedHeight(const Pine::Terrain* terrain, const Pine::Vector2i sample)
    {
        float total = 0.f;
        int count = 0;

        for (int z = -1; z <= 1; z++)
        {
            for (int x = -1; x <= 1; x++)
            {
                if (const auto height = terrain->GetSampleHeight({ sample.x + x, sample.y + z }))
                {
                    total += *height;
                    count++;
                }
            }
        }

        return count > 0 ? total / static_cast<float>(count) : 0.f;
    }

    // Grows the stroke's snapshot to cover a newly touched rectangle, and starts the stroke if this
    // is its first application.
    //
    // The samples the stroke has already moved are kept as they were first recorded; only the area
    // the snapshot did not reach yet is read from the terrain, where it is still untouched. Reading
    // the whole union instead would record half-sculpted ground as the state undo returns to.
    template <typename Field>
    void ExpandStrokeSnapshot(Stroke<Field>& stroke, Pine::Terrain* terrain, const Pine::TerrainSampleRect& rect)
    {
        if (!stroke.Terrain.IsValid())
        {
            stroke.Terrain = terrain->GetUId();
            stroke.Rect = rect;
            stroke.Before = Field::Read(terrain, rect);

            return;
        }

        if (stroke.Rect.Contains(rect.Min) && stroke.Rect.Contains(rect.Max))
        {
            return;
        }

        const auto grown = stroke.Rect.Union(rect);

        auto before = Field::Read(terrain, grown);

        // A row of the snapshot is as wide as the rectangle times whatever one sample of the field
        // carries - one height, or one weight per layer.
        const auto recordedRowLength = static_cast<std::size_t>(stroke.Rect.GetWidth()) * Field::Channels;

        for (int z = stroke.Rect.Min.y; z <= stroke.Rect.Max.y; z++)
        {
            const auto recordedRow = static_cast<std::size_t>(z - stroke.Rect.Min.y) * recordedRowLength;
            const auto grownRow = (static_cast<std::size_t>(z - grown.Min.y) * grown.GetWidth()
                                + (stroke.Rect.Min.x - grown.Min.x)) * Field::Channels;

            std::copy_n(stroke.Before.begin() + static_cast<std::ptrdiff_t>(recordedRow),
                        recordedRowLength,
                        before.begin() + static_cast<std::ptrdiff_t>(grownRow));
        }

        stroke.Rect = grown;
        stroke.Before = std::move(before);
    }

    // Ends one field's stroke, recording it in the history as a single step when it changed
    // anything.
    template <typename Field>
    void EndFieldStroke(Stroke<Field>& stroke)
    {
        if (!stroke.Terrain.IsValid())
        {
            return;
        }

        const auto terrain = Pine::Assets::Get<Pine::Terrain>(stroke.Terrain);

        // Recorded only when there is something to undo to. A terrain unloaded mid-stroke leaves the
        // snapshot with nothing to restore it onto, and an empty command in the history would
        // silently swallow the author's next undo.
        if (terrain != nullptr)
        {
            auto after = Field::Read(terrain, stroke.Rect);

            if (after.size() == stroke.Before.size() && after != stroke.Before)
            {
                Editor::Actions::RegisterCommand(std::make_unique<StrokeCommand<Field>>(
                    stroke.Terrain, stroke.Rect, stroke.Before, std::move(after)));

                Field::OnWritten(terrain);
            }
        }

        stroke = {};
    }

    // Moves the ground under the brush, which is what the four height modes all do - they differ
    // only in the target each sample is heading towards.
    void ApplyHeightBrush(Pine::Terrain* terrain, const Brush& brush, const Pine::Vector2f point,
                          const float deltaTime, const Pine::TerrainSampleRect& rect)
    {
        if (brush.Mode == BrushMode::Flatten && !m_StrokeFlattenHeight.has_value())
        {
            m_StrokeFlattenHeight = brush.FlattenHeight.has_value()
                ? brush.FlattenHeight
                : terrain->GetHeightAt(point.x, point.y);
        }

        const float spacing = terrain->GetSampleSpacing();
        const float step = brush.Strength * deltaTime;

        auto heights = terrain->GetSampleHeightRect(rect);

        for (int z = rect.Min.y; z <= rect.Max.y; z++)
        {
            for (int x = rect.Min.x; x <= rect.Max.x; x++)
            {
                const Pine::Vector2f samplePosition = Pine::Vector2f(x, z) * spacing;
                const float weight = GetBrushWeight(glm::length(samplePosition - point), brush.Radius, brush.Falloff);

                if (weight <= 0.f)
                {
                    continue;
                }

                const auto index = static_cast<std::size_t>(z - rect.Min.y) * rect.GetWidth() + (x - rect.Min.x);
                const float height = terrain->DecodeHeight(heights[index]);

                // Every mode moves a sample by at most `weight * step`, towards a target the mode
                // picks. Raise and Lower have no target to reach, so they always move the whole way.
                float movement = weight * step;

                if (brush.Mode == BrushMode::Lower)
                {
                    movement = -movement;
                }
                else if (brush.Mode == BrushMode::Smooth || brush.Mode == BrushMode::Flatten)
                {
                    const float target = brush.Mode == BrushMode::Smooth
                        ? GetSmoothedHeight(terrain, { x, z })
                        : m_StrokeFlattenHeight.value_or(height);

                    movement = std::clamp(target - height, -movement, movement);
                }

                heights[index] = terrain->EncodeHeight(height + movement);
            }
        }

        terrain->SetSampleHeightRect(rect, heights);
    }

    // Gives each sample under the brush more of the brush's layer and correspondingly less of the
    // others. Nothing here reads or writes a height: painting changes what the ground is made of,
    // not where it is.
    void ApplyPaintBrush(Pine::Terrain* terrain, const Brush& brush, const Pine::Vector2f point,
                         const float deltaTime, const Pine::TerrainSampleRect& rect)
    {
        const float spacing = terrain->GetSampleSpacing();
        const float step = brush.Strength * deltaTime;

        auto weights = terrain->GetSampleWeightRect(rect);

        for (int z = rect.Min.y; z <= rect.Max.y; z++)
        {
            for (int x = rect.Min.x; x <= rect.Max.x; x++)
            {
                const Pine::Vector2f samplePosition = Pine::Vector2f(x, z) * spacing;
                const float weight = GetBrushWeight(glm::length(samplePosition - point), brush.Radius, brush.Falloff);

                if (weight <= 0.f)
                {
                    continue;
                }

                const auto index = (static_cast<std::size_t>(z - rect.Min.y) * rect.GetWidth() + (x - rect.Min.x))
                                 * Pine::Terrain::MaximumLayerCount;

                // The share the brush hands over this step is added to what the sample already
                // gives that layer, and the encoder then renormalizes the sample. So coverage
                // approaches full rather than overshooting it however long the brush is held, and
                // the layers giving way keep their proportions to one another while they do.
                auto shares = Pine::Terrain::DecodeSampleWeights(&weights[index]);

                shares[brush.Layer] += weight * step;

                Pine::Terrain::EncodeSampleWeights(shares, &weights[index]);
            }
        }

        terrain->SetSampleWeightRect(rect, weights);
    }
}

std::optional<Editor::TerrainSculpting::ViewportRay> Editor::TerrainSculpting::BuildCursorRay(
    const Pine::Camera* camera, const Pine::Vector2f cursor, const Pine::Vector2f size)
{
    if (camera == nullptr || size.x <= 0.f || size.y <= 0.f)
    {
        return std::nullopt;
    }

    // Normalized device coordinates: -1 to 1 across the image, with y running up the screen while
    // the cursor's runs down it.
    const Pine::Vector2f normalized = {
        (cursor.x / size.x) * 2.f - 1.f,
        1.f - (cursor.y / size.y) * 2.f
    };

    const auto inverseViewProjection = glm::inverse(camera->GetProjectionMatrix() * camera->GetViewMatrix());

    const auto nearPoint = inverseViewProjection * Pine::Vector4f(normalized.x, normalized.y, -1.f, 1.f);
    const auto farPoint = inverseViewProjection * Pine::Vector4f(normalized.x, normalized.y, 1.f, 1.f);

    if (nearPoint.w == 0.f || farPoint.w == 0.f)
    {
        return std::nullopt;
    }

    const auto origin = Pine::Vector3f(nearPoint) / nearPoint.w;
    const auto target = Pine::Vector3f(farPoint) / farPoint.w;

    if (glm::length(target - origin) < std::numeric_limits<float>::epsilon())
    {
        return std::nullopt;
    }

    return ViewportRay{ origin, glm::normalize(target - origin) };
}

std::optional<Editor::TerrainSculpting::TerrainPick> Editor::TerrainSculpting::PickTerrain(
    const Pine::Vector3f& origin, const Pine::Vector3f& direction)
{
    std::optional<TerrainPick> nearest;

    for (auto& component : Pine::Components::Get<Pine::TerrainRendererComponent>())
    {
        const auto terrain = component.GetTerrain();

        if (terrain == nullptr)
        {
            continue;
        }

        const auto entityPosition = component.GetParent()->GetTransform()->GetPosition();
        const auto hit = terrain->Raycast(origin - entityPosition, direction);

        if (!hit.has_value())
        {
            continue;
        }

        if (nearest.has_value() && nearest->Distance <= hit->Distance)
        {
            continue;
        }

        nearest = TerrainPick{
            &component,
            terrain,
            hit->Position,
            hit->Position + entityPosition,
            hit->Distance
        };
    }

    return nearest;
}

std::string Editor::TerrainSculpting::BrushModeToString(const BrushMode mode)
{
    switch (mode)
    {
        case BrushMode::Raise:
            return "raise";
        case BrushMode::Lower:
            return "lower";
        case BrushMode::Smooth:
            return "smooth";
        case BrushMode::Flatten:
            return "flatten";
        case BrushMode::Paint:
            return "paint";
        default:
            return "invalid";
    }
}

std::optional<Editor::TerrainSculpting::BrushMode> Editor::TerrainSculpting::BrushModeFromString(const std::string_view name)
{
    if (name == "raise")
        return BrushMode::Raise;
    if (name == "lower")
        return BrushMode::Lower;
    if (name == "smooth")
        return BrushMode::Smooth;
    if (name == "flatten")
        return BrushMode::Flatten;
    if (name == "paint")
        return BrushMode::Paint;

    return std::nullopt;
}

bool Editor::TerrainSculpting::Apply(Pine::Terrain* terrain, const Brush& brush, const Pine::Vector2f point, const float deltaTime)
{
    if (terrain == nullptr || brush.Radius <= 0.f || deltaTime <= 0.f)
    {
        return false;
    }

    const bool painting = brush.Mode == BrushMode::Paint;

    if (painting && (brush.Layer < 0 || brush.Layer >= Pine::Terrain::MaximumLayerCount))
    {
        PWarning(fmt::format("Ignored a paint stroke on layer {}: a terrain has {} of them.",
                             brush.Layer, Pine::Terrain::MaximumLayerCount));
        return false;
    }

    // A stroke covers one terrain and one field. A brush that has moved onto another terrain, or
    // switched between moving the ground and painting it, ends the stroke that was running rather
    // than extending its rectangle onto something it does not describe.
    const auto running = painting ? m_WeightStroke.Terrain : m_HeightStroke.Terrain;
    const auto other = painting ? m_HeightStroke.Terrain : m_WeightStroke.Terrain;

    if (other.IsValid() || (running.IsValid() && running != terrain->GetUId()))
    {
        EndStroke();
    }

    const auto rect = GetBrushRect(terrain, point, brush.Radius);

    if (rect.IsEmpty())
    {
        return false;
    }

    if (painting)
    {
        ExpandStrokeSnapshot(m_WeightStroke, terrain, rect);
        ApplyPaintBrush(terrain, brush, point, deltaTime, rect);
    }
    else
    {
        ExpandStrokeSnapshot(m_HeightStroke, terrain, rect);
        ApplyHeightBrush(terrain, brush, point, deltaTime, rect);
    }

    terrain->MarkAsModified();

    return true;
}

void Editor::TerrainSculpting::EndStroke()
{
    // Both, because ending the one that is not running does nothing - and asking which one is
    // would only be a second way to say what the two IsValid checks inside already say.
    EndFieldStroke(m_HeightStroke);
    EndFieldStroke(m_WeightStroke);

    m_StrokeFlattenHeight.reset();
}

bool Editor::TerrainSculpting::IsStrokeActive()
{
    return m_HeightStroke.Terrain.IsValid() || m_WeightStroke.Terrain.IsValid();
}
