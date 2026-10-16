// DX12 Forge Engine - all shaders are compiled at startup with D3DCompile.

cbuffer FrameData : register(b0)
{
    row_major float4x4 gViewProjection;
    row_major float4x4 gInverseViewProjection;
    row_major float4x4 gLightViewProjection;
    float3 gCameraPosition;
    float gTimeSeconds;
    float3 gLightDirection;
    float gViewportWidth;
    float3 gLightColor;
    float gViewportHeight;
    float2 gViewportOrigin;
    float2 gRenderTargetSize;
    // Keep in exactly the same order as Engine.h FrameConstants.
    float4 gLightingParameters;          // directional intensity, ambient, point count, spot count
    float4 gDayNightParameters;          // daylight factor, sun elevation, time of day, reserved
    float4 gPostProcessParameters;       // bloom, SSAO, motion blur, vignette
    float4 gPostProcessOptions;          // chromatic aberration, sharpen, saturation, exposure
    float4 gPostProcessMotion;           // screen motion vector, AO radius, reserved
    float4 gSkyParameters;               // cloud coverage, cloud altitude, wind speed, reserved
    float4 gPointLightPositionRange[4];
    float4 gPointLightColorIntensity[4]; // rgb color, w intensity
    float4 gSpotLightPositionRange[4];
    float4 gSpotLightDirectionOuterCos[4];
    float4 gSpotLightColorIntensity[4];  // rgb color, w intensity
    float4 gSpotLightConeParameters[4];  // x = inner cone cosine
};

cbuffer ObjectData : register(b1)
{
    row_major float4x4 gWorld;
    row_major float4x4 gNormalTransform;
    float4 gBaseColor;
    float4 gMaterial; // metallic, roughness, has albedo texture, emissive gizmo/preview mode
    float4 gUVTransform; // tiling xy, offset zw
    float4 gMaterialMaps; // normal, metallic-roughness, AO, normal scale
};

Texture2D<float> gShadowMap : register(t0);
Texture2D<float4> gSceneColor : register(t1);
Texture2D<float4> gAlbedoTexture : register(t2);
Texture2D<float> gSceneDepth : register(t3);
Texture2D<float4> gNormalTexture : register(t4);
Texture2D<float4> gMetallicRoughnessTexture : register(t5);
Texture2D<float4> gAOTexture : register(t6);
SamplerComparisonState gShadowSampler : register(s0);
SamplerState gLinearSampler : register(s1);
SamplerState gTextureSampler : register(s2);

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct SceneVertex
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float4 lightPosition : TEXCOORD2;
    float2 uv : TEXCOORD3;
};

SceneVertex VSMain(VertexInput input)
{
    SceneVertex output;
    float4 worldPosition = mul(float4(input.position, 1.0f), gWorld);
    output.position = mul(worldPosition, gViewProjection);
    output.worldPosition = worldPosition.xyz;
    output.uv = input.uv * gUVTransform.xy + gUVTransform.zw;

    // For this starter engine the objects use uniform or axis-aligned scale.
    output.worldNormal = normalize(mul(float4(input.normal, 0.0f), gNormalTransform).xyz);
    output.lightPosition = mul(worldPosition, gLightViewProjection);
    return output;
}

struct ShadowVertexInput
{
    float3 position : POSITION;
};

float4 VSShadow(ShadowVertexInput input) : SV_POSITION
{
    float4 worldPosition = mul(float4(input.position, 1.0f), gWorld);
    return mul(worldPosition, gLightViewProjection);
}

float DistributionGGX(float nDotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denominator = nDotH * nDotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(3.14159265f * denominator * denominator, 0.000001f);
}

float GeometrySchlickGGX(float nDotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return nDotV / max(nDotV * (1.0f - k) + k, 0.000001f);
}

float GeometrySmith(float nDotV, float nDotL, float roughness)
{
    return GeometrySchlickGGX(nDotV, roughness) * GeometrySchlickGGX(nDotL, roughness);
}

float3 FresnelSchlick(float cosine, float3 f0)
{
    return f0 + (1.0f - f0) * pow(saturate(1.0f - cosine), 5.0f);
}

float CalculateShadow(float4 lightPosition, float nDotL)
{
    if (lightPosition.w <= 0.0f)
        return 1.0f;

    float3 projected = lightPosition.xyz / lightPosition.w;
    float2 uv = float2(projected.x * 0.5f + 0.5f, -projected.y * 0.5f + 0.5f);
    if (uv.x <= 0.001f || uv.x >= 0.999f || uv.y <= 0.001f || uv.y >= 0.999f ||
        projected.z < 0.0f || projected.z > 1.0f)
        return 1.0f;

    // Slope-aware bias reduces acne without detaching shadows excessively.
    float bias = max(0.00055f * (1.0f - nDotL), 0.00012f);
    float texel = 1.0f / 4096.0f; // matches the 4096 x 4096 depth map
    float visibility = 0.0f;
    [unroll]
    for (int y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int x = -2; x <= 2; ++x)
        {
            visibility += gShadowMap.SampleCmpLevelZero(gShadowSampler,
                uv + float2((float)x, (float)y) * texel, projected.z - bias);
        }
    }
    return visibility / 25.0f;
}

float3 ProceduralSky(float3 rayDirection);

float3 EvaluateDirectBRDF(float3 N, float3 V, float3 L, float3 albedo,
                          float metallic, float roughness, float3 radiance)
{
    float nDotL = saturate(dot(N, L));
    if (nDotL <= 0.0001f)
        return float3(0.0f, 0.0f, 0.0f);

    float nDotV = max(saturate(dot(N, V)), 0.001f);
    float3 H = normalize(V + L);
    float nDotH = saturate(dot(N, H));
    float hDotV = saturate(dot(H, V));
    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    float3 F = FresnelSchlick(hDotV, f0);
    float D = DistributionGGX(nDotH, roughness);
    float G = GeometrySmith(nDotV, nDotL, roughness);
    float3 specular = (D * G * F) / max(4.0f * nDotV * max(nDotL, 0.001f), 0.001f);
    float3 diffuseWeight = (1.0f - F) * (1.0f - metallic);
    float3 diffuse = diffuseWeight * albedo / 3.14159265f;
    return (diffuse + specular) * max(radiance, 0.0f) * nDotL;
}

float RangeAttenuation(float distanceToLight, float range)
{
    float normalizedDistance = distanceToLight / max(range, 0.001f);
    float cutoff = saturate(1.0f - normalizedDistance * normalizedDistance);
    // Smooth finite-range falloff combined with an inverse-square term.
    return cutoff * cutoff / max(distanceToLight * distanceToLight, 1.0f);
}

float4 PSPBR(SceneVertex input) : SV_TARGET
{
    float3 sampledAlbedo = gMaterial.z > 0.5f
        ? gAlbedoTexture.Sample(gTextureSampler, input.uv).rgb
        : float3(1.0f, 1.0f, 1.0f);
    float3 albedo = pow(max(gBaseColor.rgb * sampledAlbedo, 0.0f), 2.2f);
    float metallic = saturate(gMaterial.x);
    float roughness = clamp(gMaterial.y, 0.045f, 1.0f);
    if (gMaterialMaps.y > 0.5f)
    {
        // glTF-style packed map: G=roughness, B=metallic.
        float4 mr = gMetallicRoughnessTexture.Sample(gTextureSampler, input.uv);
        roughness = clamp(roughness * mr.g, 0.045f, 1.0f);
        metallic = saturate(metallic * mr.b);
    }
    float3 N = normalize(input.worldNormal);
    if (gMaterialMaps.x > 0.5f)
    {
        // Derivative-built tangent basis avoids requiring a tangent vertex stream, including for imported meshes.
        float3 dpdx = ddx(input.worldPosition);
        float3 dpdy = ddy(input.worldPosition);
        float2 duvdx = ddx(input.uv);
        float2 duvdy = ddy(input.uv);
        float determinant = duvdx.x * duvdy.y - duvdx.y * duvdy.x;
        if (abs(determinant) > 1.0e-6f)
        {
            float3 T = normalize((dpdx * duvdy.y - dpdy * duvdx.y) / determinant);
            T = normalize(T - N * dot(N, T));
            float3 B = normalize(cross(N, T)) * (determinant < 0.0f ? -1.0f : 1.0f);
            float3 mappedNormal = gNormalTexture.Sample(gTextureSampler, input.uv).xyz * 2.0f - 1.0f;
            mappedNormal.xy *= gMaterialMaps.w;
            N = normalize(T * mappedNormal.x + B * mappedNormal.y + N * mappedNormal.z);
        }
    }
    float3 V = normalize(gCameraPosition - input.worldPosition);

    // Directional light + high quality PCF shadow visibility.
    float3 directionalL = normalize(-gLightDirection);
    float nDotL = saturate(dot(N, directionalL));
    float shadow = CalculateShadow(input.lightPosition, nDotL);
    float directionalIntensity = max(gLightingParameters.x, 0.0f);
    float3 directLighting = EvaluateDirectBRDF(N, V, directionalL, albedo, metallic, roughness,
        gLightColor * directionalIntensity) * shadow;

    // Several local lights can be represented by real objects in the hierarchy.
    [unroll]
    for (int pointIndex = 0; pointIndex < 4; ++pointIndex)
    {
        if (pointIndex < (int)gLightingParameters.z)
        {
            float3 pointToLight = gPointLightPositionRange[pointIndex].xyz - input.worldPosition;
            float pointDistance = length(pointToLight);
            float3 pointL = pointToLight / max(pointDistance, 0.001f);
            float pointAttenuation = RangeAttenuation(pointDistance, gPointLightPositionRange[pointIndex].w);
            directLighting += EvaluateDirectBRDF(N, V, pointL, albedo, metallic, roughness,
                gPointLightColorIntensity[pointIndex].rgb * max(gPointLightColorIntensity[pointIndex].w, 0.0f) * pointAttenuation);
        }
    }

    [unroll]
    for (int spotIndex = 0; spotIndex < 4; ++spotIndex)
    {
        if (spotIndex < (int)gLightingParameters.w)
        {
            float3 spotToLight = gSpotLightPositionRange[spotIndex].xyz - input.worldPosition;
            float spotDistance = length(spotToLight);
            float3 spotL = spotToLight / max(spotDistance, 0.001f);
            float coneCosine = dot(-spotL, normalize(gSpotLightDirectionOuterCos[spotIndex].xyz));
            float cone = smoothstep(gSpotLightDirectionOuterCos[spotIndex].w,
                                    gSpotLightConeParameters[spotIndex].x, coneCosine);
            float spotAttenuation = RangeAttenuation(spotDistance, gSpotLightPositionRange[spotIndex].w) * cone;
            directLighting += EvaluateDirectBRDF(N, V, spotL, albedo, metallic, roughness,
                gSpotLightColorIntensity[spotIndex].rgb * max(gSpotLightColorIntensity[spotIndex].w, 0.0f) * spotAttenuation);
        }
    }

    // Image-based lighting samples the engine's procedural sky directly. No cube map is required.
    float3 R = reflect(-V, N);
    float3 environmentColor = min(ProceduralSky(R), float3(4.0f, 4.0f, 4.0f)) * 0.34f;
    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    float nDotV = max(saturate(dot(N, V)), 0.001f);
    float3 environmentFresnel = FresnelSchlick(nDotV, f0);
    float environmentSpecularWeight = lerp(0.80f, 0.12f, roughness);
    float3 ambientSpecular = environmentColor * environmentFresnel * environmentSpecularWeight;
    float3 ambientSky = ProceduralSky(N) * max(gLightingParameters.y, 0.0f) * 0.52f;
    float ao = gMaterialMaps.z > 0.5f ? saturate(gAOTexture.Sample(gTextureSampler, input.uv).r) : 1.0f;
    float3 ambientDiffuse = ambientSky * albedo * (1.0f - metallic) * ao;

    // Retain a very subtle geometric grounding term while respecting the authored AO texture.
    float contactOcclusion = saturate(0.92f + N.y * 0.08f);
    float3 color = directLighting + (ambientDiffuse + ambientSpecular) * contactOcclusion;
    // Light gizmos are emissive/unlit so their icon stays readable at night. Drag previews
    // use a slightly softer cyan emissive mode and remain legible against any sky condition.
    if (gMaterial.w > 1.5f)
        color = gBaseColor.rgb * 1.55f + directLighting * 0.05f;
    else if (gMaterial.w > 0.5f)
        color = gBaseColor.rgb * 2.0f;
    // Return linear HDR; the final pass performs filmic tone mapping and gamma conversion.
    return float4(max(color, 0.0f), gBaseColor.a);
}

float4 PSOutline(SceneVertex input) : SV_TARGET
{
    float3 N = normalize(input.worldNormal);
    float3 V = normalize(gCameraPosition - input.worldPosition);
    float edge = pow(1.0f - saturate(abs(dot(N, V))), 1.7f);
    float pulse = 0.5f + 0.5f * sin(gTimeSeconds * 4.5f);
    float3 cyan = float3(0.055f, 0.82f, 1.0f);
    float3 violet = float3(0.34f, 0.22f, 1.0f);
    float rim = 0.72f + edge * 0.42f;
    float3 color = lerp(cyan, violet, pulse * 0.16f) * (0.82f + 0.15f * pulse) * rim;
    return float4(saturate(color), 1.0f);
}

float4 PSAxis(SceneVertex input) : SV_TARGET
{
    return float4(gBaseColor.rgb, 1.0f);
}

struct FullscreenVertex
{
    float4 position : SV_POSITION;
};

FullscreenVertex VSSkyGrid(uint vertexId : SV_VertexID)
{
    FullscreenVertex output;
    float2 position;
    if (vertexId == 0) position = float2(-1.0f, -1.0f);
    else if (vertexId == 1) position = float2(-1.0f, 3.0f);
    else position = float2(3.0f, -1.0f);
    output.position = float4(position, 0.0f, 1.0f);
    return output;
}

float Hash21(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float Noise2D(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);
    float a = Hash21(i);
    float b = Hash21(i + float2(1.0f, 0.0f));
    float c = Hash21(i + float2(0.0f, 1.0f));
    float d = Hash21(i + float2(1.0f, 1.0f));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float FBM(float2 p)
{
    float value = 0.0f;
    float amplitude = 0.5f;
    [unroll]
    for (int octave = 0; octave < 5; ++octave)
    {
        value += amplitude * Noise2D(p);
        p = p * 2.03f + float2(13.7f, 7.1f);
        amplitude *= 0.5f;
    }
    return value;
}

float RotatedFBM(float2 p, float rotation)
{
    float c = cos(rotation);
    float s = sin(rotation);
    p = float2(c * p.x - s * p.y, s * p.x + c * p.y);
    float value = 0.0f;
    float amplitude = 0.5f;
    [unroll]
    for (int octave = 0; octave < 6; ++octave)
    {
        value += amplitude * Noise2D(p);
        p = p * float2(1.97f, 2.09f) + float2(17.3f, 5.7f);
        p = float2(c * p.x + s * p.y, -s * p.x + c * p.y);
        amplitude *= 0.5f;
    }
    return value;
}

float3 ProceduralSky(float3 rayDirection)
{
    rayDirection = normalize(rayDirection);
    const float daylight = saturate(gDayNightParameters.x);
    const float sunElevation = gDayNightParameters.y;
    const float3 sunDirection = normalize(-gLightDirection);
    const float mu = saturate(dot(rayDirection, sunDirection));
    const float height = rayDirection.y;

    // Deep blue gradient designed to survive ACES tonemap + gamma and still read
    // as vivid azure. Red/green channels are kept very low relative to blue so the
    // hue doesn't collapse toward white under compression.
    const float3 zenithDay = float3(0.04f, 0.20f, 2.80f);
    const float3 midDay = float3(0.10f, 0.45f, 2.60f);
    const float3 horizonDay = float3(0.30f, 0.60f, 1.90f);
    const float3 zenithNight = float3(0.005f, 0.012f, 0.050f);
    const float3 midNight = float3(0.010f, 0.025f, 0.080f);
    const float3 horizonNight = float3(0.025f, 0.050f, 0.130f);

    float up = saturate(height);
    float band = smoothstep(0.0f, 0.35f, up);
    float core = smoothstep(0.25f, 0.95f, up);
    float3 zenith = lerp(zenithNight, zenithDay, daylight);
    float3 mid = lerp(midNight, midDay, daylight);
    float3 horizon = lerp(horizonNight, horizonDay, daylight);
    float3 sky = lerp(horizon, mid, band);
    sky = lerp(sky, zenith, core);

    // Below the horizon the dome fades into a dim blue haze.
    float below = 1.0f - smoothstep(-0.25f, 0.0f, height);
    sky = lerp(sky, lerp(horizon, horizon * 0.35f, 0.6f) * lerp(1.0f, 0.18f, daylight * 0.5f), below);

    // Warm twilight band hugging the horizon while the sun is low.
    float twilight = (1.0f - smoothstep(0.02f, 0.40f, abs(sunElevation))) *
                     (1.0f - smoothstep(0.0f, 0.45f, abs(height)));
    float sunSide = pow(mu, 0.75f);
    float3 twilightColor = lerp(float3(0.86f, 0.30f, 0.12f), float3(1.00f, 0.58f, 0.30f), sunSide);
    sky = lerp(sky, twilightColor, twilight * (0.40f + 0.35f * sunSide) * (0.35f + 0.65f * daylight));

    // Physically-inspired sun disc with derivative antialiasing and atmospheric halo.
    float sunDot = dot(rayDirection, sunDirection);
    float discWidth = max(fwidth(sunDot) * 1.35f, 0.000001f);
    float disc = 1.0f - smoothstep(cos(radians(0.27f)) - discWidth,
                                   cos(radians(0.27f)) + discWidth, sunDot);
    float mie = pow(saturate(sunDot), 16.0f) * 0.14f + pow(saturate(sunDot), 100.0f) * 0.30f;
    float sunVisibility = smoothstep(-0.12f, 0.08f, sunElevation);
    sky += lerp(float3(1.0f, 0.82f, 0.58f), float3(1.0f, 0.95f, 0.82f), daylight) *
           (disc * 1.10f + mie * 0.12f) * sunVisibility;

    // Layered procedural clouds: anisotropic cirrus veil over domained-warped
    // cumulus field. Coverage defaults to 0.35 so the blue sky dominates; drift
    // speed is scaled so motion is visible at normal playback rates.
    if (height > -0.02f)
    {
        const float wind = max(gSkyParameters.z, 0.0f);
        const float cloudAltitude = clamp(gSkyParameters.y, 0.05f, 0.60f);
        const float coverage = clamp(gSkyParameters.x, 0.02f, 0.98f);
        float driftTime = gTimeSeconds;

        // High cirrus: thin streaks, rotated octaves keep them from looking like blobs.
        float2 cirrusUV = rayDirection.xz / max(height + 0.55f, 0.30f) * 0.22f;
        cirrusUV += float2(driftTime * 0.045f, driftTime * 0.028f) * wind;
        float cirrusField = RotatedFBM(cirrusUV * float2(0.55f, 1.6f), 0.65f);
        float cirrus = smoothstep(0.52f, 0.78f, cirrusField) * 0.08f;
        float cirrusFade = smoothstep(0.02f, 0.18f, height) * (1.0f - smoothstep(0.55f, 1.0f, up));

        // Main cumulus deck at a finite altitude with domain warping for puffy shapes.
        // Cells shrink toward the horizon so the deck never stretches into giant flat
        // sheets, and the whole layer fades out below ~3 degrees elevation.
        float2 cloudUV = rayDirection.xz / max(height + cloudAltitude, cloudAltitude * 0.7f);
        cloudUV *= lerp(0.12f, 0.48f, smoothstep(0.0f, 0.35f, up));
        cloudUV += float2(driftTime * 0.038f, -driftTime * 0.022f) * wind;
        float warpA = RotatedFBM(cloudUV * 0.34f + float2(2.4f, 7.1f), 0.4f);
        float warpB = RotatedFBM(cloudUV * 0.36f + float2(13.3f, 3.8f), -0.5f);
        float2 warpedUV = cloudUV + (float2(warpA, warpB) - 0.5f) * 0.85f;
        float broad = RotatedFBM(warpedUV * 0.62f, 0.2f);
        float detail = RotatedFBM(warpedUV * 2.4f + float2(5.1f, 17.7f), -0.35f);
        float cloudField = broad * 0.78f + detail * 0.22f;
        float clouds = smoothstep(coverage, coverage + 0.32f, cloudField);
        float erosion = smoothstep(0.30f, 0.62f, detail);
        clouds *= lerp(0.55f, 1.0f, erosion);
        float cloudFade = smoothstep(0.04f, 0.25f, height) *
                          lerp(1.0f, 0.30f, smoothstep(0.55f, 0.95f, up));

        float sunHeight = saturate(sunElevation * 2.0f + 0.30f);
        float silverLining = pow(mu, 8.0f) * 0.35f;
        float verticalShade = saturate(0.30f + detail * 0.55f + clouds * 0.25f);

        // Cloud colors stay in a moderate HDR range so they read as white-ish
        // after tonemapping without overpowering the blue sky beneath.
        float3 cloudShadow = lerp(float3(0.18f, 0.22f, 0.32f), float3(0.28f, 0.34f, 0.48f), sunHeight);
        float3 cloudLit = lerp(float3(0.55f, 0.62f, 0.78f), float3(0.72f, 0.78f, 0.92f), sunHeight);
        cloudLit = lerp(cloudLit, float3(1.0f, 0.80f, 0.55f), twilight * 0.55f + silverLining * 0.4f);
        float3 cloudDay = lerp(cloudShadow, cloudLit, verticalShade + silverLining);
        float3 cloudNight = float3(0.04f, 0.06f, 0.12f);
        float3 cloudColor = lerp(cloudNight, cloudDay, daylight);

        float cumulusAlpha = clouds * cloudFade * lerp(0.12f, 0.30f, daylight);
        sky = lerp(sky, cloudColor, cumulusAlpha);
        float3 cirrusColor = lerp(cloudNight, cloudLit * 0.95f, daylight);
        sky = lerp(sky, cirrusColor, cirrus * cirrusFade * daylight);
    }

    // Smooth star discs with a low fill rate and no faceted sky geometry.
    if (rayDirection.y > 0.015f && daylight < 0.55f)
    {
        float2 starUV = rayDirection.xz / max(rayDirection.y + 1.05f, 0.12f) * 290.0f;
        float2 cell = floor(starUV);
        float2 local = frac(starUV);
        float hash = Hash21(cell);
        float2 jitter = float2(Hash21(cell + float2(17.1f, 3.7f)), Hash21(cell + float2(5.3f, 29.1f)));
        float2 center = 0.15f + jitter * 0.70f;
        float radius = lerp(0.045f, 0.11f, Hash21(cell + float2(11.4f, 7.2f)));
        float d = length(local - center);
        float aa = max(fwidth(d), 0.004f);
        float star = (1.0f - smoothstep(radius - aa, radius + aa, d)) * smoothstep(0.90f, 0.995f, hash);
        float twinkle = 0.76f + 0.24f * sin(gTimeSeconds * (1.0f + hash * 3.5f) + hash * 90.0f);
        sky += float3(0.36f, 0.48f, 0.78f) * star * twinkle * (1.0f - daylight) * 0.42f;
    }

    // Point and spot lights add restrained localized atmospheric glows at night.
    [unroll] for (int i = 0; i < 4; ++i)
    {
        if (i < (int)gLightingParameters.z)
        {
            float3 toLight = gPointLightPositionRange[i].xyz - gCameraPosition;
            float distanceToLight = length(toLight);
            float3 lightDir = toLight / max(distanceToLight, 0.001f);
            float lobe = pow(saturate(dot(rayDirection, lightDir)), 110.0f);
            float attenuation = RangeAttenuation(distanceToLight, gPointLightPositionRange[i].w);
            sky += gPointLightColorIntensity[i].rgb * lobe * attenuation *
                   gPointLightColorIntensity[i].w * (0.018f + 0.05f * (1.0f - daylight));
        }
    }
    [unroll] for (int i = 0; i < 4; ++i)
    {
        if (i < (int)gLightingParameters.w)
        {
            float3 toLight = gSpotLightPositionRange[i].xyz - gCameraPosition;
            float distanceToLight = length(toLight);
            float3 lightDir = toLight / max(distanceToLight, 0.001f);
            float coneCos = dot(-lightDir, normalize(gSpotLightDirectionOuterCos[i].xyz));
            float cone = smoothstep(gSpotLightDirectionOuterCos[i].w - 0.01f,
                                    gSpotLightConeParameters[i].x, coneCos);
            float lobe = pow(saturate(dot(rayDirection, lightDir)), 120.0f);
            sky += gSpotLightColorIntensity[i].rgb * lobe * cone *
                   RangeAttenuation(distanceToLight, gSpotLightPositionRange[i].w) *
                   gSpotLightColorIntensity[i].w * (0.02f + 0.05f * (1.0f - daylight));
        }
    }
    return max(sky, 0.0f);
}

struct SkyGridPixel
{
    float4 color : SV_TARGET;
    float depth : SV_Depth;
};

float GridLine(float2 coordinate, float spacing)
{
    float2 scaled = coordinate / spacing;
    float2 derivative = max(fwidth(scaled), float2(0.0001f, 0.0001f));
    float2 distanceToLine = abs(frac(scaled - 0.5f) - 0.5f) / derivative;
    float lineDistance = min(distanceToLine.x, distanceToLine.y);
    return 1.0f - smoothstep(0.65f, 1.25f, lineDistance);
}

SkyGridPixel PSSkyGrid(FullscreenVertex input)
{
    SkyGridPixel output;
    float2 ndc;
    ndc.x = (input.position.x - gViewportOrigin.x) / max(gViewportWidth, 1.0f) * 2.0f - 1.0f;
    ndc.y = 1.0f - (input.position.y - gViewportOrigin.y) / max(gViewportHeight, 1.0f) * 2.0f;

    float4 farH = mul(float4(ndc, 1.0f, 1.0f), gInverseViewProjection);
    float safeW = (abs(farH.w) < 0.00001f) ? (farH.w < 0.0f ? -0.00001f : 0.00001f) : farH.w;
    float3 farPoint = farH.xyz / safeW;
    float3 rayDirection = normalize(farPoint - gCameraPosition);
    float3 skyColor = ProceduralSky(rayDirection);

    float depth = 1.0f;
    float3 finalColor = skyColor;
    // gSkyParameters.w gates the editor ground grid; the exported player renders pure sky.
    if (gSkyParameters.w >= 0.5f && abs(rayDirection.y) > 0.0001f)
    {
        float t = -gCameraPosition.y / rayDirection.y;
        if (t > 0.0f)
        {
            float3 worldPoint = gCameraPosition + rayDirection * t;
            float4 clip = mul(float4(worldPoint, 1.0f), gViewProjection);
            float surfaceDepth = (clip.w > 0.000001f) ? clip.z / clip.w : 1.0f;
            if (clip.w > 0.000001f && surfaceDepth >= 0.0f && surfaceDepth < 1.0f)
            {
                // Truly transparent grid: the sky is the background. Only line pixels
                // are composited; no grey ground plane is mixed into the empty cells.
                float subGrid = GridLine(worldPoint.xz, 0.1f);
                float fineGrid = GridLine(worldPoint.xz, 1.0f);
                float majorGrid = GridLine(worldPoint.xz, 10.0f);
                float distanceFade = saturate(1.0f - t / 260.0f);
                float subFade = saturate(1.0f - t / 36.0f);
                float3 gridColor = float3(0.42f, 0.55f, 0.74f);
                float lineIntensity = saturate(subGrid * 0.22f * subFade + fineGrid * 0.62f + majorGrid * 0.95f);

                float axisX = 1.0f - smoothstep(0.018f, 0.060f, abs(worldPoint.z));
                float axisZ = 1.0f - smoothstep(0.018f, 0.060f, abs(worldPoint.x));
                float3 lineColor = gridColor;
                lineColor = lerp(lineColor, float3(0.95f, 0.18f, 0.16f), axisX * 0.90f);
                lineColor = lerp(lineColor, float3(0.14f, 0.48f, 1.0f), axisZ * 0.90f);
                finalColor = lerp(skyColor, lineColor, saturate(lineIntensity * distanceFade));
                depth = saturate(surfaceDepth);
            }
        }
    }

    // The HDR post-process pass performs tone mapping and gamma conversion.
    output.color = float4(max(finalColor, 0.0f), 1.0f);
    output.depth = depth;
    return output;
}


// Full-screen post-processing: compact bloom approximation, ACES-like filmic tonemap,
// subtle vignette and final sRGB-style gamma conversion.
FullscreenVertex VSPostProcess(uint vertexId : SV_VertexID)
{
    FullscreenVertex output;
    float2 position;
    if (vertexId == 0) position = float2(-1.0f, -1.0f);
    else if (vertexId == 1) position = float2(-1.0f, 3.0f);
    else position = float2(3.0f, -1.0f);
    output.position = float4(position, 0.0f, 1.0f);
    return output;
}

float3 ACESFilm(float3 x)
{
    // Modified ACES that preserves color saturation in mid-range values so the
    // blue sky doesn't wash toward white/gray after tonemapping.
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    float3 mapped = (x * (a * x + b)) / max(x * (c * x + d) + e, 0.00001f);
    return clamp(mapped, 0.0f, 1.0f);
}

float3 BloomContribution(float3 sampleColor)
{
    float luminance = dot(sampleColor, float3(0.2126f, 0.7152f, 0.0722f));
    float weight = saturate((luminance - 1.45f) / max(luminance, 0.0001f));
    return sampleColor * weight;
}

float4 PSPostProcess(FullscreenVertex input) : SV_TARGET
{
    float2 uv = input.position.xy / max(gRenderTargetSize, float2(1.0f, 1.0f));
    float2 texel = 1.0f / max(gRenderTargetSize, float2(1.0f, 1.0f));
    float2 motion = gPostProcessMotion.xy * saturate(gPostProcessParameters.z);

    // Motion blur uses a short camera-motion vector and only samples the current HDR scene.
    float4 source = gSceneColor.SampleLevel(gLinearSampler, uv, 0.0f);
    if (gPostProcessParameters.z > 0.001f)
    {
        float3 blur = source.rgb;
        blur += gSceneColor.SampleLevel(gLinearSampler, saturate(uv - motion), 0.0f).rgb;
        blur += gSceneColor.SampleLevel(gLinearSampler, saturate(uv - motion * 0.5f), 0.0f).rgb;
        blur += gSceneColor.SampleLevel(gLinearSampler, saturate(uv + motion * 0.5f), 0.0f).rgb;
        blur += gSceneColor.SampleLevel(gLinearSampler, saturate(uv + motion), 0.0f).rgb;
        source.rgb = lerp(source.rgb, blur * 0.2f, saturate(gPostProcessParameters.z));
    }

    // Depth-based screen-space ambient occlusion approximation. Depth 1 is the sky.
    float centerDepth = gSceneDepth.SampleLevel(gLinearSampler, uv, 0.0f);
    float ao = 0.0f;
    if (centerDepth < 0.9995f && gPostProcessParameters.y > 0.001f)
    {
        float radius = max(gPostProcessMotion.z, 0.5f);
        float d0 = gSceneDepth.SampleLevel(gLinearSampler, saturate(uv + float2(texel.x * radius, 0)), 0.0f);
        float d1 = gSceneDepth.SampleLevel(gLinearSampler, saturate(uv - float2(texel.x * radius, 0)), 0.0f);
        float d2 = gSceneDepth.SampleLevel(gLinearSampler, saturate(uv + float2(0, texel.y * radius)), 0.0f);
        float d3 = gSceneDepth.SampleLevel(gLinearSampler, saturate(uv - float2(0, texel.y * radius)), 0.0f);
        float bias = 0.0012f + centerDepth * 0.0015f;
        ao = saturate((max(0.0f, centerDepth - d0 - bias) + max(0.0f, centerDepth - d1 - bias) +
                       max(0.0f, centerDepth - d2 - bias) + max(0.0f, centerDepth - d3 - bias)) * 22.0f);
    }
    source.rgb *= 1.0f - ao * saturate(gPostProcessParameters.y) * 0.72f;

    // Thresholded bloom with a compact cross + diagonal kernel.
    float bloomStrength = max(gPostProcessParameters.x, 0.0f);
    float3 bloom = BloomContribution(source.rgb) * 0.22f;
    bloom += BloomContribution(gSceneColor.SampleLevel(gLinearSampler, uv + texel * float2(2.0f, 0.0f), 0.0f).rgb) * 0.12f;
    bloom += BloomContribution(gSceneColor.SampleLevel(gLinearSampler, uv + texel * float2(-2.0f, 0.0f), 0.0f).rgb) * 0.12f;
    bloom += BloomContribution(gSceneColor.SampleLevel(gLinearSampler, uv + texel * float2(0.0f, 2.0f), 0.0f).rgb) * 0.12f;
    bloom += BloomContribution(gSceneColor.SampleLevel(gLinearSampler, uv + texel * float2(0.0f, -2.0f), 0.0f).rgb) * 0.12f;
    bloom += BloomContribution(gSceneColor.SampleLevel(gLinearSampler, uv + texel * float2(2.0f, 2.0f), 0.0f).rgb) * 0.075f;
    bloom += BloomContribution(gSceneColor.SampleLevel(gLinearSampler, uv + texel * float2(-2.0f, 2.0f), 0.0f).rgb) * 0.075f;
    bloom += BloomContribution(gSceneColor.SampleLevel(gLinearSampler, uv + texel * float2(2.0f, -2.0f), 0.0f).rgb) * 0.075f;
    bloom += BloomContribution(gSceneColor.SampleLevel(gLinearSampler, uv + texel * float2(-2.0f, -2.0f), 0.0f).rgb) * 0.075f;
    float3 color = source.rgb + bloom * bloomStrength;

    // Chromatic aberration is intentionally small; sharpen is an unsharp mask.
    float chroma = saturate(gPostProcessOptions.x) * 0.004f;
    if (chroma > 0.00001f)
    {
        float r = gSceneColor.SampleLevel(gLinearSampler, saturate(uv + float2(chroma, 0)), 0.0f).r;
        float b = gSceneColor.SampleLevel(gLinearSampler, saturate(uv - float2(chroma, 0)), 0.0f).b;
        color.r = lerp(color.r, r, saturate(gPostProcessOptions.x));
        color.b = lerp(color.b, b, saturate(gPostProcessOptions.x));
    }
    float3 blur3 = (gSceneColor.SampleLevel(gLinearSampler, saturate(uv + float2(texel.x,0)),0).rgb +
                    gSceneColor.SampleLevel(gLinearSampler, saturate(uv - float2(texel.x,0)),0).rgb +
                    gSceneColor.SampleLevel(gLinearSampler, saturate(uv + float2(0,texel.y)),0).rgb +
                    gSceneColor.SampleLevel(gLinearSampler, saturate(uv - float2(0,texel.y)),0).rgb) * 0.25f;
    color += (source.rgb - blur3) * clamp(gPostProcessOptions.y, 0.0f, 1.5f);

    color *= max(gPostProcessOptions.w, 0.01f); // exposure
    float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    color = lerp(luminance.xxx, color, clamp(gPostProcessOptions.z, 0.0f, 2.0f));
    color = ACESFilm(color);
    // Post-tonemap saturation recovery: ACES compresses chroma toward white for
    // bright inputs (sky, emissives). Re-inject the pre-tonemap hue ratio so blues
    // stay blue instead of collapsing to gray/white.
    float postLum = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    if (postLum > 0.001f)
    {
        float3 preACES = color / max(postLum, 0.0001f);
        float3 chromaRecovery = lerp(color, preACES * postLum, 0.35f);
        color = max(chromaRecovery, 0.0f);
    }
    float2 centered = uv * 2.0f - 1.0f;
    float vignette = saturate(1.0f - dot(centered, centered) * saturate(gPostProcessParameters.w) * 0.22f);
    color *= vignette;
    color = pow(saturate(color), 1.0f / 2.2f);
    return float4(color, 1.0f);
}
