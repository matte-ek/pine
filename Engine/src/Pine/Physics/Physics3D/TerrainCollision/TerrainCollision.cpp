#include "TerrainCollision.hpp"

#include "Pine/Assets/Terrain/Terrain.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/Physics/Physics3D/Physics3D.hpp"

#include "physx/PxPhysicsAPI.h"

#include <limits>
#include <vector>

namespace
{
    using namespace Pine;

    // PhysX stores a sample's height as a signed 16-bit integer while the terrain stores it
    // unsigned, so the two differ by exactly half the range. Nothing is lost either way: this is a
    // re-centring of the same 16-bit number, and the constant it introduces is folded into the
    // shape's local pose below.
    constexpr int SIGNED_SAMPLE_OFFSET = 32768;

    // PhysX walks its samples row-major with the *row* index running along local X and the column
    // index along local Z (PxHeightFieldDesc: "Local space X-axis corresponds to rows"), while the
    // terrain stores its field row-major in Z. So the copy transposes.
    //
    // Transposing here rather than rotating the shape is deliberate: a rotated height field would
    // make every later question about the collision - which way a row runs, what the local pose
    // means - only answerable after applying that rotation first.
    std::vector<physx::PxHeightFieldSample> BuildSamples(const Terrain& terrain)
    {
        const auto fieldSize = terrain.GetFieldSize();
        const auto& heights = terrain.GetHeightField();

        std::vector<physx::PxHeightFieldSample> samples(
            static_cast<std::size_t>(fieldSize.x) * fieldSize.y);

        for (int z = 0; z < fieldSize.y; z++)
        {
            for (int x = 0; x < fieldSize.x; x++)
            {
                auto& sample = samples[static_cast<std::size_t>(x) * fieldSize.y + z];

                sample.height = static_cast<physx::PxI16>(
                    static_cast<int>(heights[static_cast<std::size_t>(z) * fieldSize.x + x]) - SIGNED_SAMPLE_OFFSET);

                sample.materialIndex0 = 0;
                sample.materialIndex1 = 0;

                // A cleared tessellation flag splits the quad along the diagonal between its two
                // *other* corners - from (x, z + 1) to (x + 1, z) here. That is the convention
                // Terrain::IsInFirstQuadTriangle documents, the one GetHeightAt interpolates
                // against and the one the mesh generator winds its triangles to. All four have to
                // be the same rule, or a body lands somewhere the ground is not drawn.
                sample.clearTessFlag();
            }
        }

        return samples;
    }
}

physx::PxShape* Pine::Physics3D::TerrainCollision::CreateShape(const Terrain& terrain, physx::PxMaterial& material)
{
    PINE_PF_SCOPE();

    const auto fieldSize = terrain.GetFieldSize();

    // PhysX needs at least one quad on each axis, which is two samples.
    if (fieldSize.x < 2 || fieldSize.y < 2 || terrain.GetHeightField().empty())
    {
        return nullptr;
    }

    const auto samples = BuildSamples(terrain);

    physx::PxHeightFieldDesc description;

    description.nbRows = fieldSize.x;
    description.nbColumns = fieldSize.y;
    description.format = physx::PxHeightFieldFormat::eS16_TM;
    description.samples.data = samples.data();
    description.samples.stride = sizeof(physx::PxHeightFieldSample);

    // Created straight into the SDK rather than cooked to a memory stream and read back. The stream
    // round trip served no purpose here - nothing persists the cooked field, it is derived from the
    // height field and rebuilt whenever that changes.
    const auto heightField = PxCreateHeightField(description, GetPhysics()->getPhysicsInsertionCallback());

    if (heightField == nullptr)
    {
        PError("TerrainCollision::CreateShape(): PhysX refused to cook the height field.");
        return nullptr;
    }

    // Both scales come from the terrain's own spacing, and the height scale from its own decoder,
    // so the collision surface is built out of the same numbers as the mesh instead of a second set
    // that has to be kept in agreement with them.
    const float spacing = terrain.GetSampleSpacing();
    const float heightScale =
        (terrain.DecodeHeight(std::numeric_limits<std::uint16_t>::max()) - terrain.DecodeHeight(0)) /
        static_cast<float>(std::numeric_limits<std::uint16_t>::max());

    const physx::PxHeightFieldGeometry geometry(heightField, physx::PxMeshGeometryFlags(), heightScale, spacing, spacing);

    const auto shape = GetPhysics()->createShape(geometry, material);

    // The shape took its own reference when it was created, so dropping ours here leaves the shape
    // as the field's only owner.
    heightField->release();

    if (shape == nullptr)
    {
        PError("TerrainCollision::CreateShape(): PhysX refused to create a shape for the height field.");
        return nullptr;
    }

    // Sample (0, 0) of the cooked field sits at the shape's origin, but the terrain's first sample
    // is at GetSampleMin() - which is negative for a terrain grown on its -x or -z edge - and its
    // heights were re-centred above. Both corrections are constants, so both live here.
    const auto sampleMin = terrain.GetSampleMin();

    const physx::PxTransform localPose(physx::PxVec3(
        static_cast<float>(sampleMin.x) * spacing,
        terrain.DecodeHeight(0) + SIGNED_SAMPLE_OFFSET * heightScale,
        static_cast<float>(sampleMin.y) * spacing));

    shape->setLocalPose(localPose);

    return shape;
}
