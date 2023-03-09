cbuffer PerFrame										: register(b12)
{
	row_major float4x4 CameraView						: packoffset(c0);
	row_major float4x4 CameraProj						: packoffset(c4);
	row_major float4x4 CameraViewProj					: packoffset(c8);
	row_major float4x4 CameraViewProjUnjittered			: packoffset(c12);
	row_major float4x4 CameraPreviousViewProjUnjittered : packoffset(c16);
	row_major float4x4 CameraProjUnjittered				: packoffset(c20);
	row_major float4x4 CameraProjUnjitteredInverse		: packoffset(c24);
	row_major float4x4 CameraViewInverse				: packoffset(c28);
	row_major float4x4 CameraViewProjInverse			: packoffset(c32);
	row_major float4x4 CameraProjInverse				: packoffset(c36);
	float4 PosAdjust									: packoffset(c40);
	float4 PreviousPosAdjust							: packoffset(c41);	// fDRClampOffset in w
	float4 FrameParams									: packoffset(c42);	// inverse fGamma in x, some flags in yzw
	float4 DynamicResolutionParams1						: packoffset(c43);	// fDynamicResolutionWidthRatio in x, fDynamicResolutionHeightRatio in y, fDynamicResolutionPreviousWidthRatio in z, fDynamicResolutionPreviousHeightRatio in w
	float4 DynamicResolutionParams2						: packoffset(c44);  // inverse fDynamicResolutionWidthRatio in x, inverse fDynamicResolutionHeightRatio in y, fDynamicResolutionWidthRatio - fDRClampOffset in z, fDynamicResolutionPreviousWidthRatio - fDRClampOffset in w
}

float2 GetDynamicResolutionAdjustedScreenPosition(float2 screenPosition)
{
	float2 adjustedScreenPosition = max(0.0.xx, DynamicResolutionParams1.xy * screenPosition);
	adjustedScreenPosition.x = min(DynamicResolutionParams2.z, adjustedScreenPosition.x);
	adjustedScreenPosition.y = min(DynamicResolutionParams1.y, adjustedScreenPosition.y);
	return adjustedScreenPosition;
}

float2 GetPreviousDynamicResolutionAdjustedScreenPosition(float2 screenPosition)
{
	float2 adjustedScreenPosition = max(0.0.xx, DynamicResolutionParams1.zw * screenPosition);
	adjustedScreenPosition.x = min(DynamicResolutionParams2.w, adjustedScreenPosition.x);
	adjustedScreenPosition.y = min(DynamicResolutionParams1.w, adjustedScreenPosition.y);
	return adjustedScreenPosition;
}
