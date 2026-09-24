#include "Rendering.hpp"

#include "Pine/Assets/Shader/ShaderSpecificationRegistry/ShaderSpecificationRegistry.hpp"
#include "Pine/Rendering/Renderer3D/Specifications.hpp"

void Pine::Rendering::Internal::RegisterShaderSpecifications()
{
    using namespace Renderer3D;

    ShaderSpecificationRegistry::Register("MAX_INSTANCE_COUNT", Specifications::General::MAX_INSTANCE_COUNT);
    ShaderSpecificationRegistry::Register("DYNAMIC_LIGHT_COUNT", Specifications::General::DYNAMIC_LIGHT_COUNT);
    ShaderSpecificationRegistry::Register("MATERIAL_SLOT_COUNT", Specifications::General::MATERIAL_SLOT_COUNT);
    ShaderSpecificationRegistry::Register("TERRAIN_LAYER_COUNT", Specifications::TerrainLayers::COUNT);
    ShaderSpecificationRegistry::Register("SHADOW_VIEW_COUNT", Specifications::Shadows::SHADOW_VIEW_COUNT);
    ShaderSpecificationRegistry::Register("TERRAIN_DETAIL_INSTANCE_BINDING", Specifications::StorageBuffers::TERRAIN_DETAIL_INSTANCES);
}
