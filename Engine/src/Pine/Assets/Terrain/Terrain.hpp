#pragma once
#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Assets/Model/Model.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Rendering/Renderer3D/LightSlotData.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace Pine
{
    class Mesh;

    namespace Graphics
    {
        class ITexture;
    }

    // A terrain is a rectangular grid of chunks over *one* shared height field. Chunks exist for
    // rendering (LOD and culling) and are views into the field rather than copies of it, so two
    // neighbouring chunks agree along their shared edge by construction.
    //
    // Three coordinate spaces appear throughout, and mixing them up is the easiest mistake to make
    // here:
    //
    //   chunk coordinate    A chunk's place on the grid. The terrain covers
    //                       [ChunkOrigin, ChunkOrigin + ChunkCount).
    //   sample coordinate   A height sample's place on the grid, ChunkQuads samples per chunk edge.
    //                       Sample (0, 0) sits at terrain-local (0, 0).
    //   terrain-local       World units, origin at chunk coordinate (0, 0). The component adds the
    //                       entity transform; the asset knows nothing about entities.
    //
    // Chunk and sample coordinates are anchored to chunk (0, 0), so growing the terrain on the -x
    // or -z edge only moves ChunkOrigin. Flat indices into the height field change on every
    // resize, so nothing outside this class may hold one across one.
    struct TerrainChunk
    {
        Vector2i Coordinate{};

        // Terrain-local bounds, the y range taken from this chunk's own samples. The renderer culls
        // against this and the editor frames on it.
        Vector3f BoundsMin{};
        Vector3f BoundsMax{};

        // Set when a sample this chunk covers changed. RebuildDirtyChunkMeshes() is what acts on
        // it for the mesh below; collision picks it up in its own unit.
        bool IsDirty = true;

        // The lights that reach this chunk, assigned from the centre of its box by the scene
        // processor. Per chunk because a whole terrain is too large to be lit from one point.
        // Runtime only, like the meshes below.
        Renderer3D::LightSlotData LightSlots;

        // One renderable mesh per detail level, finest first, built by RebuildDirtyChunkMeshes()
        // and empty until then. Read it through Terrain::GetChunkMesh, which clamps the level.
        std::vector<Mesh*> LodMeshes;

        // Changes whenever something this chunk's detail placements are generated from changes:
        // its heights, the layer weights it covers, or the terrain's detail types. Unique across
        // every terrain in the process, so a renderer caching placements can compare it without
        // also tracking which terrain object stamped it. Runtime only.
        std::uint64_t DetailRevision = 0;
    };

    // Small scenery scattered over the ground wherever one layer is painted: a grass clump, a fern,
    // a pebble. Only the rule is stored. The placements are regenerated from the height field and
    // the layer weights (Terrain::GenerateDetailInstances), so painting the layer is what adds or
    // removes it. Rendering::TerrainDetail draws it around the camera.
    struct TerrainDetailType
    {
        AssetHandle<Model> DetailModel;

        // The splat channel whose weight decides where this grows. Density follows the weight, so
        // ground painted half into this layer grows half as much.
        std::int32_t Layer = 0;

        // Instances per square world unit where the layer is painted at full weight.
        float Density = 1.f;

        // Each instance is scaled uniformly by a value picked between these.
        float ScaleMin = 0.8f;
        float ScaleMax = 1.2f;

        // In world units from the camera. Instances shrink into the ground as they approach it and
        // are not drawn past it.
        float DrawDistance = 40.f;
    };

    // One placement of a detail type, terrain-local like everything else here.
    struct TerrainDetailInstance
    {
        Vector3f Position{};
        float Scale = 1.f;

        // Rotation about the vertical axis, in radians.
        float Yaw = 0.f;

        // The normal the ground is shaded with where this placement stands (Terrain::GetNormalAt),
        // so the renderer can light the placement the way it lights the ground around it.
        Vector3f GroundNormal = { 0.f, 1.f, 0.f };
    };

    // One band of noise summed into the height field. Each band is an octave stack of its own at
    // its own frequency, so a coarse band gives the terrain its shape and a finer one breaks up the
    // ground the bands before it laid down. Unrelated to terrain layers, which are surface
    // materials.
    struct TerrainNoiseBand
    {
        // Noise units per world unit, so a smaller value stretches the same shape over more ground.
        float CoordinateScale = 0.004f;

        std::int32_t Octaves = 8;

        // How much height, in world units, this band adds at full strength.
        float Scale = 5.f;

        // The band is only added where the bands before it have already reached above this height,
        // which keeps lowland smooth while higher ground gets broken up. Lowest() means ungated.
        float Cutoff = std::numeric_limits<float>::lowest();
    };

    // Gives a freshly created terrain some shape before it is sculpted. Not meant as a
    // procedural-generation feature.
    struct TerrainNoiseSettings
    {
        // Shape, variation and detail. Raising it is a change to this number and nothing else.
        static constexpr int BandCount = 3;

        std::int32_t Seed = 123456;

        std::array<TerrainNoiseBand, BandCount> Bands;

        TerrainNoiseSettings()
        {
            // The last band is detail, gated at zero to keep it off the lowland.
            Bands.back().Cutoff = 0.f;
        }
    };

    // Where a ray met the terrain surface, in terrain-local coordinates like everything else here.
    struct TerrainRayHit
    {
        Vector3f Position{};

        // How far along the ray the hit is, in world units, for comparing against other candidates.
        float Distance = 0.f;

        // Unit geometric face normal, pointing out of the top of the height field.
        Vector3f Normal{};
    };

    // A rectangle of samples, inclusive on both corners - so a rectangle whose corners are equal
    // is one sample, and its width is Max.x - Min.x + 1. The unit the sample rectangle accessors
    // below work in, and the unit an editing tool records for its undo step.
    struct TerrainSampleRect
    {
        Vector2i Min{};
        Vector2i Max{};

        // True when the corners are the wrong way around, e.g. a rectangle clamped to nothing.
        bool IsEmpty() const
        {
            return Min.x > Max.x || Min.y > Max.y;
        }

        int GetWidth() const
        {
            return Max.x - Min.x + 1;
        }

        int GetHeight() const
        {
            return Max.y - Min.y + 1;
        }

        bool Contains(const Vector2i sample) const
        {
            return sample.x >= Min.x && sample.x <= Max.x && sample.y >= Min.y && sample.y <= Max.y;
        }

        // The smallest rectangle covering both.
        TerrainSampleRect Union(const TerrainSampleRect& other) const
        {
            return { glm::min(Min, other.Min), glm::max(Max, other.Max) };
        }
    };

    class Terrain : public Asset
    {
    public:
        // How many layers blend across the surface: Specifications::Samplers reserves four texture
        // units per texture type, and four weights are one RGBA8 splat texel. The stored format
        // carries its own count, so raising this does not break saved terrains.
        static constexpr int MaximumLayerCount = 4;

    private:
        Vector2i m_ChunkCount = { 4, 4 };
        Vector2i m_ChunkOrigin = { 0, 0 };

        int m_ChunkQuads = 64;
        float m_ChunkSize = 64.f;

        // The range the normalized samples in m_Heights map onto. Heights written outside it clamp.
        float m_HeightMin = -64.f;
        float m_HeightMax = 64.f;

        // GetFieldSize().x * GetFieldSize().y samples, row major in z.
        //
        // uint16 because PhysX's height field stores int16 samples anyway. Over a 128 unit range
        // this resolves ~0.002 units.
        std::vector<std::uint16_t> m_Heights;

        TerrainNoiseSettings m_NoiseSettings;

        // The material of each layer, by splat channel. A slot may be empty.
        std::array<AssetHandle<Material>, MaximumLayerCount> m_Layers;

        // How much of each layer every sample takes, MaximumLayerCount bytes per sample in the
        // same row-major layout as m_Heights. Normalized on write, so a sample's weights sum to
        // one. Uploaded as the splat texture below.
        std::vector<std::uint8_t> m_LayerWeights;

        std::vector<TerrainDetailType> m_DetailTypes;

        // Runtime only, never serialized.
        std::vector<TerrainChunk> m_Chunks;

        // m_LayerWeights on the GPU, one texel per sample. Runtime only, and null until
        // RebuildDirtySplatMap() has run.
        Graphics::ITexture* m_SplatMap = nullptr;
        bool m_IsSplatMapDirty = true;

        bool LoadAssetData(const ByteSpan& span) override;
        ByteSpan SaveAssetData() override;

        struct TerrainSerializer : Serialization::Serializer
        {
            // The grid fields are written as separate Int32s rather than as one Vec2, because
            // DataType::Vec2 is two floats - storing integers in one would make `EngineCli --dump`,
            // which is how this asset is checked, print nonsense.
            PINE_SERIALIZE_PRIMITIVE(ChunkCountX, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(ChunkCountZ, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(ChunkOriginX, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(ChunkOriginZ, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(ChunkQuads, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(ChunkSize, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(HeightMin, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(HeightMax, Serialization::DataType::Float32);
            PINE_SERIALIZE_ARRAY_FIXED(Heights, std::uint16_t);
            PINE_SERIALIZE_DATA(NoiseSettings);

            // A list rather than MaximumLayerCount fields, so the layer count can change without
            // changing the format. The weights' channel count is their length over the sample
            // count.
            PINE_SERIALIZE_ARRAY_FIXED(Layers, UId);
            PINE_SERIALIZE_ARRAY_FIXED(LayerWeights, std::uint8_t);

            // A list of data blocks, one per detail type. Terrains saved before it existed have
            // none.
            PINE_SERIALIZE_ARRAY(DetailTypes);
        };

        struct TerrainDetailTypeSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_ASSET(DetailModel);
            PINE_SERIALIZE_PRIMITIVE(Layer, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(Density, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(ScaleMin, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(ScaleMax, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(DrawDistance, Serialization::DataType::Float32);
        };

        // The bands are a list of data blocks, so BandCount can change without changing the format.
        // Bands past this build's count are dropped on load.
        struct TerrainNoiseSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_PRIMITIVE(Seed, Serialization::DataType::Int32);
            PINE_SERIALIZE_ARRAY(Bands);
        };

        struct TerrainNoiseBandSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_PRIMITIVE(CoordinateScale, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(Octaves, Serialization::DataType::Int32);
            PINE_SERIALIZE_PRIMITIVE(Scale, Serialization::DataType::Float32);
            PINE_SERIALIZE_PRIMITIVE(Cutoff, Serialization::DataType::Float32);
        };

        // A chunk stops getting coarser here. Below it the quads are wide enough that dropping
        // another sample changes the silhouette rather than just the triangle count.
        static constexpr int MinimumLodQuads = 8;

        // The most quads a terrain may span along one axis, counting every chunk. 4096 quads is a
        // 33 MB height field, a 67 MB weight field and a 4097x4097 splat texture, and still fits a
        // 64x64 grid of default 64-quad chunks.
        static constexpr int MaximumFieldQuadsPerAxis = 4096;

        // How far from chunk (0, 0) the grid may sit, independent of its span.
        static constexpr int MaximumSampleCoordinate = 1 << 24;

        // Whether a chunk grid of this shape describes a field this build can hold and index.
        // Worked out in 64 bits, because GetFieldSize() and GetSampleMin/Max are int arithmetic and
        // the grid may come from a corrupt '.passet'.
        static bool IsLayoutSupported(Vector2i chunkOrigin, Vector2i chunkCount, int chunkQuads);

        std::size_t GetSampleIndex(Vector2i sample) const;

        // Whether a sample coordinate is inside the field. Quiet, because single-sample queries
        // past the edge are normal.
        bool IsSampleInside(Vector2i sample) const;

        // Whether a sample rectangle is wholly inside the field. Warns when it is not, because
        // callers are expected to clamp their rectangles first.
        bool IsSampleRectInside(const TerrainSampleRect& rect) const;

        // Where a ray meets the two triangles of one quad, nearest first, or empty when it misses
        // both. The quad is given by its low corner sample and has to be inside the field.
        std::optional<TerrainRayHit> IntersectQuad(Vector2i quad, const Vector3f& origin, const Vector3f& direction) const;

        // Index of a sample's first weight byte, at MaximumLayerCount bytes per sample.
        std::size_t GetWeightIndex(Vector2i sample) const;

        void ResetHeightField();

        // Gives every sample entirely to layer 0, which is what an unpainted terrain looks like.
        void ResetLayerWeights();

        void UpdateChunkBounds(TerrainChunk& chunk) const;

        // The chunk coordinates whose samples a sample rectangle touches, as an inclusive
        // (first, last) pair. A sample on a chunk edge belongs to the chunks on both sides of it.
        std::pair<Vector2i, Vector2i> GetChunkRangeCovering(const TerrainSampleRect& rect) const;

        // Gives the chunk a new DetailRevision, from a counter shared by every terrain.
        static void StampDetailRevision(TerrainChunk& chunk);

        // Stamps every chunk covering a sample rectangle, for a change that moves detail
        // placements without moving the ground, such as a repaint.
        void MarkRegionDetailChanged(const TerrainSampleRect& rect);

        // One layer's weight at a terrain-local point, interpolated bilinearly between the four
        // samples around it - the way the splat texture is filtered, so detail grows where the
        // ground looks painted. Zero off the terrain.
        float GetLayerWeightAt(int layer, float x, float z) const;

        // Height at a sample, with coordinates outside the field clamped onto its rim. For the
        // normals' central differences along the terrain's outer edge.
        float GetClampedSampleHeight(Vector2i sample) const;

        // The surface gradient at a sample, as (dh/dx, dh/dz). Both the vertex normals and their
        // tangents come out of it.
        Vector2f ComputeSampleSlopes(Vector2i sample) const;

        // The unit normal of a surface with these slopes, which is what a chunk mesh's vertex at
        // that sample carries.
        static Vector3f NormalFromSlopes(Vector2f slopes);

        // Where a terrain-local point falls in the sample grid: the quad whose low corner is sample
        // Quad, and the point's position within that quad, each coordinate in [0, 1].
        struct QuadPoint
        {
            Vector2i Quad{};
            Vector2f Offset{};
        };

        // Empty outside the terrain. A point exactly on the far rim belongs to the last quad.
        std::optional<QuadPoint> LocateQuadPoint(float x, float z) const;

        // Builds one detail level of one chunk. Level l keeps every 2^l-th sample along both axes,
        // so the mesh is a quarter of the size of the level below it and still lands exactly on
        // real samples - no resampling, and the chunk's corner samples are in every level.
        void BuildChunkMesh(TerrainChunk& chunk, int lodLevel) const;

        // Appends the vertical rim that hides the crack between two neighbouring chunks drawn at
        // different detail levels. Each skirt vertex copies an edge vertex's normal and uv, so it
        // shades as a continuation of the ground.
        static void AppendChunkSkirt(int quads,
                                     float skirtDepth,
                                     std::vector<Vector3f>& vertices,
                                     std::vector<Vector3f>& normals,
                                     std::vector<Vector3f>& tangents,
                                     std::vector<Vector2f>& uvs,
                                     std::vector<std::uint32_t>& indices);

        static void DestroyChunkMeshes(TerrainChunk& chunk);
        void DestroyAllChunkMeshes();

        void DestroySplatMap();
    public:
        // Level 0 is one vertex per sample; each level above it halves that along both axes. Four
        // levels take the default 64-quad chunk down to 8 quads (MinimumLodQuads).
        static constexpr int MaximumLodCount = 4;

        explicit Terrain();

        /* Layout */

        Vector2i GetChunkCount() const;
        Vector2i GetChunkOrigin() const;

        int GetChunkQuads() const;
        float GetChunkSize() const;

        // World units between two neighbouring samples.
        float GetSampleSpacing() const;

        // Samples along each axis of the height field, one more than the quad count because chunks
        // share their edge samples.
        Vector2i GetFieldSize() const;

        // Inclusive sample coordinate range the field covers.
        Vector2i GetSampleMin() const;
        Vector2i GetSampleMax() const;

        // Moves and/or resizes the chunk grid, keeping every sample that both layouts cover where
        // the author put it. Rows can be added or removed on any edge; growing on -x or -z moves
        // the origin rather than renumbering the chunks.
        void Resize(Vector2i chunkOrigin, Vector2i chunkCount);

        // Changing the sample density has no sensible mapping onto the existing samples, so it
        // resets the terrain to flat. Chunk size is only a world-space scale and leaves them alone.
        void SetChunkQuads(int chunkQuads);
        void SetChunkSize(float chunkSize);

        float GetHeightMin() const;
        float GetHeightMax() const;

        // Re-encodes the existing samples against the new range, so widening or narrowing it does
        // not move the terrain out from under the author.
        void SetHeightRange(float heightMin, float heightMax);

        /* Height field */

        // Empty outside the terrain, rather than clamped to the rim.
        std::optional<float> GetSampleHeight(Vector2i sample) const;
        bool SetSampleHeight(Vector2i sample, float height);

        // The encoded heights of a rectangle of samples, row major in z exactly as the field
        // itself is laid out, or empty when the rectangle is not wholly inside the terrain.
        // Encoded, so an undo record restores the ground exactly.
        std::vector<std::uint16_t> GetSampleHeightRect(const TerrainSampleRect& rect) const;

        // Writes a rectangle back in the layout GetSampleHeightRect returns, marking the chunks it
        // covers dirty once rather than per sample. Fails, changing nothing, when the rectangle is
        // outside the terrain or the data is not the size the rectangle describes.
        bool SetSampleHeightRect(const TerrainSampleRect& rect, const std::vector<std::uint16_t>& heights);

        // Interpolated height at a terrain-local point, empty outside the terrain.
        //
        // Interpolates across the triangle the point falls in, not bilinearly across the quad, so
        // it matches the rendered and simulated surface. The diagonal follows
        // IsInFirstQuadTriangle, as do the mesh generator and the PhysX tessellation flags.
        std::optional<float> GetHeightAt(float x, float z) const;

        // The unit normal the ground is shaded with at a terrain-local point, empty outside the
        // terrain. The vertex normals of the triangle the point falls in, interpolated the way
        // GetHeightAt interpolates heights, which is how the rasterizer blends them across the
        // finest detail level.
        std::optional<Vector3f> GetNormalAt(float x, float z) const;

        // Where a ray first meets the ground, or empty when it misses the terrain entirely.
        //
        // Marched across the quad grid in the order the ray crosses it, intersecting the same two
        // triangles per quad that are drawn and simulated. The first hit found is the nearest.
        //
        // The direction does not have to be normalized; Distance is in world units either way.
        std::optional<TerrainRayHit> Raycast(const Vector3f& origin, const Vector3f& direction) const;

        // The quad with its low corner at sample (x, z) is split along the diagonal running from
        // (x, z + 1) to (x + 1, z). True for the triangle on the (x, z) side of it, given a point's
        // position within the quad in [0, 1].
        static bool IsInFirstQuadTriangle(float quadX, float quadZ);

        float DecodeHeight(std::uint16_t sample) const;
        std::uint16_t EncodeHeight(float height) const;

        // Raw field access for bulk consumers such as mesh generation and physics cooking. Row
        // major in z, GetFieldSize().x samples per row, first element at GetSampleMin().
        const std::vector<std::uint16_t>& GetHeightField() const;

        /* Layers */

        // The material a splat channel draws with, or null for an empty or out-of-range channel.
        Material* GetLayer(int layer) const;
        void SetLayer(int layer, Material* material);

        // How much of each layer a sample takes, as (layer 0, 1, 2, 3), summing to one. Empty
        // outside the terrain, like GetSampleHeight.
        std::optional<Vector4f> GetSampleWeights(Vector2i sample) const;

        // Normalizes what it is given before storing it. All-zero weights are stored as entirely
        // layer 0.
        bool SetSampleWeights(Vector2i sample, const Vector4f& weights);

        // The encoded weights of a rectangle of samples, MaximumLayerCount bytes per sample in the
        // field's own row-major layout, or empty when the rectangle is not wholly inside the
        // terrain. The weight counterpart of GetSampleHeightRect.
        std::vector<std::uint8_t> GetSampleWeightRect(const TerrainSampleRect& rect) const;

        // Writes a rectangle back in the layout GetSampleWeightRect returns. Fails, changing
        // nothing, when the rectangle is outside the terrain or the data is not the size the
        // rectangle describes.
        //
        // Unlike SetSampleWeights, stores the bytes as given without normalizing them. Marks only
        // the splat map dirty, since chunk meshes carry no weights.
        bool SetSampleWeightRect(const TerrainSampleRect& rect, const std::vector<std::uint8_t>& weights);

        // The encoding one sample's weights are stored in. Public so a brush writing a rectangle
        // encodes exactly as SetSampleWeights does. 'destination' and 'source' are
        // MaximumLayerCount bytes.
        static void EncodeSampleWeights(const Vector4f& weights, std::uint8_t* destination);
        static Vector4f DecodeSampleWeights(const std::uint8_t* source);

        // Raw weight field for bulk consumers, laid out as GetWeightIndex describes: row major in
        // z, MaximumLayerCount bytes per sample, first sample at GetSampleMin().
        const std::vector<std::uint8_t>& GetLayerWeightField() const;

        /* Splat map */

        // The weight field as a texture, or null before RebuildDirtySplatMap() has built it.
        Graphics::ITexture* GetSplatMap() const;

        // Maps a terrain-local uv - which is what the chunk meshes carry - onto the splat texture:
        // (scale.x, scale.z, offset.x, offset.z), applied as uv * scale + offset.
        Vector4f GetSplatTransform() const;

        // Uploads the weight field if it has changed since the last upload. Needs a graphics
        // context, so the renderer calls it once per frame alongside the chunk mesh rebuild.
        void RebuildDirtySplatMap();

        /* Detail */

        // How many placements one chunk may carry for one detail type. Generation stops here, so a
        // density typed in by mistake cannot allocate without bound; SetDetailTypes warns when a
        // type would reach it.
        static constexpr int MaximumDetailInstancesPerChunk = 1 << 16;

        const std::vector<TerrainDetailType>& GetDetailTypes() const;

        // Replaces the whole list and restamps every chunk, since any field can move placements.
        // Values out of range are clamped rather than rejected: a layer onto an existing channel,
        // a negative density to zero, and the scale range into a positive, ordered pair.
        void SetDetailTypes(const std::vector<TerrainDetailType>& detailTypes);

        // Where one detail type grows within one chunk. The same inputs always give the same
        // placements, and every candidate point draws its random numbers whether it is kept or
        // not, so repainting one patch of ground adds and removes instances there without
        // shuffling the rest. Empty for a detail type index out of range.
        std::vector<TerrainDetailInstance> GenerateDetailInstances(const TerrainChunk& chunk, int detailType) const;

        /* Noise */

        const TerrainNoiseSettings& GetNoiseSettings() const;
        void SetNoiseSettings(const TerrainNoiseSettings& settings);

        // Overwrites the whole field, discarding any sculpted edits. Sampled in sample coordinates,
        // so growing the terrain and regenerating leaves the existing shape in place.
        void GenerateFromNoise();

        /* Chunks */

        const std::vector<TerrainChunk>& GetChunks() const;
        std::vector<TerrainChunk>& GetChunks();

        // Rebuilds the chunk views from the current layout and marks all of them dirty.
        void RebuildChunks();

        // How many detail levels every chunk carries: halvings of the chunk's quad count, down to
        // MinimumLodQuads.
        int GetLodCount() const;

        // A chunk's mesh at a detail level, with the level clamped into what this terrain actually
        // has. Null before RebuildDirtyChunkMeshes() has run, which is where a terrain loaded
        // without a graphics context stays.
        Mesh* GetChunkMesh(const TerrainChunk& chunk, int lodLevel) const;

        // How many of a chunk mesh's indices describe the ground itself. The skirt is appended
        // after the ground, so drawing this many leaves it out (see
        // TerrainRenderer::TerrainView::DrawSkirts). Clamps the level the same way GetChunkMesh
        // does.
        std::uint32_t GetChunkGroundIndexCount(int lodLevel) const;

        // Builds every detail level of every chunk whose samples have changed since its last one,
        // and clears the flag. Needs a graphics context; the renderer calls it once per frame
        // before anything draws. Returns whether any chunk was rebuilt, for the shadow tile cache.
        bool RebuildDirtyChunkMeshes();

        // Marks every chunk covering a sample rectangle dirty and refreshes its bounds. A sample on
        // a chunk edge belongs to the chunks on both sides of it.
        void MarkRegionDirty(const TerrainSampleRect& rect);

        void Dispose() override;
    };
}
