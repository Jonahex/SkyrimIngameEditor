#include "Common/FrameBuffer.hlsl"
#include "Common/MotionBlur.hlsl"

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
    nointerpolation float TexCoord4             : TEXCOORD4;
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
#if defined(UNDERWATER) || defined(SIMPLE) || defined(LOD) || defined(SPECULAR)
	float4 Lighting                             : SV_Target0;
#endif

#if defined(STENCIL)
	float4 WaterMask                            : SV_Target0;
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
SamplerState SSRReflectionSampler               : register(s10);
SamplerState RawSSRReflectionSampler            : register(s11);

Texture2D<float4> ReflectionTex                 : register(t0);
Texture2D<float4> RefractionTex                 : register(t1);
Texture2D<float4> DisplacementTex               : register(t2);
TextureCube<float4> CubeMapTex                  : register(t3);
Texture2D<float4> Normals01Tex                  : register(t4);
Texture2D<float4> Normals02Tex                  : register(t5);
Texture2D<float4> Normals03Tex                  : register(t6);
Texture2D<float4> DepthTex                      : register(t7);
Texture2D<float4> FlowMapTex                    : register(t8);
Texture2D<float4> FlowMapNormalsTex             : register(t9);
Texture2D<float4> SSRReflectionTex              : register(t10);
Texture2D<float4> RawSSRReflectionTex           : register(t11);

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

cbuffer PerGeometry                             : register(b2)
{
	float4x4 TextureProj					    : packoffset(c0);
	float4 ReflectPlane							: packoffset(c4);
	float4 ProjData								: packoffset(c5);
	float4 LightPos[8]							: packoffset(c6);
	float4 LightColor[8]						: packoffset(c14);
}

#if defined(FLOWMAP)
float3 GetFlowmapNormal(PS_INPUT input, float2 uvShift, float multiplier, float offset)
{
	float4 flowmapColor = FlowMapTex.Sample(FlowMapSampler, input.TexCoord2.zw + uvShift);
	float2 flowVector = (64 * input.TexCoord3.xy) * sqrt(1.01 - flowmapColor.z);
	float2 flowSinCos = flowmapColor.xy * 2 - 1;
	float2x2 flowRotationMatrix = float2x2(flowSinCos.x, flowSinCos.y, -flowSinCos.y, flowSinCos.x);
	float2 rotatedFlowVector = mul(transpose(flowRotationMatrix), flowVector);
	float2 uv = offset + (-float2(multiplier * ((0.001 * ReflectionColor.w) * flowmapColor.w), 0) +
							 rotatedFlowVector);
	return float3(FlowMapNormalsTex.Sample(FlowMapNormalsSampler, uv).xy, flowmapColor.z);
}
#endif

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT psout;

#if defined(SIMPLE) || defined(UNDERWATER) || defined(LOD) || defined(SPECULAR)
	float3 viewDirection = normalize(input.WPosition.xyz);

    float distanceFraction = saturate(lerp(FrameParams.w, 1, (input.WPosition.w - 8192) / (WaterParams.x - 8192)));
	float4 distanceMul0 = saturate(lerp(VarAmounts.z, 1, -(distanceFraction - 1))).xxxx;

	bool isSpecular = false;

#if defined(DEPTH)
#if defined(VERTEX_ALPHA_DEPTH)
	distanceMul0 = 0;
#else
	float depth = DepthTex
	                  .Load(float3(DynamicResolutionParams1.xy *
									   (DynamicResolutionParams2.xy * input.HPosition.xy),
						  0))
	                  .x;
	float cameraDepth = (CameraData.w / (-depth * CameraData.z + CameraData.x));
	float2 depthOffset =
		DynamicResolutionParams2.xy * input.HPosition.xy * VPOSOffset.xy + VPOSOffset.zw;
	float depthMul = length(float3(cameraDepth * (depthOffset * 2 - 1) / ProjData.xy, cameraDepth));

	float3 depthAdjustedViewDirection = -viewDirection * depthMul;
	float viewSurfaceAngle = dot(depthAdjustedViewDirection, ReflectPlane.xyz);

	float planeMul = (1 - ReflectPlane.w / viewSurfaceAngle);
	distanceMul0 = float4(saturate((planeMul * length(depthAdjustedViewDirection)) / FogParam.z), 0,
		saturate((planeMul * abs(viewSurfaceAngle)) / FogParam.z).xx);
#endif
#endif

	float4 distanceMul = distanceMul0 - 1;

#if defined(UNDERWATER)
	float4 depthControl = float4(0, 1, 0, 0);
#elif defined(LOD)
	float4 depthControl = float4(1, 0, 0, 1);
#elif defined(SPECULAR) && (NUM_SPECULAR_LIGHTS != 0)
	float4 depthControl = float4(0, 0, 1, 0);
#else
	float4 depthControl = DepthControl * distanceMul + 1;
#endif

#if defined(FLOWMAP)
	float2 normalMul =
		0.5 + -(-0.5 + abs(frac(input.TexCoord2.zw * (64 * input.TexCoord4)) * 2 - 1));
	float uvShift = 1 / (128 * input.TexCoord4);

	float3 flowmapNormal0 = GetFlowmapNormal(input, uvShift.xx, 9.92, 0);
	float3 flowmapNormal1 = GetFlowmapNormal(input, float2(0, uvShift), 10.64, 0.27);
	float3 flowmapNormal2 = GetFlowmapNormal(input, 0.0.xx, 8, 0);
	float3 flowmapNormal3 = GetFlowmapNormal(input, float2(uvShift, 0), 8.48, 0.62);

	float2 flowmapNormalWeighted =
		normalMul.y * ((1 - normalMul.x) * flowmapNormal3.xy + normalMul.x * flowmapNormal2.xy) +
		(1 - normalMul.y) *
			(normalMul.x * flowmapNormal1.xy + (1 - normalMul.x) * flowmapNormal0.xy);
	float2 flowmapDenominator = sqrt(normalMul * normalMul + (1 - normalMul) * (1 - normalMul));
	float3 flowmapNormal =
		float3(((-0.5 + flowmapNormalWeighted) / (flowmapDenominator.x * flowmapDenominator.y)) *
				   max(0.4, depthControl.z),
			0);
	flowmapNormal.z =
		sqrt(1 - flowmapNormal.x * flowmapNormal.x - flowmapNormal.y * flowmapNormal.y);
#endif

	float3 normals1 = Normals01Tex.Sample(Normals01Sampler, input.TexCoord1.xy).xyz * 2.0.xxx +
	                  float3(-1, -1, -2);

#if defined (FLOWMAP) && !defined(BLEND_NORMALS)
	float3 finalNormal =
		normalize(lerp(normals1 + float3(0, 0, 1), flowmapNormal, distanceFraction));
#elif !defined(LOD)
	float3 normals2 =
		Normals02Tex.Sample(Normals02Sampler, input.TexCoord1.zw).xyz * 2.0.xxx - 1.0.xxx;
	float3 normals3 =
		Normals03Tex.Sample(Normals03Sampler, input.TexCoord2.xy).xyz * 2.0.xxx - 1.0.xxx;

	float3 blendedNormal =
		normalize(float3(0, 0, 1) + NormalsAmplitude.xxx * normals1 +
				  NormalsAmplitude.yyy * normals2 + NormalsAmplitude.zzz * normals3);
#if defined (UNDERWATER)
	float3 finalNormal = blendedNormal;
#else
	float3 finalNormal = normalize(lerp(float3(0, 0, 1), blendedNormal, depthControl.zzz));
#endif

#if defined(FLOWMAP)
	float normalBlendFactor =
		normalMul.y * ((1 - normalMul.x) * flowmapNormal3.z + normalMul.x * flowmapNormal2.z) +
		(1 - normalMul.y) * (normalMul.x * flowmapNormal1.z + (1 - normalMul.x) * flowmapNormal0.z);
	finalNormal = normalize(lerp(normals1 + float3(0, 0, 1), normalize(lerp(finalNormal, flowmapNormal, normalBlendFactor)), distanceFraction));
#endif
#else
	float3 finalNormal =
		normalize(float3(0, 0, 1) + NormalsAmplitude.xxx * normals1);
#endif

#if defined(WADING)
#if defined(FLOWMAP)
	float2 displacementUv = input.TexCoord3.zw;
#else
	float2 displacementUv = input.TexCoord3.xy;
#endif
	float3 displacement = normalize(float3(NormalsAmplitude.ww * (-0.5.xx + DisplacementTex.Sample(DisplacementSampler, displacementUv).zw),
			0.04));
	finalNormal = lerp(displacement, finalNormal, displacement.zzz);
#endif
	
#if defined(REFLECTIONS)
	float3 finalSsrReflectionColor = 0.0.xxx;
	float ssrFraction = 0;
#if defined(CUBEMAP)
#if NUM_SPECULAR_LIGHTS == 0
	float2 ssrReflectionUv = GetDynamicResolutionAdjustedScreenPosition(
		(DynamicResolutionParams2.xy * input.HPosition.xy) * SSRParams.zw +
		SSRParams2.x * finalNormal.xy);
	float4 ssrReflectionColor1 = SSRReflectionTex.Sample(SSRReflectionSampler, ssrReflectionUv);
	float4 SsrReflectionColor2 = RawSSRReflectionTex.Sample(RawSSRReflectionSampler, ssrReflectionUv);
	float4 ssrReflectionColor = lerp(SsrReflectionColor2, ssrReflectionColor1, SSRParams.y);

	finalSsrReflectionColor = ssrReflectionColor.xyz;
	ssrFraction = saturate(ssrReflectionColor.w * (SSRParams.x * distanceFraction));
#endif

	precise float3 normalOffset = float3(0, 0, 1 - WaterParams.y);
	float3 viewNormal = WaterParams.yyy * finalNormal + normalOffset;
	float3 cubemapUV = viewNormal * -(dot(viewDirection, viewNormal) * 2).xxx + viewDirection;
	float3 reflectionColor = CubeMapTex.Sample(CubeMapSampler, cubemapUV).xyz;
#else
#if NUM_SPECULAR_LIGHTS == 0
	float4 reflectionNormalRaw = float4((VarAmounts.w * depthControl.y) * finalNormal.xy + input.MPosition.xy, input.MPosition.z, 1);
#else
	float4 reflectionNormalRaw = float4(VarAmounts.ww * finalNormal.xy, reflectionNormalZ, 1);
#endif
	float4 reflectionNormal = mul(transpose(TextureProj), reflectionNormalRaw);
	float3 reflectionColor =
		ReflectionTex.Sample(ReflectionSampler, reflectionNormal.xy / reflectionNormal.ww).xyz;
#endif
	float3 finalReflectionColor =
		lerp(reflectionColor * WaterParams.www, finalSsrReflectionColor, ssrFraction);

	float3 specularColor =
		VarAmounts.yyy * (finalReflectionColor - ReflectionColor.xyz) + ReflectionColor.xyz;
#else
	float3 specularColor = ReflectionColor.xyz * VarAmounts.yyy;
#endif

#if defined(UNDERWATER)
	float viewAngle = 1 - saturate(dot(-viewDirection, -finalNormal));
#else
	float viewAngle = 1 - saturate(dot(-viewDirection, finalNormal));
#endif
	float shallowFraction = (1 - FresnelRI.x) * pow(viewAngle, 5) + FresnelRI.x;

#if defined(SPECULAR) && (NUM_SPECULAR_LIGHTS != 0)
	float3 finalColor = 0.0.xxx;

	for (int lightIndex = 0; lightIndex < NUM_SPECULAR_LIGHTS; ++lightIndex)
	{
		float3 lightVector = LightPos[lightIndex].xyz - (PosAdjust.xyz + input.WPosition.xyz);
		float3 lightDirection = normalize(normalize(lightVector) - viewDirection);
		float lightFade = saturate(length(lightVector) / LightPos[lightIndex].w);
		float lightColorMul = (1 - lightFade * lightFade);
		float LdotN = saturate(dot(lightDirection, finalNormal));
		float3 lightColor = (LightColor[lightIndex].xyz * exp2(FresnelRI.z * log2(LdotN))) * lightColorMul;
		finalColor += lightColor;
	}

	finalColor *= shallowFraction;

	isSpecular = true;
#else

#if defined(INTERIOR) || defined(UNDERWATER)
	float LdotN = 1;
	float3 sunColor = 0.0.xxx;
#else
	float VdotN = dot(viewDirection, finalNormal);
	float3 reflectionDirection = finalNormal * -(VdotN * 2).xxx + viewDirection;

	float refelectionMul = exp2(VarAmounts.x * log2(saturate(dot(reflectionDirection, SunDir.xyz))));

    float3 sunDirection = SunColor.xyz * SunDir.www;
	float LdotN = saturate(dot(SunDir.xyz, finalNormal));
	float sunMul = exp2(ShallowColor.w * log2(saturate(dot(finalNormal, float3(-0.099, -0.099, 0.99)))));
	float3 sunColor = (refelectionMul.xxx * sunDirection) * DeepColor.www +
	                  WaterParams.zzz * (sunMul.xxx * sunDirection);
#endif

#if defined(REFRACTIONS)
	float4 refractionNormal = mul(transpose(TextureProj),
		float4((VarAmounts.w * depthControl.y).xx * finalNormal.xy + input.MPosition.xy,
			input.MPosition.z, 1));

	float2 refractionUvRaw =
		float2(refractionNormal.x, refractionNormal.w - refractionNormal.y) / refractionNormal.ww;

#if defined(DEPTH)
	float refractionDepth =
		DepthTex.Load(float3(DynamicResolutionParams1.xy * (refractionUvRaw / VPOSOffset.xy), 0)).x;

	float refractionCameraDepth = (CameraData.w / (-refractionDepth * CameraData.z + CameraData.x));
	float refractionDepthMul = length(float3(
		(refractionCameraDepth *
			((VPOSOffset.zw + refractionUvRaw / VPOSOffset.xy) * 2 - 1)) /
			ProjData.xy,
		cameraDepth));

	float3 refractionDepthAdjustedViewDirection = -viewDirection * refractionDepthMul;
	float refractionViewSurfaceAngle = dot(refractionDepthAdjustedViewDirection, ReflectPlane.xyz);

	float refractionPlaneMul =
		sign(refractionViewSurfaceAngle) * (1 - ReflectPlane.w / refractionViewSurfaceAngle);

	if (refractionPlaneMul < 0)
	{
		refractionUvRaw = depthOffset;
	}
#endif

	float2 refractionUV = GetDynamicResolutionAdjustedScreenPosition(refractionUvRaw);
	float3 refractionColor = RefractionTex.Sample(RefractionSampler, refractionUV).xyz;
	float3 refractionDiffuseColor = lerp(ShallowColor.xyz, DeepColor.xyz, distanceMul0.xyz);
#if defined (UNDERWATER)
	float refractionMul = 0;
#else
	float refractionMul =
		1 - exp2(FogNearColor.w *
				 log2(saturate((-distanceMul0.x * FogParam.z + FogParam.z) / FogParam.w)));
#endif
	float3 diffuseColor =
		lerp(refractionColor * WaterParams.w, refractionDiffuseColor, refractionMul);
#else
	float3 diffuseColor = lerp(ShallowColor.xyz, DeepColor.xyz, shallowFraction.xxx) * LdotN.xxx;
#endif

#if defined (UNDERWATER)
	float3 finalSpecularColor = lerp(ShallowColor.xyz, specularColor, 0.5);
	float3 finalColor = saturate(1 - input.WPosition.w * 0.002) *
	                        ((1 - shallowFraction) * (diffuseColor - finalSpecularColor)) +
	                    finalSpecularColor;
#else
	float diffuseFraction = distanceFraction * (shallowFraction * depthControl.x - 1) + 1;

	float3 finalColorPreFog =
		lerp(diffuseColor, specularColor, diffuseFraction.xxx) + sunColor * depthControl.www;
	float3 finalColor = lerp(finalColorPreFog, input.FogParam.xyz, input.FogParam.www);
#endif
#endif

	psout.Lighting = saturate(float4(finalColor * PosAdjust.www, isSpecular));
#endif

#if defined(STENCIL)    
	float3 viewDirection = normalize(input.WorldPosition.xyz);
    float3 normal =
		normalize(cross(ddx_coarse(input.WorldPosition.xyz), ddy_coarse(input.WorldPosition.xyz)));
	float VdotN = dot(viewDirection, normal);
	psout.WaterMask = float4(0, 0, VdotN, 0);

	psout.MotionVector = GetSSMotionVector(input.WorldPosition, input.PreviousWorldPosition);
#endif

	return psout;
}

#endif
