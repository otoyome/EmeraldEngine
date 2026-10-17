#ifndef __CLUSTERED_LIGHTING_HLSLI__
#define __CLUSTERED_LIGHTING_HLSLI__

#include "lights.hlsli"

//  クラスタードライティング定数
//	@tool フレーム・視点層（定数バッファは b0, b12-b13）
cbuffer CLUSTERED_LIGHTING_CONSTANT_BUFFER : register(b12)
{
    uint3 cluster_count;            //  クラスター数（x : 横, y : 縦, z : 深度）
    uint cluster_light_count;       //  ライト数
    float2 cluster_screen_size;     //  画面サイズ
    float cluster_tile_size;        //  タイルの大きさ
    uint cluster_index_capacity;    //  ライト番号リストの容量
    float cluster_near;             //  深度分割の開始距離
    float cluster_far;              //  深度分割の終了距離
    float cluster_slice_scale;      //  分割番号 = log(深度) * scale + bias
    float cluster_slice_bias;
    uint cluster_flags;             //  clustered_flag_xxx
    float cluster_visualize_max;    //  ヒートマップで最大色になるライト数
    float2 cluster_dummy;
};

//  クラスターフラグ
static const uint clustered_flag_depth_culling = 1 << 0;   //  タイル内の最も奥の面より奥のクラスターを除く

//  クラスターの視線空間AABB
struct cluster_aabb
{
    float3 bmin;
    float dummy0;
    float3 bmax;
    float dummy1;
};

//  視線空間深度から深度分割番号を求める
uint get_cluster_slice(float view_z)
{
    float slice = log(max(view_z, cluster_near)) * cluster_slice_scale + cluster_slice_bias;
    return (uint) clamp(slice, 0.0f, (float) (cluster_count.z - 1));
}

//  深度分割の境界の深度
//  slice = 0 から cluster_count.z まで
float get_cluster_slice_depth(uint slice)
{
    return cluster_near * pow(cluster_far / cluster_near, (float) slice / (float) cluster_count.z);
}

//  クラスター番号取得
uint get_cluster_index(uint3 cluster)
{
    return cluster.x + (cluster.y + cluster.z * cluster_count.y) * cluster_count.x;
}

//  タイル番号取得
uint get_cluster_tile_index(uint2 tile)
{
    return tile.x + tile.y * cluster_count.x;
}

//  UV座標と視線空間深度からクラスターを求める
uint3 get_cluster(float2 texcoord, float view_z)
{
    uint2 tile = (uint2) max(texcoord * cluster_screen_size / cluster_tile_size, 0.0f);
    tile = min(tile, cluster_count.xy - 1);
    return uint3(tile, get_cluster_slice(view_z));
}

#endif  //  __CLUSTERED_LIGHTING_HLSLI__
