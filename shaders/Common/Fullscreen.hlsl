struct FFullscreenVertex
{
	float4 Position : SV_Position;
	float2 Uv : TEXCOORD0;
};

FFullscreenVertex VSMain(float2 InPosition : POSITION)
{
	FFullscreenVertex Result;
	Result.Position = float4(InPosition, 0, 1);
	Result.Uv = InPosition * float2(.5, -.5) + .5;
	return Result;
}
