uniform sampler2DArrayShadow ShadowMap;
uniform bool hasDirectionalShadowMap;

#shader bind ShadowMap 16

float ComputeShadowFactor()
{
    if (!hasDirectionalShadowMap)
    {
        return 1.f;
    }

    float fragCameraDistance = length(vIn.worldPosition - vIn.cameraPos);
    
    float th1 = 10.f;
    //float th2 = 30.f;
    //float th3 = 30.f;

    int cascadeIndex = int(fragCameraDistance > th1);// + int(fragCameraDistance > th2);// + int(fragCameraDistance > th3);
    mat4 lsMatrix = lightSpaceMatrix[cascadeIndex];

    vec4 pointInDepthMap = lsMatrix * vec4(vIn.worldPosition, 1.0);
    vec3 pointNormalized = pointInDepthMap.xyz / pointInDepthMap.w;

    pointNormalized = pointNormalized * 0.5 + 0.5;

    vec2 samplePoint = pointNormalized.xy;
    float currentDepth = pointNormalized.z;
    float shadow = 0.0;
    float texelSize = 1.0 / textureSize(ShadowMap, 0).x;

    for (int x = -1; x <= 1;x++)
    {
        for (int y = -1;y <= 1;y++)
        {
            shadow += texture(ShadowMap, 
                vec4(samplePoint.x + x * texelSize, samplePoint.y + y * texelSize, cascadeIndex, currentDepth)).x;

            // Manual PCF:
            // shadow += mix(0.45, 1.0, step(currentDepth - depth, 0.002));
        }
    }

    shadow /= 9.0;
    shadow = min(shadow, 1);
    shadow = max(shadow, 0.4);

    return shadow;

    // For debugging of CSM
    /*
            if (cascadeIndex == 0) 
                result.diffuse *= vec3(0, 1, 0);
            else if (cascadeIndex == 1)
                result.diffuse *= vec3(1, 1, 0);
            else if (cascadeIndex == 2)
                result.diffuse *= vec3(1, 0, 0);
            else if (cascadeIndex == 3)
                result.diffuse *= vec3(0, 0, 1);
    */
}
