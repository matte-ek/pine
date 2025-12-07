#pragma once
#include "Pine/World/Components/IComponent/IComponent.hpp"
#include "Pine/Assets/IAsset/IAsset.hpp"

namespace Pine
{
    class AudioFile;

    class AudioSource final : public IComponent
    {
    private:
        AssetHandle<AudioFile> m_AudioFile;
    public:
        AudioSource();

        void SetAudioFile(AudioFile* file);
        AudioFile* GetAudioFile() const;
    };

}
