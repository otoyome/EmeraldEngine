//	メッシュレット単位のフラスタム+オクルージョンカリング
struct MeshletInstance
{
	uint   vertex_offset;
	uint   triangle_offset;
	uint   triangle_count;
	uint   primitive_index;
	float3 bounds_center;
	float  bounds_radius;
	uint   transform_index;
	uint   no_cull;
	uint   pipeline_group;
	uint   padding;
};

struct SubmeshTransform
{
	row_major float4x4 world_transform;
};

StructuredBuffer<MeshletInstance> meshlet_instances : register(t0);			//	表示候補メッシュレットバッファ
StructuredBuffer<SubmeshTransform> submesh_transforms : register(t1);		//	姿勢行列バッファ
Texture2D<float> hiz : register(t2);										//	階層型Zバッファ
SamplerState hiz_sampler : register(s0);									//	階層型Zバッファサンプラー
AppendStructuredBuffer<MeshletInstance> visible_meshlets : register(u0);	//	表示メッシュレットバッファ

cbuffer MESHLET_CULL_CONSTANT_BUFFER : register(b0)
{
	row_major float4x4 view_projection;
	uint meshlet_instance_count;
	uint hiz_mip_count;
	float2 hiz_size;		//	ミップ0の解像度
	float depth_bias;
	float2 clip_near_far;	//	x:near y:far
	float dummy;
};

float linearize_depth(float z_ndc, float near_z, float far_z)
{
	return (far_z * near_z) / (far_z - z_ndc * (far_z - near_z));
}

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint i = id.x;
	if (i >= meshlet_instance_count)
		return;

	MeshletInstance inst = meshlet_instances[i];

	//	カリング無効
	if (inst.no_cull != 0)
	{
		visible_meshlets.Append(inst);
		return;
	}

	//	ローカルバウンディングスフィアをワールド空間へ変換
	float4x4 world_transform = submesh_transforms[inst.transform_index].world_transform;
	float3 world_center = mul(float4(inst.bounds_center, 1.0f), world_transform).xyz;
	float scale = max(length(world_transform[0].xyz), max(length(world_transform[1].xyz), length(world_transform[2].xyz)));
	float world_radius = inst.bounds_radius * scale;

	//	視錐台の6平面と境界球で判定
	float4x4 vp_t = transpose(view_projection);
	float4 frustum_planes[6] =
	{
		vp_t[3] + vp_t[0],	//	左
		vp_t[3] - vp_t[0],	//	右
		vp_t[3] + vp_t[1],	//	下
		vp_t[3] - vp_t[1],	//	上
		vp_t[2],			//	近
		vp_t[3] - vp_t[2],	//	遠
	};
	[unroll]
	for (int plane_index = 0; plane_index < 6; ++plane_index)
	{
		if (dot(frustum_planes[plane_index].xyz, world_center) + frustum_planes[plane_index].w < -world_radius * length(frustum_planes[plane_index].xyz))
		{
			return;	//	画面外
		}
	}

	float3 bmin = world_center - world_radius;
	float3 bmax = world_center + world_radius;

	float2 uv_min = float2(1e9f, 1e9f);
	float2 uv_max = float2(-1e9f, -1e9f);
	float  min_z = 1e9f;
	int behind_count = 0;

	[unroll]
	for (int c = 0; c < 8; ++c)
	{
		float3 p = float3(
			(c & 1) ? bmax.x : bmin.x,
			(c & 2) ? bmax.y : bmin.y,
			(c & 4) ? bmax.z : bmin.z);

		float4 clip = mul(float4(p, 1.0f), view_projection);
		if (clip.w <= 1e-4f)
		{
			//	視点より後ろの頂点はスクリーン座標に変換できないため、ループ後にまとめて判定する
			behind_count++;
			continue;
		}

		float3 ndc = clip.xyz / clip.w;
		float2 uv = ndc.xy * float2(0.5f, -0.5f) + 0.5f;
		uv_min = min(uv_min, uv);
		uv_max = max(uv_max, uv);
		min_z = min(min_z, ndc.z);
	}

	if (behind_count == 8)
	{
		return;	//	画面外
	}
	if (behind_count > 0)
	{
		//	一部の頂点だけ視点より後ろ：近クリップ面をまたぐので可視扱いする
		visible_meshlets.Append(inst);
		return;
	}

	if (min_z <= 0.0f)
	{
		visible_meshlets.Append(inst);
		return;
	}

	uv_min = saturate(uv_min);
	uv_max = saturate(uv_max);
	if (uv_min.x >= uv_max.x || uv_min.y >= uv_max.y)
	{
		return;	//	画面外
	}

	float2 rect_px = (uv_max - uv_min) * hiz_size;
	float  lod = ceil(log2(max(max(rect_px.x, rect_px.y), 1.0f)));
	if (lod > (float)(hiz_mip_count - 1))
	{
		//	@tool HiZの最大ミップで覆えない大きさなので遮蔽判定しない
		visible_meshlets.Append(inst);
		return;
	}

	float occluder = 0.0f;
	occluder = max(occluder, hiz.SampleLevel(hiz_sampler, float2(uv_min.x, uv_min.y), lod));
	occluder = max(occluder, hiz.SampleLevel(hiz_sampler, float2(uv_max.x, uv_min.y), lod));
	occluder = max(occluder, hiz.SampleLevel(hiz_sampler, float2(uv_min.x, uv_max.y), lod));
	occluder = max(occluder, hiz.SampleLevel(hiz_sampler, float2(uv_max.x, uv_max.y), lod));

	float min_z_linear = linearize_depth(min_z, clip_near_far.x, clip_near_far.y);
	float occluder_linear = linearize_depth(occluder, clip_near_far.x, clip_near_far.y);

	bool occluded = (min_z_linear - depth_bias) > occluder_linear;
	if (!occluded)
	{
		visible_meshlets.Append(inst);
	}
}
