#include "Audio.hpp"

#include <vector>

#include "OpenAL/OpenAL.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/Core/Log/Log.hpp"
#include "Pine/Performance/Performance.hpp"
#include "Pine/World/Components/AudioListener/AudioListener.hpp"
#include "Pine/World/Components/AudioSource/AudioSource.hpp"
#include "Pine/World/Components/Components.hpp"
#include "Pine/World/Components/Transform/Transform.hpp"
#include "Pine/World/World.hpp"

namespace
{
    using namespace Pine;

    // How many voices the engine will keep. Every OpenAL implementation caps the number of sources
    // that can exist, well below the number of sounds a level holds, so this is a budget rather
    // than a ceiling anyone should be running into: a source that asks to play while all of them
    // are busy simply is not heard. Reserved up front so that exhausting it is a plain "the pool is
    // empty" check instead of a device error in the middle of a frame.
    constexpr int VoiceCount = 32;

    struct Voice
    {
        Audio::IAudioSource* Source = nullptr;

        // Whether the voice has been lent out. Kept alongside Owner because a voice whose owner has
        // been destroyed still needs stopping exactly once.
        bool InUse = false;

        // Bumped on every hand-back, so that a PlaybackHintData recorded before then no longer
        // matches. This is what makes a stale handle harmless rather than a way to drive somebody
        // else's sound.
        std::uint32_t Generation = 1;

        // Re-validates on access, which is what tells us the difference between a voice still in
        // use and one whose component has been destroyed - including the case where the ECS has
        // since reused that component's slot for something else.
        ComponentHandle<AudioSource> Owner;

        // What is currently bound, so the clip is only rebound when it actually changes. Rebinding
        // stops playback, so doing it every frame would hold every sound at its first sample.
        const AudioFile* Clip = nullptr;

        // Whether the voice has been told to play since its clip was bound. Without it, "the device
        // says stopped" cannot tell a voice that has finished its clip from one that has not
        // started playing yet.
        bool HasStarted = false;
    };

    Audio::IAudioAPI* m_AudioAPI = nullptr;

    std::vector<Voice> m_Voices;

    // A level is only ever meant to have one set of ears, so the warning about finding several is
    // worth saying once rather than sixty times a second.
    bool m_HasWarnedAboutMultipleListeners = false;

    void CreateVoices()
    {
        m_Voices.reserve(VoiceCount);

        for (int i = 0; i < VoiceCount; i++)
        {
            const auto source = m_AudioAPI->CreateSource();

            // A device that will not part with the whole budget still works, just with fewer sounds
            // playing at once. Stop asking at the first refusal.
            if (source == nullptr)
            {
                break;
            }

            Voice voice;

            voice.Source = source;

            m_Voices.push_back(voice);
        }

        if (m_Voices.empty())
        {
            PWarning("The audio device would not provide a single voice, nothing will be audible.");

            return;
        }

        PVerbose(fmt::format("Reserved {} audio voices.", m_Voices.size()));
    }

    Voice* FindVoice(AudioSource& source)
    {
        const auto& hintData = source.GetPlaybackHintData();

        if (hintData.VoiceIndex < 0 || hintData.VoiceIndex >= static_cast<int>(m_Voices.size()))
        {
            return nullptr;
        }

        auto& voice = m_Voices[hintData.VoiceIndex];

        if (!voice.InUse || voice.Generation != hintData.VoiceGeneration)
        {
            return nullptr;
        }

        return &voice;
    }

    // Returns the voice this source already holds, or lends it a free one. Returns nullptr when
    // every voice is busy, which the caller is expected to treat as "not heard this frame".
    Voice* AcquireVoice(AudioSource& source)
    {
        if (const auto heldVoice = FindVoice(source))
        {
            return heldVoice;
        }

        for (std::size_t i = 0; i < m_Voices.size(); i++)
        {
            auto& voice = m_Voices[i];

            if (voice.InUse)
            {
                continue;
            }

            voice.InUse = true;
            voice.Owner = &source;
            voice.HasStarted = false;

            auto& hintData = source.GetPlaybackHintData();

            hintData.VoiceIndex = static_cast<int>(i);
            hintData.VoiceGeneration = voice.Generation;

            return &voice;
        }

        return nullptr;
    }

    void FreeVoice(Voice& voice)
    {
        voice.Source->Stop();

        voice.InUse = false;
        voice.Owner = nullptr;
        voice.Clip = nullptr;
        voice.HasStarted = false;

        voice.Generation++;
    }

    void ReleaseOrphanedVoices()
    {
        for (auto& voice : m_Voices)
        {
            // The component that borrowed this voice is gone - its entity was destroyed, or a level
            // was unloaded out from under it. Nothing is left to hand the voice back, and it would
            // otherwise keep playing its clip to the end.
            if (!voice.InUse || voice.Owner.Get() != nullptr)
            {
                continue;
            }

            FreeVoice(voice);
        }
    }

    AudioListener* FindActiveListener()
    {
        AudioListener* activeListener = nullptr;

        int listenerCount = 0;

        for (auto& listener : Components::Get<AudioListener>())
        {
            listenerCount++;

            if (activeListener == nullptr)
            {
                activeListener = &listener;
            }
        }

        if (listenerCount > 1 && !m_HasWarnedAboutMultipleListeners)
        {
            PWarning(fmt::format("Found {} enabled audio listeners, only the first one will be used.", listenerCount));

            m_HasWarnedAboutMultipleListeners = true;
        }
        else if (listenerCount <= 1)
        {
            m_HasWarnedAboutMultipleListeners = false;
        }

        return activeListener;
    }

    void UpdateListener()
    {
        const auto listener = FindActiveListener();

        // Nothing is listening, so nothing should be heard. Silencing the device beats leaving the
        // last listener's position in place, which would keep the level audible from wherever the
        // ears happened to be standing when they were removed.
        if (listener == nullptr)
        {
            m_AudioAPI->SetListenerVolume(0.f);

            return;
        }

        const auto transform = listener->GetTransform();

        m_AudioAPI->SetListenerTransform(transform->GetPosition(), transform->GetForward(), transform->GetUp());
        m_AudioAPI->SetListenerVolume(listener->GetVolume());
    }

    void ApplySourceSettings(AudioSource& source, Audio::IAudioSource& voiceSource)
    {
        voiceSource.SetVolume(source.GetVolume());
        voiceSource.SetPitch(source.GetPitch());
        voiceSource.SetLooping(source.GetLoop());
        voiceSource.SetSpatial(source.GetSpatial());

        // A non-spatial source sits on the listener, so neither where its entity is nor how it
        // would fade with distance means anything.
        if (!source.GetSpatial())
        {
            return;
        }

        voiceSource.SetPosition(source.GetTransform()->GetPosition());
        voiceSource.SetAttenuation(
            source.GetReferenceDistance(),
            source.GetMaxDistance(),
            source.GetRolloffFactor());
    }

    void UpdateSource(AudioSource& source, const bool worldPaused)
    {
        // A source on a disabled entity, or one that has been stopped, gives its voice back.
        // Neither is going to be asked to stop again, and a sound still running on something the
        // author has switched off is never what they meant.
        if (!source.IsWorldEnabled() || source.GetPlaybackState() == Audio::PlaybackState::Stopped)
        {
            Audio::Internal::ReleaseVoice(source);

            return;
        }

        const auto clip = source.GetAudioFile();
        const auto buffer = clip != nullptr ? clip->GetBuffer() : nullptr;

        // No clip set, or its asset has not finished loading, or there was no device to upload it
        // to. The requested state is left alone on purpose: a source pointed at an asset that is
        // still loading should start on its own once the clip arrives, not be quietly stopped.
        if (buffer == nullptr)
        {
            Audio::Internal::ReleaseVoice(source);

            return;
        }

        const auto voice = AcquireVoice(source);

        // Every voice is busy. The source goes on asking, and is heard as soon as one frees up.
        if (voice == nullptr)
        {
            return;
        }

        if (voice->Clip != clip)
        {
            voice->Source->SetBuffer(buffer);

            voice->Clip = clip;
            voice->HasStarted = false;
        }

        ApplySourceSettings(source, *voice->Source);

        auto& hintData = source.GetPlaybackHintData();

        // Seek before deciding what to do about playback below, so that a source asked to start
        // from an offset starts there rather than starting at zero and jumping.
        if (hintData.SeekRequested)
        {
            voice->Source->SetPlaybackPosition(hintData.PlaybackPosition);

            hintData.SeekRequested = false;
        }

        // A paused world holds every sound where it is rather than stopping it, so that leaving
        // play mode and going back in continues the clip instead of restarting it.
        const auto requestedState = source.GetPlaybackState();
        const auto targetState = worldPaused && requestedState == Audio::PlaybackState::Playing
            ? Audio::PlaybackState::Paused
            : requestedState;

        const auto deviceState = voice->Source->GetState();

        if (targetState == Audio::PlaybackState::Playing)
        {
            // Stopped while we were asking it to play means the clip ran off its end. The component
            // is the stale one here, so correct it and hand the voice back for something else.
            if (deviceState == Audio::PlaybackState::Stopped && voice->HasStarted)
            {
                source.Stop();

                Audio::Internal::ReleaseVoice(source);

                return;
            }

            if (deviceState != Audio::PlaybackState::Playing)
            {
                voice->Source->Play();

                voice->HasStarted = true;
            }
        }
        else if (deviceState == Audio::PlaybackState::Playing)
        {
            voice->Source->Pause();
        }

        hintData.PlaybackPosition = voice->Source->GetPlaybackPosition();
    }
}

bool Pine::Audio::Setup()
{
    const auto audioAPI = new OpenAL();

    if (!audioAPI->Setup())
    {
        delete audioAPI;

        return false;
    }

    m_AudioAPI = audioAPI;

    CreateVoices();

    return true;
}

void Pine::Audio::Shutdown()
{
    if (m_AudioAPI == nullptr)
    {
        return;
    }

    for (auto& voice : m_Voices)
    {
        m_AudioAPI->DestroySource(voice.Source);
    }

    m_Voices.clear();

    m_AudioAPI->Shutdown();

    delete m_AudioAPI;

    m_AudioAPI = nullptr;
}

void Pine::Audio::Update()
{
    PINE_PF_SCOPE();

    // The engine runs perfectly well without an output device, and everything below would be
    // driving a device that is not there.
    if (m_AudioAPI == nullptr)
    {
        return;
    }

    UpdateListener();

    ReleaseOrphanedVoices();

    const auto worldPaused = World::IsPaused();

    // Disabled components are included deliberately: a source that has just been switched off still
    // holds a voice, and the iterator would otherwise skip the one frame where it needs collecting.
    for (auto& source : Components::Get<AudioSource>(true))
    {
        UpdateSource(source, worldPaused);
    }
}

void Pine::Audio::Internal::ReleaseVoice(AudioSource& source)
{
    if (const auto voice = FindVoice(source))
    {
        FreeVoice(*voice);
    }

    auto& hintData = source.GetPlaybackHintData();

    hintData.VoiceIndex = -1;
    hintData.VoiceGeneration = 0;
}

Pine::Audio::IAudioAPI* Pine::Audio::GetAudioAPI()
{
    return m_AudioAPI;
}

bool Pine::Audio::HasInitializedAudioAPI()
{
    return m_AudioAPI != nullptr;
}
