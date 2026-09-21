#ifndef __CUBIC_SDF_COLLISION_HLSLI__
#define __CUBIC_SDF_COLLISION_HLSLI__

struct sdf_header
{
    uint voxel_offset;
    uint resolution;
    float3 bounds_min;
    float3 bounds_max;
};

static const uint invalid_sdf_id = 0xFFFFFFFF;

//	ボクセルインデックス(x,y,z)から線形インデックスを算出
uint cubic_sdf_index_of(sdf_header header, uint x, uint y, uint z)
{
    return header.voxel_offset + (z * header.resolution + y) * header.resolution + x;
}

//	指定したsdf_id 1件分の距離を算出
//	bounds範囲外、または未登録のsdf_idの場合はfalseを返す
bool sample_cubic_sdf_single(StructuredBuffer<sdf_header> header_buffer,
                             StructuredBuffer<float> voxel_buffer,
                             uint sdf_id, float3 position, out float out_distance)
{
    out_distance = 0.0f;
    if (sdf_id == invalid_sdf_id)
        return false;

    sdf_header header = header_buffer[sdf_id];
    if (header.resolution == 0)
        return false;

    //	ブロードフェーズ: バウンズ範囲外なら8点フェッチをせずに打ち切る
    float3 extent = header.bounds_max - header.bounds_min;
    float3 normalized = (position - header.bounds_min) / extent;
    if (any(normalized < 0.0f) || any(normalized > 1.0f))
        return false;

    //	ボクセル空間座標(0 ～ resolution-1)に変換し、基準ボクセルと補間係数を求める
    float3 voxel_position = normalized * (float) (header.resolution - 1);
    uint3 i0 = (uint3) floor(voxel_position);
    uint3 i1 = min(i0 + 1, header.resolution - 1);
    float3 frac_position = voxel_position - (float3) i0;

    //	8点フェッチ
    float c000 = voxel_buffer[cubic_sdf_index_of(header, i0.x, i0.y, i0.z)];
    float c100 = voxel_buffer[cubic_sdf_index_of(header, i1.x, i0.y, i0.z)];
    float c010 = voxel_buffer[cubic_sdf_index_of(header, i0.x, i1.y, i0.z)];
    float c110 = voxel_buffer[cubic_sdf_index_of(header, i1.x, i1.y, i0.z)];
    float c001 = voxel_buffer[cubic_sdf_index_of(header, i0.x, i0.y, i1.z)];
    float c101 = voxel_buffer[cubic_sdf_index_of(header, i1.x, i0.y, i1.z)];
    float c011 = voxel_buffer[cubic_sdf_index_of(header, i0.x, i1.y, i1.z)];
    float c111 = voxel_buffer[cubic_sdf_index_of(header, i1.x, i1.y, i1.z)];

    //	トライリニア補間(x → y → zの順)
    float c00 = lerp(c000, c100, frac_position.x);
    float c10 = lerp(c010, c110, frac_position.x);
    float c01 = lerp(c001, c101, frac_position.x);
    float c11 = lerp(c011, c111, frac_position.x);

    float c0 = lerp(c00, c10, frac_position.y);
    float c1 = lerp(c01, c11, frac_position.y);

    out_distance = lerp(c0, c1, frac_position.z);
    return true;
}

//	全SDFに対して判定を行い、最も近い距離とそのsdf_idを返す
bool sample_cubic_sdf_nearest(StructuredBuffer<sdf_header> header_buffer,
                              StructuredBuffer<float> voxel_buffer,
                              float3 position,
                              uint2 skip_mask,
                              out float out_distance,
                              out uint out_sdf_id)
{
    out_distance = 0.0f;
    out_sdf_id = invalid_sdf_id;

    uint header_count, header_stride;
    header_buffer.GetDimensions(header_count, header_stride);

    bool hit_any = false;
    float nearest_distance = 0.0f;

    [loop]
    for (uint sdf_id = 0; sdf_id < header_count; ++sdf_id)
    {
        //	マスクIdスキップ
        uint mask_word = (sdf_id < 32) ? skip_mask.x : skip_mask.y;
        uint mask_bit = 1u << (sdf_id % 32);
        if (sdf_id < 64 && (mask_word & mask_bit) != 0)
            continue;

        float distance;
        if (!sample_cubic_sdf_single(header_buffer, voxel_buffer, sdf_id, position, distance))
            continue;

        if (!hit_any || distance < nearest_distance)
        {
            hit_any = true;
            nearest_distance = distance;
            out_sdf_id = sdf_id;
        }
    }

    out_distance = nearest_distance;
    return hit_any;
}

//	中心差分でSDFの勾配算出
float3 sample_cubic_sdf_gradient(StructuredBuffer<sdf_header> header_buffer, StructuredBuffer<float> voxel_buffer,
    uint sdf_id, float3 position)
{
    sdf_header header = header_buffer[sdf_id];
    float voxel_size = header.resolution > 1
        ? (header.bounds_max.x - header.bounds_min.x) / (float) (header.resolution - 1)
        : 1.0f;
    float delta = voxel_size * 0.5f;

    float dx0 = 0.0f, dx1 = 0.0f, dy0 = 0.0f, dy1 = 0.0f, dz0 = 0.0f, dz1 = 0.0f;
    sample_cubic_sdf_single(header_buffer, voxel_buffer, sdf_id, position + float3(delta, 0, 0), dx1);
    sample_cubic_sdf_single(header_buffer, voxel_buffer, sdf_id, position - float3(delta, 0, 0), dx0);
    sample_cubic_sdf_single(header_buffer, voxel_buffer, sdf_id, position + float3(0, delta, 0), dy1);
    sample_cubic_sdf_single(header_buffer, voxel_buffer, sdf_id, position - float3(0, delta, 0), dy0);
    sample_cubic_sdf_single(header_buffer, voxel_buffer, sdf_id, position + float3(0, 0, delta), dz1);
    sample_cubic_sdf_single(header_buffer, voxel_buffer, sdf_id, position - float3(0, 0, delta), dz0);

    float3 gradient = float3(dx1 - dx0, dy1 - dy0, dz1 - dz0);
    float gradient_length = length(gradient);
    return gradient_length > 1e-6f ? gradient / gradient_length : float3(0, 1, 0);
}

#endif	//	__CUBIC_SDF_COLLISION_HLSLI__
