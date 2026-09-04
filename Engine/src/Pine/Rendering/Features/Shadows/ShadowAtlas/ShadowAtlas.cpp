#include "ShadowAtlas.hpp"

#include "Pine/Graphics/Graphics.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Rendering/GraphicsSettings/GraphicsSettings.hpp"

using namespace Pine;

namespace
{
    Graphics::IFrameBuffer* m_FrameBuffer = nullptr;
    Graphics::ITexture* m_Texture = nullptr;

    int m_Resolution = 0;

    std::vector<Rendering::ShadowAtlas::Slot> m_Slots;

    std::uint64_t m_Frame = 0;

    // -1 disables the cap. 0 is a real setting - "no tiles at all" - which is the one value that
    // unambiguously proves the control is wired up, and the denial path is worth being able to hit
    // on purpose.
    int m_DebugSlotLimit = -1;

    // Tiles granted this frame, reused or freshly allocated. The cap counts claims rather than
    // owned slots because owners are only swept in EndFrame: during Acquire, slots still carry last
    // frame's owners, so counting those would compare against a number that includes lights which
    // have already stopped casting.
    int m_ClaimsThisFrame = 0;

    // The atlas is split into four quadrants, each subdivided into a uniform grid of one tile size:
    // two quadrants of one tile each, one of four, one of sixteen. 22 tiles in total.
    //
    // Half the atlas goes to two tiles because the directional cascades pin them, and a cascade
    // covers the whole visible world where a local light covers a room. That is a real cost - it is
    // spent whether or not the level has a sun - and it is still cheaper than what it replaced,
    // which was a separate 4096x4096x2 depth array sitting alongside a full atlas.
    //
    // The rest is sized against SHADOW_VIEW_COUNT (32), the real ceiling on live views.
    //
    // This is Godot's quadrant scheme and it is deliberately dumber than a packer. With a budget
    // measured in low tens of tiles there is nothing worth packing, and the cost of a general
    // allocator is paid in bugs that only show up when it is nearly full.
    struct QuadrantLayout
    {
        Rendering::ShadowAtlas::TileSize Size;
        int TilesPerSide;
    };

    constexpr QuadrantLayout m_Quadrants[4] =
    {
        { Rendering::ShadowAtlas::TileSize::Half,    1 },
        { Rendering::ShadowAtlas::TileSize::Half,    1 },
        { Rendering::ShadowAtlas::TileSize::Quarter, 2 },
        { Rendering::ShadowAtlas::TileSize::Eighth,  4 },
    };
}

void Rendering::ShadowAtlas::Setup()
{
    m_Resolution = GraphicsSettings::GetShadowAtlasResolution();

    m_FrameBuffer = Graphics::GetGraphicsAPI()->CreateFrameBuffer();
    m_FrameBuffer->Bind();
    m_FrameBuffer->Prepare();

    m_Texture = Graphics::GetGraphicsAPI()->CreateTexture();
    m_Texture->Bind();

    // Depth16 rather than the driver's choice, and it holds up for the cascades too now that they
    // live here. A local light's range is short; a cascade is orthographic, and ortho depth is
    // *linear*, so 16 bits over even a 150 metre cascade is millimetre resolution - far finer than
    // the separation front-face culling already provides. This is the thing to suspect first if
    // banding ever appears in a cascade, and Depth32F is the one-line answer, at double the memory.
    m_Texture->UploadTextureData(m_Resolution, m_Resolution, 0, Graphics::TextureFormat::Depth16, Graphics::TextureDataFormat::Float, nullptr);

    // Linear + compare mode is what makes a single sampler2DShadow fetch a 2x2 hardware PCF tap.
    // That is the right default for a local light: with up to seven shadowed slots per fragment,
    // filter width is the wall, not depth resolution.
    m_Texture->SetFilteringMode(Graphics::TextureFilteringMode::Linear);
    m_Texture->SetTextureWrapMode(Graphics::TextureWrapMode::ClampToEdge);
    m_Texture->SetCompareModeLowerEqual();

    m_FrameBuffer->AttachTexture(m_Texture, Graphics::BufferAttachment::Depth);
    m_FrameBuffer->Finish();

    // Build the slot table once. Slots never move, which is the whole point.
    m_Slots.clear();

    const int quadrantSize = m_Resolution / 2;

    for (int q = 0; q < 4; q++)
    {
        const int originX = (q % 2) * quadrantSize;
        const int originY = (q / 2) * quadrantSize;

        const int tilesPerSide = m_Quadrants[q].TilesPerSide;
        const int tileSize = quadrantSize / tilesPerSide;

        for (int y = 0; y < tilesPerSide; y++)
        {
            for (int x = 0; x < tilesPerSide; x++)
            {
                Slot slot;

                slot.Rect = Vector4i(originX + x * tileSize, originY + y * tileSize, tileSize, tileSize);
                slot.Size = m_Quadrants[q].Size;

                m_Slots.push_back(slot);
            }
        }
    }

    PInfo(fmt::format("Shadow atlas: {}x{} D16, {} tiles", m_Resolution, m_Resolution, m_Slots.size()));
}

void Rendering::ShadowAtlas::Shutdown()
{
    if (m_FrameBuffer != nullptr)
    {
        Graphics::GetGraphicsAPI()->DestroyFrameBuffer(m_FrameBuffer);
        m_FrameBuffer = nullptr;
    }

    m_Slots.clear();
}

void Rendering::ShadowAtlas::BeginFrame()
{
    m_Frame++;

    m_ClaimsThisFrame = 0;

    for (auto& slot : m_Slots)
    {
        slot.RenderedThisFrame = false;
    }
}

void Rendering::ShadowAtlas::EndFrame()
{
    // Sweep: anything not re-claimed this frame loses its tile. An owner that keeps asking keeps
    // the same slot, which is what a cached tile will depend on.
    for (auto& slot : m_Slots)
    {
        if (!slot.Pinned && slot.Owner != nullptr && slot.LastClaimedFrame != m_Frame)
        {
            slot.Owner = nullptr;
        }
    }
}

bool Rendering::ShadowAtlas::Acquire(const TileSize size, const void* owner, const int count, int* outSlots)
{
    if (count <= 0)
    {
        return false;
    }

    // Checked before the incumbency scan below, deliberately. Holding tiles must not exempt a light
    // from the cap, or lowering it could only ever deny *new* lights and would look like it did
    // nothing in any scene whose lights were already settled. The cap applies to the group as a
    // whole for the same reason the grant does.
    if (m_DebugSlotLimit >= 0 && m_ClaimsThisFrame + count > m_DebugSlotLimit)
    {
        return false;
    }

    int found = 0;

    // Ours first, so a group keeps the tiles it already had and their contents stay valid.
    for (std::size_t i = 0; i < m_Slots.size() && found < count; i++)
    {
        if (m_Slots[i].Size == size && m_Slots[i].Owner == owner)
        {
            outSlots[found++] = static_cast<int>(i);
        }
    }

    for (std::size_t i = 0; i < m_Slots.size() && found < count; i++)
    {
        if (m_Slots[i].Size == size && m_Slots[i].Owner == nullptr)
        {
            outSlots[found++] = static_cast<int>(i);
        }
    }

    // Nothing has been written to a slot yet, so a partial match simply does not happen.
    if (found < count)
    {
        return false;
    }

    for (int i = 0; i < count; i++)
    {
        auto& slot = m_Slots[outSlots[i]];

        slot.Owner = owner;
        slot.LastClaimedFrame = m_Frame;
    }

    m_ClaimsThisFrame += count;

    return true;
}

bool Rendering::ShadowAtlas::Reserve(const TileSize size, const void* owner, const int count, int* outSlots)
{
    int found = 0;

    for (std::size_t i = 0; i < m_Slots.size() && found < count; i++)
    {
        if (m_Slots[i].Size == size && m_Slots[i].Owner == nullptr)
        {
            outSlots[found++] = static_cast<int>(i);
        }
    }

    if (found < count)
    {
        return false;
    }

    for (int i = 0; i < count; i++)
    {
        auto& slot = m_Slots[outSlots[i]];

        slot.Owner = owner;
        slot.Pinned = true;
    }

    return true;
}

int Rendering::ShadowAtlas::GetTileCapacity(const TileSize size)
{
    int count = 0;

    for (const auto& slot : m_Slots)
    {
        if (slot.Size == size && !slot.Pinned)
        {
            count++;
        }
    }

    return count;
}

bool Rendering::ShadowAtlas::HasTiles(const void* owner)
{
    for (const auto& slot : m_Slots)
    {
        if (slot.Owner == owner)
        {
            return true;
        }
    }

    return false;
}

Vector4i Rendering::ShadowAtlas::GetViewport(const int slot)
{
    return m_Slots[slot].Rect;
}

Vector4f Rendering::ShadowAtlas::GetUvRect(const int slot)
{
    const auto& rect = m_Slots[slot].Rect;
    const auto resolution = static_cast<float>(m_Resolution);

    return Vector4f(static_cast<float>(rect.x) / resolution,
                    static_cast<float>(rect.y) / resolution,
                    static_cast<float>(rect.z) / resolution,
                    static_cast<float>(rect.w) / resolution);
}

void Rendering::ShadowAtlas::MarkRendered(const int slot)
{
    m_Slots[slot].RenderedThisFrame = true;
}

Graphics::IFrameBuffer* Rendering::ShadowAtlas::GetFrameBuffer()
{
    return m_FrameBuffer;
}

Graphics::ITexture* Rendering::ShadowAtlas::GetTexture()
{
    return m_Texture;
}

int Rendering::ShadowAtlas::GetResolution()
{
    return m_Resolution;
}

const std::vector<Rendering::ShadowAtlas::Slot>& Rendering::ShadowAtlas::GetSlots()
{
    return m_Slots;
}

void Rendering::ShadowAtlas::SetDebugSlotLimit(const int limit)
{
    m_DebugSlotLimit = limit;
}

int Rendering::ShadowAtlas::GetDebugSlotLimit()
{
    return m_DebugSlotLimit;
}
