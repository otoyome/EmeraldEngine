#ifndef __CUBIC_SDF_COLLISION_HLSLI__
#define __CUBIC_SDF_COLLISION_HLSLI__

//	@tool collision::cubic_sdf_gpu_resourceが構築する2枚のバッファをサンプリングするための共通処理
//	@tool 呼び出し側で以下の2つのバッファを束縛しておく必要がある
//	@tool   StructuredBuffer<sdf_header> ????_header_buffer
//	@tool   StructuredBuffer<float>      ????_voxel_buffer

//	collision::cubic_sdf_gpu_resource::sdf_headerとフィールド順を一致させること
struct sdf_header
{
    uint voxel_offset;
    uint resolution;
    float3 bounds_min;
    float3 bounds_max;
};

static const uint invalid_sdf_id = 0xFFFFFFFF;

//	@tool ボクセルインデックス(x,y,z)から線形インデックスを求める(collision::cubic_sdf::index_ofと同じ式)
uint cubic_sdf_index_of(sdf_header header, uint x, uint y, uint z)
{
    return header.voxel_offset + (z * header.resolution + y) * header.resolution + x;
}

//	@tool 指定したsdf_idのローカル座標での距離を、8点手動トライリニア補間で求める
//	@tool bounds範囲外、または未登録のsdf_idの場合はfalseを返す(呼び出し側は「衝突なし」として扱う)
bool sample_cubic_sdf(StructuredBuffer<sdf_header> header_buffer, StructuredBuffer<float> voxel_buffer,
    uint sdf_id, float3 local_position, out float out_distance)
{
    out_distance = 0.0f;
    if (sdf_id == invalid_sdf_id)
        return false;

    sdf_header header = header_buffer[sdf_id];
    if (header.resolution == 0)
        return false;

    //	@tool ブロードフェーズ: バウンズ範囲外なら8点フェッチをせずに打ち切る
    float3 extent = header.bounds_max - header.bounds_min;
    float3 normalized = (local_position - header.bounds_min) / extent;
    if (any(normalized < 0.0f) || any(normalized > 1.0f))
        return false;

    //	@tool ボクセル空間座標(0 ～ resolution-1)に変換し、基準ボクセルと補間係数を求める
    float3 voxel_position = normalized * (float) (header.resolution - 1);
    uint3 i0 = (uint3) floor(voxel_position);
    uint3 i1 = min(i0 + 1, header.resolution - 1);
    float3 frac_position = voxel_position - (float3) i0;

    //	@tool 8点フェッチ
    float c000 = voxel_buffer[cubic_sdf_index_of(header, i0.x, i0.y, i0.z)];
    float c100 = voxel_buffer[cubic_sdf_index_of(header, i1.x, i0.y, i0.z)];
    float c010 = voxel_buffer[cubic_sdf_index_of(header, i0.x, i1.y, i0.z)];
    float c110 = voxel_buffer[cubic_sdf_index_of(header, i1.x, i1.y, i0.z)];
    float c001 = voxel_buffer[cubic_sdf_index_of(header, i0.x, i0.y, i1.z)];
    float c101 = voxel_buffer[cubic_sdf_index_of(header, i1.x, i0.y, i1.z)];
    float c011 = voxel_buffer[cubic_sdf_index_of(header, i0.x, i1.y, i1.z)];
    float c111 = voxel_buffer[cubic_sdf_index_of(header, i1.x, i1.y, i1.z)];

    //	@tool トライリニア補間(x → y → zの順)
    float c00 = lerp(c000, c100, frac_position.x);
    float c10 = lerp(c010, c110, frac_position.x);
    float c01 = lerp(c001, c101, frac_position.x);
    float c11 = lerp(c011, c111, frac_position.x);

    float c0 = lerp(c00, c10, frac_position.y);
    float c1 = lerp(c01, c11, frac_position.y);

    out_distance = lerp(c0, c1, frac_position.z);
    return true;
}

//	@tool 中心差分でSDFの勾配(＝法線方向)を求める。事前にsample_cubic_sdfで範囲内と分かっている位置に対して使うこと
float3 sample_cubic_sdf_gradient(StructuredBuffer<sdf_header> header_buffer, StructuredBuffer<float> voxel_buffer,
    uint sdf_id, float3 local_position)
{
    sdf_header header = header_buffer[sdf_id];
    float voxel_size = header.resolution > 1
        ? (header.bounds_max.x - header.bounds_min.x) / (float) (header.resolution - 1)
        : 1.0f;
    float delta = voxel_size * 0.5f;

    float dx0 = 0.0f, dx1 = 0.0f, dy0 = 0.0f, dy1 = 0.0f, dz0 = 0.0f, dz1 = 0.0f;
    sample_cubic_sdf(header_buffer, voxel_buffer, sdf_id, local_position + float3(delta, 0, 0), dx1);
    sample_cubic_sdf(header_buffer, voxel_buffer, sdf_id, local_position - float3(delta, 0, 0), dx0);
    sample_cubic_sdf(header_buffer, voxel_buffer, sdf_id, local_position + float3(0, delta, 0), dy1);
    sample_cubic_sdf(header_buffer, voxel_buffer, sdf_id, local_position - float3(0, delta, 0), dy0);
    sample_cubic_sdf(header_buffer, voxel_buffer, sdf_id, local_position + float3(0, 0, delta), dz1);
    sample_cubic_sdf(header_buffer, voxel_buffer, sdf_id, local_position - float3(0, 0, delta), dz0);

    float3 gradient = float3(dx1 - dx0, dy1 - dy0, dz1 - dz0);
    float gradient_length = length(gradient);
    return gradient_length > 1e-6f ? gradient / gradient_length : float3(0, 1, 0);
}

#endif	//	__CUBIC_SDF_COLLISION_HLSLI__
