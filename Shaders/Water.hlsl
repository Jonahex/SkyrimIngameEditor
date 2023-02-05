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

cbuffer PerTechnique : register(b0)
{
	float4 VPOSOffset					        : packoffset(c0);
	float4 PosAdjust					        : packoffset(c1);
	float4 CameraData					        : packoffset(c2);
	float4 SunDir						        : packoffset(c3);
	float4 SunColor						        : packoffset(c4);
}

cbuffer PerMaterial : register(b1)
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

cbuffer PerFrame : register(b12)
{
	float4 cb12[43];
}

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT psout;

#if defined(SIMPLE) || defined(UNDERWATER)
    float4 r0, r1, r2, r3;

    float3 normals1;
	normals1 = Normals01Tex.Sample(Normals01Sampler, input.TexCoord1.xy).xyz;
    normals1 = normals1 * float3(2, 2, 2) + float3(-1, -1, -2);

    float3 normals2;
    normals2 = Normals02Tex.Sample(Normals02Sampler, input.TexCoord1.zw).xyz;
    normals2 = normals2 * float3(2, 2, 2) + float3(-1, -1, -1);

    float3 normals3;
    normals3 = Normals03Tex.Sample(Normals03Sampler, input.TexCoord2.xy).xyz;
    normals3 = normals3 * float3(2, 2, 2) + float3(-1, -1, -1);

    r0.xyz = float3(0, 0, 1);
    r0.xyz += NormalsAmplitude.xxx * normals1;
    r0.xyz += NormalsAmplitude.yyy * normals2;
    r0.xyz += NormalsAmplitude.zzz * normals3;
    r0.xyz = normalize(r0.xyz) - float3(0, 0, 1);

    r0.w = input.WPosition.w - 8192;
    r1.x = WaterParams.x - 8192;
    r0.w = r0.w / r1.x;
    r1.x = 1 - cb12[42].w;
    r0.w = saturate(r0.w * r1.x + cb12[42].w);

    r1.x = r0.w - 1;
    r1.y = 1 - VarAmounts.z;
    r1.x = saturate(-r1.x * r1.y + VarAmounts.z) - 1;
    r1.xyz = DepthControl.xzw * r1.xxx + float3(1, 1, 1);
    r0.xyz = r1.yyy * r0.xyz + float3(0, 0, 1);

    r0.xyz = normalize(r0.xyz);
    r2.xyz = normalize(input.WPosition.xyz);
    
    r1.y = dot(r2.xyz, r0.xyz) * 2;
    r3.xyz = r0.xyz * -r1.yyy + r2.xyz;
    r1.y = 1 - saturate(dot(-r2.xyz, r0.xyz));

    r1.w = saturate(dot(r3.xyz, SunDir.xyz));
    r1.w = exp2(VarAmounts.x * log2(r1.w));

    r2.xyz = SunColor.xyz * SunDir.www;
    r3.xyz = r2.xyz * r1.www;
    r1.w = saturate(dot(r0.xyz, float3(-99.0 / 1000.0, -99.0 / 1000.0, 99.0 / 100.0)));
    r0.x = saturate(dot(SunDir.xyz, r0.xyz));
    r0.y = exp2(ShallowColor.w * log2(r1.w));
    r2.xyz = r0.yyy * r2.xyz;
    r2.xyz = WaterParams.zzz * r2.xyz;
    r2.xyz = r3.xyz * DeepColor.www + r2.xyz;

    r0.y = r1.y * r1.y;
    r0.y = r0.y * r0.y;
    r0.y = r1.y * r0.y;
    r0.z = 1 - FresnelRI.x;
    r0.y = r0.z * r0.y + FresnelRI.x;
    r0.z = r0.y * r1.x - 1;
    r0.z = r0.w * r0.z + 1;
    
	r1.xyw = DeepColor.xyz - ShallowColor.xyz;
    r1.xyw = r0.yyy * r1.xyw + ShallowColor.xyz;
    r0.xyw = r1.xyw * r0.xxx;
    r1.xyw = ReflectionColor.xyz * VarAmounts.yyy - r0.xyw;
    r0.xyz = r0.zzz * r1.xyw + r0.xyw;
    r0.xyz = r2.xyz * r1.zzz + r0.xyz;
    r1.xyz = input.FogParam.xyz - r0.xyz;
    r0.xyz = input.FogParam.www * r1.xyz + r0.xyz;

    psout.Lighting = saturate(float4(r0.xyz * PosAdjust.www, 0));
#endif

#if defined(STENCIL)
    float4 r0;
    float4 r1;

	r0.xyz = ddx_coarse(input.WorldPosition.zxy);
	r1.xyz = ddy_coarse(input.WorldPosition.yzx);
    
    r0.xyz = normalize(cross(r0.yzx, r1.zxy));
    r1.xyz = normalize(input.WorldPosition.xyz);

    psout.Lighting = float4(0, 0, dot(r1.xyz, r0.xyz), 0);

	r0.x = dot(cb12[12].xyzw, input.WorldPosition.xyzw);
	r0.y = dot(cb12[13].xyzw, input.WorldPosition.xyzw);
	r0.z = dot(cb12[15].xyzw, input.WorldPosition.xyzw);
	r0.xy = r0.xy / r0.zz;
	r1.x = dot(cb12[16].xyzw, input.PreviousWorldPosition.xyzw);
	r1.y = dot(cb12[17].xyzw, input.PreviousWorldPosition.xyzw);
	r0.z = dot(cb12[19].xyzw, input.PreviousWorldPosition.xyzw);
	r0.zw = r1.xy / r0.zz;
	r0.xy = r0.xy + -r0.zw;
	psout.MotionVector = float2(-0.5, 0.5) * r0.xy;
#endif

#if defined(LOD)
    float4 r0, r1, r2, r3;

    float3 normals1;
    normals1 = Normals01Tex.Sample(Normals01Sampler, input.TexCoord1.xy).xyz;
    normals1 = normals1 * float3(2, 2, 2) + float3(-1, -1, -2);

    r0.xyz = float3(0, 0, 1);
    r0.xyz += NormalsAmplitude.xxx * normals1;
    r0.xyz = normalize(r0.xyz) - float3(0, 0, 1);

    r0.xyz = normalize(r0.xyz);
    r1.xyz = normalize(input.WPosition.xyz);

    r0.w = dot(r1.xyz, r0.xyz) * 2;
    r2.xyz = r0.xyz * -r0.www + r1.xyz;
    r0.w = saturate(dot(-r1.xyz, r0.xyz));
    r0.w = 1 - r0.w;

    r1.x = saturate(dot(r2.xyz, SunDir.xyz));
    r1.x = exp2(VarAmounts.x * log2(r1.x));

    r1.yzw = SunColor.xyz * SunDir.www;
    r2.xyz = r1.xxx * r1.yzw;
    r1.x = saturate(dot(r0.xyz, float3(-99.0 / 1000.0, -99.0 / 1000.0, 99.0 / 100.0)));
    r0.x = saturate(dot(SunDir.xyz, r0.xyz));
    r0.y = exp2(ShallowColor.w * log2(r1.x));
    r1.xyz = r0.yyy * r1.yzw;
    r1.xyz = WaterParams.zzz * r1.xyz;
    r1.xyz = r2.xyz * DeepColor.www + r1.xyz;

    r0.y = r0.w * r0.w;
    r0.y = r0.y * r0.y;
    r0.y = r0.w * r0.y;
    r0.z = 1 - FresnelRI.x;
    r0.y = r0.z * r0.y + FresnelRI.x;

    r2.xyz = DeepColor.xyz - ShallowColor.xyz;
    r2.xyz = r0.yyy * r2.xyz + ShallowColor.xyz;

    r0.y = r0.y - 1;
    r0.xzw = r2.xyz * r0.xxx;
    r1.w = input.WPosition.w - 8192;
    r2.x = WaterParams.x - 8192;
    r1.w = r1.w / r2.x;
    r2.x = 1 - cb12[42].w;
    r1.w = saturate(r1.w * r2.x + cb12[42].w);
    r0.y = r1.w * r0.y + 1;

    r2.xyz = ReflectionColor.xyz * VarAmounts.yyy + -r0.xzw;
    r0.xyz = r0.yyy * r2.xyz + r0.xzw;
    r0.xyz = r0.xyz + r1.xyz;
    r1.xyz = input.FogParam.xyz + -r0.xyz;
    r0.xyz = input.FogParam.www * r1.xyz + r0.xyz;

    psout.Lighting = saturate(float4(r0.xyz * PosAdjust.www, 0));
#endif

	return psout;
}

#endif
