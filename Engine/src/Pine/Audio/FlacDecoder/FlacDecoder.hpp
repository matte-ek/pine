#pragma once

#include <FLAC++/decoder.h>
#include <vector>

namespace Pine::Audio::FLAC
{
    class Decoder : public ::FLAC::Decoder::File
    {
    private:
        std::vector<FLAC__int32> m_pcmData;
    public:
        ::FLAC__StreamDecoderWriteStatus write_callback(
                const ::FLAC__Frame *frame,
                const ::FLAC__int32 * const buffer[]
        ) override;

        void error_callback(
                ::FLAC__StreamDecoderErrorStatus status
        ) override;
    };
}

