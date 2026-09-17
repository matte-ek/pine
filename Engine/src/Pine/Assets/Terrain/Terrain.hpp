#pragma once
#include "Pine/Assets/Asset/Asset.hpp"
#include "Pine/Assets/Material/Material.hpp"
#include "Pine/Core/Math/Math.hpp"
#include "Pine/Rendering/Renderer3D/LightSlotData.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace Pine
{
    class Mesh;

    namespace Graphics
    {
        class ITexture;
    }

    // A terrain is a rectangular grid of chunks over *one* shared height field. Chunks exist for
    // rendering - they are what gets an LOD level and a frustum test - and are views into the field
    // rather than owners of a copy of it. That is what makes two neighbouring chunks agree along
    // their shared edge by construction, instead of by two separately written expressions happening
    // to produce the same number.
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
    // Chunk and sample coordinates are anchored to chunk (0, 0) rather than to the field's first
    // element, so growing the terrain on the -x or -z edge only moves ChunkOrigin and leaves every
    // existing coordinate - and so everything the author placed - exactly where it was. Flat indices
    // into the height field have no such guarantee: both the row stride and the first row change on
    // a resize, so nothing outside this class may hold one across one.
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

        // The lights that reach this chunk, assigned from the centre of its box by the same scene
        // processor that lights model renderers. Per chunk rather than per terrain because a
        // terrain is far too large to be lit at one point - the whole ground would take the five
        // lights nearest its middle and nothing else.
        //
        // Runtime only, like the meshes below: it holds handles to components, which a saved asset
        // has no business remembering.
        Renderer3D::LightSlotData LightSlots;

        // One renderable mesh per detail level, finest first, owned by the terrain and built by
        // RebuildDirtyChunkMeshes(). Empty until then, which is the state an asset loaded without a
        // graphics context stays in. Read it through Terrain::GetChunkMesh rather than directly:
        // the level a viewer asks for is a distance, and clamping it belongs in one place.
        std::vector<Mesh*> LodMeshes;
    };

    // One band of noise summed into the height field. Each band is an octave stack of its own at
    // its own frequency, so a coarse band gives the terrain its shape and a finer one breaks up the
    // ground the bands before it laid down.
    //
    // Deliberately not called a layer. A terrain layer is one of the four materials the surface
    // blends between, and the two have nothing to do with each other - they do not even have the
    // same count.
    struct TerrainNoiseBand
    {
        // Noise units per world unit, so a smaller value stretches the same shape over more ground.
        float CoordinateScale = 0.004f;

        std::int32_t Octaves = 8;

        // How much height, in world units, this band adds at full strength.
        float Scale = 5.f;

        // The band is only added where the bands before it have already reached above this height,
        // which is what keeps lowland smooth while higher ground gets broken up. Lowest() is the
        // "wherever it falls" a band with nothing to gate on wants.
        float Cutoff = std::numeric_limits<float>::lowest();
    };

    // Seeding and debugging convenience rather than a procedural-generation feature: it is what
    // gives a freshly created terrain some shape to look at before the sculpting brush exists.
    struct TerrainNoiseSettings
    {
        // A fixed set of bands rather than a list that grows: three covers shape, variation and
        // detail, and every band costs a pass over the whole field whether or not it changes
        // anything. Raising it is a change to this number and nothing else.
        static constexpr int BandCount = 3;

        std::int32_t Seed = 123456;

        std::array<TerrainNoiseBand, BandCount> Bands;

        TerrainNoiseSettings()
        {
            // The last band is detail, and gating it at zero is what keeps it off the lowland and
            // on the hills the bands before it raised.
            Bands.back().Cutoff = 0.f;
        }
    };

    // Where a ray met the terrain surface. Terrain-local, like every other coordinate the asset
    // deals in, so the caller that transformed its ray into this space is the one that transforms
    // the answer back out of it.
    struct TerrainRayHit
    {
        Vector3f Position{};

        // How far along the ray the hit is, in world units. Lets a caller holding several
        // candidates - the terrain and an entity picked out of the colour buffer, say - decide
        // which one is in front.
        float Distance = 0.f;
    };

    // A rectangle of samples, inclusive on both corners - so a rectangle whose corners are equal
    // is one sample, and its width is Max.x - Min.x + 1. That is the unit the sample rectangle
    // accessors below work in, and the unit an editing tool records for its undo step.
    //
    // Inclusive because the accessors describe a region of the field rather than an area to draw:
    // asking for "samples 4 through 9" reads more naturally at a call site than a half-open range
    // that stops at 10. Anything measuring a size in world units wants a different type.
    struct TerrainSampleRect
    {
        Vector2i Min{};
        Vector2i Max{};

        // True when the corners are the wrong way around, which is how a tool says its rectangle
        // clamped to nothing - a brush dragged clean off the edge of the terrain, say.
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

        // The smallest rectangle covering both. A stroke grows its recorded region this way as the
        // brush is dragged past what it has already saved.
        TerrainSampleRect Union(const TerrainSampleRect& other) const
        {
            return { glm::min(Min, other.Min), glm::max(Max, other.Max) };
        }
    };

    class Terrain : public Asset
    {
    public:
        // How many layers blend across the surface. Four is what the renderer has room for on both
        // counts: Specifications::Samplers reserves four texture units per texture type, and four
        // weights are exactly one RGBA8 splat texel. Raising it means a wider splat format and a
        // different sampler layout, which is why the stored format carries its own count rather
        // than assuming this one.
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
        // uint16 rather than float because PhysX's height field stores int16 samples anyway
        // (PxHeightFieldFormat::eS16_TM), so float precision is thrown away at the collision
        // boundary regardless. Over a 128 unit range this still resolves ~0.002 units.
        std::vector<std::uint16_t> m_Heights;

        TerrainNoiseSettings m_NoiseSettings;

        // The material of each layer, by splat channel. A slot is allowed to be empty - an unused
        // channel simply carries no weight anywhere - so this is a fixed set of slots rather than
        // a list that gets appended to.
        std::array<AssetHandle<Material>, MaximumLayerCount> m_Layers;

        // How much of each layer every sample takes, MaximumLayerCount bytes per sample in the
        // same row-major layout as m_Heights. Normalized on write, so a sample's weights sum to
        // one.
        //
        // One shared field rather than one per chunk, for the same reason the heights are shared:
        // neighbouring chunks read the same samples along the edge they share, so they agree there
        // by construction. It reaches the GPU as the single splat texture below.
        std::vector<std::uint8_t> m_LayerWeights;

        // Runtime only, deliberately never serialized. Keeping the persisted and the derived state
        // in separate structs is what stops "the thing you picked in the editor was silently
        // dropped on reload" from being expressible.
        std::vector<TerrainChunk> m_Chunks;

        // m_LayerWeights on the GPU, one texel per sample. Runtime only, and null until
        // RebuildDirtySplatMap() has run - a terrain loaded without a graphics context has no
        // texture, exactly as it has no chunk meshes.
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

            // Written as a list rather than as MaximumLayerCount separate fields, so that raising
            // the layer count later is a shader and binding change rather than a change to the
            // format every saved terrain is written in. The weights carry their own channel count
            // the same way: it is their length divided by the sample count.
            PINE_SERIALIZE_ARRAY_FIXED(Layers, UId);
            PINE_SERIALIZE_ARRAY_FIXED(LayerWeights, std::uint8_t);
        };

        // The bands are written as a list of data blocks, the way AssetSerializer writes its
        // sources, rather than as BandCount * four flat fields. Adding a band is then a change to
        // TerrainNoiseSettings::BandCount and nothing else, and a file written by a build with
        // more bands than this one still loads - with the bands past the end dropped, the same way
        // the layers above are.
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

        // The most quads a terrain may span along one axis, counting every chunk. The height field
        // is one allocation and its weights reach the GPU as one texture, so this caps all three:
        // 4096 quads an axis is 4097 samples, a 33 MB height field, a 67 MB weight field and a
        // 4097x4097 splat texture. That is already past what any of the three want to be, and it
        // still leaves room for a 64x64 grid of default 64-quad chunks.
        static constexpr int MaximumFieldQuadsPerAxis = 4096;

        // How far from chunk (0, 0) the grid may sit. Separate from the span above because the two
        // are independent: growing the terrain on its -x edge moves the origin without making the
        // field any larger, and it can be done over and over.
        static constexpr int MaximumSampleCoordinate = 1 << 24;

        // Whether a chunk grid of this shape describes a field this build can hold and index.
        //
        // Worked out in 64 bits deliberately, because an overflowed int is the thing it guards
        // against: GetFieldSize() and GetSampleMin/Max are int arithmetic, so a grid large enough
        // to wrap them would otherwise pass every check and then index the height field with
        // coordinates that are nowhere near where they claim to be. Both the editor's layout
        // fields and a corrupt '.passet' can name one.
        static bool IsLayoutSupported(Vector2i chunkOrigin, Vector2i chunkCount, int chunkQuads);

        std::size_t GetSampleIndex(Vector2i sample) const;

        // Whether a sample coordinate is inside the field. Quiet, unlike IsSampleRectInside below:
        // a single sample outside the terrain is a normal answer - a ray march or a height query
        // past the edge - rather than a caller that forgot to clamp.
        bool IsSampleInside(Vector2i sample) const;

        // Whether a sample rectangle is wholly inside the field. Warns when it is not, because
        // every caller of the rectangle accessors clamps its own rectangle first - one reaching
        // past the edge is a mistake rather than a normal answer, unlike a single sample query.
        bool IsSampleRectInside(const TerrainSampleRect& rect) const;

        // Where a ray meets the two triangles of one quad, nearest first, or empty when it misses
        // both. The quad is given by its low corner sample and has to be inside the field.
        std::optional<TerrainRayHit> IntersectQuad(Vector2i quad, const Vector3f& origin, const Vector3f& direction) const;

        // Index of a sample's first weight byte. Separate from GetSampleIndex because the two
        // strides differ - one byte per sample against MaximumLayerCount of them.
        std::size_t GetWeightIndex(Vector2i sample) const;

        void ResetHeightField();

        // Gives every sample entirely to layer 0, which is what an unpainted terrain looks like.
        void ResetLayerWeights();

        void UpdateChunkBounds(TerrainChunk& chunk) const;

        // Height at a sample, with coordinates outside the field clamped onto its rim. Only the
        // normals ask for one: a chunk's own samples are always inside the field, but the
        // neighbours a central difference needs are not, along the outer edge of the terrain.
        float GetClampedSampleHeight(Vector2i sample) const;

        // The surface gradient at a sample, as (dh/dx, dh/dz). Both the vertex normals and their
        // tangents come out of it, which is how the two are guaranteed to describe the same
        // surface.
        Vector2f ComputeSampleSlopes(Vector2i sample) const;

        // Builds one detail level of one chunk. Level l keeps every 2^l-th sample along both axes,
        // so the mesh is a quarter of the size of the level below it and still lands exactly on
        // real samples - no resampling, and the chunk's corner samples are in every level.
        void BuildChunkMesh(TerrainChunk& chunk, int lodLevel) const;

        // Appends the vertical rim that hides the crack between two neighbouring chunks drawn at
        // different detail levels. Takes the grid the caller just built and extends it, because a
        // skirt vertex is an edge vertex copied downwards - same normal, same uv, so it shades as
        // a continuation of the ground rather than as a wall.
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
        // levels take the default 64-quad chunk down to 8 quads, which is coarse enough that the
        // next halving would cost more in a visible silhouette change than it saves in triangles.
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

        // Empty outside the terrain. Out of bounds is a normal answer here, not a failure - a ray
        // march queries past the edge on most casts - and clamping to the rim would hand the caller
        // a plausible wrong number instead.
        std::optional<float> GetSampleHeight(Vector2i sample) const;
        bool SetSampleHeight(Vector2i sample, float height);

        // The encoded heights of a rectangle of samples, row major in z exactly as the field
        // itself is laid out, or empty when the rectangle is not wholly inside the terrain.
        //
        // Encoded rather than decoded because both callers want precisely what is stored: the
        // sculpting brush reads a rectangle before it edits it, and hands that copy to the undo
        // record. A restore that decoded and re-encoded could leave a sample one step from where it
        // started, and undoing a stroke is supposed to put the ground back exactly.
        std::vector<std::uint16_t> GetSampleHeightRect(const TerrainSampleRect& rect) const;

        // Writes a rectangle back in the layout GetSampleHeightRect returns, and marks the chunks
        // it covers dirty once rather than once per sample - which is the whole reason a brush uses
        // this instead of a loop over SetSampleHeight. Fails, changing nothing, when the rectangle
        // is outside the terrain or the data is not the size the rectangle describes.
        bool SetSampleHeightRect(const TerrainSampleRect& rect, const std::vector<std::uint16_t>& heights);

        // Interpolated height at a terrain-local point, empty outside the terrain.
        //
        // This interpolates across the triangle the point actually falls in, not bilinearly across
        // the quad: a quad is two triangles, so the bilinear patch is not the surface that gets
        // rendered or simulated. The diagonal convention it follows is the one documented on
        // IsInFirstQuadTriangle, and the mesh generator and the PhysX tessellation flags read that
        // same rule - three expressions that must agree is exactly the shape the old terrain got
        // wrong.
        std::optional<float> GetHeightAt(float x, float z) const;

        // Where a ray first meets the ground, or empty when it misses the terrain entirely.
        //
        // Marched across the quad grid a cell at a time and intersected against the two triangles
        // of each one, so the answer is the surface that is actually drawn and simulated - the same
        // triangles, split along the same diagonal - rather than an approximation of it. Cells are
        // visited in the order the ray crosses them, so the first hit found is the nearest one.
        //
        // The direction does not have to be normalized; Distance is in world units either way.
        std::optional<TerrainRayHit> Raycast(const Vector3f& origin, const Vector3f& direction) const;

        // The quad with its low corner at sample (x, z) is split along the diagonal running from
        // (x, z + 1) to (x + 1, z). True for the triangle on the (x, z) side of it, given a point's
        // position within the quad in [0, 1].
        static bool IsInFirstQuadTriangle(float quadX, float quadZ);

        float DecodeHeight(std::uint16_t sample) const;
        std::uint16_t EncodeHeight(float height) const;

        // Raw field access for bulk consumers - mesh generation, physics cooking - that would
        // rather not pay for a bounds check per sample. Indexed by GetSampleIndex' layout: row
        // major in z, GetFieldSize().x samples per row, first element at GetSampleMin().
        const std::vector<std::uint16_t>& GetHeightField() const;

        /* Layers */

        // The material a splat channel draws with, or null for a channel nothing has been assigned
        // to. Out-of-range indices answer null rather than asserting: the editor and the debug
        // server both walk all MaximumLayerCount slots.
        Material* GetLayer(int layer) const;
        void SetLayer(int layer, Material* material);

        // How much of each layer a sample takes, as (layer 0, 1, 2, 3), summing to one. Empty
        // outside the terrain, like GetSampleHeight.
        std::optional<Vector4f> GetSampleWeights(Vector2i sample) const;

        // Normalizes what it is given before storing it, so a caller can pass unnormalized shares
        // - which is what a brush accumulating into one channel produces. All-zero weights are
        // taken as "entirely layer 0" rather than stored as a sample with no layer at all.
        bool SetSampleWeights(Vector2i sample, const Vector4f& weights);

        // The encoded weights of a rectangle of samples, MaximumLayerCount bytes per sample in the
        // field's own row-major layout, or empty when the rectangle is not wholly inside the
        // terrain.
        //
        // The weight counterpart of GetSampleHeightRect, and it exists for the same caller: a paint
        // stroke reads the region it is about to change and hands that copy to its undo record.
        // Encoded rather than normalized floats, so a restore puts back exactly the bytes that
        // were read.
        std::vector<std::uint8_t> GetSampleWeightRect(const TerrainSampleRect& rect) const;

        // Writes a rectangle back in the layout GetSampleWeightRect returns. Fails, changing
        // nothing, when the rectangle is outside the terrain or the data is not the size the
        // rectangle describes.
        //
        // Unlike SetSampleWeights this stores what it is given rather than normalizing it, because
        // its callers - a brush that has already normalized each sample, and an undo record - both
        // want the bytes they hand over to be the bytes that land. It also leaves the chunk meshes
        // alone: weights are a texture, so painting costs a splat upload rather than a rebuild of
        // every chunk the brush crossed.
        bool SetSampleWeightRect(const TerrainSampleRect& rect, const std::vector<std::uint8_t>& weights);

        // The encoding one sample's weights are stored in, as the height pair above is for one
        // sample's height. Static and public because a brush writing a whole rectangle has to
        // encode exactly as SetSampleWeights does: two spellings of "a sample's weights sum to
        // one" would leave a rectangle write and a single-sample write a step apart.
        //
        // 'destination' and 'source' are MaximumLayerCount bytes, which is what one sample of the
        // weight field and one texel of the splat texture both are.
        static void EncodeSampleWeights(const Vector4f& weights, std::uint8_t* destination);
        static Vector4f DecodeSampleWeights(const std::uint8_t* source);

        // Raw weight field for bulk consumers, laid out as GetWeightIndex describes: row major in
        // z, MaximumLayerCount bytes per sample, first sample at GetSampleMin().
        const std::vector<std::uint8_t>& GetLayerWeightField() const;

        /* Splat map */

        // The weight field as a texture, or null before RebuildDirtySplatMap() has built it.
        Graphics::ITexture* GetSplatMap() const;

        // Maps a terrain-local uv - which is what the chunk meshes carry - onto the splat texture:
        // (scale.x, scale.z, offset.x, offset.z), applied as uv * scale + offset. Lives here
        // rather than in the renderer because every term in it is a property of the height field.
        Vector4f GetSplatTransform() const;

        // Uploads the weight field if it has changed since the last upload. Needs a graphics
        // context, so the renderer calls it once per frame alongside the chunk mesh rebuild.
        void RebuildDirtySplatMap();

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

        // How many detail levels every chunk carries. Falls out of the chunk's quad count: a level
        // only exists while halving leaves a whole number of quads, and stops before a chunk gets
        // so coarse that its silhouette visibly changes.
        int GetLodCount() const;

        // A chunk's mesh at a detail level, with the level clamped into what this terrain actually
        // has. Null before RebuildDirtyChunkMeshes() has run, which is where a terrain loaded
        // without a graphics context stays.
        Mesh* GetChunkMesh(const TerrainChunk& chunk, int lodLevel) const;

        // How many of a chunk mesh's indices describe the ground itself. The skirt is appended
        // after the ground, so drawing this many draws the surface and leaves the skirt out - which
        // is what a shadow pass wants: a skirt hangs below the surface to hide a crack between two
        // detail levels, and letting it write depth turns every chunk edge into a wall that shadows
        // the ground beside it.
        //
        // Takes the same level a viewer would ask GetChunkMesh for, and clamps it the same way, so
        // the count and the mesh it counts into cannot disagree.
        std::uint32_t GetChunkGroundIndexCount(int lodLevel) const;

        // Builds every detail level of every chunk whose samples have changed since its last one,
        // and clears the flag. Needs a graphics context, so it is the renderer that calls it - once
        // per frame, before anything draws, rather than from inside a draw pass.
        //
        // Returns whether any chunk was rebuilt, which is how the frame learns that the ground has
        // a different shape than the one the cached shadow tiles were drawn from.
        bool RebuildDirtyChunkMeshes();

        // Marks every chunk covering a sample rectangle dirty and refreshes its bounds. Takes a
        // rectangle rather than a sample because the sculpting brush works in rectangles, and
        // because a sample on a chunk edge belongs to the chunks on both sides of it.
        void MarkRegionDirty(const TerrainSampleRect& rect);

        void Dispose() override;
    };
}
