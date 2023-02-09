struct VS_INPUT
{
#if defined(SPECULAR) || defined(UNDERWATER) || defined(STENCIL) || defined(SIMPLE)
	float4 Position                             : POSITION0;
#if defined(NORMAL_TEXCOORD)
    float2 TexCoord0                            : TEXCOORD0;
#endif
#if defined(VC)
	float4 Color                                : COLOR0;
#endif
#endif

#if defined(LOD)
    float4 Position                             : POSITION0;
#if defined(VC)
	float4 Color                                : COLOR0;
#endif
#endif
};

struct VS_OUTPUT
{
#if defined(SPECULAR) || defined(UNDERWATER)
	float4 HPosition                            : SV_POSITION0;
	float4 FogParam                             : COLOR0;
	float4 WPosition                            : TEXCOORD0;
	float4 TexCoord1                            : TEXCOORD1;
	float4 TexCoord2                            : TEXCOORD2;
#if defined(WADING) || (defined(FLOWMAP) && (defined(REFRACTIONS) || defined(BLEND_NORMALS))) || (defined(VERTEX_ALPHA_DEPTH) && defined(VC)) || ((defined(SPECULAR) && NUM_SPECULAR_LIGHTS == 0) && defined(FLOWMAP) /*!defined(NORMAL_TEXCOORD) && !defined(BLEND_NORMALS) && !defined(VC)*/)
    float4 TexCoord3                            : TEXCOORD3;
#endif
#if defined(FLOWMAP)
    float TexCoord4                             : TEXCOORD4;
#endif
#if NUM_SPECULAR_LIGHTS == 0
	float4 MPosition                            : TEXCOORD5;
#endif
#endif

#if defined(SIMPLE)
	float4 HPosition                            : SV_POSITION0;
	float4 FogParam                             : COLOR0;
	float4 WPosition                            : TEXCOORD0;
	float4 TexCoord1                            : TEXCOORD1;
	float4 TexCoord2                            : TEXCOORD2;
	float4 MPosition                            : TEXCOORD5;
#endif

#if defined(LOD)
    float4 HPosition                            : SV_POSITION0;
    float4 FogParam                             : COLOR0;
    float4 WPosition                            : TEXCOORD0;
    float4 TexCoord1                            : TEXCOORD1;
#endif

#if defined(STENCIL)
	float4 HPosition                            : SV_POSITION0;
    float4 WorldPosition                        : POSITION1;
    float4 PreviousWorldPosition                : POSITION2;
#endif
};

#ifdef VSHADER

cbuffer PerTechnique : register(b0)
{
	float4 QPosAdjust					        : packoffset(c0);
};

cbuffer PerMaterial : register(b1)
{
	float4 VSFogParam					        : packoffset(c0);
	float4 VSFogNearColor				        : packoffset(c1);
	float4 VSFogFarColor				        : packoffset(c2);
	float4 NormalsScroll0				        : packoffset(c3);
	float4 NormalsScroll1				        : packoffset(c4);
	float4 NormalsScale					        : packoffset(c5);
};

cbuffer PerGeometry : register(b2)
{
	row_major float4x4 World			        : packoffset(c0);
	row_major float4x4 PreviousWorld	        : packoffset(c4);
	row_major float4x4 WorldViewProj	        : packoffset(c8);
	float3 ObjectUV						        : packoffset(c12);
	float4 CellTexCoordOffset			        : packoffset(c13);
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT vsout;

    float4 inputPosition = float4(input.Position.xyz, 1.0);
    float4 worldPos = mul(World, inputPosition);
    float4 worldViewPos = mul(WorldViewProj, inputPosition);

    float heightMult = min((1.0 / 10000.0) * max(worldViewPos.z - 70000, 0), 1);

    vsout.HPosition.xy = worldViewPos.xy;
    vsout.HPosition.z = heightMult * 0.5 + worldViewPos.z;
    vsout.HPosition.w = worldViewPos.w;

#if defined(STENCIL)
	vsout.WorldPosition = worldPos;
	vsout.PreviousWorldPosition = mul(PreviousWorld, inputPosition);
#else
	float fogColorParam = min(VSFogFarColor.w,
		exp2(NormalsScale.w *
			 log2(saturate(length(worldViewPos.xyz) * VSFogParam.y - VSFogParam.x))));
	vsout.FogParam.xyz = lerp(VSFogNearColor.xyz, VSFogFarColor.xyz, fogColorParam);
	vsout.FogParam.w = fogColorParam;

    vsout.WPosition.xyz = worldPos.xyz;
    vsout.WPosition.w = length(worldPos.xyz);

#if defined(LOD)
	float4 posAdjust =
		ObjectUV.x ? 0.0.xxxx : (QPosAdjust.xyxy + worldPos.xyxy) / NormalsScale.xxyy;

	vsout.TexCoord1.xyzw = NormalsScroll0 + posAdjust;
#else
#if !defined(SPECULAR) || (NUM_SPECULAR_LIGHTS == 0)
    vsout.MPosition.xyzw = inputPosition.xyzw;
#endif

    float2 posAdjust = worldPos.xy + QPosAdjust.xy;

	float2 scrollAdjust1 = posAdjust / NormalsScale.xx;
	float2 scrollAdjust2 = posAdjust / NormalsScale.yy;
	float2 scrollAdjust3 = posAdjust / NormalsScale.zz;

#if !(defined(FLOWMAP) && (defined(REFRACTIONS) || defined(BLEND_NORMALS) || defined(DEPTH) || NUM_SPECULAR_LIGHTS == 0))
#if defined(NORMAL_TEXCOORD)
	float3 normalsScale = 0.001.xxx * NormalsScale.xyz;
	if (ObjectUV.x)
	{
		scrollAdjust1 = input.TexCoord0.xy / normalsScale.xx;
		scrollAdjust2 = input.TexCoord0.xy / normalsScale.yy;
		scrollAdjust3 = input.TexCoord0.xy / normalsScale.zz;
	}
#else
    if (ObjectUV.x)
    {
		scrollAdjust1 = 0.0.xx;
		scrollAdjust2 = 0.0.xx;
		scrollAdjust3 = 0.0.xx;
    }
#endif
#endif

	vsout.TexCoord1 = 0.0.xxxx;
	vsout.TexCoord2 = 0.0.xxxx;
#if defined(FLOWMAP)
#if !(((defined(SPECULAR) || NUM_SPECULAR_LIGHTS == 0) || (defined(UNDERWATER) && defined(REFRACTIONS))) && !defined(NORMAL_TEXCOORD))
#if defined(BLEND_NORMALS)
	vsout.TexCoord1.xy = NormalsScroll0.xy + scrollAdjust1;
	vsout.TexCoord1.zw = NormalsScroll0.zw + scrollAdjust2;
	vsout.TexCoord2.xy = NormalsScroll1.xy + scrollAdjust3;
#else
	vsout.TexCoord1.xy = NormalsScroll0.xy + scrollAdjust1;
	vsout.TexCoord1.zw = 0.0.xx;
	vsout.TexCoord2.xy = 0.0.xx;
#endif
#endif
#if !defined(NORMAL_TEXCOORD)
	vsout.TexCoord3 = 0.0.xxxx;
#elif defined(WADING)
	vsout.TexCoord2.zw = ((-0.5.xx + input.TexCoord0.xy) * 0.1.xx + CellTexCoordOffset.xy) +
		float2(CellTexCoordOffset.z, -CellTexCoordOffset.w + ObjectUV.x) / ObjectUV.xx;
	vsout.TexCoord3.xy = -0.25.xx + (input.TexCoord0.xy * 0.5.xx + ObjectUV.yz);
	vsout.TexCoord3.zw = input.TexCoord0.xy;
#elif (defined(REFRACTIONS) || NUM_SPECULAR_LIGHTS == 0 || defined(BLEND_NORMALS))
	vsout.TexCoord2.zw = (CellTexCoordOffset.xy + input.TexCoord0.xy) / ObjectUV.xx;
	vsout.TexCoord3.xy = (CellTexCoordOffset.zw + input.TexCoord0.xy);
	vsout.TexCoord3.zw = input.TexCoord0.xy;
#endif
    vsout.TexCoord4 = ObjectUV.x;
#else
	vsout.TexCoord1.xy = NormalsScroll0.xy + scrollAdjust1;
	vsout.TexCoord1.zw = NormalsScroll0.zw + scrollAdjust2;
	vsout.TexCoord2.xy = NormalsScroll1.xy + scrollAdjust3;
    vsout.TexCoord2.z = worldViewPos.w;
    vsout.TexCoord2.w = 0;
#if (defined(WADING) || (defined(VERTEX_ALPHA_DEPTH) && defined(VC)))
    vsout.TexCoord3 = 0.0.xxxx;
#if (defined(NORMAL_TEXCOORD) && ((!defined(BLEND_NORMALS) && !defined(VERTEX_ALPHA_DEPTH)) || defined(WADING)))
    vsout.TexCoord3.xy = input.TexCoord0;
#endif
#if defined(VERTEX_ALPHA_DEPTH) && defined(VC)
    vsout.TexCoord3.z = input.Color.w;
#endif
#endif
#endif
#endif
#endif

    return vsout;
}

#endif

typedef VS_OUTPUT PS_INPUT;

struct PS_OUTPUT
{
#if defined(UNDERWATER) || defined(SIMPLE)
	float4 Lighting                             : SV_Target0;
#endif

#if defined(LOD)
	float4 Lighting                             : SV_Target0;
#endif

#if defined(STENCIL)
	float4 Lighting                             : SV_Target0;
	float2 MotionVector                         : SV_Target1;
#endif
};

#ifdef PSHADER

SamplerState ReflectionSampler                  : register(s0);
SamplerState RefractionSampler                  : register(s1);
SamplerState DisplacementSampler                : register(s2);
SamplerState CubeMapSampler                     : register(s3);
SamplerState Normals01Sampler                   : register(s4);
SamplerState Normals02Sampler                   : register(s5);
SamplerState Normals03Sampler                   : register(s6);
SamplerState DepthSampler                       : register(s7);
SamplerState FlowMapSampler                     : register(s8);
SamplerState FlowMapNormalsSampler              : register(s9);
SamplerState SSReflectionSampler                : register(s10);
SamplerState RawSSReflectionSampler             : register(s11);

Texture2D<float4> ReflectionTex                 : register(t0);
Texture2D<float4> RefractionTex                 : register(t1);
Texture2D<float4> DisplacementTex               : register(t2);
Texture2D<float4> CubeMapTex                    : register(t3);
Texture2D<float4> Normals01Tex                  : register(t4);
Texture2D<float4> Normals02Tex                  : register(t5);
Texture2D<float4> Normals03Tex                  : register(t6);
Texture2D<float4> DepthTex                      : register(t7);
Texture2D<float4> FlowMapTex                    : register(t8);
Texture2D<float4> FlowMapNormalsTex             : register(t9);
Texture2D<float4> SSReflectionTex               : register(t10);
Texture2D<float4> RawSSReflectionTex            : register(t11);

cbuffer PerFrame                                : register(b12)
{
	float4 UnknownPerFrame1[12]					: packoffset(c0);
	row_major float4x4 ScreenProj				: packoffset(c12);
	row_major float4x4 PreviousScreenProj		: packoffset(c16);
	float4 UnknownPerFrame2[23]					: packoffset(c20);
}

cbuffer PerTechnique                            : register(b0)
{
	float4 VPOSOffset					        : packoffset(c0);
	float4 PosAdjust					        : packoffset(c1);
	float4 CameraData					        : packoffset(c2);
	float4 SunDir						        : packoffset(c3);
	float4 SunColor						        : packoffset(c4);
}

cbuffer PerMaterial                             : register(b1)
{
	float4 ShallowColor					        : packoffset(c0);
	float4 DeepColor					        : packoffset(c1);
	float4 ReflectionColor				        : packoffset(c2);
	float4 FresnelRI					        : packoffset(c3);
	float4 BlendRadius					        : packoffset(c4);
	float4 VarAmounts					        : packoffset(c5);
	float4 NormalsAmplitude				        : packoffset(c6);
	float4 WaterParams					        : packoffset(c7);
	float4 FogNearColor					        : packoffset(c8);
	float4 FogFarColor					        : packoffset(c9);
	float4 FogParam						        : packoffset(c10);
	float4 DepthControl					        : packoffset(c11);
	float4 SSRParams					        : packoffset(c12);
	float4 SSRParams2					        : packoffset(c13);
}

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT psout;

#if defined(SIMPLE) || defined(UNDERWATER) || defined(LOD)
    float distanceFraction = saturate(
		lerp(UnknownPerFrame2[22].w, 1, (input.WPosition.w - 8192) / (WaterParams.x - 8192)));
	float distanceMul = saturate(lerp(VarAmounts.z, 1, -(distanceFraction - 1))) - 1;

	float4 depthControl = DepthControl * distanceMul + 1;

	float3 normals1 = Normals01Tex.Sample(Normals01Sampler, input.TexCoord1.xy).xyz * 2.0.xxx +
	                  float3(-1, -1, -2);

#if !defined(LOD)
	float3 normals2 =
		Normals02Tex.Sample(Normals02Sampler, input.TexCoord1.zw).xyz * 2.0.xxx - 1.0.xxx;
	float3 normals3 =
		Normals03Tex.Sample(Normals03Sampler, input.TexCoord2.xy).xyz * 2.0.xxx - 1.0.xxx;

    float3 blendedNormal =
		normalize(float3(0, 0, 1) + NormalsAmplitude.xxx * normals1 +
				  NormalsAmplitude.yyy * normals2 + NormalsAmplitude.zzz * normals3);

	float3 finalNormal = normalize(lerp(float3(0, 0, 1), blendedNormal, depthControl.zzz));
#else
	float3 finalNormal =
		normalize(float3(0, 0, 1) + NormalsAmplitude.xxx * normals1) - float3(0, 0, 1);
#endif

    float3 viewDirection = normalize(input.WPosition.xyz);
    
	float VdotN = dot(viewDirection, finalNormal);
	float3 reflectionDirection = finalNormal * -(VdotN * 2).xxx + viewDirection;

	float refelectionMul = exp2(VarAmounts.x * log2(saturate(dot(reflectionDirection, SunDir.xyz))));

    float3 sunDirection = SunColor.xyz * SunDir.www;
	float LdotN = saturate(dot(SunDir.xyz, finalNormal));
	float sunMul = exp2(ShallowColor.w * log2(saturate(dot(finalNormal, float3(-0.099, -0.099, 0.99)))));
	float3 sunColor = (sunDirection * refelectionMul.xxx) * DeepColor.www +
	                  WaterParams.zzz * (sunMul.xxx * sunDirection);

	float viewAngle = 1 - saturate(dot(-viewDirection, finalNormal));
	float shallowFraction = (1 - FresnelRI.x) * pow(viewAngle, 5) + FresnelRI.x;
	float diffuseFraction = distanceFraction * (shallowFraction * depthControl.x - 1) + 1;

	float3 diffuseColor = lerp(ShallowColor.xyz, DeepColor.xyz, shallowFraction.xxx) * LdotN.xxx;
	float3 specularColor = ReflectionColor.xyz * VarAmounts.yyy;
    
	float3 finalColorPreFog =
		sunColor * depthControl.www + lerp(diffuseColor, specularColor, diffuseFraction.xxx);
	float3 finalColor = lerp(finalColorPreFog, input.FogParam.xyz, input.FogParam.www);

    psout.Lighting = saturate(float4(finalColor * PosAdjust.www, 0));
#endif

#if defined(STENCIL)    
    float3 normal =
		normalize(cross(ddx_coarse(input.WorldPosition.xyz), ddy_coarse(input.WorldPosition.xyz)));
	float3 viewDirection = normalize(input.WorldPosition.xyz);

	float VdotN = dot(viewDirection, normal);

    psout.Lighting = float4(0, 0, VdotN, 0);

	float4 screenPosition = mul(ScreenProj, input.WorldPosition);
	screenPosition.xy = screenPosition.xy / screenPosition.ww;
	float4 previousScreenPosition = mul(PreviousScreenProj, input.PreviousWorldPosition);
	previousScreenPosition.xy = previousScreenPosition.xy / previousScreenPosition.ww;
	float2 screenMotionVector = float2(-0.5, 0.5) * (screenPosition.xy - previousScreenPosition.xy);

	psout.MotionVector = screenMotionVector;
#endif

	return psout;
}

#endif
