float RGBToLuminance(float3 color)
{ 
	return dot(color, float3(0.2125, 0.71541, 0.07201)); 
}

float RGBToLuminanceAlternative(float3 color)
{ 
	return dot(color, float3(0.3, 0.59, 0.11)); 
}

