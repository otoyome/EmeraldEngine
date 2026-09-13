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
#define DECLARATION_IN_COLOR    float4 color[VertexColorMax] : COLOR;
#define DECLARATION_OUT_COLOR   float4 color[VertexColorMax] : COLOR;
#else   //  VERTEX_COLOR_MAX > 0
#define DECLARATION_IN_COLOR
#define DECLARATION_OUT_COLOR
#endif  //  VERTEX_COLOR_MAX > 0

#if TEXCOORD_MAX > 0
static const uint TexcoordMax = TEXCOORD_MAX;
#define DECLARATION_IN_TEXCOORD     float2 texcoord[TexcoordMax] : TEXCOORD;
#define DECLARATION_OUT_TEXCOORD    float2 texcoord[TexcoordMax] : TEXCOORD;
#else   //  TEXCOORD_MAX > 0
#define DECLARATION_IN_TEXCOORD
#define DECLARATION_OUT_TEXCOORD
#endif  //  TEXCOORD_MAX > 0

#if BONE_MAX > 0
static const uint AlignmentBoneMax = alignment( BONE_MAX, 4 ) / 4;
static const uint BoneMax = BONE_MAX;
#define DECLARATION_IN_JOINTS   uint4 joints[AlignmentBoneMax] : JOINTS;
#define DECLARATION_IN_WEIGHTS  float4 weights[AlignmentBoneMax] : WEIGHTS;
#else   //  BONE_MAX > 0
#define DECLARATION_IN_JOINTS
#define DECLARATION_IN_WEIGHTS
#endif  //  BONE_MAX > 0

#if defined(HAS_TANGENT)
#define DECLARATION_IN_TANGENT  float4 tangent : TANGENT;
#define DECLARATION_OUT_TANGENT float4 w_tangent : TANGENT;
#else   //  defined(HAS_TANGENT)
#define DECLARATION_IN_TANGENT
#define DECLARATION_OUT_TANGENT
#endif  //  defined(HAS_TANGENT)

////////////////////////////////////////////////////////////////////////////////////////////////////
//  深度書き込み用
struct VS_DEPTH_IN
{
    float3 position : POSITION;
    float3 normal : NORMAL;
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
    float3 position : POSITION;
    float3 normal : NORMAL;
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
    float3 position : POSITION;
    float3 normal : NORMAL;
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
};

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
};

////////////////////////////////////////////////////////////////////////////////////////////////////
//  ボーン対応する必要のあるモデルの計算
static const uint mesh_bone_max = 500;	//	1メッシュ500本あれば大丈夫？
static const uint morph_max = 8; // 8個もブレンドするかな？
struct PrimitiveHeader
{
    uint4 header_primitive_parameter;	//	x : 接線有無, y : 位置モーフ数, z : 法線モーフ数, w : 未使用
    uint  bone_offset;			//	bone_matrices 内の現在ボーン先頭
    uint  previous_bone_offset;	//	bone_matrices 内の前フレームボーン先頭
    uint  bone_count;			//	ボーン数
    uint  base_vertex;

    float4 header_position_morph[morph_max / 4];
    float4 header_normal_morph[morph_max / 4];
};

struct BoneMatrix
{
    row_major float4x4 transform;
};

StructuredBuffer<PrimitiveHeader> primitive_headers : register(t100);
StructuredBuffer<BoneMatrix> bone_matrices : register(t101);

cbuffer PRIMITIVE_CONSTANT_BUFFER
{
    uint primitive_index;
    uint3 primitive_index_padding;
};

#define primitive_parameter (primitive_headers[primitive_index].header_primitive_parameter)

float4x4 get_bone_transform(uint bone_index)
{
    return bone_matrices[primitive_headers[primitive_index].bone_offset + bone_index].transform;
}

float4x4 get_previous_bone_transform(uint bone_index)
{
    return bone_matrices[primitive_headers[primitive_index].previous_bone_offset + bone_index].transform;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(HAS_MORPH)
StructuredBuffer<float3> morph_positions[morph_max] : register(t80);
StructuredBuffer<float3> morph_normals[morph_max] : register(t88);

#define position_morph      (primitive_headers[primitive_index].header_position_morph)
#define normal_morph        (primitive_headers[primitive_index].header_normal_morph)

void apply_morph_position(inout float3 position, uint vertex_id)
{
    uint local_vertex_id = vertex_id - primitive_headers[primitive_index].base_vertex;
    [unroll] for (uint index = 0; index < morph_max; ++index)
    {
        if (index >= primitive_parameter.y)
            break;

        float weight = position_morph[index / 4][index % 4];
        position += morph_positions[index][local_vertex_id] * weight;
    }
}

void apply_morph_normal(inout float3 normal, uint vertex_id)
{
    uint local_vertex_id = vertex_id - primitive_headers[primitive_index].base_vertex;
    [unroll] for (uint index = 0; index < morph_max; ++index)
    {
        if (index >= primitive_parameter.z)
            break;

        float weight = normal_morph[index / 4][index % 4];
        normal = lerp(normal, morph_normals[index][local_vertex_id], weight);
    }
}
#endif  //  defined(HAS_MORPH)

#include "surface.hlsli"


#endif  //  __MODEL_HLSLI__
