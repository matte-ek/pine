#include "Terrain.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "PerlinNoise.hpp"
#include "Pine/Assets/Mesh/Mesh.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Graphics/Interfaces/ITexture.hpp"
#include "Pine/Performance/Performance.hpp"

using namespace Pine;

namespace
{
    constexpr float SAMPLE_MAXIMUM = static_cast<float>(std::numeric_limits<std::uint16_t>::max());

    // What one layer weight is stored as. A byte per channel, so a sample's four weights are one
    // RGBA8 texel and the field can be handed to the GPU without being converted first.
    constexpr float WEIGHT_MAXIMUM = static_cast<float>(std::numeric_limits<std::uint8_t>::max());

    // Where a ray enters and leaves an axis-aligned box, as distances along a unit direction.
    // False when it misses. The slab test: each axis gives the span of the ray that lies between
    // that axis' pair of planes, and the ray is inside the box over the overlap of the three.
    //
    // The axes are walked one at a time rather than divided through as a vector, because an axis
    // the ray does not move along has to be answered rather than divided by: a ray straight down
    // the terrain's corner is at x exactly 0 with no x motion at all, and 0/0 would take the whole
    // test out with a NaN.
    bool IntersectBounds(const Vector3f& origin,
                         const Vector3f& direction,
                         const Vector3f& boundsMin,
                         const Vector3f& boundsMax,
                         float& entryDistance,
                         float& exitDistance)
    {
        entryDistance = -std::numeric_limits<float>::infinity();
        exitDistance = std::numeric_limits<float>::infinity();

        for (int axis = 0; axis < 3; axis++)
        {
            if (direction[axis] == 0.f)
            {
                // Parallel to this pair of planes: either always between them, or never.
                if (origin[axis] < boundsMin[axis] || origin[axis] > boundsMax[axis])
                {
                    return false;
                }

                continue;
            }

            const float toMin = (boundsMin[axis] - origin[axis]) / direction[axis];
            const float toMax = (boundsMax[axis] - origin[axis]) / direction[axis];

            entryDistance = std::max(entryDistance, std::min(toMin, toMax));
            exitDistance = std::min(exitDistance, std::max(toMin, toMax));
        }

        return exitDistance >= entryDistance && exitDistance >= 0.f;
    }

    // Where a ray meets one triangle, as a distance along a unit direction, or empty when it misses.
    //
    // Moller-Trumbore, accepting a hit from either side: the editor camera is normally above the
    // ground looking down, but a camera that has dropped below it still has to be able to pick the
    // surface it is looking up at.
    std::optional<float> IntersectTriangle(const Vector3f& origin,
                                           const Vector3f& direction,
                                           const Vector3f& first,
                                           const Vector3f& second,
                                           const Vector3f& third)
    {
        constexpr float ParallelEpsilon = 1e-7f;

        // How far outside a triangle a hit is still taken as being on it. Every triangle of the
        // ground shares each of its edges with a neighbour, so a ray arriving exactly along one -
        // which is what clicking on a grid line does, and what a ray grazing the surface does -
        // would otherwise be rejected by both of them and fall through the ground. Accepting it
        // twice is harmless: the caller keeps the nearer of the two, and they agree along the edge.
        constexpr float EdgeEpsilon = 1e-5f;

        const auto firstEdge = second - first;
        const auto secondEdge = third - first;

        const auto normalCross = glm::cross(direction, secondEdge);
        const float determinant = glm::dot(firstEdge, normalCross);

        // Edge on: the ray runs parallel to the triangle's plane and either misses it or lies in
        // it, and a hit in the plane of a ground triangle is not a point anyone wants back.
        if (std::abs(determinant) < ParallelEpsilon)
        {
            return std::nullopt;
        }

        const float inverseDeterminant = 1.f / determinant;

        const auto toOrigin = origin - first;
        const float firstBarycentric = glm::dot(toOrigin, normalCross) * inverseDeterminant;

        if (firstBarycentric < -EdgeEpsilon || firstBarycentric > 1.f + EdgeEpsilon)
        {
            return std::nullopt;
        }

        const auto originCross = glm::cross(toOrigin, firstEdge);
        const float secondBarycentric = glm::dot(direction, originCross) * inverseDeterminant;

        if (secondBarycentric < -EdgeEpsilon || firstBarycentric + secondBarycentric > 1.f + EdgeEpsilon)
        {
            return std::nullopt;
        }

        const float distance = glm::dot(secondEdge, originCross) * inverseDeterminant;

        // Behind the ray's origin rather than in front of it.
        if (distance < 0.f)
        {
            return std::nullopt;
        }

        return distance;
    }

    // Integer division that rounds towards negative infinity. Plain / truncates towards zero, which
    // puts sample -1 and sample 0 in the same chunk - the terrain grid extends into negative
    // coordinates, so that matters here.
    int FloorDivide(const int value, const int divisor)
    {
        const int quotient = value / divisor;

        if (value % divisor != 0 && (value < 0) != (divisor < 0))
        {
            return quotient - 1;
        }

        return quotient;
    }
}

Terrain::Terrain()
{
    m_Type = AssetType::Terrain;

    ResetHeightField();
    ResetLayerWeights();
    RebuildChunks();
}

/* Layout */

Vector2i Terrain::GetChunkCount() const
{
    return m_ChunkCount;
}

Vector2i Terrain::GetChunkOrigin() const
{
    return m_ChunkOrigin;
}

int Terrain::GetChunkQuads() const
{
    return m_ChunkQuads;
}

float Terrain::GetChunkSize() const
{
    return m_ChunkSize;
}

float Terrain::GetSampleSpacing() const
{
    return m_ChunkSize / static_cast<float>(m_ChunkQuads);
}

Vector2i Terrain::GetFieldSize() const
{
    return m_ChunkCount * m_ChunkQuads + Vector2i(1);
}

Vector2i Terrain::GetSampleMin() const
{
    return m_ChunkOrigin * m_ChunkQuads;
}

Vector2i Terrain::GetSampleMax() const
{
    return (m_ChunkOrigin + m_ChunkCount) * m_ChunkQuads;
}

bool Terrain::IsLayoutSupported(const Vector2i chunkOrigin, const Vector2i chunkCount, const int chunkQuads)
{
    if (chunkCount.x < 1 || chunkCount.y < 1 || chunkQuads < 1)
    {
        return false;
    }

    const auto quads = static_cast<std::int64_t>(chunkQuads);

    const auto isAxisSupported = [quads](const int origin, const int count)
    {
        if (static_cast<std::int64_t>(count) * quads > MaximumFieldQuadsPerAxis)
        {
            return false;
        }

        // The first and last sample coordinates this axis covers - what GetSampleMin and
        // GetSampleMax work out in int, done here before an int has the chance to wrap. The last
        // is always above the first, so bounding those two bounds everything between them.
        const std::int64_t firstSample = static_cast<std::int64_t>(origin) * quads;
        const std::int64_t lastSample = (static_cast<std::int64_t>(origin) + count) * quads;

        return firstSample >= -MaximumSampleCoordinate && lastSample <= MaximumSampleCoordinate;
    };

    return isAxisSupported(chunkOrigin.x, chunkCount.x) &&
           isAxisSupported(chunkOrigin.y, chunkCount.y);
}

void Terrain::Resize(const Vector2i chunkOrigin, const Vector2i chunkCount)
{
    if (chunkCount.x < 1 || chunkCount.y < 1)
    {
        PWarning("Ignored terrain resize: a terrain needs at least one chunk on each axis.");
        return;
    }

    if (!IsLayoutSupported(chunkOrigin, chunkCount, m_ChunkQuads))
    {
        PWarning(fmt::format("Ignored terrain resize to {}x{} chunks at ({}, {}): a terrain spans "
                             "at most {} quads on each axis, no further than {} samples out.",
                             chunkCount.x, chunkCount.y, chunkOrigin.x, chunkOrigin.y,
                             MaximumFieldQuadsPerAxis, MaximumSampleCoordinate));
        return;
    }

    if (chunkOrigin == m_ChunkOrigin && chunkCount == m_ChunkCount)
    {
        return;
    }

    const auto previousHeights = std::move(m_Heights);
    const auto previousWeights = std::move(m_LayerWeights);
    const auto previousSampleMin = GetSampleMin();
    const auto previousSampleMax = GetSampleMax();
    const int previousStride = GetFieldSize().x;

    m_ChunkOrigin = chunkOrigin;
    m_ChunkCount = chunkCount;

    ResetHeightField();
    ResetLayerWeights();

    // Copied by sample coordinate rather than by flat index, because both the row stride and the
    // first row have just changed - only coordinates survive a resize.
    const auto overlapMin = glm::max(previousSampleMin, GetSampleMin());
    const auto overlapMax = glm::min(previousSampleMax, GetSampleMax());

    for (int z = overlapMin.y; z <= overlapMax.y; z++)
    {
        for (int x = overlapMin.x; x <= overlapMax.x; x++)
        {
            const std::size_t source =
                static_cast<std::size_t>(z - previousSampleMin.y) * previousStride + (x - previousSampleMin.x);

            m_Heights[GetSampleIndex({ x, z })] = previousHeights[source];

            // The weights move with the heights rather than being reset: a resize is meant to leave
            // what the author built alone, and painted ground that came back as bare layer 0 would
            // be just as much of a loss as flattened ground.
            std::copy_n(previousWeights.begin() + static_cast<std::ptrdiff_t>(source * MaximumLayerCount),
                        MaximumLayerCount,
                        m_LayerWeights.begin() + static_cast<std::ptrdiff_t>(GetWeightIndex({ x, z })));
        }
    }

    RebuildChunks();
}

void Terrain::SetChunkQuads(const int chunkQuads)
{
    if (chunkQuads < 1)
    {
        PWarning("Ignored terrain resolution change: a chunk needs at least one quad on each axis.");
        return;
    }

    if (!IsLayoutSupported(m_ChunkOrigin, m_ChunkCount, chunkQuads))
    {
        PWarning(fmt::format("Ignored terrain resolution change to {} quads a chunk: across {}x{} "
                             "chunks that spans more than the {} quads an axis a terrain may hold.",
                             chunkQuads, m_ChunkCount.x, m_ChunkCount.y, MaximumFieldQuadsPerAxis));
        return;
    }

    if (chunkQuads == m_ChunkQuads)
    {
        return;
    }

    m_ChunkQuads = chunkQuads;

    ResetHeightField();
    ResetLayerWeights();
    RebuildChunks();
}

void Terrain::SetChunkSize(const float chunkSize)
{
    if (chunkSize <= 0.f)
    {
        PWarning("Ignored terrain chunk size change: a chunk needs a positive world size.");
        return;
    }

    if (chunkSize == m_ChunkSize)
    {
        return;
    }

    m_ChunkSize = chunkSize;

    // Only the world-space scale changed, so the samples are still valid - but every chunk's bounds
    // were computed against the old spacing.
    RebuildChunks();
}

float Terrain::GetHeightMin() const
{
    return m_HeightMin;
}

float Terrain::GetHeightMax() const
{
    return m_HeightMax;
}

void Terrain::SetHeightRange(const float heightMin, const float heightMax)
{
    if (heightMax <= heightMin)
    {
        PWarning("Ignored terrain height range change: the maximum has to be above the minimum.");
        return;
    }

    if (heightMin == m_HeightMin && heightMax == m_HeightMax)
    {
        return;
    }

    // Kept, because the samples have to be decoded against the range they were encoded in and
    // EncodeHeight below reads the new one.
    const float previousMin = m_HeightMin;
    const float previousRange = m_HeightMax - m_HeightMin;

    m_HeightMin = heightMin;
    m_HeightMax = heightMax;

    // Re-encoded in place. Decoding the whole field into an array of floats first would cost four
    // bytes a sample - tens of megabytes on a large terrain - to hold a value each sample needs
    // exactly once.
    for (auto& sample : m_Heights)
    {
        const float height = previousMin + (static_cast<float>(sample) / SAMPLE_MAXIMUM) * previousRange;

        sample = EncodeHeight(height);
    }

    RebuildChunks();
}

/* Height field */

std::size_t Terrain::GetSampleIndex(const Vector2i sample) const
{
    const auto sampleMin = GetSampleMin();

    return static_cast<std::size_t>(sample.y - sampleMin.y) * GetFieldSize().x + (sample.x - sampleMin.x);
}

bool Terrain::IsSampleInside(const Vector2i sample) const
{
    const auto sampleMin = GetSampleMin();
    const auto sampleMax = GetSampleMax();

    return sample.x >= sampleMin.x && sample.x <= sampleMax.x &&
           sample.y >= sampleMin.y && sample.y <= sampleMax.y;
}

std::optional<float> Terrain::GetSampleHeight(const Vector2i sample) const
{
    if (!IsSampleInside(sample))
    {
        return std::nullopt;
    }

    return DecodeHeight(m_Heights[GetSampleIndex(sample)]);
}

bool Terrain::SetSampleHeight(const Vector2i sample, const float height)
{
    if (!IsSampleInside(sample))
    {
        return false;
    }

    m_Heights[GetSampleIndex(sample)] = EncodeHeight(height);

    MarkRegionDirty({ sample, sample });

    return true;
}

std::vector<std::uint16_t> Terrain::GetSampleHeightRect(const TerrainSampleRect& rect) const
{
    if (!IsSampleRectInside(rect))
    {
        return {};
    }

    const int width = rect.GetWidth();

    std::vector<std::uint16_t> heights;

    heights.reserve(static_cast<std::size_t>(width) * rect.GetHeight());

    for (int z = rect.Min.y; z <= rect.Max.y; z++)
    {
        const auto rowStart = GetSampleIndex({ rect.Min.x, z });

        heights.insert(heights.end(),
                       m_Heights.begin() + static_cast<std::ptrdiff_t>(rowStart),
                       m_Heights.begin() + static_cast<std::ptrdiff_t>(rowStart) + width);
    }

    return heights;
}

bool Terrain::SetSampleHeightRect(const TerrainSampleRect& rect, const std::vector<std::uint16_t>& heights)
{
    if (!IsSampleRectInside(rect))
    {
        return false;
    }

    const int width = rect.GetWidth();
    const int height = rect.GetHeight();

    const auto expected = static_cast<std::size_t>(width) * height;

    if (heights.size() != expected)
    {
        PWarning(fmt::format("Ignored a terrain height rectangle of {} sample(s): its {}x{} region holds {}.",
                             heights.size(), width, height, expected));
        return false;
    }

    for (int z = 0; z < height; z++)
    {
        const auto rowStart = GetSampleIndex({ rect.Min.x, rect.Min.y + z });
        const auto sourceStart = static_cast<std::ptrdiff_t>(z) * width;

        std::copy(heights.begin() + sourceStart,
                  heights.begin() + sourceStart + width,
                  m_Heights.begin() + static_cast<std::ptrdiff_t>(rowStart));
    }

    // Once for the whole rectangle. Marking per sample would walk every chunk of the terrain for
    // each of the thousands of samples a brush touches.
    MarkRegionDirty(rect);

    return true;
}

bool Terrain::IsInFirstQuadTriangle(const float quadX, const float quadZ)
{
    return quadX + quadZ <= 1.f;
}

std::optional<TerrainRayHit> Terrain::Raycast(const Vector3f& origin, const Vector3f& direction) const
{
    PINE_PF_SCOPE();

    const float directionLength = glm::length(direction);

    if (directionLength < std::numeric_limits<float>::epsilon())
    {
        return std::nullopt;
    }

    const auto ray = direction / directionLength;

    const float spacing = GetSampleSpacing();
    const auto sampleMin = GetSampleMin();
    const auto sampleMax = GetSampleMax();

    // The box the whole terrain lives in: its footprint, and the full range the samples encode
    // into rather than the range they currently use. A tighter y range would need the field's own
    // extremes, and the only thing this box decides is where marching starts and stops.
    const Vector3f boundsMin = { static_cast<float>(sampleMin.x) * spacing, m_HeightMin, static_cast<float>(sampleMin.y) * spacing };
    const Vector3f boundsMax = { static_cast<float>(sampleMax.x) * spacing, m_HeightMax, static_cast<float>(sampleMax.y) * spacing };

    float entryDistance = 0.f;
    float exitDistance = 0.f;

    if (!IntersectBounds(origin, ray, boundsMin, boundsMax, entryDistance, exitDistance))
    {
        return std::nullopt;
    }

    // A ray starting inside the box enters it at its origin, not behind it.
    entryDistance = std::max(entryDistance, 0.f);

    const auto entryPoint = origin + ray * entryDistance;

    // The quad the march starts in. Clamped because the entry point sits exactly on a face of the
    // box, where rounding can put it a hair outside the grid, and because the far rim belongs to
    // the last quad rather than to one past the end.
    Vector2i quad = {
        std::clamp(static_cast<int>(std::floor(entryPoint.x / spacing)), sampleMin.x, sampleMax.x - 1),
        std::clamp(static_cast<int>(std::floor(entryPoint.z / spacing)), sampleMin.y, sampleMax.y - 1)
    };

    // Standard grid march: step is the direction each axis advances in, delta is how far along the
    // ray one whole quad takes, and next is the distance at which the ray crosses into the quad
    // after this one. An axis the ray does not move along never crosses, which the infinities
    // express without a special case in the loop.
    const Vector2i step = { ray.x >= 0.f ? 1 : -1, ray.z >= 0.f ? 1 : -1 };

    const Vector2f delta = {
        ray.x != 0.f ? std::abs(spacing / ray.x) : std::numeric_limits<float>::infinity(),
        ray.z != 0.f ? std::abs(spacing / ray.z) : std::numeric_limits<float>::infinity()
    };

    const auto distanceToBoundary = [&](const float position, const float component, const int quadCoordinate, const int stepValue)
    {
        if (component == 0.f)
        {
            return std::numeric_limits<float>::infinity();
        }

        const float boundary = static_cast<float>(stepValue > 0 ? quadCoordinate + 1 : quadCoordinate) * spacing;

        return (boundary - position) / component;
    };

    Vector2f next = {
        entryDistance + distanceToBoundary(entryPoint.x, ray.x, quad.x, step.x),
        entryDistance + distanceToBoundary(entryPoint.z, ray.z, quad.y, step.y)
    };

    while (quad.x >= sampleMin.x && quad.x < sampleMax.x &&
           quad.y >= sampleMin.y && quad.y < sampleMax.y)
    {
        if (const auto hit = IntersectQuad(quad, origin, ray))
        {
            return hit;
        }

        // Where the ray leaves this quad is where it enters the next one. Past the far face of the
        // box means the ray has left the terrain through its top or bottom, which the footprint
        // test above cannot see - a steep ray would otherwise march the whole grid for nothing.
        const float quadExitDistance = std::min(next.x, next.y);

        if (quadExitDistance > exitDistance)
        {
            break;
        }

        // Whichever boundary the ray reaches first is the one it crosses.
        if (next.x < next.y)
        {
            quad.x += step.x;
            next.x += delta.x;
        }
        else
        {
            quad.y += step.y;
            next.y += delta.y;
        }
    }

    return std::nullopt;
}

std::optional<float> Terrain::GetHeightAt(const float x, const float z) const
{
    const float spacing = GetSampleSpacing();

    const float sampleX = x / spacing;
    const float sampleZ = z / spacing;

    const auto sampleMin = GetSampleMin();
    const auto sampleMax = GetSampleMax();

    if (sampleX < static_cast<float>(sampleMin.x) || sampleX > static_cast<float>(sampleMax.x) ||
        sampleZ < static_cast<float>(sampleMin.y) || sampleZ > static_cast<float>(sampleMax.y))
    {
        return std::nullopt;
    }

    // A point exactly on the far rim belongs to the last quad, not to one past the end of the field.
    const Vector2i quad = {
        std::min(static_cast<int>(std::floor(sampleX)), sampleMax.x - 1),
        std::min(static_cast<int>(std::floor(sampleZ)), sampleMax.y - 1)
    };

    const float quadX = sampleX - static_cast<float>(quad.x);
    const float quadZ = sampleZ - static_cast<float>(quad.y);

    const float lowLow = DecodeHeight(m_Heights[GetSampleIndex({ quad.x, quad.y })]);
    const float highLow = DecodeHeight(m_Heights[GetSampleIndex({ quad.x + 1, quad.y })]);
    const float lowHigh = DecodeHeight(m_Heights[GetSampleIndex({ quad.x, quad.y + 1 })]);
    const float highHigh = DecodeHeight(m_Heights[GetSampleIndex({ quad.x + 1, quad.y + 1 })]);

    if (IsInFirstQuadTriangle(quadX, quadZ))
    {
        return lowLow + (highLow - lowLow) * quadX + (lowHigh - lowLow) * quadZ;
    }

    return highHigh + (lowHigh - highHigh) * (1.f - quadX) + (highLow - highHigh) * (1.f - quadZ);
}

float Terrain::DecodeHeight(const std::uint16_t sample) const
{
    return m_HeightMin + (static_cast<float>(sample) / SAMPLE_MAXIMUM) * (m_HeightMax - m_HeightMin);
}

std::uint16_t Terrain::EncodeHeight(const float height) const
{
    const float range = m_HeightMax - m_HeightMin;

    if (range <= 0.f)
    {
        return 0;
    }

    const float normalized = std::clamp((height - m_HeightMin) / range, 0.f, 1.f);

    return static_cast<std::uint16_t>(std::lround(normalized * SAMPLE_MAXIMUM));
}

const std::vector<std::uint16_t>& Terrain::GetHeightField() const
{
    return m_Heights;
}

void Terrain::ResetHeightField()
{
    const auto fieldSize = GetFieldSize();

    m_Heights.assign(static_cast<std::size_t>(fieldSize.x) * fieldSize.y, EncodeHeight(0.f));
}

/* Layers */

std::size_t Terrain::GetWeightIndex(const Vector2i sample) const
{
    return GetSampleIndex(sample) * MaximumLayerCount;
}

void Terrain::ResetLayerWeights()
{
    const auto fieldSize = GetFieldSize();
    const auto sampleCount = static_cast<std::size_t>(fieldSize.x) * fieldSize.y;

    m_LayerWeights.assign(sampleCount * MaximumLayerCount, 0);

    for (std::size_t sample = 0; sample < sampleCount; sample++)
    {
        m_LayerWeights[sample * MaximumLayerCount] = std::numeric_limits<std::uint8_t>::max();
    }

    m_IsSplatMapDirty = true;
}

Material* Terrain::GetLayer(const int layer) const
{
    if (layer < 0 || layer >= MaximumLayerCount)
    {
        return nullptr;
    }

    return m_Layers[layer].Get();
}

void Terrain::SetLayer(const int layer, Material* material)
{
    if (layer < 0 || layer >= MaximumLayerCount)
    {
        PWarning(fmt::format("Ignored terrain layer {}: a terrain has {} of them.", layer, MaximumLayerCount));
        return;
    }

    m_Layers[layer] = material;
}

void Terrain::EncodeSampleWeights(const Vector4f& weights, std::uint8_t* destination)
{
    const auto clamped = glm::max(weights, Vector4f(0.f));
    const float total = clamped.x + clamped.y + clamped.z + clamped.w;

    // Nothing to divide by, so there is no blend to preserve. Layer 0 is the answer an unpainted
    // sample already gives, which keeps "no weights" meaning one thing rather than two.
    const auto normalized = total > 0.f ? clamped / total : Vector4f(1.f, 0.f, 0.f, 0.f);

    int encodedTotal = 0;
    int largest = 0;

    for (int layer = 0; layer < MaximumLayerCount; layer++)
    {
        const auto encoded = static_cast<int>(std::lround(normalized[layer] * WEIGHT_MAXIMUM));

        destination[layer] = static_cast<std::uint8_t>(encoded);

        encodedTotal += encoded;

        if (normalized[layer] > normalized[largest])
        {
            largest = layer;
        }
    }

    // Four independently rounded weights land a step either side of the total rather than on it.
    // Correcting the largest channel costs it the least in relative terms, and it keeps the stored
    // weights summing to exactly one so that reading them back gives what was written.
    destination[largest] = static_cast<std::uint8_t>(
        std::clamp(static_cast<int>(destination[largest]) + (255 - encodedTotal), 0, 255));
}

Vector4f Terrain::DecodeSampleWeights(const std::uint8_t* source)
{
    Vector4f weights{};

    // Over MaximumLayerCount rather than the four components spelled out, so that this and
    // EncodeSampleWeights describe the same number of channels in the same way.
    for (int layer = 0; layer < MaximumLayerCount; layer++)
    {
        weights[layer] = static_cast<float>(source[layer]) / WEIGHT_MAXIMUM;
    }

    return weights;
}

std::optional<Vector4f> Terrain::GetSampleWeights(const Vector2i sample) const
{
    if (!IsSampleInside(sample))
    {
        return std::nullopt;
    }

    return DecodeSampleWeights(&m_LayerWeights[GetWeightIndex(sample)]);
}

bool Terrain::SetSampleWeights(const Vector2i sample, const Vector4f& weights)
{
    if (!IsSampleInside(sample))
    {
        return false;
    }

    EncodeSampleWeights(weights, &m_LayerWeights[GetWeightIndex(sample)]);

    m_IsSplatMapDirty = true;

    return true;
}

std::vector<std::uint8_t> Terrain::GetSampleWeightRect(const TerrainSampleRect& rect) const
{
    if (!IsSampleRectInside(rect))
    {
        return {};
    }

    const int width = rect.GetWidth();

    std::vector<std::uint8_t> weights;

    weights.reserve(static_cast<std::size_t>(width) * rect.GetHeight() * MaximumLayerCount);

    for (int z = rect.Min.y; z <= rect.Max.y; z++)
    {
        const auto rowStart = GetWeightIndex({ rect.Min.x, z });

        weights.insert(weights.end(),
                       m_LayerWeights.begin() + static_cast<std::ptrdiff_t>(rowStart),
                       m_LayerWeights.begin() + static_cast<std::ptrdiff_t>(rowStart) + width * MaximumLayerCount);
    }

    return weights;
}

bool Terrain::SetSampleWeightRect(const TerrainSampleRect& rect, const std::vector<std::uint8_t>& weights)
{
    if (!IsSampleRectInside(rect))
    {
        return false;
    }

    const int width = rect.GetWidth();
    const int height = rect.GetHeight();

    const auto expected = static_cast<std::size_t>(width) * height * MaximumLayerCount;

    if (weights.size() != expected)
    {
        PWarning(fmt::format("Ignored a terrain weight rectangle of {} byte(s): its {}x{} region holds {}.",
                             weights.size(), width, height, expected));
        return false;
    }

    const auto rowLength = static_cast<std::ptrdiff_t>(width) * MaximumLayerCount;

    for (int z = 0; z < height; z++)
    {
        const auto rowStart = GetWeightIndex({ rect.Min.x, rect.Min.y + z });
        const auto sourceStart = static_cast<std::ptrdiff_t>(z) * rowLength;

        std::copy(weights.begin() + sourceStart,
                  weights.begin() + sourceStart + rowLength,
                  m_LayerWeights.begin() + static_cast<std::ptrdiff_t>(rowStart));
    }

    // No MarkRegionDirty here, unlike the height rectangle: a chunk mesh carries no weights, so
    // rebuilding one would produce exactly the same vertices. The splat texture is the only thing
    // downstream of this field.
    m_IsSplatMapDirty = true;

    return true;
}

const std::vector<std::uint8_t>& Terrain::GetLayerWeightField() const
{
    return m_LayerWeights;
}

/* Splat map */

Graphics::ITexture* Terrain::GetSplatMap() const
{
    return m_SplatMap;
}

Vector4f Terrain::GetSplatTransform() const
{
    const auto fieldSize = Vector2f(GetFieldSize());
    const auto sampleMin = Vector2f(GetSampleMin());
    const float spacing = GetSampleSpacing();

    // A chunk mesh's uv is its sample coordinate times the sample spacing, so dividing by that
    // spacing recovers the coordinate. The half sample shifts it onto the centre of that sample's
    // texel rather than onto its corner, which is where the stored weight actually is.
    return {
        1.f / (spacing * fieldSize.x),
        1.f / (spacing * fieldSize.y),
        (0.5f - sampleMin.x) / fieldSize.x,
        (0.5f - sampleMin.y) / fieldSize.y
    };
}

void Terrain::RebuildDirtySplatMap()
{
    PINE_PF_SCOPE();

    if (!m_IsSplatMapDirty)
    {
        return;
    }

    const auto fieldSize = GetFieldSize();

    if (m_SplatMap == nullptr)
    {
        m_SplatMap = Graphics::GetGraphicsAPI()->CreateTexture();
    }

    m_SplatMap->Bind();

    // Weights are data rather than colour, so no sRGB decode: bending them through a gamma curve
    // would move the blend away from what was painted. Linear filtering is what turns the border
    // between two layers into a gradient instead of a staircase of sample-sized squares, and
    // clamping keeps the far rim's weights from wrapping onto the near one.
    m_SplatMap->SetSRGB(false);
    m_SplatMap->SetFilteringMode(Graphics::TextureFilteringMode::Linear);
    m_SplatMap->SetTextureWrapMode(Graphics::TextureWrapMode::ClampToEdge);

    // A full upload rather than a sub-image of what changed: the whole field is four bytes a
    // sample, so even a large terrain is a couple of megabytes, and a resize changes the
    // dimensions anyway.
    m_SplatMap->UploadTextureData(fieldSize.x, fieldSize.y, 0,
        Graphics::TextureFormat::RGBA, Graphics::TextureDataFormat::UnsignedByte, m_LayerWeights.data());

    m_IsSplatMapDirty = false;
}

void Terrain::DestroySplatMap()
{
    if (m_SplatMap == nullptr)
    {
        return;
    }

    Graphics::GetGraphicsAPI()->DestroyTexture(m_SplatMap);

    m_SplatMap = nullptr;
    m_IsSplatMapDirty = true;
}

/* Noise */

const TerrainNoiseSettings& Terrain::GetNoiseSettings() const
{
    return m_NoiseSettings;
}

void Terrain::SetNoiseSettings(const TerrainNoiseSettings& settings)
{
    m_NoiseSettings = settings;
}

void Terrain::GenerateFromNoise()
{
    PINE_PF_SCOPE();

    const siv::PerlinNoise perlin{ static_cast<std::uint32_t>(m_NoiseSettings.Seed) };

    const auto sampleMin = GetSampleMin();
    const auto sampleMax = GetSampleMax();

    for (int z = sampleMin.y; z <= sampleMax.y; z++)
    {
        for (int x = sampleMin.x; x <= sampleMax.x; x++)
        {
            float height = 0.f;

            // Summed in order, and each band is skipped where the bands before it have not taken
            // the ground above its cutoff - which is what lets a detail band break up the hills
            // the coarse bands raised and leave the lowland between them smooth.
            for (const auto& band : m_NoiseSettings.Bands)
            {
                if (height <= band.Cutoff)
                {
                    continue;
                }

                height += static_cast<float>(perlin.octave2D_11(
                    x * band.CoordinateScale,
                    z * band.CoordinateScale,
                    band.Octaves)) * band.Scale;
            }

            m_Heights[GetSampleIndex({ x, z })] = EncodeHeight(height);
        }
    }

    RebuildChunks();
}

/* Chunks */

const std::vector<TerrainChunk>& Terrain::GetChunks() const
{
    return m_Chunks;
}

std::vector<TerrainChunk>& Terrain::GetChunks()
{
    return m_Chunks;
}

void Terrain::UpdateChunkBounds(TerrainChunk& chunk) const
{
    const auto sampleMin = chunk.Coordinate * m_ChunkQuads;
    const auto sampleMax = sampleMin + Vector2i(m_ChunkQuads);

    float lowest = std::numeric_limits<float>::max();
    float highest = std::numeric_limits<float>::lowest();

    // Inclusive of the far edge, which this chunk shares with its neighbour.
    for (int z = sampleMin.y; z <= sampleMax.y; z++)
    {
        for (int x = sampleMin.x; x <= sampleMax.x; x++)
        {
            const float height = DecodeHeight(m_Heights[GetSampleIndex({ x, z })]);

            lowest = std::min(lowest, height);
            highest = std::max(highest, height);
        }
    }

    const auto cornerLow = Vector2f(chunk.Coordinate) * m_ChunkSize;
    const auto cornerHigh = cornerLow + Vector2f(m_ChunkSize);

    chunk.BoundsMin = { cornerLow.x, lowest, cornerLow.y };
    chunk.BoundsMax = { cornerHigh.x, highest, cornerHigh.y };

    // The box is where the chunk's lights are picked from, so a chunk that just changed shape has
    // to pick them again. This is the one place a chunk's box moves, which makes it the one place
    // that has to say so.
    chunk.LightSlots.HasComputedData = false;
}

void Terrain::RebuildChunks()
{
    DestroyAllChunkMeshes();

    m_Chunks.clear();
    m_Chunks.resize(static_cast<std::size_t>(m_ChunkCount.x) * m_ChunkCount.y);

    for (int z = 0; z < m_ChunkCount.y; z++)
    {
        for (int x = 0; x < m_ChunkCount.x; x++)
        {
            auto& chunk = m_Chunks[static_cast<std::size_t>(z) * m_ChunkCount.x + x];

            chunk.Coordinate = m_ChunkOrigin + Vector2i(x, z);

            UpdateChunkBounds(chunk);
        }
    }
}

void Terrain::MarkRegionDirty(const TerrainSampleRect& rect)
{
    // A sample sitting exactly on a chunk edge belongs to the chunks on both sides of it, so the
    // dirty range reaches one chunk further back than the low corner's own chunk.
    const Vector2i firstChunk = {
        FloorDivide(rect.Min.x - 1, m_ChunkQuads),
        FloorDivide(rect.Min.y - 1, m_ChunkQuads)
    };

    const Vector2i lastChunk = {
        FloorDivide(rect.Max.x, m_ChunkQuads),
        FloorDivide(rect.Max.y, m_ChunkQuads)
    };

    for (auto& chunk : m_Chunks)
    {
        if (chunk.Coordinate.x < firstChunk.x || chunk.Coordinate.x > lastChunk.x ||
            chunk.Coordinate.y < firstChunk.y || chunk.Coordinate.y > lastChunk.y)
        {
            continue;
        }

        chunk.IsDirty = true;

        UpdateChunkBounds(chunk);
    }
}

/* Meshes */

bool Terrain::IsSampleRectInside(const TerrainSampleRect& rect) const
{
    if (rect.IsEmpty())
    {
        PWarning("Ignored a terrain sample rectangle whose low corner is above its high one.");
        return false;
    }

    const auto fieldMin = GetSampleMin();
    const auto fieldMax = GetSampleMax();

    if (rect.Min.x < fieldMin.x || rect.Min.y < fieldMin.y ||
        rect.Max.x > fieldMax.x || rect.Max.y > fieldMax.y)
    {
        PWarning(fmt::format("Ignored a terrain sample rectangle ({}, {})-({}, {}) reaching outside "
                             "the field's ({}, {})-({}, {}).",
                             rect.Min.x, rect.Min.y, rect.Max.x, rect.Max.y,
                             fieldMin.x, fieldMin.y, fieldMax.x, fieldMax.y));
        return false;
    }

    return true;
}

std::optional<TerrainRayHit> Terrain::IntersectQuad(const Vector2i quad, const Vector3f& origin, const Vector3f& direction) const
{
    const float spacing = GetSampleSpacing();

    const auto corner = [this, spacing](const Vector2i sample)
    {
        return Vector3f(static_cast<float>(sample.x) * spacing,
                        DecodeHeight(m_Heights[GetSampleIndex(sample)]),
                        static_cast<float>(sample.y) * spacing);
    };

    const auto lowLow = corner(quad);
    const auto highLow = corner({ quad.x + 1, quad.y });
    const auto lowHigh = corner({ quad.x, quad.y + 1 });
    const auto highHigh = corner({ quad.x + 1, quad.y + 1 });

    // The same two triangles BuildChunkMesh emits, split along the same diagonal
    // IsInFirstQuadTriangle documents - a pick that used the other diagonal would miss the surface
    // by up to the height difference across the quad, and only on half of every quad.
    auto nearest = IntersectTriangle(origin, direction, lowLow, lowHigh, highLow);
    const auto second = IntersectTriangle(origin, direction, highLow, lowHigh, highHigh);

    // A ray can meet both, along the diagonal they share or when it runs nearly flat across the
    // quad, so the two are compared rather than the first answer taken.
    if (second.has_value() && (!nearest.has_value() || *second < *nearest))
    {
        nearest = second;
    }

    if (!nearest.has_value())
    {
        return std::nullopt;
    }

    return TerrainRayHit{ origin + direction * *nearest, *nearest };
}

float Terrain::GetClampedSampleHeight(const Vector2i sample) const
{
    const auto sampleMin = GetSampleMin();
    const auto sampleMax = GetSampleMax();

    const Vector2i clamped = {
        std::clamp(sample.x, sampleMin.x, sampleMax.x),
        std::clamp(sample.y, sampleMin.y, sampleMax.y)
    };

    return DecodeHeight(m_Heights[GetSampleIndex(clamped)]);
}

Vector2f Terrain::ComputeSampleSlopes(const Vector2i sample) const
{
    const float spacing = GetSampleSpacing();

    const auto sampleMin = GetSampleMin();
    const auto sampleMax = GetSampleMax();

    // Central differences over the shared field. This is the payoff of one field rather than one
    // array per chunk: a sample on a chunk edge reads its neighbour's samples, so both chunks
    // compute the same slope there and the seam disappears instead of being smoothed over.
    const auto slopeAlong = [&](const Vector2i step)
    {
        const Vector2i lower = glm::clamp(sample - step, sampleMin, sampleMax);
        const Vector2i upper = glm::clamp(sample + step, sampleMin, sampleMax);

        // One of the two neighbours does not exist along the terrain's outer rim, and clamping
        // collapsed it onto the sample itself - so the span is measured rather than assumed. A
        // one-sided difference over the full central distance would halve the slope and leave the
        // rim lit as if it were flatter than it is.
        const float span = static_cast<float>((upper.x - lower.x) + (upper.y - lower.y)) * spacing;

        if (span <= 0.f)
        {
            return 0.f;
        }

        return (GetClampedSampleHeight(upper) - GetClampedSampleHeight(lower)) / span;
    };

    return { slopeAlong({ 1, 0 }), slopeAlong({ 0, 1 }) };
}

void Terrain::AppendChunkSkirt(const int quads,
                               const float skirtDepth,
                               std::vector<Vector3f>& vertices,
                               std::vector<Vector3f>& normals,
                               std::vector<Vector3f>& tangents,
                               std::vector<Vector2f>& uvs,
                               std::vector<std::uint32_t>& indices)
{
    const int verticesPerEdge = quads + 1;

    // The chunk's boundary vertices in one ring, walked so that the outside is always to the left
    // of the direction of travel: +x along the low z edge, then +z, then -x, then -z. Building the
    // ring once means the four edges need one winding rule between them rather than four, which is
    // where a skirt normally goes wrong - three edges face out and the fourth is culled away.
    std::vector<std::uint32_t> ring;

    ring.reserve(static_cast<std::size_t>(quads) * 4);

    const auto gridVertex = [verticesPerEdge](const int x, const int z)
    {
        return static_cast<std::uint32_t>(z * verticesPerEdge + x);
    };

    // Each edge stops one short of its far corner, which is where the next edge starts.
    for (int x = 0; x < quads; x++)
    {
        ring.push_back(gridVertex(x, 0));
    }

    for (int z = 0; z < quads; z++)
    {
        ring.push_back(gridVertex(quads, z));
    }

    for (int x = quads; x > 0; x--)
    {
        ring.push_back(gridVertex(x, quads));
    }

    for (int z = quads; z > 0; z--)
    {
        ring.push_back(gridVertex(0, z));
    }

    // One lowered copy per ring vertex, carrying the edge vertex's own normal, tangent and uv. A
    // skirt is meant to be mistaken for the ground it hangs off, and giving it an outward-facing
    // normal of its own is what would make it read as a wall instead.
    const auto firstSkirtVertex = static_cast<std::uint32_t>(vertices.size());

    // Reserved before the loop because each push_back below reads an element of the vector it is
    // appending to. That is well defined, but it is easier to be sure of when nothing can reallocate
    // half way through.
    vertices.reserve(vertices.size() + ring.size());
    normals.reserve(normals.size() + ring.size());
    tangents.reserve(tangents.size() + ring.size());
    uvs.reserve(uvs.size() + ring.size());
    indices.reserve(indices.size() + ring.size() * 6);

    for (const auto edgeVertex : ring)
    {
        vertices.push_back(vertices[edgeVertex] - Vector3f(0.f, skirtDepth, 0.f));
        normals.push_back(normals[edgeVertex]);
        tangents.push_back(tangents[edgeVertex]);
        uvs.push_back(uvs[edgeVertex]);
    }

    for (std::size_t segment = 0; segment < ring.size(); segment++)
    {
        // Wrapping closes the ring at the last segment, which runs from the final -z edge vertex
        // back to the corner the first edge started from.
        const std::size_t next = (segment + 1) % ring.size();

        const auto topLeft = ring[segment];
        const auto topRight = ring[next];
        const auto bottomLeft = firstSkirtVertex + static_cast<std::uint32_t>(segment);
        const auto bottomRight = firstSkirtVertex + static_cast<std::uint32_t>(next);

        indices.push_back(topLeft);
        indices.push_back(topRight);
        indices.push_back(bottomLeft);

        indices.push_back(topRight);
        indices.push_back(bottomRight);
        indices.push_back(bottomLeft);
    }
}

void Terrain::BuildChunkMesh(TerrainChunk& chunk, const int lodLevel) const
{
    PINE_PF_SCOPE();

    // Every level keeps every 2^l-th sample, so a coarse mesh is a subset of the fine one rather
    // than an average of it: the chunk's corner and edge samples survive to the coarsest level, and
    // two neighbours at the same level still meet exactly.
    const int sampleStride = 1 << lodLevel;
    const int quads = m_ChunkQuads / sampleStride;

    const int verticesPerEdge = quads + 1;
    const float sampleSpacing = GetSampleSpacing();
    const float vertexSpacing = sampleSpacing * static_cast<float>(sampleStride);

    const auto chunkSampleOrigin = chunk.Coordinate * m_ChunkQuads;

    const auto vertexCount = static_cast<std::size_t>(verticesPerEdge) * verticesPerEdge;
    const auto indexCount = static_cast<std::size_t>(quads) * quads * 6;

    std::vector<Vector3f> vertices(vertexCount);
    std::vector<Vector3f> normals(vertexCount);
    std::vector<Vector3f> tangents(vertexCount);
    std::vector<Vector2f> uvs(vertexCount);
    std::vector<std::uint32_t> indices(indexCount);

    for (int z = 0; z < verticesPerEdge; z++)
    {
        for (int x = 0; x < verticesPerEdge; x++)
        {
            const Vector2i sample = chunkSampleOrigin + Vector2i(x, z) * sampleStride;
            const auto vertex = static_cast<std::size_t>(z) * verticesPerEdge + x;

            const float height = DecodeHeight(m_Heights[GetSampleIndex(sample)]);

            // Slopes come from the neighbouring *samples*, not from the neighbouring vertices of
            // this level, so a chunk drawn coarsely is still lit by the surface the height field
            // describes. Deriving them from the level's own vertices would make the same ground
            // change shade as it crossed an LOD boundary.
            const auto slopes = ComputeSampleSlopes(sample);

            // Chunk-local positions, with the chunk's world offset carried by the transform the
            // renderer draws it with. Terrain-local positions would put the far corner of a large
            // terrain thousands of units from the mesh origin for no gain.
            vertices[vertex] = { static_cast<float>(x) * vertexSpacing, height, static_cast<float>(z) * vertexSpacing };

            normals[vertex] = glm::normalize(Vector3f(-slopes.x, 1.f, -slopes.y));

            // The surface tangent along +x, which is the direction u runs in - so a normal-mapped
            // terrain material gets a tangent basis that agrees with its texture.
            tangents[vertex] = glm::normalize(Vector3f(1.f, slopes.x, 0.f));

            // Terrain-local world units rather than a 0..1 span per chunk, so the texture is
            // continuous across chunk edges whatever the material's UV scale is, and so that scale
            // means "repeats per world unit" regardless of how the terrain is chunked.
            uvs[vertex] = Vector2f(sample) * sampleSpacing;
        }
    }

    std::size_t index = 0;

    for (int z = 0; z < quads; z++)
    {
        for (int x = 0; x < quads; x++)
        {
            const auto lowLow = static_cast<std::uint32_t>(z * verticesPerEdge + x);
            const auto highLow = lowLow + 1;
            const auto lowHigh = lowLow + verticesPerEdge;
            const auto highHigh = lowHigh + 1;

            // Split along the diagonal from (x, z + 1) to (x + 1, z), which is the convention
            // IsInFirstQuadTriangle documents and GetHeightAt interpolates against - the rendered
            // surface and the queried one have to be the same surface. Wound counter-clockwise seen
            // from above so the ground faces up under back-face culling.
            indices[index++] = lowLow;
            indices[index++] = lowHigh;
            indices[index++] = highLow;

            indices[index++] = highLow;
            indices[index++] = lowHigh;
            indices[index++] = highHigh;
        }
    }

    // Hanging down by the chunk's own height range is what makes the skirt provably tall enough:
    // a neighbour drawn coarsely can only miss this chunk's shared edge by as much as that edge
    // rises and falls, and that is inside the range. A flat chunk gets no skirt, correctly - its
    // edge samples are the same at every level, so there is nothing to hide.
    AppendChunkSkirt(quads, chunk.BoundsMax.y - chunk.BoundsMin.y, vertices, normals, tangents, uvs, indices);

    auto& mesh = chunk.LodMeshes[lodLevel];

    // How many vertices a level has depends only on the chunk's quad count, so a sculpting stroke
    // changes what is in the buffers and nothing about their shape - and the existing mesh can take
    // the new values in place. That matters because a stroke rebuilds every level of every chunk it
    // touches, once per frame while the brush is dragged, and building a fresh Mesh allocates a
    // vertex array and five GPU buffers that the array only frees when it is disposed.
    //
    // The quad count does change, on SetChunkQuads, and it does not always change the number of
    // levels along with it - so this compares the vertex count rather than assuming.
    const bool canUpdateInPlace = mesh != nullptr && mesh->GetVertexCount() == vertices.size();

    if (canUpdateInPlace)
    {
        mesh->UpdateVertices(reinterpret_cast<const float*>(vertices.data()), vertices.size() * sizeof(Vector3f));
        mesh->UpdateNormals(reinterpret_cast<const float*>(normals.data()), normals.size() * sizeof(Vector3f));
        mesh->UpdateTangents(reinterpret_cast<const float*>(tangents.data()), tangents.size() * sizeof(Vector3f));
        mesh->UpdateUvs(reinterpret_cast<const float*>(uvs.data()), uvs.size() * sizeof(Vector2f));

        // The indices are deliberately left alone: the triangles of a level are fixed by its quad
        // count, so the same index buffer describes the moved vertices.
    }
    else
    {
        if (mesh != nullptr)
        {
            mesh->Dispose();

            delete mesh;
        }

        mesh = new Mesh(nullptr);

        // DynamicDraw because the branch above is the common case for a terrain being sculpted:
        // these buffers are written far more often than a model's, which are filled once on import.
        constexpr auto usage = Graphics::BufferUsageHint::DynamicDraw;

        // Vertices before indices: SetVertices sets the render count to a vertex count, and
        // SetIndices is what corrects it to the index count the draw call actually needs.
        mesh->SetVertices(reinterpret_cast<float*>(vertices.data()), vertices.size() * sizeof(Vector3f), usage);
        mesh->SetNormals(reinterpret_cast<float*>(normals.data()), normals.size() * sizeof(Vector3f), usage);
        mesh->SetTangents(reinterpret_cast<float*>(tangents.data()), tangents.size() * sizeof(Vector3f), usage);
        mesh->SetUvs(reinterpret_cast<float*>(uvs.data()), uvs.size() * sizeof(Vector2f), usage);
        mesh->SetIndices(indices.data(), indices.size() * sizeof(std::uint32_t));
    }

    // The chunk's bounds are terrain-local and the mesh is chunk-local, so only the horizontal
    // offset differs; the height range is the same range either way. The skirt hangs below the
    // bounds on purpose and is deliberately not included: the bounds are what culling tests, and a
    // chunk whose skirt is visible but whose ground is not has nothing worth drawing.
    const auto chunkCorner = Vector3f(chunk.BoundsMin.x, 0.f, chunk.BoundsMin.z);

    mesh->SetAABB(chunk.BoundsMin - chunkCorner, chunk.BoundsMax - chunkCorner);
}

bool Terrain::RebuildDirtyChunkMeshes()
{
    PINE_PF_SCOPE();

    const int lodCount = GetLodCount();

    bool hasRebuiltAnything = false;

    for (auto& chunk : m_Chunks)
    {
        if (!chunk.IsDirty)
        {
            continue;
        }

        // Resized rather than cleared, so a chunk rebuilt after a resolution change drops the
        // levels it no longer has instead of keeping stale meshes under them.
        if (static_cast<int>(chunk.LodMeshes.size()) != lodCount)
        {
            DestroyChunkMeshes(chunk);

            chunk.LodMeshes.resize(lodCount, nullptr);
        }

        for (int lodLevel = 0; lodLevel < lodCount; lodLevel++)
        {
            BuildChunkMesh(chunk, lodLevel);
        }

        chunk.IsDirty = false;
        hasRebuiltAnything = true;
    }

    return hasRebuiltAnything;
}

int Terrain::GetLodCount() const
{
    int count = 1;

    while (count < MaximumLodCount &&
           m_ChunkQuads % (1 << count) == 0 &&
           m_ChunkQuads >> count >= MinimumLodQuads)
    {
        count++;
    }

    return count;
}

std::uint32_t Terrain::GetChunkGroundIndexCount(const int lodLevel) const
{
    const int level = std::clamp(lodLevel, 0, GetLodCount() - 1);
    const int quads = m_ChunkQuads >> level;

    // Two triangles per quad, three indices each - the same grid BuildChunkMesh writes before it
    // appends the skirt.
    return static_cast<std::uint32_t>(quads) * quads * 6;
}

Mesh* Terrain::GetChunkMesh(const TerrainChunk& chunk, const int lodLevel) const
{
    if (chunk.LodMeshes.empty())
    {
        return nullptr;
    }

    // Clamped in int space: a viewer asks for a level by distance, and this terrain may carry fewer
    // levels than the one the caller last looked at.
    const auto coarsest = static_cast<int>(chunk.LodMeshes.size()) - 1;

    return chunk.LodMeshes[std::clamp(lodLevel, 0, coarsest)];
}

void Terrain::DestroyChunkMeshes(TerrainChunk& chunk)
{
    for (const auto mesh : chunk.LodMeshes)
    {
        if (mesh == nullptr)
        {
            continue;
        }

        mesh->Dispose();

        delete mesh;
    }

    chunk.LodMeshes.clear();
}

void Terrain::DestroyAllChunkMeshes()
{
    for (auto& chunk : m_Chunks)
    {
        DestroyChunkMeshes(chunk);
    }
}

/* Serialization */

bool Terrain::LoadAssetData(const ByteSpan& span)
{
    TerrainSerializer terrainSerializer;

    if (!terrainSerializer.Read(span))
    {
        return false;
    }

    // Read into locals and commit further down, once the layout is known to be usable. This runs
    // on a re-load too, and a false return there leaves the terrain registered and rendering with
    // whatever it already had (see Asset::ReLoad) - so a file that gets refused has to leave it
    // exactly as it was. Applying the layout first would leave the sample count describing the new
    // file while the field still held the old one's samples, and every index below is derived from
    // that layout.
    Vector2i chunkCount{};
    Vector2i chunkOrigin{};

    int chunkQuads = 0;
    float chunkSize = 0.f;
    float heightMin = 0.f;
    float heightMax = 0.f;

    terrainSerializer.ChunkCountX.Read(chunkCount.x);
    terrainSerializer.ChunkCountZ.Read(chunkCount.y);
    terrainSerializer.ChunkOriginX.Read(chunkOrigin.x);
    terrainSerializer.ChunkOriginZ.Read(chunkOrigin.y);
    terrainSerializer.ChunkQuads.Read(chunkQuads);
    terrainSerializer.ChunkSize.Read(chunkSize);
    terrainSerializer.HeightMin.Read(heightMin);
    terrainSerializer.HeightMax.Read(heightMax);

    // A file that disagrees with itself is worse than one that will not load at all: a grid too
    // large to index would take the field down with it.
    if (!IsLayoutSupported(chunkOrigin, chunkCount, chunkQuads) ||
        chunkSize <= 0.f || heightMax <= heightMin)
    {
        PError(fmt::format("Terrain '{}' has an invalid layout, refusing to load it.", m_Path));
        return false;
    }

    m_ChunkCount = chunkCount;
    m_ChunkOrigin = chunkOrigin;
    m_ChunkQuads = chunkQuads;
    m_ChunkSize = chunkSize;
    m_HeightMin = heightMin;
    m_HeightMax = heightMax;

    std::vector<UId> layers;

    terrainSerializer.Layers.Read(layers);

    // Emptied first, because this also runs when an already-loaded terrain is re-loaded: a slot the
    // new file does not mention has to end up empty rather than keeping what the previous one put
    // there. The height field gets this for free - reading it replaces the whole vector.
    for (auto& layer : m_Layers)
    {
        layer = static_cast<Material*>(nullptr);
    }

    // Only as many as this build has slots for. The format carries its own count so that a file
    // written by a build with more layers still loads here - with its extra layers dropped, which
    // is the honest outcome when there is no channel left to blend them through.
    for (std::size_t layer = 0; layer < layers.size() && layer < MaximumLayerCount; layer++)
    {
        m_Layers[layer] = layers[layer];
    }

    // Back to the defaults first, for the same reason the layers are: a band the new file does not
    // mention must not keep what a previous load left in it.
    m_NoiseSettings = TerrainNoiseSettings();

    ByteSpan noiseSpan;

    if (terrainSerializer.NoiseSettings.Read(noiseSpan))
    {
        TerrainNoiseSerializer noiseSerializer;

        if (noiseSerializer.Read(noiseSpan))
        {
            noiseSerializer.Seed.Read(m_NoiseSettings.Seed);

            const auto bandCount = std::min(noiseSerializer.Bands.GetDataCount(),
                                            static_cast<std::size_t>(TerrainNoiseSettings::BandCount));

            for (std::size_t index = 0; index < bandCount; index++)
            {
                TerrainNoiseBandSerializer bandSerializer;

                bandSerializer.Read(noiseSerializer.Bands.GetData(index));

                auto& band = m_NoiseSettings.Bands[index];

                bandSerializer.CoordinateScale.Read(band.CoordinateScale);
                bandSerializer.Octaves.Read(band.Octaves);
                bandSerializer.Scale.Read(band.Scale);
                bandSerializer.Cutoff.Read(band.Cutoff);
            }
        }
    }

    terrainSerializer.Heights.Read(m_Heights);

    const auto fieldSize = GetFieldSize();
    const auto expectedSampleCount = static_cast<std::size_t>(fieldSize.x) * fieldSize.y;

    if (m_Heights.size() != expectedSampleCount)
    {
        PWarning(fmt::format(
            "Terrain '{}' stores {} height samples but its layout needs {}, resetting it to flat.",
            m_Path, m_Heights.size(), expectedSampleCount));

        ResetHeightField();
    }

    terrainSerializer.LayerWeights.Read(m_LayerWeights);

    // A terrain saved before the weights existed has none, and one saved by a build with a wider
    // splat format has too many to interpret against this one. Both mean the same thing here: the
    // heights are still good, the painting is not, so the surface falls back to bare layer 0
    // rather than to a blend read out of the wrong stride.
    if (m_LayerWeights.size() != expectedSampleCount * MaximumLayerCount)
    {
        if (!m_LayerWeights.empty())
        {
            PWarning(fmt::format(
                "Terrain '{}' stores {} layer weights but its layout needs {}, resetting them to layer 0.",
                m_Path, m_LayerWeights.size(), expectedSampleCount * MaximumLayerCount));
        }

        ResetLayerWeights();
    }

    m_IsSplatMapDirty = true;

    RebuildChunks();

    return true;
}

ByteSpan Terrain::SaveAssetData()
{
    TerrainSerializer terrainSerializer;

    terrainSerializer.ChunkCountX.Write(m_ChunkCount.x);
    terrainSerializer.ChunkCountZ.Write(m_ChunkCount.y);
    terrainSerializer.ChunkOriginX.Write(m_ChunkOrigin.x);
    terrainSerializer.ChunkOriginZ.Write(m_ChunkOrigin.y);
    terrainSerializer.ChunkQuads.Write(m_ChunkQuads);
    terrainSerializer.ChunkSize.Write(m_ChunkSize);
    terrainSerializer.HeightMin.Write(m_HeightMin);
    terrainSerializer.HeightMax.Write(m_HeightMax);
    terrainSerializer.Heights.Write(m_Heights);
    terrainSerializer.LayerWeights.Write(m_LayerWeights);

    std::vector<UId> layers;

    layers.reserve(m_Layers.size());

    for (const auto& layer : m_Layers)
    {
        layers.push_back(layer.GetUId());
    }

    terrainSerializer.Layers.Write(layers);

    TerrainNoiseSerializer noiseSerializer;

    noiseSerializer.Seed.Write(m_NoiseSettings.Seed);

    for (const auto& band : m_NoiseSettings.Bands)
    {
        TerrainNoiseBandSerializer bandSerializer;

        bandSerializer.CoordinateScale.Write(band.CoordinateScale);
        bandSerializer.Octaves.Write(band.Octaves);
        bandSerializer.Scale.Write(band.Scale);
        bandSerializer.Cutoff.Write(band.Cutoff);

        noiseSerializer.Bands.AddData(bandSerializer.Write());
    }

    terrainSerializer.NoiseSettings.Write(noiseSerializer.Write());

    return terrainSerializer.Write();
}

void Terrain::Dispose()
{
    // The cooked PhysX height field joins this once the physics unit lands.
    DestroyAllChunkMeshes();
    DestroySplatMap();

    m_Chunks.clear();
}
