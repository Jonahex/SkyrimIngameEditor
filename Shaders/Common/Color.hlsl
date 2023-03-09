float RGBToLuminance(float3 color)
{ 
	return dot(color, float3(0.2125, 0.71541, 0.07201)); 
}
