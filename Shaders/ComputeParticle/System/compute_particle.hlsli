#ifndef __COMPUND_COMPUTE_PARTICLE_HLSLI_
#define __COMPUND_COMPUTE_PARTICLE_HLSLI_

#include "scene_constant_buffer.hlsli"
#include "compute_particle_structs.hlsli"

static const float PI  = 3.1415926f;
static const float PI2 = 6.28318530718f;

//  パーティクルスレッド数
static const int NumDispatchThread = 256;

struct draw_indirect
{
    uint vertex_count_per_instance;
    uint instance_count;
    uint start_vertex_location;
    uint start_instance_location;
};
static const uint update_indirect_size = 12;
static const uint draw_indirect_size = 16;
//	00バイト目：現在のパーティクル総数
//	04バイト目：1F前のパーティクル総数
//  08バイト目：パーティクル更新回数
//  20バイト目以降エミッターIDごとの
static const uint NumCurrentParticleOffset = 0; //  size = 4
static const uint NumPreviousParticleOffset = NumCurrentParticleOffset + 4; //  size = 4
static const uint UpdateParticleDispatchIndirect = NumPreviousParticleOffset + 4; //  size = 12
static const uint FixedIndirectDataSize = UpdateParticleDispatchIndirect + update_indirect_size;

//  エミッターヘッダー
struct emitter_header
{
	uint emitter_binary_head; // エミッターバイナリ先頭アドレス
    uint particle_binary_head; // パーティクルバイナリ先頭アドレス
    uint particle_index_head; // パーティクル番号先頭アドレス
    uint particle_size; // パーティクル一つのサイズ( エミッター毎にパーティクルの型が変わるため )
};

//  エミッター情報
struct emitter_data
{
    uint spawn_head; //	パーティクルヘッダーの割り当て先頭位置
    uint spawn_num; //	パーティクルヘッダーの割り当て数
    uint particle_num; //	現在パーティクル数
    uint particle_max; //	最大パーティクル数
};

//  エミッター範囲
struct emitter_range
{
    uint alive_head; //	生存範囲の先頭
    uint alive_end; //	生存範囲の終端
    uint dead_head; //	死亡範囲の先頭
    uint dead_end; //	死亡範囲の終端
};

////////////////////////////////////////////////////////////////////////////////////////////////////////
//  パーティクルヘッダー
struct particle_header
{
    uint tag; //	1 : 生存フラグ, 10 : エミッター番号, 21 : パーティクル番号
    float depth; //	深度
};
static const uint invalid_tag = 0xFFFFFFFF;
static const uint tag_alive_bit_count = 1;
static const uint tag_emitter_id_bit_count = 10;
static const uint tag_particle_index_bit_count = 21;

static const uint tag_alive_bit_shift_count = tag_emitter_id_bit_count + tag_particle_index_bit_count;
static const uint tag_emitter_id_bit_shift_count = tag_particle_index_bit_count;
static const uint tag_particle_index_bit_shift_count = 0;

static const uint tag_emitter_id_bit_mask = 0x000003FF;
static const uint tag_particle_index_bit_mask = 0x001FFFFF;

////////////////////////////////////////////////////////////////////////////////////////////////////////
//	定数バッファ
cbuffer COMMON_CONSTANTS : register(b10)
{
	float elapsed_time;	// 経過時間
    uint3 x_of_shift_seed; // XorShift乱数種
    uint total_emitter_count; // システムで管理できるエミッター総数
    uint total_particle_count; // システムで管理できるパーティクル総数
    uint total_cpu_spawn_count; // 発生予定のパーティクル総数
    uint erase_id_count; //	破棄Id数
    uint4 erase_id_list[128 / 4]; //	エミッター破棄Idリスト
};

////////////////////////////////////////////////////////////////////////////////////////////////////////
uint set_particle_header_tag(bool alive, uint emitter_id, uint particle_index)
{
    uint tag = 0;

    //  パーティクル番号を記録
    tag = (particle_index & tag_particle_index_bit_mask) << tag_particle_index_bit_shift_count;
    
    //  エミッターIDを記録
    tag |= (emitter_id & tag_emitter_id_bit_mask) << tag_emitter_id_bit_shift_count;

    //  生存フラグを立てる
    tag = alive ? tag : (tag | (1 << tag_alive_bit_shift_count));
    return tag;
}

//  パーティクルヘッダー生成関数
particle_header generate_particle_header(uint emitter_id, uint particle_index)
{
    particle_header header;
    header.tag = set_particle_header_tag(true, emitter_id, particle_index);
    header.depth = 0.0f;    //  これは更新時に記録
    return header;
}

//  パーティクルヘッダーから情報取得関数
void get_particle_header_parameter(in particle_header header, out bool alive, out uint emitter_id, out uint particle_index)
{
    //  生存フラグ
    alive = !(header.tag & (1 << tag_alive_bit_shift_count));

    //  エミッターID
    emitter_id = (header.tag >> tag_emitter_id_bit_shift_count) & tag_emitter_id_bit_mask;

    //  パーティクルID
    particle_index = (header.tag >> tag_particle_index_bit_shift_count) & tag_particle_index_bit_mask;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
//  その他汎用関数群
////////////////////////////////////////////////////////////////////////////////////////////////////////
//  フラグ操作
bool has_flag(uint state, uint flag)
{
    return (state & flag) != 0u;
}

void set_flag(inout uint state, uint flag)
{
    state |= flag;
}

void clear_flag(inout uint state, uint flag)
{
    state &= ~flag;
}

//  乱数生成関数
struct x_or_shift_128_state
{
    uint x;
    uint y;
    uint z;
    uint w;
};

uint x_or_shift_128(inout x_or_shift_128_state state)
{
    uint t = state.x ^ (state.x << 11);
    state.x = state.y;
    state.y = state.z;
    state.z = state.w;
    state.w = (state.w ^ (state.w >> 19)) ^ (t ^ (t >> 8));
    return state.w;
}

x_or_shift_128_state generate_random_seed(uint3 dispatch_thread_id)
{
    x_or_shift_128_state state;
    state.x = x_of_shift_seed.x + dispatch_thread_id.x * 123456789u;
    state.y = x_of_shift_seed.y + dispatch_thread_id.y * 362436069u + dispatch_thread_id.y * 2654435761u;
    state.z = x_of_shift_seed.z + dispatch_thread_id.z * 521288629u + dispatch_thread_id.z * 2246822519u;
    state.w = 88675123u ^ (dispatch_thread_id.x * 3266489917u);
    return state;
}

float x_or_shift_random(inout x_or_shift_128_state state)
{
    return (float) x_or_shift_128(state) / 4294967296.0f;
}

float2 x_or_shift_random2(inout x_or_shift_128_state state)
{
    return float2(x_or_shift_random(state), x_or_shift_random(state));
}

float3 x_or_shift_random3(inout x_or_shift_128_state state)
{
    return float3(x_or_shift_random(state), x_or_shift_random(state), x_or_shift_random(state));
}

float4 x_or_shift_random4(inout x_or_shift_128_state state)
{
    return float4(x_or_shift_random(state), x_or_shift_random(state), x_or_shift_random(state), x_or_shift_random(state));
}

//  切り上げ
uint uceil(uint numerator, uint denominator)
{
    return numerator + (numerator % denominator);
}

//  拡大行列生成
float4x4 matrix_scaling(float3 scale)
{
    float4x4 m = (float4x4)0;
    m._11 = scale.x;
    m._22 = scale.y;
    m._33 = scale.z;
    m._44 = 1.0f;
    return m;
}

//  回転行列生成
float4x4 matrix_rotation_roll_pitch_yaw(float3 rotation)
{
    float cp, sp;
    float cy, sy;
    float cr, sr;
    sincos(rotation.x, sp, cp);
    sincos(rotation.y, sy, cy);
    sincos(rotation.z, sr, cr);

    float4x4 m = (float4x4) 0;
    m._11 = cr * cy + sr * sp * sy;
    m._12 = sr * cp;
    m._13 = sr * sp * cy - cr * sy;
    m._21 = cr * sp * sy - sr * cy;
    m._22 = cr * cp;
    m._23 = sr * sy + cr * sp * cy;
    m._31 = cp * sy;
    m._32 = -sp;
    m._33 = cp * cy;
    m._44 = 1.0f;
    return m;
}

float4x4 matrix_rotation_quaternion(float4 rotation)
{
    float qx = rotation.x;
    float qxx = qx * qx;
    float qy = rotation.y;
    float qyy = qy * qy;
    float qz = rotation.z;
    float qzz = qz * qz;
    float qw = rotation.w;

    float4x4 m = (float4x4) 0;
    m._11 = 1.0f - 2.0f * qyy - 2.0f * qzz;
    m._12 = 2.0f * qx * qy + 2.0f * qz * qw;
    m._13 = 2.0f * qx * qz - 2.0f * qy * qw;
    m._21 = 2.0f * qx * qy - 2.0f * qz * qw;
    m._22 = 1.0f - 2.0f * qxx - 2.0f * qzz;
    m._23 = 2.0f * qy * qz + 2.0f * qx * qw;
    m._31 = 2.0f * qx * qz + 2.0f * qy * qw;
    m._32 = 2.0f * qy * qz - 2.0f * qx * qw;
    m._33 = 1.0f - 2.0f * qxx - 2.0f * qyy;
    m._44 = 1.0f;
    return m;
}

//  移動行列生成
float4x4 matrix_translation(float3 translation)
{
    float4x4 m = (float4x4) 0;
    m._11_22_33_44 = 1.0f;
    m._41 = translation.x;
    m._42 = translation.y;
    m._43 = translation.z;
    return m;
}

#endif  //  __COMPUND_COMPUTE_PARTICLE_HLSLI_
