#include "Common/FrameBuffer.hlsl"
#include "Common/LodLandscape.hlsl"
#include "Common/Skinned.hlsl"

#if defined(RENDER_SHADOWMASK) || defined(RENDER_SHADOWMASKSPOT) || defined(RENDER_SHADOWMASKPB) || defined(RENDER_SHADOWMASKDPB)
#define RENDER_SHADOWMASK_ANY
#endif

struct VS_INPUT
{
    float4 PositionMS                               : POSITION0;
    
#if defined(TEXTURE)
    float2 TexCoord                                 : TEXCOORD0;
#endif
	
#if defined(NORMALS)
    float4 Normal									: NORMAL0;
    float4 Bitangent								: BINORMAL0;
#endif
#if defined(VC)
	float4 Color									: COLOR0;
#endif
#if defined(SKINNED)
	float4 BoneWeights								: BLENDWEIGHT0;
	float4 BoneIndices								: BLENDINDICES0;
#endif
};

struct VS_OUTPUT
{
    float4 PositionCS                               : SV_POSITION0;
    
#if !(defined(RENDER_DEPTH) && defined(RENDER_SHADOWMASK_ANY)) && SHADOWFILTER != 2
#if (defined(ALPHA_TEST) && ((!defined(RENDER_DEPTH) && !defined(RENDER_SHADOWMAP)) || defined(RENDER_SHADOWMAP_PB))) || defined(RENDER_NORMAL) || defined(DEBUG_SHADOWSPLIT) || defined(RENDER_BASE_TEXTURE)
    float4 TexCoord0                                : TEXCOORD0;
#endif
    
#if defined(RENDER_NORMAL)
    float4 Normal                                   : TEXCOORD1;
#endif
    
#if defined(RENDER_SHADOWMAP_PB)
    float3 TexCoord1                                : TEXCOORD2;
#elif defined(ALPHA_TEST) && (defined(RENDER_DEPTH) || defined(RENDER_SHADOWMAP))
    float4 TexCoord1                                : TEXCOORD2;
#elif defined(ADDITIONAL_ALPHA_MASK)
    float2 TexCoord1                                : TEXCOORD2;
#endif
    
#if defined(LOCALMAP_FOGOFWAR)
    float Alpha                                     : TEXCOORD3;
#endif

#if defined(RENDER_SHADOWMASK_ANY)
    float4 PositionMS                               : TEXCOORD5;
#endif
    
#if defined(ALPHA_TEST) && defined(VC) && defined(RENDER_SHADOWMASK_ANY)
    float2 Alpha                                    : TEXCOORD4;
#elif (defined(ALPHA_TEST) && defined(VC) && !defined(TREE_ANIM)) || defined(RENDER_SHADOWMASK_ANY)
    float Alpha                                     : TEXCOORD4;
#endif

#if defined(DEBUG_SHADOWSPLIT)
    float TexCoord1                                 : TEXCOORD2;
#endif
#endif
};

#ifdef VSHADER
cbuffer PerTechnique								: register(b0)
{
	float4 HighDetailRange							: packoffset(c0);	// loaded cells center in xy, size in zw
    float4 ParabolaParam                            : packoffset(c1);
};

cbuffer PerMaterial									: register(b1)
{
	float4 TexcoordOffset							: packoffset(c0);
};

cbuffer PerGeometry									: register(b2)
{
    float4 ShadowFadeParam                          : packoffset(c0);
    row_major float4x4 World                        : packoffset(c1);
    float4 EyePos                                   : packoffset(c5);
    float4 WaterParams                              : packoffset(c6);
    float4 TreeParams                               : packoffset(c7);
};

float2 SmoothSaturate(float2 value)
{
    return value * value * (3 - 2 * value);
}

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT vsout;
    
#if (defined(RENDER_DEPTH) && defined(RENDER_SHADOWMASK_ANY)) || SHADOWFILTER == 2
    vsout.PositionCS.xy = input.PositionMS.xy;
#if defined(RENDER_SHADOWMASKDPB)
    vsout.PositionCS.z = ShadowFadeParam.z;
#else
    vsout.PositionCS.z = HighDetailRange.x;
#endif
    vsout.PositionCS.w = 1;
#elif defined(STENCIL_ABOVE_WATER)
    vsout.PositionCS.y = WaterParams.x * 2 + input.PositionMS.y;
    vsout.PositionCS.xzw = input.PositionMS.xzw;
#else
    
    precise float4 positionMS = float4(input.PositionMS.xyz, 1.0);
    float4 positionCS = float4(0, 0, 0, 0);
    
    float3 normalMS = float3(1, 1, 1);
#if defined(NORMALS)
    normalMS = input.Normal.xyz * 2 - 1;
#endif
    
#if defined(VC) && defined(NORMALS) && defined(TREE_ANIM)
    float2 treeTmp1 = SmoothSaturate(abs(2 * frac(float2(0.1, 0.25) * (TreeParams.w * TreeParams.y * TreeParams.x) + dot(input.PositionMS.xyz, 1.0.xxx) + 0.5) - 1));
    float normalMult = (treeTmp1.x + 0.1 * treeTmp1.y) * (input.Color.w * TreeParams.z);
    positionMS.xyz += normalMS.xyz * normalMult;
#endif

#if defined (LOD_LANDSCAPE)
	positionMS = AdjustLodLandscapeVertexPositionMS(positionMS, World, HighDetailRange);
#endif

#if defined(SKINNED)
    precise int4 boneIndices = 765.01.xxxx * input.BoneIndices.xyzw;

    float3x4 worldMatrix = GetBoneTransformMatrix(Bones, boneIndices, CameraPosAdjust.xyz, input.BoneWeights);
    precise float4 positionWS = float4(mul(positionMS, transpose(worldMatrix)), 1);

    positionCS = mul(CameraViewProj, positionWS);
#else
    precise float4x4 modelViewProj = mul(CameraViewProj, World);
    positionCS = mul(modelViewProj, positionMS);
#endif
    
#if defined(RENDER_SHADOWMAP) && defined(RENDER_SHADOWMAP_CLAMPED)
    positionCS.z = max(0, positionCS.z);
#endif
    
#if defined (LOD_LANDSCAPE)
	vsout.PositionCS = AdjustLodLandscapeVertexPositionCS(positionCS);
#elif defined(RENDER_SHADOWMAP_PB)
    float3 positionCSPerspective = positionCS.xyz / positionCS.w;
    float3 shadowDirection = normalize(normalize(positionCSPerspective) + float3(0, 0, ParabolaParam.y));
    vsout.PositionCS.xy = shadowDirection.xy / shadowDirection.z;
    vsout.PositionCS.z = ParabolaParam.x * length(positionCSPerspective);
    vsout.PositionCS.w = positionCS.w;
    
    vsout.TexCoord1.x = ParabolaParam.x * length(positionCSPerspective);
    vsout.TexCoord1.y = positionCS.w;
    vsout.TexCoord1.z = 0.5 * ParabolaParam.y * positionCS.z + 0.5;
#else
    vsout.PositionCS = positionCS;
#endif
    
#if defined(RENDER_NORMAL)
    float3 normalVS = float3(1, 1, 1);
#if defined(SKINNED)
	float3x3 boneRSMatrix = GetBoneRSMatrix(Bones, boneIndices, input.BoneWeights);
    normalMS = normalize(mul(normalMS, transpose(boneRSMatrix)));
    normalVS = mul(CameraView, float4(normalMS, 0)).xyz;
#else
    normalVS = mul(mul(CameraView, World), float4(normalMS, 0)).xyz;
#endif
#if defined(RENDER_NORMAL_CLAMP)
    normalVS = max(min(normalVS, 0.1), -0.1);
#endif
    vsout.Normal.xyz = normalVS;
    
#if defined(VC)
    vsout.Normal.w = input.Color.w;
#else
    vsout.Normal.w = 1;
#endif
    
#endif
    
#if (defined(ALPHA_TEST) && ((!defined(RENDER_DEPTH) && !defined(RENDER_SHADOWMAP)) || defined(RENDER_SHADOWMAP_PB))) || defined(RENDER_NORMAL) || defined(DEBUG_SHADOWSPLIT) || defined(RENDER_BASE_TEXTURE)
    float4 texCoord = float4(0, 0, 1, 1);
    texCoord.xy = input.TexCoord * TexcoordOffset.zw + TexcoordOffset.xy;
    
#if defined(RENDER_NORMAL)
    texCoord.z = max(1, 0.0013333333 * positionCS.z + 0.8);
    
    float falloff = 1;
#if defined(RENDER_NORMAL_FALLOFF)
#if defined(SKINNED)
    falloff = dot(normalMS, normalize(EyePos.xyz - positionWS.xyz));
#else
    falloff = dot(normalMS, normalize(EyePos.xyz - positionMS.xyz));
#endif
#endif
    texCoord.w = EyePos.w * falloff;
#endif
    
    vsout.TexCoord0 = texCoord;
#endif
    
#if defined(RENDER_SHADOWMAP_PB)
    vsout.TexCoord1.x = ParabolaParam.x * length(positionCSPerspective);
    vsout.TexCoord1.y = positionCS.w;
    precise float parabolaParam = ParabolaParam.y * positionCS.z;
    vsout.TexCoord1.z = parabolaParam * 0.5 + 0.5;
#elif defined(ALPHA_TEST) && (defined(RENDER_DEPTH) || defined(RENDER_SHADOWMAP))
    float4 texCoord1 = float4(0, 0, 0, 0);
    texCoord1.xy = positionCS.zw;
    texCoord1.zw = input.TexCoord * TexcoordOffset.zw + TexcoordOffset.xy;
    
    vsout.TexCoord1 = texCoord1;
#elif defined(ADDITIONAL_ALPHA_MASK)
    vsout.TexCoord1 = positionCS.zw;
#elif defined(DEBUG_SHADOWSPLIT)
    vsout.TexCoord1 = positionCS.z;
#endif

#if defined(RENDER_SHADOWMASK_ANY)
    vsout.Alpha.x = 1 - pow(saturate(dot(positionCS.xyz, positionCS.xyz) / ShadowFadeParam.x), 8);
    
#if defined(SKINNED)
    vsout.PositionMS.xyz = positionWS.xyz;
#else
    vsout.PositionMS.xyz = positionMS.xyz;
#endif
    vsout.PositionMS.w = positionCS.z;
#endif
    
#if (defined(ALPHA_TEST) && defined(VC)) || defined(LOCALMAP_FOGOFWAR)
#if defined(RENDER_SHADOWMASK_ANY)
    vsout.Alpha.y = input.Color.w;
#elif !defined(TREE_ANIM)
    vsout.Alpha.x = input.Color.w;
#endif
#endif
    
#endif
    
    return vsout;
}
#endif
