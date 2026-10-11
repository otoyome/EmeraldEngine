#ifndef __CLUSTERED_LIGHTING_HLSLI__
#define __CLUSTERED_LIGHTING_HLSLI__

#include "lights.hlsli"

//  @tool クラスタードライティング定数（clustered_lighting_system.h の clustered_constants と一致させる）
cbuffer CLUSTERED_LIGHTING_CONSTANT_BUFFER : register(b7)
{
    uint3 cluster_count;            //  @tool クラスター数（x : 横, y : 縦, z : 奥行き）
    uint cluster_light_count;       //  @tool ライト数
    float2 cluster_screen_size;     //  @tool 画面サイズ（ピクセル）
    float cluster_tile_size;        //  @tool タイルの大きさ（ピクセル）
    uint cluster_index_capacity;    //  @tool ライト番号リストの容量
    float cluster_near;             //  @tool 奥行き分割の開始距離
    float cluster_far;              //  @tool 奥行き分割の終了距離（以降は最後の分割にまとめる）
    float cluster_slice_scale;      //  @tool 分割番号 = log(奥行き) * scale + bias
    float cluster_slice_bias;
};

//  @tool クラスターの view 空間 AABB
struct cluster_aabb
{
    float3 bmin;
    float dummy0;
    float3 bmax;
    float dummy1;
};

//  @tool view 空間の奥行きから奥行き分割番号を求める
uint get_cluster_slice(float view_z)
{
    float slice = log(max(view_z, cluster_near)) * cluster_slice_scale + cluster_slice_bias;
    return (uint) clamp(slice, 0.0f, (float) (cluster_count.z - 1));
}

//  @tool 奥行き分割の境界の奥行き（slice = 0 から cluster_count.z まで）
float get_cluster_slice_depth(uint slice)
{
    return cluster_near * pow(cluster_far / cluster_near, (float) slice / (float) cluster_count.z);
}

//  @tool クラスター番号
uint get_cluster_index(uint3 cluster)
{
    return cluster.x + (cluster.y + cluster.z * cluster_count.y) * cluster_count.x;
}

//  @tool テクスチャ座標と view 空間の奥行きからクラスターを求める
uint3 get_cluster(float2 texcoord, float view_z)
{
    uint2 tile = (uint2) max(texcoord * cluster_screen_size / cluster_tile_size, 0.0f);
    tile = min(tile, cluster_count.xy - 1);
    return uint3(tile, get_cluster_slice(view_z));
}

#endif  //  __CLUSTERED_LIGHTING_HLSLI__
