#version 420 core

layout(location = 0) out vec4 m_Output;

layout(std140) uniform Matrices
{
	mat4 projectionMatrix;
	mat4 viewMatrix;
};

in VertexData
{
    vec3 normal;
}vIn;

#shader hooks

void main(void)
{
    #shader preFragment

    // Flipped towards the viewer on a face kept by a two-sided material (MaterialRenderFace::Both),
    // the same way the scene pass flips the one it shades. Ambient occlusion builds its sampling
    // hemisphere from this buffer, and a normal pointing away from the camera puts the whole scene
    // inside that hemisphere - the surface then occludes itself to black. A face drawn with its
    // other side culled never reaches this stage through that side, so gl_FrontFacing is always
    // true for it and this is a no-op.
    //
    // TODO: Maybe encode additional data in the alpha channel?
	m_Output = vec4(normalize(gl_FrontFacing ? vIn.normal : -vIn.normal), 1.0);

    #shader postFragment
}