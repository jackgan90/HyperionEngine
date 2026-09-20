Texture2D<float> ObjectMask : register(t0);

cbuffer OutlineV1 : register(b0)
{
	float4 Parameters;
};

float Exterior(int2 InPixel, int2 InSize, float InRadius)
{
	if (ObjectMask.Load(int3(InPixel, 0)) > .5)
	{
		return 0;
	}
	float Coverage = 0;
	int Radius = int(ceil(InRadius + .5));
	for (int Y = -Radius; Y <= Radius; ++Y)
	{
		for (int X = -Radius; X <= Radius; ++X)
		{
			int2 Position = InPixel + int2(X, Y);
			if (all(Position >= 0) && all(Position < InSize))
			{
				float Edge = saturate(InRadius + .5 - length(float2(X, Y)));
				Coverage = max(Coverage, Edge * ObjectMask.Load(int3(Position, 0)));
			}
		}
	}
	return Coverage;
}

float PSMain(float4 InPosition : SV_Position) : SV_Target0
{
	uint Width;
	uint Height;
	ObjectMask.GetDimensions(Width, Height);
	int Scale = int(Parameters.y);
	int2 Pixel = int2(InPosition.xy) * Scale;
	float Coverage = 0;
	for (int Y = 0; Y < Scale; ++Y)
	{
		for (int X = 0; X < Scale; ++X)
		{
			Coverage += Exterior(Pixel + int2(X, Y), int2(Width, Height), Parameters.x * Scale);
		}
	}
	return Coverage / (Scale * Scale);
}
