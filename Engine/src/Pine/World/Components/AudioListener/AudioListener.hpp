#pragma once

#include "Pine/Core/Serialization/Serialization.hpp"
#include "Pine/World/Components/Component/Component.hpp"

namespace Pine
{
    // The ears of the world: every spatial AudioSource is heard from this entity's transform, and
    // fades with its distance from it. Like AudioSource, this only holds data - Pine::Audio reads it
    // once a frame and moves the audio device's listener to match.
    //
    // A device has exactly one listener, so the first enabled AudioListener in the world is the one
    // that counts, and Pine::Audio warns if it finds more than one. With none at all, nothing is
    // audible: there is nobody there to hear it.
    class AudioListener final : public Component
    {
    private:
        // Master volume, applied on top of whatever each source is set to. It belongs to the
        // listener rather than to a global setting because it is a property of who is hearing -
        // which means a level can carry its own, and a cutscene can swap ears and volume together.
        float m_Volume = 1.f;

        struct AudioListenerSerializer : Serialization::Serializer
        {
            PINE_SERIALIZE_PRIMITIVE(Volume, Serialization::DataType::Float32);
        };
    public:
        AudioListener();

        // Clamped to zero and up, with no ceiling - above 1 the whole mix is amplified.
        void SetVolume(float volume);
        float GetVolume() const;

        void LoadData(const ByteSpan& span) override;
        ByteSpan SaveData() override;
    };
}
