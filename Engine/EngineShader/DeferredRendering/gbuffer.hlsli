#ifndef __GBUFFER_HLSLI__
#define __GBUFFER_HLSLI__

#include "common.hlsli"

//  シェーディングモデル定数
static const uint shading_model_unlit       = 0;    //  シェーディング計算：ライティング無し
static const uint shading_model_default_lit = 1;    //  シェーディング計算：基本計算
static const uint shading_model_pmx_lit     = 2;    //  シェーディング計算：PMX計算
static const uint shading_model_toon_lit    = 3;    //  シェーディング計算：トゥーン計算

//  マテリアルステンシル
static const uint material_attribute_indirect_lighting_off  = 1 <<  0;  //  間接光の処理を適応させない
static const uint material_attribute_shadow_receiver        = 1 <<  1;  //  影適応
static const uint material_attribute_decal_off              = 1 <<  2;  //  デカールを適応させない

//  空き

//static const uint material_attribute_ = 1 << 31;

//  G-Buffer ステンシルのビット数(RT1.a の下位 15bit)
static const uint gbuffer_stencil_bit_count = 15;
static const uint gbuffer_stencil_mask = (1u << gbuffer_stencil_bit_count) - 1u;
//  従法線の符号ビット(RT1.a のステンシルの次のビット)
static const uint gbuffer_bitangent_sign_bit = 1u << gbuffer_stencil_bit_count;

//  GBuffer情報構造体
struct GBufferData
{
    float3 base_color;          //  ベースカラー
    uint shading_model;         //  シェーディング方式
    float3 w_normal;            //  ワールド法線
    float3 w_tangent;           //  ワールド接線
    float bitangent_sign;       //  従法線の向き
    float3 w_position;          //  ワールド座標
    float2 velocity;            //  速度
    float linear_depth;         //  線形深度(Decode時のみ)
    float roughness;            //  粗さ
    float metalness;            //  金属質
    float occlusion;            //  遮蔽値
    float specular;             //  鏡面反射率の調整値
    uint stencil;               //  ビットフラグ(下位 15bit のみ有効)

    float4 custom_data0;
    float4 custom_data1;
    float4 custom_data2;
};

//  GBufferテクスチャ受け渡し用構造体
struct GBufferTextures
{
    Texture2D<float4> base_color;
    Texture2D<float4> normal_tangent;
    Texture2D<float4> parameter;
    Texture2D<float2> velocity;
    Texture2D<float4> custom_data0;
    Texture2D<float4> custom_data1;
    Texture2D<float4> custom_data2;
    Texture2D<float> depth;
};

//  ピクセルシェーダーへの出力用構造体
struct encode_g_buffer_data
{
    float4 base_color;
    float4 normal_tangent;
    float4 parameter;
    float2 velocity;
    float4 custom_data0;
    float4 custom_data1;
    float4 custom_data2;
};

//  float2 <> uint
uint PackFloat2ToUInt(float2 value)
{
    uint a16 = f32tof16(value.x);
    uint b16 = f32tof16(value.y);
    uint abPacked = (a16 << 16) | b16;
    return abPacked;
}
float2 UnpackUIntToFloat2(uint packedValue)
{
    float2 result;
    result.x = f16tof32(packedValue >> 16);
    result.y = f16tof32(packedValue);
    return result;
}

//	float2 <> float
float PackFloat2ToFloat(float2 value)
{
    return asfloat(PackFloat2ToUInt(value));
}

float2 UnpackFloatToFloat2(float packedValue)
{
    return UnpackUIntToFloat2(asuint(packedValue));
}

//  Octahedral 法線エンコード
//  https://liamtyler.github.io/posts/octahedral_analysis/
uint PackNormal(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    float2 p = (n.z >= 0.0) ? n.xy : (1.0 - abs(n.yx)) * sign(n.xy);
    uint x = (uint)((p.x * 0.5 + 0.5) * 65535.0);
    uint y = (uint)((p.y * 0.5 + 0.5) * 65535.0);
    return (x << 16) | y;
}

//  Octahedral 法線デコード
float3 UnpackNormal(uint p)
{
    float2 f;
    f.x = ((p >> 16) & 0xFFFF) / 65535.0 * 2.0 - 1.0;
    f.y = ( p        & 0xFFFF) / 65535.0 * 2.0 - 1.0;
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.xy += (n.xy >= 0.0 ? -t : t);
    return normalize(n);
}

//  Signed Octahedron 法線エンコード
float3 EncodeSignedOctahedronNormal(float3 n)
{
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    float3 encoded;
    encoded.x = ( n.x + n.y) * 0.5f + 0.5f;
    encoded.y = (-n.x + n.y) * 0.5f + 0.5f;
    encoded.z = (n.z >= 0.0f) ? 1.0f : 0.0f;
    return encoded;
}

//  Signed Octahedron 法線デコード
float3 DecodeSignedOctahedronNormal(float3 encoded)
{
    float3 n;
    n.x = encoded.x - encoded.y;
    n.y = encoded.x + encoded.y - 1.0f;

    //  量子化で符号が 0/1 からずれても判定できるよう 0.5 で切り分ける
    float z_sign = (encoded.z >= 0.5f) ? 1.0f : -1.0f;
    n.z = z_sign * (1.0f - abs(n.x) - abs(n.y));
    return normalize(n);
}

//  八面体法線エンコード
float2 EncodeOctahedronNormal(float3 n)
{
    n /= max(abs(n.x) + abs(n.y) + abs(n.z), 1.0e-20f);
    float2 p = n.xy;
    if (n.z < 0.0f)
    {
        p = (1.0f - abs(n.yx)) * float2(n.x >= 0.0f ? 1.0f : -1.0f, n.y >= 0.0f ? 1.0f : -1.0f);
    }
    return saturate(p * 0.5f + 0.5f);
}

//  八面体法線デコード
float3 DecodeOctahedronNormal(float2 encoded)
{
    float2 f = encoded * 2.0f - 1.0f;
    float3 n = float3(f.x, f.y, 1.0f - abs(f.x) - abs(f.y));
    float t = saturate(-n.z);
    n.x += (n.x >= 0.0f) ? -t : t;
    n.y += (n.y >= 0.0f) ? -t : t;
    return normalize(n);
}

//  法線から一意に決まる正規直交基底を作る
void BuildOrthonormalBasis(float3 n, out float3 b1, out float3 b2)
{
    float s = (n.z >= 0.0f) ? 1.0f : -1.0f;
    float a = -1.0f / (s + n.z);
    float b = n.x * n.y * a;
    b1 = float3(1.0f + s * n.x * n.x * a, s * b, -s * n.x);
    b2 = float3(b, s + n.y * n.y * a, -n.y);
}

//  ダイヤモンドエンコード
float EncodeDiamond(float2 p)
{
    float x = p.x / max(abs(p.x) + abs(p.y), 1.0e-20f);
    float y_sign = (p.y >= 0.0f) ? 1.0f : -1.0f;
    return -y_sign * 0.25f * x + 0.5f + y_sign * 0.25f;
}

//  ダイヤモンドデコード
float2 DecodeDiamond(float p)
{
    float p_sign = (p >= 0.5f) ? 1.0f : -1.0f;
    float2 v;
    v.x = -p_sign * 4.0f * p + 1.0f + p_sign * 2.0f;
    v.y = p_sign * (1.0f - abs(v.x));
    return normalize(v);
}

//  16bit UNORM <> float
float EncodeUIntToUNorm16(uint value)
{
    return (float) (value & 0xFFFFu) / 65535.0f;
}

uint DecodeUNorm16ToUInt(float value)
{
    return (uint) (saturate(value) * 65535.0f + 0.5f);
}

//  8bit UNORM <> float
float EncodeUIntToUNorm8(uint value)
{
    return (float) (value & 0xFFu) / 255.0f;
}

uint DecodeUNorm8ToUInt(float value)
{
    return (uint) (saturate(value) * 255.0f + 0.5f);
}

//  法線/接線/従法線の符号/ステンシルをエンコード
float4 EncodeNormalTangent(float3 w_normal, float3 w_tangent, float bitangent_sign, uint stencil)
{
    float4 encoded;

    //  法線
    encoded.xy = round(EncodeOctahedronNormal(w_normal) * 65535.0f) / 65535.0f;
    float3 n = DecodeOctahedronNormal(encoded.xy);

    //  接線
    float3 b1, b2;
    BuildOrthonormalBasis(n, b1, b2);
    float2 t = float2(dot(w_tangent, b1), dot(w_tangent, b2));
    if (dot(t, t) < 1.0e-12f)
    {
        t = float2(1.0f, 0.0f);
    }
    encoded.z = EncodeDiamond(t);

    //  G-Bufferステンシル(15bit) + 法線の符号(1bit)
    uint flags = (stencil & gbuffer_stencil_mask) | ((bitangent_sign < 0.0f) ? gbuffer_bitangent_sign_bit : 0u);
    encoded.w = EncodeUIntToUNorm16(flags);
    return encoded;
}

//  法線/接線/従法線の符号/ステンシルをデコード
void DecodeNormalTangent(float4 encoded, out float3 w_normal, out float3 w_tangent, out float bitangent_sign, out uint stencil)
{
    w_normal = DecodeOctahedronNormal(encoded.xy);

    float3 b1, b2;
    BuildOrthonormalBasis(w_normal, b1, b2);
    float2 t = DecodeDiamond(encoded.z);
    w_tangent = normalize(t.x * b1 + t.y * b2);

    uint flags = DecodeUNorm16ToUInt(encoded.w);
    stencil = flags & gbuffer_stencil_mask;
    bitangent_sign = (flags & gbuffer_bitangent_sign_bit) ? -1.0f : 1.0f;
}

//  ステンシルだけを取り出す
uint DecodeGBufferStencil(float4 encoded_normal_tangent)
{
    return DecodeUNorm16ToUInt(encoded_normal_tangent.w) & gbuffer_stencil_mask;
}

//  シェーディングモデルをエンコード/デコードする
float EncodeShadingModel(uint shading_model)
{
    return EncodeUIntToUNorm8(shading_model);
}

uint DecodeShadingModel(float encoded)
{
    return DecodeUNorm8ToUInt(encoded);
}

//  UV空間上の速度計算用関数
float2 calculate_uvspace_velocity(float4 current_clip_position, float4 previous_clip_position)
{
    current_clip_position /= current_clip_position.w;
    previous_clip_position /= previous_clip_position.w;
    return clamp((current_clip_position.xy - previous_clip_position.xy) * 0.5f, -1.0f, 1.0f);
}

//  非線形深度から線形深度へ変換
float convert_projection_depth_to_linear_depth(float depth, float4 z_buffer_parameteres)
{
    //	遠クリップ基準(0～1)ではなくビュー空間の深度を返す(遠クリップが無限遠でも成立する)
    return 1.0f / max(z_buffer_parameteres.z * depth + z_buffer_parameteres.w, 1.0f / gbuffer_max_linear_depth);
}

//  線形深度から非線形深度へ変換
float convert_linear_depth_to_projection_depth(float depth, float4 z_buffer_parameteres)
{
    //	遠クリップが有限の場合、空の値は遠クリップより奥になり逆射影が発散するので遠クリップ面(ほぼ0)で止める
    return max((1.0f / depth - z_buffer_parameteres.w) / z_buffer_parameteres.z, min_projection_depth);
}

//	2つの線形深度の相対差(0～1)。深度のスケールに依存しない比較に使う
float calculate_relative_linear_depth_difference(float depth0, float depth1)
{
    return abs(depth0 - depth1) / max(max(depth0, depth1), relative_linear_depth_epsilon);
}

//  GBufferDataに纏めた情報をピクセルシェーダーの出力用構造体に変換
encode_g_buffer_data encode_g_buffer(in GBufferData data, matrix view_projection_matrix, float4 z_buffer_parameteres)
{
    encode_g_buffer_data ret = (encode_g_buffer_data) 0;
    ret.base_color.rgb = data.base_color.rgb;
    ret.base_color.a = EncodeShadingModel(data.shading_model);

    ret.normal_tangent = EncodeNormalTangent(data.w_normal, data.w_tangent, data.bitangent_sign, data.stencil);

    ret.parameter.r = data.occlusion;
    ret.parameter.g = data.roughness;
    ret.parameter.b = data.metalness;
    ret.parameter.a = data.specular;

    ret.velocity = data.velocity;

    ret.custom_data0 = data.custom_data0;
    ret.custom_data1 = data.custom_data1;
    ret.custom_data2 = data.custom_data2;

    return ret;
};

//  ピクセルシェーダーの出力用構造体からGBufferData情報に変換
GBufferData DecodeGBuffer(GBufferTextures textures, float2 uv, matrix inverse_view_projection_transform, float4 z_buffer_parameteres)
{
    uint2 texture_size;
    textures.base_color.GetDimensions(texture_size.x, texture_size.y);

    //  エンコードした値を補間しない
    int3 texel = int3(min((uint2) (uv * texture_size), texture_size - 1), 0);

    //  各テクスチャから情報を取得
    float4 base_color = textures.base_color.Load(texel);
    float4 normal_tangent = textures.normal_tangent.Load(texel);
    float4 parameter = textures.parameter.Load(texel);
    float2 velocity = textures.velocity.Load(texel);
    float projection_depth = textures.depth.Load(texel);

    GBufferData data = (GBufferData) 0;
    data.base_color = base_color.rgb;
    data.shading_model = DecodeShadingModel(base_color.a);
    DecodeNormalTangent(normal_tangent, data.w_normal, data.w_tangent, data.bitangent_sign, data.stencil);
    data.linear_depth = convert_projection_depth_to_linear_depth(projection_depth, z_buffer_parameteres);
    data.occlusion = parameter.r;
    data.roughness = parameter.g;
    data.metalness = parameter.b;
    data.specular = parameter.a;
    data.velocity = velocity;

    data.custom_data0 = textures.custom_data0.Load(texel);
    data.custom_data1 = textures.custom_data1.Load(texel);
    data.custom_data2 = textures.custom_data2.Load(texel);

    float4 position = float4(uv.xy * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), convert_linear_depth_to_projection_depth(data.linear_depth, z_buffer_parameteres), 1);
    position = mul(position, inverse_view_projection_transform);
    data.w_position = position.xyz / position.w;

    return data;
}

//  GBufferステンシルのフラグが立っているか判定
bool IsGBufferFlags(uint data, uint bit_flag)
{
    return data & bit_flag;
}

bool IsGBufferFlags(GBufferData data, uint bit_flag)
{
    return data.stencil & bit_flag;
}

void SetGBufferFlags(inout uint data, uint bit_flag, bool flag)
{
    data = flag ? (data | bit_flag) : (data & ~bit_flag);
}

void SetGBufferFlags(inout GBufferData data, uint bit_flag, bool flag)
{
    SetGBufferFlags(data.stencil, bit_flag, flag);
}

//	デバッグ用：メッシュレット用にIDから疑似ランダムな色を生成する
float3 DebugMeshletIdToColor(uint id)
{
    uint h = id * 2654435761u;
    float r = ((h >>  0) & 0xFF) / 255.0f;
    float g = ((h >>  8) & 0xFF) / 255.0f;
    float b = ((h >> 16) & 0xFF) / 255.0f;
    return float3(r, g, b);
}

#endif  //  __GBUFFER_HLSLI__
