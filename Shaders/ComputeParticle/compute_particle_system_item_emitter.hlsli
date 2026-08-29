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
    uint active_items; // アイテムアクティブフラグ
    float2 spawn_life_time_range; // パーティクル生存時間範囲
    float2 spawn_scale_range; // パーティクル生成拡大率時間範囲
    float3 spawn_area_min; // パーティクル生成範囲最小
    float3 spawn_area_max; // パーティクル生成範囲最大
    uint spawn_count; // パーティクル生成数

    //  エミッターの姿勢情報
    float3 scale; // 拡縮情報
    float4 rotation; // 回転情報
    float3 translation; // 移動情報
    float elapsed_time; // シミュレーション経過時間
};

static const uint em_common_item_state_no_billboard = 1 << 1;   //	ビルボード無し

//  共通アイテム読み込み
void stream_common_item(uint rw, inout uint offset, inout em_common_item value)
{
    em_stream_ptr(rw, offset, value.emitter_state);
    em_stream_ptr(rw, offset, value.active_items);
    em_stream_ptr(rw, offset, value.spawn_life_time_range);
    em_stream_ptr(rw, offset, value.spawn_scale_range);
    em_stream_ptr(rw, offset, value.spawn_area_min);
    em_stream_ptr(rw, offset, value.spawn_area_max);
    em_stream_ptr(rw, offset, value.spawn_count);

    em_stream_ptr(rw, offset, value.scale);
    em_stream_ptr(rw, offset, value.rotation);
    em_stream_ptr(rw, offset, value.translation);
    em_stream_ptr(rw, offset, value.elapsed_time);
}

//////////////////////////////////////////////////////////////////////////
//  軌跡アイテム
struct em_trail_item
{
    uint state;
    uint max; // 軌跡総数
    float2 time_fade_rate; //	フェード
};

static const uint emitter_active_trail_item_bit_flag = 1 << 0;

static const uint em_trail_item_state_use_fade = 1 << 0; //	軌跡描画時にフェードさせる
static const uint em_trail_item_state_use_time_fade = 1 << 1; //	軌跡描画時にフェードさせる

//  軌跡アイテム読み込み
void stream_trail_item(uint rw, inout uint offset, inout em_trail_item value)
{
    em_stream_ptr(rw, offset, value.state);
    em_stream_ptr(rw, offset, value.max);
    em_stream_ptr(rw, offset, value.time_fade_rate);
}

//////////////////////////////////////////////////////////////////////////
//  UVアイテム
struct em_uv_item
{
    uint state;	// ステート
    uint index;	// スプリット数に応じた番号
    float time; // 次のスプライトに移動するタイミング
    uint2 split_count; // 分割数
    float4 rect; // 矩形
};

static const uint emitter_active_uv_item_bit_flag = 1 << 1;

//  ステートビットフラグ
static const uint emitter_uv_item_state_direct = 1 << 0;	//	UV座標を直接指定
static const uint emitter_uv_item_state_animation = 1 << 1;	//	アニメーション使用フラグ

//  UVスクロールの方向制御用
static const int2 emitter_uv_item_animation_direction_basis_value = uint2(5, 5);

//  UVアイテム読み込み
void stream_uv_item(uint rw, inout uint offset, inout em_uv_item value)
{
    em_stream_ptr(rw, offset, value.state);
    em_stream_ptr(rw, offset, value.index);
    em_stream_ptr(rw, offset, value.time);
    em_stream_ptr(rw, offset, value.split_count);
    em_stream_ptr(rw, offset, value.rect);
}

//////////////////////////////////////////////////////////////////////////
//  色アイテム
struct em_color_item
{
    uint state; //  ステート
    float4 color_min; // 色の範囲
    float4 color_max; // 色の範囲
};

static const uint emitter_active_color_item_bit_flag = 1 << 2;

static const uint emitter_color_item_state_random = 1 << 0; //	ランダム設定

//  色アイテム読み込み
void stream_color_item(uint rw, inout uint offset, inout em_color_item value)
{
    em_stream_ptr(rw, offset, value.state);
    em_stream_ptr(rw, offset, value.color_min);
    em_stream_ptr(rw, offset, value.color_max);
}

//////////////////////////////////////////////////////////////////////////
//  加速アイテム
struct em_veloctiy_item
{
    uint state; //  ステート
    float3 direction; // 初期方向
    float2 angle_range; // 初期角度範囲
    float2 power_range; // 初速威力範囲

    float3 acceleration_min; // 最低加速
    float3 acceleration_max; // 最大加速

    float3 deceleration_min; // 最低減速
    float3 deceleration_max; // 最大減速

    float3 gravity; // 重力
};

static const uint emitter_active_veloctiy_item_bit_flag = 1 << 3;

static const uint emitter_veloctiy_item_state_random_direction = 1 << 0;
static const uint emitter_veloctiy_item_state_random_power = 1 << 1;
static const uint emitter_veloctiy_item_state_random_acceleration = 1 << 2;
static const uint emitter_veloctiy_item_state_random_deceleration = 1 << 3;

//  加速アイテム読み込み
void stream_veloctiy_item(uint rw, inout uint offset, inout em_veloctiy_item value)
{
    em_stream_ptr(rw, offset, value.state);
    em_stream_ptr(rw, offset, value.direction);
    em_stream_ptr(rw, offset, value.angle_range);
    em_stream_ptr(rw, offset, value.power_range);

    em_stream_ptr(rw, offset, value.acceleration_min);
    em_stream_ptr(rw, offset, value.acceleration_max);

    em_stream_ptr(rw, offset, value.deceleration_min);
    em_stream_ptr(rw, offset, value.deceleration_max);

    em_stream_ptr(rw, offset, value.gravity);
}

#endif  //  __COMPUTE_PARTICLES_SYSTEM_ITEM_EITTER_HLSLI__
