#ifndef __COMPUTE_PARTICLES_SYSTEM_ITEM_PARTICLE_HLSLI__
#define __COMPUTE_PARTICLES_SYSTEM_ITEM_PARTICLE_HLSLI__
//  インクルード前に下記の物を定義しておく必要がある
//  使わない場合はからの物を指定しておく
//  #define UAV RWByteAddressBuffer
//  #define SRV ByteAddressBuffer
#define PREFIX pt_
#include "compute_particle_base_stream_item.hlsli"

struct emitter_context
{
    uint active_items; // アクティブアイテムフラグ
    x_or_shift_128_state random_seed; // random関数の乱数種
    float elapsed_time; // 経過時間
    float3 scale; // 拡縮情報
    float4 rotation; // 回転情報
    float3 translation; // 移動情報
};

//////////////////////////////////////////////////////////////////////////
//  共通アイテム
struct pt_common_item
{
    uint state;
    float life_time;
    float life_time_max;
    float3 scale;
    float4 rotation;
    float3 translation;
    float4 color;
};

//  パーティクルステートビット
static const uint particle_binary_state_bit_spawn_frame = 1 << 0; //  生成フレームフラグ

void stream_common_item(uint rw, inout uint offset, inout pt_common_item common)
{
    pt_stream_ptr(rw, offset, common.state);
    pt_stream_ptr(rw, offset, common.life_time);
    pt_stream_ptr(rw, offset, common.life_time_max);
    pt_stream_ptr(rw, offset, common.scale);
    pt_stream_ptr(rw, offset, common.rotation);
    pt_stream_ptr(rw, offset, common.translation);
    pt_stream_ptr(rw, offset, common.color);
}

//////////////////////////////////////////////////////////////////////////
//  軌跡アイテム
struct pt_trail_header_item
{
    uint counter;
};
struct pt_trail_body_item
{
    float3 scale;
    float4 rotation;
    float3 translation;
};

static const uint pt_trail_body_size_of_bytes = (3 + 4 + 3) * 4;

void stream_trail_header(uint rw, inout uint offset, inout pt_trail_header_item trail_header)
{
    pt_stream_ptr(rw, offset, trail_header.counter);
}

void stream_trail_body(uint rw, inout uint offset, inout pt_trail_body_item trail_body)
{
    pt_stream_ptr(rw, offset, trail_body.scale);
    pt_stream_ptr(rw, offset, trail_body.rotation);
    pt_stream_ptr(rw, offset, trail_body.translation);
}
//////////////////////////////////////////////////////////////////////////
//  速度アイテム
struct pt_velocity_item
{
    float3 velocity;
};

void stream_velocity_item(uint rw, inout uint offset, inout pt_velocity_item velocity)
{
    pt_stream_ptr(rw, offset, velocity.velocity);
}

#endif  //  __COMPUTE_PARTICLES_SYSTEM_ITEM_PARTICLE_HLSLI__
