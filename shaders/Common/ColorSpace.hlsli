#ifndef HYP_COLOR_SPACE
#define HYP_COLOR_SPACE

float3 SrgbToLinear(float3 InColor)
{
	float3 Color = saturate(InColor);
	return float3(Color.r <= .04045 ? Color.r / 12.92 : pow((Color.r + .055) / 1.055, 2.4),
	              Color.g <= .04045 ? Color.g / 12.92 : pow((Color.g + .055) / 1.055, 2.4),
	              Color.b <= .04045 ? Color.b / 12.92 : pow((Color.b + .055) / 1.055, 2.4));
}
#endif
