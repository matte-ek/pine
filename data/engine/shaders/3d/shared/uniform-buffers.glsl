layout(std140) uniform Matrices
{
	mat4 projectionMatrix;
	mat4 viewMatrix;
};

layout(std140) uniform Instances
{
	Instance instances[MAX_INSTANCE_COUNT];
};

layout(std140) uniform Lights
{
	Light lights[DYNAMIC_LIGHT_COUNT];
};

layout(std140) uniform Material
{
    MaterialProperties matPropeties[MATERIAL_SLOT_COUNT];
};

layout(std140) uniform ShadowViews
{
	ShadowView shadowViews[SHADOW_VIEW_COUNT];
};

layout(std140) uniform World
{
    vec4 ambientColor;
    vec4 fogColor;
    vec4 fogSettings;

    // xy = the direction the wind blows along the ground (x, z), z = strength, w = phase.
    vec4 wind;
}world;