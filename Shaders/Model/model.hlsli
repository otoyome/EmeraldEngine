//  下記の定数を定義してインクルードする事
//  #define HAS_TANGENT         : 接線を所持しているかどうか
//  #define VERTEX_COLOR_MAX    : 頂点カラー数
//  #define TEXCOORD_MAX        : UV座標数
//  #define BONE_MAX            : ボーンの最大数
//  #define HAS_MORPH           : モーフィングするかどうか
//  #define FORWARD_RENDERING   : フォワードレンダリング用の計算を行う
#ifndef __MODEL_HLSLI__
#define __MODEL_HLSLI__

#include	"lights.hlsli"
#include    "common.hlsli"

#include	"scene_constant_buffer.hlsli"
#include	"shadow_map.hlsli"

#include    "gbuffer.hlsli"

//  ↑のマクロの宣言状況に応じて追加するかしないかを決めるためのマクロ
#if VERTEX_COLOR_MAX > 0
static const uint VertexColorMax = VERTEX_COLOR_MAX;
#define DECLARATION_IN_COLOR    float4 color[VertexColorMax];
#define DECLARATION_OUT_COLOR   float4 color[VertexColorMax] : COLOR;
#else   //  VERTEX_COLOR_MAX > 0
#define DECLARATION_IN_COLOR
#define DECLARATION_OUT_COLOR
#endif  //  VERTEX_COLOR_MAX > 0

#if TEXCOORD_MAX > 0
static const uint TexcoordMax = TEXCOORD_MAX;
#define DECLARATION_IN_TEXCOORD     float2 texcoord[TexcoordMax];
#define DECLARATION_OUT_TEXCOORD    float2 texcoord[TexcoordMax] : TEXCOORD;
#else   //  TEXCOORD_MAX > 0
#define DECLARATION_IN_TEXCOORD
#define DECLARATION_OUT_TEXCOORD
#endif  //  TEXCOORD_MAX > 0

#if BONE_MAX > 0
static const uint AlignmentBoneMax = alignment( BONE_MAX, 4 ) / 4;
static const uint BoneMax = BONE_MAX;
#define DECLARATION_IN_JOINTS   uint4 joints[AlignmentBoneMax];
#define DECLARATION_IN_WEIGHTS  float4 weights[AlignmentBoneMax];
#else   //  BONE_MAX > 0
#define DECLARATION_IN_JOINTS
#define DECLARATION_IN_WEIGHTS
#endif  //  BONE_MAX > 0

#if defined(HAS_TANGENT)
#define DECLARATION_IN_TANGENT  float4 tangent;
#define DECLARATION_OUT_TANGENT float4 w_tangent : TANGENT;
#else   //  defined(HAS_TANGENT)
#define DECLARATION_IN_TANGENT
#define DECLARATION_OUT_TANGENT
#endif  //  defined(HAS_TANGENT)

////////////////////////////////////////////////////////////////////////////////////////////////////
//  深度書き込み用
struct VS_DEPTH_IN
{
    float3 position;
    float3 normal;
    DECLARATION_IN_TANGENT
    DECLARATION_IN_COLOR
    DECLARATION_IN_TEXCOORD
    DECLARATION_IN_JOINTS
    DECLARATION_IN_WEIGHTS
};

struct VS_DEPTH_OUT
{
    float4 position : SV_POSITION;
    float4 w_position : POSITION;   //  xyz : ワールド座標, w = 空き
    float4 w_normal : NORMAL; //  xyz : ワールド法線, w = 空き
    DECLARATION_OUT_TANGENT
    DECLARATION_OUT_COLOR
    DECLARATION_OUT_TEXCOORD
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  シャドウマップ生成用
struct VS_SHADOW_MAP_IN
{
    float3 position;
    float3 normal;
    DECLARATION_IN_COLOR
    DECLARATION_IN_TEXCOORD
    DECLARATION_IN_JOINTS
    DECLARATION_IN_WEIGHTS
};

struct VS_SHADOW_MAP_OUT
{
    float4 position : SV_POSITION;
    float4 w_position : POSITION;   //  xyz : ワールド座標, w = 空き
    float4 w_normal : NORMAL; //  xyz : ワールド法線, w = 空き
    DECLARATION_OUT_COLOR
    DECLARATION_OUT_TEXCOORD
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  GBuffer生成用
struct VS_GBUFFER_IN
{
    float3 position;
    float3 normal;
    DECLARATION_IN_TANGENT
    DECLARATION_IN_COLOR
    DECLARATION_IN_TEXCOORD
    DECLARATION_IN_JOINTS
    DECLARATION_IN_WEIGHTS
};

struct VS_GBUFFER_OUT
{
    float4 position : SV_POSITION;
    float4 w_position : POSITION;   //  xyz : ワールド座標, w = 空き
    float4 w_normal : NORMAL; //  xyz : ワールド法線, w = 空き
    DECLARATION_OUT_TANGENT
    DECLARATION_OUT_COLOR
    DECLARATION_OUT_TEXCOORD

    float4 current_clip_position : CLIP_POSITION0;
    float4 previous_clip_position : CLIP_POSITION1;
    nointerpolation uint primitive_index : PRIMITIVE_INDEX;
    nointerpolation uint meshlet_debug_id : MESHLET_DEBUG_ID;
};
#define HAS_MESHLET_DEBUG_ID

////////////////////////////////////////////////////////////////////////////////////////////////////
//  フォワードレンダリング用
#define VS_FORWARD_IN VS_GBUFFER_IN

struct VS_FORWARD_OUT
{
    float4 position : SV_POSITION;
    float4 w_position : POSITION; //  xyz : ワールド座標, w = 空き
    float4 w_normal : NORMAL; //  xyz : ワールド法線, w = 空き
    DECLARATION_OUT_TANGENT
    DECLARATION_OUT_COLOR
    DECLARATION_OUT_TEXCOORD

    float4 current_clip_position : CLIP_POSITION0;
    float4 previous_clip_position : CLIP_POSITION1;
    float4 shadow_texcoords[shadow_map_max] : SHADOW_TEXCOORDS;
    nointerpolation uint primitive_index : PRIMITIVE_INDEX;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  ボーン対応する必要のあるモデルの計算
static const uint mesh_bone_max = 500;	//	1メッシュ500本あれば大丈夫？
static const uint morph_max = 8; // 8個もブレンドするかな？
struct PrimitiveHeader
{
    uint4 header_primitive_parameter;	//	x : 接線有無, y : 位置モーフ数, z : 法線モーフ数, w : 未使用
    uint  bone_offset;			        //	今回ボーン先頭位置
    uint  previous_bone_offset;	        //	前回ボーン先頭位置
    uint  bone_count;			        //	ボーン数
    uint  base_vertex;                  //  先頭頂点番号

    float4 header_position_morph[morph_max / 4];
    float4 header_normal_morph[morph_max / 4];
};

struct BoneMatrix
{
    row_major float4x4 transform;
};

StructuredBuffer<PrimitiveHeader> primitive_headers : register(t100);
StructuredBuffer<BoneMatrix> bone_matrices : register(t101);

//	メッシュレットコンテキスト
struct MeshletContext
{
	uint primitive_index;
	uint mesh_base_vertex;
	uint meshlet_vertex_offset;
	uint meshlet_triangle_offset;
};

#define primitive_parameter(context) (primitive_headers[(context).primitive_index].header_primitive_parameter)

float4x4 get_bone_transform(MeshletContext context, uint bone_index)
{
    return bone_matrices[primitive_headers[context.primitive_index].bone_offset + bone_index].transform;
}

float4x4 get_previous_bone_transform(MeshletContext context, uint bone_index)
{
    return bone_matrices[primitive_headers[context.primitive_index].previous_bone_offset + bone_index].transform;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(HAS_MORPH)
StructuredBuffer<float3> morph_positions[morph_max] : register(t80);
StructuredBuffer<float3> morph_normals[morph_max] : register(t88);

#define position_morph(context)      (primitive_headers[(context).primitive_index].header_position_morph)
#define normal_morph(context)        (primitive_headers[(context).primitive_index].header_normal_morph)

void apply_morph_position(MeshletContext context, inout float3 position, uint vertex_id)
{
    uint local_vertex_id = vertex_id - context.mesh_base_vertex;
    [unroll] for (uint index = 0; index < morph_max; ++index)
    {
        if (index >= primitive_parameter(context).y)
            break;

        float weight = position_morph(context)[index / 4][index % 4];
        position += morph_positions[index][local_vertex_id] * weight;
    }
}

void apply_morph_normal(MeshletContext context, inout float3 normal, uint vertex_id)
{
    uint local_vertex_id = vertex_id - context.mesh_base_vertex;
    [unroll] for (uint index = 0; index < morph_max; ++index)
    {
        if (index >= primitive_parameter(context).z)
            break;

        float weight = normal_morph(context)[index / 4][index % 4];
        normal = lerp(normal, morph_normals[index][local_vertex_id], weight);
    }
}
#endif  //  defined(HAS_MORPH)

////////////////////////////////////////////////////////////////////////////////////////////////////
//	メッシュレット頂点プールバッファ
ByteAddressBuffer meshlet_positions : register(t102);
ByteAddressBuffer meshlet_normals : register(t103);
ByteAddressBuffer meshlet_tangents : register(t104);
ByteAddressBuffer meshlet_colors0 : register(t105);
ByteAddressBuffer meshlet_texcoords0 : register(t106);
ByteAddressBuffer meshlet_texcoords1 : register(t107);
ByteAddressBuffer meshlet_joints0 : register(t108);
ByteAddressBuffer meshlet_joints1 : register(t109);
ByteAddressBuffer meshlet_joints2 : register(t110);
ByteAddressBuffer meshlet_joints3 : register(t111);
ByteAddressBuffer meshlet_weights0 : register(t112);
ByteAddressBuffer meshlet_weights1 : register(t113);
ByteAddressBuffer meshlet_weights2 : register(t114);
ByteAddressBuffer meshlet_weights3 : register(t115);
ByteAddressBuffer meshlet_vertex_indices : register(t116);
ByteAddressBuffer meshlet_triangle_indices : register(t117);

//	メッシュレットインスタンス情報
struct MeshletInstanceRecord
{
	uint vertex_offset;
	uint triangle_offset;
	uint triangle_count;
	uint primitive_index;
	float3 bounds_center;
	float bounds_radius;
	uint transform_index;
	uint no_cull;
	uint pipeline_group;
	uint padding;
};
StructuredBuffer<MeshletInstanceRecord> visible_meshlet_instances : register(t118);

//	メッシュレットコンテキスト構築
MeshletContext meshlet_build_context(uint instance_id, uint vertex_id, out bool is_degenerate)
{
	MeshletInstanceRecord record = visible_meshlet_instances[instance_id];

	MeshletContext context;
	context.primitive_index = record.primitive_index;
	context.mesh_base_vertex = primitive_headers[record.primitive_index].base_vertex;
	context.meshlet_vertex_offset = record.vertex_offset;
	context.meshlet_triangle_offset = record.triangle_offset;

	is_degenerate = vertex_id >= record.triangle_count * 3;
	return context;
}

//	SV_VertexIDから、このメッシュレット内のローカル頂点番号を経由してグローバル頂点番号を求める
uint meshlet_resolve_global_vertex_index(MeshletContext context, uint vertex_id)
{
	uint triangle_index_pool_index = context.meshlet_triangle_offset + vertex_id;
	uint local_vertex_index = meshlet_triangle_indices.Load(triangle_index_pool_index * 4);
	uint mesh_local_vertex_index = meshlet_vertex_indices.Load((context.meshlet_vertex_offset + local_vertex_index) * 4);
	return context.mesh_base_vertex + mesh_local_vertex_index;
}

float3 meshlet_load_position(uint vertex_index) { return asfloat(meshlet_positions.Load3(vertex_index * 12)); }
float3 meshlet_load_normal(uint vertex_index) { return asfloat(meshlet_normals.Load3(vertex_index * 12)); }
float4 meshlet_load_tangent(uint vertex_index) { return asfloat(meshlet_tangents.Load4(vertex_index * 16)); }
float4 meshlet_load_color0(uint vertex_index) { return asfloat(meshlet_colors0.Load4(vertex_index * 16)); }
float2 meshlet_load_texcoord0(uint vertex_index) { return asfloat(meshlet_texcoords0.Load2(vertex_index * 8)); }
float2 meshlet_load_texcoord1(uint vertex_index) { return asfloat(meshlet_texcoords1.Load2(vertex_index * 8)); }
uint4 meshlet_load_joints0(uint vertex_index) { return meshlet_joints0.Load4(vertex_index * 16); }
uint4 meshlet_load_joints1(uint vertex_index) { return meshlet_joints1.Load4(vertex_index * 16); }
uint4 meshlet_load_joints2(uint vertex_index) { return meshlet_joints2.Load4(vertex_index * 16); }
uint4 meshlet_load_joints3(uint vertex_index) { return meshlet_joints3.Load4(vertex_index * 16); }
float4 meshlet_load_weights0(uint vertex_index) { return asfloat(meshlet_weights0.Load4(vertex_index * 16)); }
float4 meshlet_load_weights1(uint vertex_index) { return asfloat(meshlet_weights1.Load4(vertex_index * 16)); }
float4 meshlet_load_weights2(uint vertex_index) { return asfloat(meshlet_weights2.Load4(vertex_index * 16)); }
float4 meshlet_load_weights3(uint vertex_index) { return asfloat(meshlet_weights3.Load4(vertex_index * 16)); }



#include "surface.hlsli"


#endif  //  __MODEL_HLSLI__
