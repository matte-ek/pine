#include "IGPUAsset.hpp"

Pine::GPUAssetLoadStage Pine::IGPUAsset::GetGpuAssetLoadStage() const
{
    return m_GpuAssetLoadState;
}
