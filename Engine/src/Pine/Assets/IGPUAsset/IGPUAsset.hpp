#pragma once

#include "Pine/Assets/IAsset/IAsset.hpp"

namespace Pine
{

    enum class GPUAssetLoadStage
    {
        Preparing,
        Upload,
        Ready
    };

    class IGPUAsset : public IAsset
    {
    protected:
        GPUAssetLoadStage m_GpuAssetLoadState = GPUAssetLoadStage::Preparing;
        void* m_PreparedGpuData = nullptr;
    public:
        GPUAssetLoadStage GetGpuAssetLoadStage() const;

        virtual void PrepareGpuData() = 0;
        virtual void UploadGpuData() = 0;
    };

}