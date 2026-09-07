struct instance_aabb
{
	float3 bmin;
	float  testable;	//	1:判定対象 0:常に可視
	float3 bmax;
	float  dummy;
};

StructuredBuffer<instance_aabb> instances : register(t0);
Texture2D<float> hiz : register(t1);
SamplerState hiz_sampler : register(s0);
RWStructuredBuffer<uint> visibility : register(u0);

cbuffer OCCLUSION_CULL_CONSTANT_BUFFER : register(b0)
{
	row_major float4x4 view_projection;
	uint instance_count;
	uint hiz_mip_count;
	float2 hiz_size;		//	ミップ0の解像度
	float depth_bias;
	float3 dummy;
};

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint i = id.x;
	if (i >= instance_count)
		return;

	instance_aabb inst = instances[i];
	if (inst.testable < 0.5f)
	{
		visibility[i] = 1;
		return;
	}

	float2 uv_min = float2(1e9f, 1e9f);
	float2 uv_max = float2(-1e9f, -1e9f);
	float  min_z = 1e9f;

	[unroll]
	for (int c = 0; c < 8; ++c)
	{
		float3 p = float3(
			(c & 1) ? inst.bmax.x : inst.bmin.x,
			(c & 2) ? inst.bmax.y : inst.bmin.y,
			(c & 4) ? inst.bmax.z : inst.bmin.z);

		float4 clip = mul(float4(p, 1.0f), view_projection);
		if (clip.w <= 1e-4f)
		{
			//	近クリップ面より手前の頂点を含む
			visibility[i] = 1;
			return;
		}

		float3 ndc = clip.xyz / clip.w;
		float2 uv = ndc.xy * float2(0.5f, -0.5f) + 0.5f;
		uv_min = min(uv_min, uv);
		uv_max = max(uv_max, uv);
		min_z = min(min_z, ndc.z);
	}

	if (min_z <= 0.0f)
	{
		visibility[i] = 1;
		return;
	}

	uv_min = saturate(uv_min);
	uv_max = saturate(uv_max);
	if (uv_min.x >= uv_max.x || uv_min.y >= uv_max.y)
	{
		//	画面外
		visibility[i] = 0;
		return;
	}

	float2 rect_px = (uv_max - uv_min) * hiz_size;
	float  lod = ceil(log2(max(max(rect_px.x, rect_px.y), 1.0f)));
	lod = clamp(lod, 0.0f, (float)(hiz_mip_count - 1));

	float occluder = 0.0f;
	occluder = max(occluder, hiz.SampleLevel(hiz_sampler, float2(uv_min.x, uv_min.y), lod));
	occluder = max(occluder, hiz.SampleLevel(hiz_sampler, float2(uv_max.x, uv_min.y), lod));
	occluder = max(occluder, hiz.SampleLevel(hiz_sampler, float2(uv_min.x, uv_max.y), lod));
	occluder = max(occluder, hiz.SampleLevel(hiz_sampler, float2(uv_max.x, uv_max.y), lod));

	bool occluded = (min_z - depth_bias) > occluder;
	visibility[i] = occluded ? 0 : 1;
}
