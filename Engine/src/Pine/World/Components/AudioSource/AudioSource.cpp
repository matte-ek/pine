#include "AudioSource.hpp"

#include <algorithm>

#include "Pine/Assets/AudioFile/AudioFile.hpp"

Pine::AudioSource::AudioSource()
    : Component(ComponentType::AudioSource)
{
}

void Pine::AudioSource::SetAudioFile(AudioFile* audioFile)
{
    m_AudioFile = audioFile;
}

Pine::AudioFile* Pine::AudioSource::GetAudioFile() const
{
    return m_AudioFile.Get();
}

void Pine::AudioSource::Play()
{
    m_PlaybackState = Audio::PlaybackState::Playing;
}

void Pine::AudioSource::Pause()
{
    m_PlaybackState = Audio::PlaybackState::Paused;
}

void Pine::AudioSource::Stop()
{
    m_PlaybackState = Audio::PlaybackState::Stopped;

    // Stopping rewinds - pausing is the one that keeps its place. Written straight into the hint
    // data rather than through SetPlaybackPosition() so it does not queue a seek that the source
    // no longer holds a voice to apply.
    m_PlaybackHintData.PlaybackPosition = 0.f;
    m_PlaybackHintData.SeekRequested = false;
}

Pine::Audio::PlaybackState Pine::AudioSource::GetPlaybackState() const
{
    return m_PlaybackState;
}

bool Pine::AudioSource::IsPlaying() const
{
    return m_PlaybackState == Audio::PlaybackState::Playing;
}

void Pine::AudioSource::SetPlayOnStart(const bool playOnStart)
{
    m_PlayOnStart = playOnStart;
}

bool Pine::AudioSource::GetPlayOnStart() const
{
    return m_PlayOnStart;
}

void Pine::AudioSource::SetLoop(const bool loop)
{
    m_Loop = loop;
}

bool Pine::AudioSource::GetLoop() const
{
    return m_Loop;
}

void Pine::AudioSource::SetSpatial(const bool spatial)
{
    m_Spatial = spatial;
}

bool Pine::AudioSource::GetSpatial() const
{
    return m_Spatial;
}

void Pine::AudioSource::SetVolume(const float volume)
{
    m_Volume = std::max(volume, 0.f);
}

float Pine::AudioSource::GetVolume() const
{
    return m_Volume;
}

void Pine::AudioSource::SetPitch(const float pitch)
{
    m_Pitch = std::max(pitch, 0.01f);
}

float Pine::AudioSource::GetPitch() const
{
    return m_Pitch;
}

void Pine::AudioSource::SetReferenceDistance(const float distance)
{
    m_ReferenceDistance = std::max(distance, 0.f);
    m_MaxDistance = std::max(m_MaxDistance, m_ReferenceDistance);
}

float Pine::AudioSource::GetReferenceDistance() const
{
    return m_ReferenceDistance;
}

void Pine::AudioSource::SetMaxDistance(const float distance)
{
    m_MaxDistance = std::max(distance, m_ReferenceDistance);
}

float Pine::AudioSource::GetMaxDistance() const
{
    return m_MaxDistance;
}

void Pine::AudioSource::SetRolloffFactor(const float factor)
{
    m_RolloffFactor = std::max(factor, 0.f);
}

float Pine::AudioSource::GetRolloffFactor() const
{
    return m_RolloffFactor;
}

void Pine::AudioSource::SetPlaybackPosition(const float seconds)
{
    m_PlaybackHintData.PlaybackPosition = std::max(seconds, 0.f);
    m_PlaybackHintData.SeekRequested = true;
}

float Pine::AudioSource::GetPlaybackPosition() const
{
    return m_PlaybackHintData.PlaybackPosition;
}

Pine::Audio::PlaybackHintData& Pine::AudioSource::GetPlaybackHintData()
{
    return m_PlaybackHintData;
}

void Pine::AudioSource::OnSetup()
{
    Component::OnSetup();

    // A standalone component is not part of the world - a blueprint's stored copy, say - so it has
    // no business making noise.
    if (m_Standalone)
    {
        return;
    }

    if (m_PlayOnStart)
    {
        Play();
    }
}

void Pine::AudioSource::OnCopied()
{
    Component::OnCopied();

    // The copy has inherited a handle to a voice the original is holding. Clearing it is the whole
    // job: the copy is its own source and will be lent a voice of its own when it needs one.
    m_PlaybackHintData = Audio::PlaybackHintData();
}

void Pine::AudioSource::OnDestroyed()
{
    Component::OnDestroyed();

    Audio::Internal::ReleaseVoice(*this);
}

void Pine::AudioSource::LoadData(const ByteSpan& span)
{
    AudioSourceSerializer serializer;

    serializer.Read(span);

    serializer.AudioFile.Read(m_AudioFile);
    serializer.PlayOnStart.Read(m_PlayOnStart);
    serializer.Loop.Read(m_Loop);
    serializer.Spatial.Read(m_Spatial);
    serializer.Volume.Read(m_Volume);
    serializer.Pitch.Read(m_Pitch);
    serializer.ReferenceDistance.Read(m_ReferenceDistance);
    serializer.MaxDistance.Read(m_MaxDistance);
    serializer.RolloffFactor.Read(m_RolloffFactor);
}

Pine::ByteSpan Pine::AudioSource::SaveData()
{
    AudioSourceSerializer serializer;

    serializer.AudioFile.Write(m_AudioFile);
    serializer.PlayOnStart.Write(m_PlayOnStart);
    serializer.Loop.Write(m_Loop);
    serializer.Spatial.Write(m_Spatial);
    serializer.Volume.Write(m_Volume);
    serializer.Pitch.Write(m_Pitch);
    serializer.ReferenceDistance.Write(m_ReferenceDistance);
    serializer.MaxDistance.Write(m_MaxDistance);
    serializer.RolloffFactor.Write(m_RolloffFactor);

    return serializer.Write();
}
