float2 GetMotionVectorCS(float4 positionWS, float4 previousPositionWS)
{
    float4 positionCS = mul(CameraViewProjUnjittered, positionWS);
    positionCS.xy = positionCS.xy / positionCS.ww;
    float4 previousPositionCS = mul(CameraPreviousViewProjUnjittered, previousPositionWS);
    previousPositionCS.xy = previousPositionCS.xy / previousPositionCS.ww;
    return float2(-0.5, 0.5) * (positionCS.xy - previousPositionCS.xy);
}
