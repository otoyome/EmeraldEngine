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
//  頂点入力(全パス共通。頂点はメッシュレット経由で読み込む)
struct VS_MODEL_IN
{
    float3 position;
    float3 normal;
    DECLARATION_IN_TANGENT
    DECLARATION_IN_COLOR
    DECLARATION_IN_TEXCOORD
    DECLARATION_IN_JOINTS
    DECLARATION_IN_WEIGHTS
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  深度書き込み用
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
//  プリミティブヘッダー
static const uint vertex_element_capacity = 16; // 頂点エレメント最大数
static const uint mesh_bone_max = 500;	//	1メッシュ500本あれば大丈夫？
static const uint morph_max = 8; // 8個もブレンドするかな？
struct PrimitiveHeader
{
    uint4 header_primitive_parameter;	                //	x : 未使用（接線有無は has_element へ移行）, y : 位置モーフ数, z : 法線モーフ数, w : 未使用
    uint  bone_offset;			                        //	今回ボーン先頭位置
    uint  previous_bone_offset;	                        //	前回ボーン先頭位置
    uint  bone_count;			                        //	ボーン数
    uint  has_element;                                  //  頂点属性の所持ビット
    uint4 element_bases[vertex_element_capacity / 4];   //  属性毎の頂点プール内での先頭位置
    float4 header_position_morph[morph_max / 4];        //	座標モーフオフセット
    float4 header_normal_morph[morph_max / 4];	        //	法線オフセット
};

//	頂点属性の番号
#define VERTEX_ELEMENT_POSITION     0
#define VERTEX_ELEMENT_NORMAL       1
#define VERTEX_ELEMENT_TANGENT      2
#define VERTEX_ELEMENT_COLOR0       3
#define VERTEX_ELEMENT_TEXCOORD0    4
#define VERTEX_ELEMENT_TEXCOORD1    5
#define VERTEX_ELEMENT_JOINTS0      6
#define VERTEX_ELEMENT_JOINTS1      7
#define VERTEX_ELEMENT_JOINTS2      8
#define VERTEX_ELEMENT_JOINTS3      9
#define VERTEX_ELEMENT_WEIGHTS0     10
#define VERTEX_ELEMENT_WEIGHTS1     11
#define VERTEX_ELEMENT_WEIGHTS2     12
#define VERTEX_ELEMENT_WEIGHTS3     13

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

void apply_morph_position(MeshletContext context, inout float3 position, uint local_vertex_id)
{
    [unroll] for (uint index = 0; index < morph_max; ++index)
    {
        if (index >= primitive_parameter(context).y)
            break;

        float weight = position_morph(context)[index / 4][index % 4];
        position += morph_positions[index][local_vertex_id] * weight;
    }
}

void apply_morph_normal(MeshletContext context, inout float3 normal, uint local_vertex_id)
{
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
	context.meshlet_vertex_offset = record.vertex_offset;
	context.meshlet_triangle_offset = record.triangle_offset;

	is_degenerate = vertex_id >= record.triangle_count * 3;
	return context;
}

//	SV_VertexIDからメッシュレット内のローカル頂点番号を経由してメッシュ内の頂点番号を算出
uint meshlet_resolve_local_vertex_index(MeshletContext context, uint vertex_id)
{
	uint triangle_index_pool_index = context.meshlet_triangle_offset + vertex_id;
	uint local_vertex_index = meshlet_triangle_indices.Load(triangle_index_pool_index * 4);
	return meshlet_vertex_indices.Load((context.meshlet_vertex_offset + local_vertex_index) * 4);
}

//	頂点属性を所持しているか
bool meshlet_has_element(MeshletContext context, uint element)
{
	return (primitive_headers[context.primitive_index].has_element & (1u << element)) != 0;
}

//	属性毎のプール内での頂点番号
uint meshlet_element_vertex_index(MeshletContext context, uint element, uint local_vertex_index)
{
	return primitive_headers[context.primitive_index].element_bases[element / 4][element % 4] + local_vertex_index;
}

//	頂点属性の読み出し（所持していない属性は既定値を返す）
float3 meshlet_load_position(MeshletContext context, uint local_vertex_index)
{
	return asfloat(meshlet_positions.Load3(meshlet_element_vertex_index(context, VERTEX_ELEMENT_POSITION, local_vertex_index) * 12));
}
float3 meshlet_load_normal(MeshletContext context, uint local_vertex_index)
{
	if (!meshlet_has_element(context, VERTEX_ELEMENT_NORMAL))
		return float3(0, 1, 0);
	return asfloat(meshlet_normals.Load3(meshlet_element_vertex_index(context, VERTEX_ELEMENT_NORMAL, local_vertex_index) * 12));
}
float4 meshlet_load_tangent(MeshletContext context, uint local_vertex_index)
{
	if (!meshlet_has_element(context, VERTEX_ELEMENT_TANGENT))
		return float4(1, 0, 0, 1);
	return asfloat(meshlet_tangents.Load4(meshlet_element_vertex_index(context, VERTEX_ELEMENT_TANGENT, local_vertex_index) * 16));
}
float4 meshlet_load_color0(MeshletContext context, uint local_vertex_index)
{
	if (!meshlet_has_element(context, VERTEX_ELEMENT_COLOR0))
		return float4(1, 1, 1, 1);
	return asfloat(meshlet_colors0.Load4(meshlet_element_vertex_index(context, VERTEX_ELEMENT_COLOR0, local_vertex_index) * 16));
}
float2 meshlet_load_texcoord0(MeshletContext context, uint local_vertex_index)
{
	if (!meshlet_has_element(context, VERTEX_ELEMENT_TEXCOORD0))
		return float2(0, 0);
	return asfloat(meshlet_texcoords0.Load2(meshlet_element_vertex_index(context, VERTEX_ELEMENT_TEXCOORD0, local_vertex_index) * 8));
}
float2 meshlet_load_texcoord1(MeshletContext context, uint local_vertex_index)
{
	if (!meshlet_has_element(context, VERTEX_ELEMENT_TEXCOORD1))
		return float2(0, 0);
	return asfloat(meshlet_texcoords1.Load2(meshlet_element_vertex_index(context, VERTEX_ELEMENT_TEXCOORD1, local_vertex_index) * 8));
}
uint4 meshlet_load_joints0(MeshletContext context, uint local_vertex_index)
{
	if (!meshlet_has_element(context, VERTEX_ELEMENT_JOINTS0))
		return uint4(0, 0, 0, 0);
	return meshlet_joints0.Load4(meshlet_element_vertex_index(context, VERTEX_ELEMENT_JOINTS0, local_vertex_index) * 16);
}
uint4 meshlet_load_joints1(MeshletContext context, uint local_vertex_index)
{
	if (!meshlet_has_element(context, VERTEX_ELEMENT_JOINTS1))
		return uint4(0, 0, 0, 0);
	return meshlet_joints1.Load4(meshlet_element_vertex_index(context, VERTEX_ELEMENT_JOINTS1, local_vertex_index) * 16);
}
uint4 meshlet_load_joints2(MeshletContext context, uint local_vertex_index)
{
	if (!meshlet_has_element(context, VERTEX_ELEMENT_JOINTS2))
		return uint4(0, 0, 0, 0);
	return meshlet_joints2.Load4(meshlet_element_vertex_index(context, VERTEX_ELEMENT_JOINTS2, local_vertex_index) * 16);
}
uint4 meshlet_load_joints3(MeshletContext context, uint local_vertex_index)
{
	if (!meshlet_has_element(context, VERTEX_ELEMENT_JOINTS3))
		return uint4(0, 0, 0, 0);
	return meshlet_joints3.Load4(meshlet_element_vertex_index(context, VERTEX_ELEMENT_JOINTS3, local_vertex_index) * 16);
}
float4 meshlet_load_weights0(MeshletContext context, uint local_vertex_index)
{
	if (!meshlet_has_element(context, VERTEX_ELEMENT_WEIGHTS0))
		return float4(1, 0, 0, 0);
	return asfloat(meshlet_weights0.Load4(meshlet_element_vertex_index(context, VERTEX_ELEMENT_WEIGHTS0, local_vertex_index) * 16));
}
float4 meshlet_load_weights1(MeshletContext context, uint local_vertex_index)
{
	if (!meshlet_has_element(context, VERTEX_ELEMENT_WEIGHTS1))
		return float4(0, 0, 0, 0);
	return asfloat(meshlet_weights1.Load4(meshlet_element_vertex_index(context, VERTEX_ELEMENT_WEIGHTS1, local_vertex_index) * 16));
}
float4 meshlet_load_weights2(MeshletContext context, uint local_vertex_index)
{
	if (!meshlet_has_element(context, VERTEX_ELEMENT_WEIGHTS2))
		return float4(0, 0, 0, 0);
	return asfloat(meshlet_weights2.Load4(meshlet_element_vertex_index(context, VERTEX_ELEMENT_WEIGHTS2, local_vertex_index) * 16));
}
float4 meshlet_load_weights3(MeshletContext context, uint local_vertex_index)
{
	if (!meshlet_has_element(context, VERTEX_ELEMENT_WEIGHTS3))
		return float4(0, 0, 0, 0);
	return asfloat(meshlet_weights3.Load4(meshlet_element_vertex_index(context, VERTEX_ELEMENT_WEIGHTS3, local_vertex_index) * 16));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//	メッシュレット頂点情報読込
VS_MODEL_IN load_vertex_pulled(MeshletContext meshlet_context, uint vertex_id)
{
    VS_MODEL_IN vin = (VS_MODEL_IN)0;
    uint local_vertex_index = meshlet_resolve_local_vertex_index(meshlet_context, vertex_id);

    vin.position = meshlet_load_position(meshlet_context, local_vertex_index);
    vin.normal = meshlet_load_normal(meshlet_context, local_vertex_index);
#if defined(HAS_TANGENT)
    vin.tangent = meshlet_load_tangent(meshlet_context, local_vertex_index);
#endif  //  defined(HAS_TANGENT)
#if VERTEX_COLOR_MAX > 0
    vin.color[0] = meshlet_load_color0(meshlet_context, local_vertex_index);
#endif  //  VERTEX_COLOR_MAX > 0
#if TEXCOORD_MAX > 0
    vin.texcoord[0] = meshlet_load_texcoord0(meshlet_context, local_vertex_index);
#endif  //  TEXCOORD_MAX > 0
#if TEXCOORD_MAX > 1
    vin.texcoord[1] = meshlet_load_texcoord1(meshlet_context, local_vertex_index);
#endif  //  TEXCOORD_MAX > 1
#if BONE_MAX > 0
    vin.joints[0] = meshlet_load_joints0(meshlet_context, local_vertex_index);
    vin.weights[0] = meshlet_load_weights0(meshlet_context, local_vertex_index);
#endif  //  BONE_MAX > 0
#if BONE_MAX > 4
    vin.joints[1] = meshlet_load_joints1(meshlet_context, local_vertex_index);
    vin.weights[1] = meshlet_load_weights1(meshlet_context, local_vertex_index);
#endif  //  BONE_MAX > 4
#if BONE_MAX > 8
    vin.joints[2] = meshlet_load_joints2(meshlet_context, local_vertex_index);
    vin.weights[2] = meshlet_load_weights2(meshlet_context, local_vertex_index);
#endif  //  BONE_MAX > 8
#if BONE_MAX > 12
    vin.joints[3] = meshlet_load_joints3(meshlet_context, local_vertex_index);
    vin.weights[3] = meshlet_load_weights3(meshlet_context, local_vertex_index);
#endif  //  BONE_MAX > 12

    return vin;
}

//	頂点を読み込んでモーフを適用する(全パス共通)
VS_MODEL_IN load_model_vertex(MeshletContext meshlet_context, uint vertex_id)
{
    VS_MODEL_IN vin = load_vertex_pulled(meshlet_context, vertex_id);

#if defined(HAS_MORPH)  //  モーフィング対応
    uint local_vertex_index_for_morph = meshlet_resolve_local_vertex_index(meshlet_context, vertex_id);
    apply_morph_position(meshlet_context, vin.position, local_vertex_index_for_morph);
    apply_morph_normal(meshlet_context, vin.normal, local_vertex_index_for_morph);
#endif  //  defined(HAS_MORPH)

    return vin;
}

//	スキニング後のワールド空間の頂点(正規化はパスごとに行う)
struct ModelVertex
{
    float3 position;
    float3 normal;
#if defined(HAS_TANGENT)
    float3 tangent;
#endif  //  defined(HAS_TANGENT)
};

//	現在のボーンでスキニングする(全パス共通)
ModelVertex skin_model_vertex(MeshletContext meshlet_context, VS_MODEL_IN vin)
{
    ModelVertex vertex;
#if BONE_MAX > 0
    vertex.position = 0;
    vertex.normal = 0;
#if defined(HAS_TANGENT)
    vertex.tangent = 0;
#endif  //  defined(HAS_TANGENT)
    [unroll] for (uint index = 0; index < BoneMax; ++index)
    {
        uint bone_index = vin.joints[index / 4][index % 4];
        float bone_weight = vin.weights[index / 4][index % 4];
        vertex.position += mul(float4(vin.position, 1), get_bone_transform(meshlet_context, bone_index)).xyz * bone_weight;
        vertex.normal += mul(float4(vin.normal, 0), get_bone_transform(meshlet_context, bone_index)).xyz * bone_weight;
#if defined(HAS_TANGENT)
        vertex.tangent += mul(float4(vin.tangent.xyz, 0), get_bone_transform(meshlet_context, bone_index)).xyz * bone_weight;
#endif  //  defined(HAS_TANGENT)
    }
#else  //  BONE_MAX > 0
    vertex.position = vin.position;
    vertex.normal = vin.normal;
#if defined(HAS_TANGENT)
    vertex.tangent = vin.tangent.xyz;
#endif  //  defined(HAS_TANGENT)
#endif  //  BONE_MAX > 0
    return vertex;
}

//	前フレームのボーンでスキニングした座標
float3 skin_previous_position(MeshletContext meshlet_context, VS_MODEL_IN vin)
{
#if BONE_MAX > 0
    float3 previous_position = 0;
    [unroll] for (uint index = 0; index < BoneMax; ++index)
    {
        uint bone_index = vin.joints[index / 4][index % 4];
        float bone_weight = vin.weights[index / 4][index % 4];
        previous_position += mul(float4(vin.position, 1), get_previous_bone_transform(meshlet_context, bone_index)).xyz * bone_weight;
    }
    return previous_position;
#else  //  BONE_MAX > 0
    return vin.position;
#endif  //  BONE_MAX > 0
}



#include "surface.hlsli"


#endif  //  __MODEL_HLSLI__
