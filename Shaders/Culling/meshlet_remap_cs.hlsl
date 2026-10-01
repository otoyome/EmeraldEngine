//	@tool 前フレームの可視メッシュレットリストのプリミティブ番号を今フレームの番号へ付け替える
//	@tool meshlet_culling_cs.hlsl の MeshletInstance と同じレイアウトにしておくこと
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

RWStructuredBuffer<MeshletInstance> visible_meshlets : register(u0);	//	@tool 前フレームの表示メッシュレットバッファ
StructuredBuffer<uint> remap_table : register(t0);						//	@tool 前フレームの番号 → 今フレームの番号

cbuffer MESHLET_REMAP_CONSTANT_BUFFER : register(b0)
{
	uint meshlet_capacity;
	uint remap_count;
	uint2 remap_dummy;
};

static const uint invalid_primitive_index = 0xFFFFFFFF;

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	uint i = id.x;
	if (i >= meshlet_capacity)
		return;

	uint primitive_index = visible_meshlets[i].primitive_index;
	uint new_primitive_index = (primitive_index < remap_count) ? remap_table[primitive_index] : invalid_primitive_index;
	if (new_primitive_index == invalid_primitive_index)
	{
		//	@tool 付け替え先が無い(消えた・LODが変わった)メッシュレットは三角形数0にして描画させない
		visible_meshlets[i].triangle_count = 0;
		visible_meshlets[i].primitive_index = 0;
		return;
	}
	visible_meshlets[i].primitive_index = new_primitive_index;
}
