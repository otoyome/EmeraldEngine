#ifndef __COMPUTE_PARTICLES_SYSTEM_ITEM_EITTER_HLSLI__
#define __COMPUTE_PARTICLES_SYSTEM_ITEM_EITTER_HLSLI__
//  インクルード前に下記の物を定義しておく必要がある
//  使わない場合はからの物を指定しておく
//  #define UAV RWByteAddressBuffer
//  #define SRV ByteAddressBuffer
#define PREFIX em_
#include "compute_particle_base_stream_item.hlsli"

///////////////////////////////////////////////////////////////////////////////////////////////////////
//  エミッター側

//////////////////////////////////////////////////////////////////////////
//  共通アイテム
struct em_common_item
{
    uint emitter_state; // エミッターステート
    uint active_items; // アクティブアイテムフラグ

    //  エミッターの姿勢情報
    float3 scale; // 拡縮情報
    float4 rotation; // 回転情報
    float3 translation; // 移動情報

    //  パーティクル発生情報
    float2 spawn_life_time_range; // 生存時間範囲
    float2 spawn_scale_range; // 生成拡大率時間範囲
    uint spawn_count; // 生成数
    float3 spawn_area_min; // 生成範囲最小
    float3 spawn_area_max; // 生成範囲最大

	//	更新時に使用する汎用値
    float3 gravity; // 重力
};

//  共通アイテム読み込み
void stream_common_item(uint rw, inout uint offset, inout em_common_item value)
{
    em_stream_ptr(rw, offset, value.emitter_state);
    em_stream_ptr(rw, offset, value.active_items);

    em_stream_ptr(rw, offset, value.scale);
    em_stream_ptr(rw, offset, value.rotation);
    em_stream_ptr(rw, offset, value.translation);

    em_stream_ptr(rw, offset, value.spawn_life_time_range);
    em_stream_ptr(rw, offset, value.spawn_scale_range);
    em_stream_ptr(rw, offset, value.spawn_count);
    em_stream_ptr(rw, offset, value.spawn_area_min);
    em_stream_ptr(rw, offset, value.spawn_area_max);

    em_stream_ptr(rw, offset, value.gravity);
}

//////////////////////////////////////////////////////////////////////////
//  軌跡アイテム
struct em_trail_item
{
    uint max; // 軌跡総数
};

static const uint emitter_active_trail_item_bit_flag = 1 << 0;

//  軌跡アイテム読み込み
void stream_trail_item(uint rw, inout uint offset, inout em_trail_item value)
{
    em_stream_ptr(rw, offset, value.max);
}

//////////////////////////////////////////////////////////////////////////
//  スプライトアニメーションアイテム
struct em_sprite_animation_item
{
	uint2 split_count;	//	分割数
	float increment_time;	//	次のスプライトに移動するタイミング
};

static const uint emitter_active_sprite_animation_item_bit_flag = 1 << 1;

//  スプライトアニメーションアイテム読み込み
void stream_sprite_animation_item(uint rw, inout uint offset, inout em_sprite_animation_item value)
{
    em_stream_ptr(rw, offset, value.split_count);
    em_stream_ptr(rw, offset, value.increment_time);
}

//////////////////////////////////////////////////////////////////////////
//  加速アイテム
struct em_veloctiy_item
{
    float3 direction; // 初期方向
    float2 angle_range; // 初期角度範囲
    float2 power_range; // 初速威力範囲
    
    float3 acceleration; // 加速度
};

static const uint emitter_active_veloctiy_item_bit_flag = 1 << 5;

//  加速アイテム読み込み
void stream_veloctiy_item(uint rw, inout uint offset, inout em_veloctiy_item value)
{
    em_stream_ptr(rw, offset, value.direction);
    em_stream_ptr(rw, offset, value.angle_range);
    em_stream_ptr(rw, offset, value.power_range);

    em_stream_ptr(rw, offset, value.acceleration);
}

#endif  //  __COMPUTE_PARTICLES_SYSTEM_ITEM_EITTER_HLSLI__
