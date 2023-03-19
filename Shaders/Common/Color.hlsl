static float GammaCorrectionValue = 2.2;

float RGBToLuminance(float3 color)
{ 
	return dot(color, float3(0.2125, 0.7154, 0.0721));
}

float RGBToLuminanceAlternative(float3 color)
{ 
	return dot(color, float3(0.3, 0.59, 0.11)); 
}

float RGBToLuminance2(float3 color)
{ 
	return dot(color, float3(0.299, 0.587, 0.114));
}
