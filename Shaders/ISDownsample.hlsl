#include "Common/DummyVSTexCoord.hlsl"
#include "Common/Color.hlsl"
#include "Common/FrameBuffer.hlsl"

typedef VS_OUTPUT PS_INPUT;

struct PS_OUTPUT
{
	float4 Color						: SV_Target0;
};

#if defined(PSHADER)
SamplerState SourceSampler				: register(s0);
SamplerState PreviousFrameSourceSampler : register(s1);
SamplerState MotionVectorsSampler		: register(s2);

Texture2D<float4> SourceTex				: register(t0);
Texture2D<float4> PreviousFrameSourceTex: register(t1);
Texture2D<float4> MotionVectorsTex		: register(t2);

cbuffer PerGeometry						: register(b2)
{
	float2 TexelSize					: packoffset(c0);
	float SamplesCount					: packoffset(c0.z);
	bool CompensateJittering			: packoffset(c0.w);
	float4 OffsetsAndWeights[16]		: packoffset(c1);
};

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT psout;

	float4 downsampledColor = 0;
	float luminance = 0;
	for (uint sampleIndex = 0; sampleIndex < asuint(SamplesCount); ++sampleIndex)
	{
		float2 texCoord = OffsetsAndWeights[sampleIndex].xy * TexelSize.xy + input.TexCoord;
#if defined(IGNORE_BRIGHTEST)
		texCoord = GetDynamicResolutionAdjustedScreenPosition(texCoord);
#endif
		float4 sourceColor = SourceTex.Sample(SourceSampler, texCoord);
#if defined(IGNORE_BRIGHTEST)
		downsampledColor += sourceColor;
#else
		float sampleLuminance = RGBToLuminanceAlternative(sourceColor.xyz);
		if (sampleLuminance > luminance)
		{
			downsampledColor = sourceColor;
			luminance = sampleLuminance;
		}
#endif
	}
#if defined(IGNORE_BRIGHTEST)
	psout.Color = downsampledColor / asuint(SamplesCount);
#else
	if (CompensateJittering)
	{
		float2 adjustedTexCoord = GetDynamicResolutionAdjustedScreenPosition(input.TexCoord);
		float2 motion = MotionVectorsTex.Sample(MotionVectorsSampler, adjustedTexCoord).xy;
		float4 previousFrameColor =
			PreviousFrameSourceTex.Sample(PreviousFrameSourceSampler, input.TexCoord + motion).xyzw;
		psout.Color = 0.5 * (previousFrameColor + downsampledColor);
	}
	else
	{
		psout.Color = downsampledColor;
	}
#endif

	return psout;
}
#endif
