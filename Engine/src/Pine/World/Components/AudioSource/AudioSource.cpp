#include "AudioSource.hpp"
#include "Pine/Assets/AudioFile/AudioFile.hpp"
#include "Pine/World/Entity/Entity.hpp"

Pine::AudioSource::AudioSource()
    : IComponent(ComponentType::AudioSource)
{
}

void Pine::AudioSource::Play()
{
    alSourcePlay(m_Id);
}

void Pine::AudioSource::OnCreated()
{
    IComponent::OnCreated();

    if (m_Standalone)
        return;

    // play source.
}

void Pine::AudioSource::SetAudioFile(AudioFile *file)
{
    m_AudioFile = file;
}

Pine::AudioFile * Pine::AudioSource::GetAudioFile() const
{
    return m_AudioFile.Get();
}

void Pine::AudioSource::OnUpdate(float deltaTime)
{
    IComponent::OnUpdate(deltaTime);

    auto position = GetParent()->GetTransform()->GetPosition();

    //alSourcePosition(m
}
