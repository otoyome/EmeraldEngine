#ifndef __COLOR_SPACE_HLSLI__
#define __COLOR_SPACE_HLSLI__

//  色域
static const uint gamut_rec709 = 0;
static const uint gamut_rec2020 = 1;

//  白色座標
static const float2 d60_white_point = float2(0.32168, 0.33767);
static const float2 d65_white_point = float2(0.3127, 0.3290);

//  出力の伝達関数
static const uint output_curve_srgb = 0;

//  作業色空間
static const uint rendering_gamut = gamut_rec2020;

//  メモ
//  〇XYZ (CIE1931)
//    目に見える色をすべて 0 以上で表せる基準の色空間。RGB 色空間どうしの変換はここを経由する
//  　Yが明るさになるように作られています。視感度と同じ形
//  〇xyY (CIE 1931)
//    XYZ を色味(x, y)と明るさ(Y)に分けて書き直したもの。
//    x = X / (X + Y + Z)
//    y = Y / (X + Y + Z)
//  〇D65 : 0.3127, 0.3290
//    昼光(約6504K)の標準の光。Rec.709・Rec.2020・sRGB の白
//  〇Rec.709 (ITU-R BT.709)
//    HDTV の規格。白(D65)、目に見える色の約35%を覆う
//  〇Rec.2020 (ITU-R BT.2020)
//     UHDTV(4K/8K)・HDR の規格。白(D65)、目に見える色の約75%を覆う
//  〇sRGB(IEC 61966-2-1:1999)
//    PC・Web の標準。原色と白は Rec.709 と同じで、違うのは伝達関数(ガンマ約2.2の区分関数)だけ

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	sRGB 曲線(IEC 61966-2-1)
float3 convert_srgb_to_linear(float3 x)
{
    x = max(x, 0.0);
    return (x < 0.04045) ? (x / 12.92) : pow((x + 0.055) / 1.055, 2.4);
}

float3 convert_linear_to_srgb(float3 x)
{
    x = max(x, 0.0);
    return (x < 0.0031308) ? (12.92 * x) : (1.055 * pow(x, 1.0 / 2.4) - 0.055);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Rec.709 リニア <> Rec.2020 リニア
float3 convert_linear_rec709_to_linear_rec2020(float3 rgb709)
{
    static const float3x3 rec709_to_rec2020_matrix =
    {
        0.627402, 0.329292, 0.043306,
        0.069095, 0.919544, 0.011360,
        0.016394, 0.088028, 0.895578
    };

    return mul(rec709_to_rec2020_matrix, rgb709);
}

float3 convert_linear_rec2020_to_linear_rec709(float3 rgb2020)
{
    static const float3x3 rec2020_to_rec709_matrix =
    {
        +1.660496, -0.587656, -0.072840,
        -0.124547, +1.132895, -0.008348,
        -0.018154, -0.100597, +1.118751
    };

    return mul(rec2020_to_rec709_matrix, rgb2020);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Rec.709 リニア <> XYZ
float3 convert_linear_rec709_to_xyz(float3 rgb709)
{
    static const float3x3 m =
    {
        0.41239080, 0.35758434, 0.18048079,
        0.21263901, 0.71516868, 0.07219232,
        0.01933082, 0.11919478, 0.95053215
    };
    return mul(m, rgb709);
}

float3 convert_xyz_to_linear_rec709(float3 xyz)
{
    static const float3x3 m =
    {
        +3.24096994, -1.53738318, -0.49861076,
        -0.96924364, +1.87596750, +0.04155506,
        +0.05563008, -0.20397696, +1.05697151
    };
    return mul(m, xyz);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Rec.2020 リニア <> XYZ
float3 convert_linear_rec2020_to_xyz(float3 rgb2020)
{
    static const float3x3 m =
    {
        +0.63695806, +0.14461690, +0.16888096,
        +0.26270020, +0.67799806, +0.05930171,
        +0.00000000, +0.02807269, +1.06098508
    };
    return mul(m, rgb2020);
}

float3 convert_xyz_to_linear_rec2020(float3 xyz)
{
    static const float3x3 m =
    {
        +1.71665119, -0.35567078, -0.25336628,
        -0.66668435, +1.61648124, +0.01576855,
        +0.01763986, -0.04277061, +0.94210312
    };
    return mul(m, xyz);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	xyY <> XYZ 変換
float3 convert_xyY_to_xyz(float3 xyY)
{
    if (xyY.y <= 0.0f)
        return float3(0, 0, 0);

    float scale = xyY.z / xyY.y;
    return float3(scale * xyY.x, xyY.z, scale * (1.0f - xyY.x - xyY.y));
}

float3 convert_xyz_to_xyY(float3 xyz, float2 white_point)
{
    float sum = xyz.x + xyz.y + xyz.z;
    if (sum <= 0.0f)
        return float3(white_point.xy, 0.0f);

    return float3(xyz.x / sum, xyz.y / sum, xyz.y);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	輝度(Y)
float convert_linear_rec709_to_luminance(float3 rgb709)
{
    return dot(float3(0.2126, 0.7152, 0.0722), rgb709);
}

float convert_linear_rec2020_to_luminance(float3 rgb2020)
{
    return dot(float3(0.2627, 0.6780, 0.0593), rgb2020);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Rec709色空間情報<>線形色空間情報への変換関数
//  ITU-R BT.709勧告 規格準拠
float3 convert_linear_to_rec709(float3 x)
{
    return (x < 0.0181) ? (4.5 * x) : (1.0993 * pow(x, 0.45) - 0.0993);
}

float3 convert_rec709_to_linear(float3 x)
{
    return (x < 0.08145) ? (x / 4.5) : (pow((x + 0.0993) / 1.0993, 1.0 / 0.45));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	ST.2084色空間情報への変換関数
//  入力・出力は0から1に正規化された絶対輝度(1.0 = 10000 nits)
static const float st2084_max_luminance = 10000.0f;

float3 apply_st2084_curve(float3 L)
{
    float m1 = 2610.0 / 4096.0 / 4;
    float m2 = 2523.0 / 4096.0 * 128;
    float c1 = 3424.0 / 4096.0;
    float c2 = 2413.0 / 4096.0 * 32;
    float c3 = 2392.0 / 4096.0 * 32;
    float3 Lp = pow(L, m1);
    return pow((c1 + c2 * Lp) / (1.0f + c3 * Lp), m2);
}

float3 remove_st2084_curve(float3 N)
{
    float m1 = 2610.0 / 4096.0 / 4;
    float m2 = 2523.0 / 4096.0 * 128;
    float c1 = 3424.0 / 4096.0;
    float c2 = 2413.0 / 4096.0 * 32;
    float c3 = 2392.0 / 4096.0 * 32;
    float3 Np = pow(N, 1 / m2);
    return pow(max(Np - c1, 0) / (c2 - c3 * Np), 1.0f / m1);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	RGB色空間の数値からHSV色空間の数値への変換関数
float3 convert_rgb_to_hsv( float3 rgb )
{
    float4 K = float4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    float4 p = (rgb.g < rgb.b) ? float4(rgb.bg, K.wz) : float4(rgb.gb, K.xy);
    float4 q = (rgb.r < p.x) ? float4(p.xyw, rgb.r) : float4(rgb.r, p.yzx);
    float d = q.x - min(q.w, q.y);
    float eps = 1.0e-10;
    return float3(abs(q.z + (q.w - q.y) / (6.0 * d + eps)), d / (q.x + eps), q.x);
}

float3	convert_hsv_to_rgb( float3 hsv )
{
    float4 K = float4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    float3 p = abs(frac(hsv.xxx + K.xyz) * 6.0 - K.www);
    return hsv.z * lerp(K.xxx, clamp(p - K.xxx, 0.0, 1.0), clamp(hsv.y, 0.0, 1.0));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	RGB色を対数圧縮関数
float3 compress_rgb_to_log(float3 color)
{
	return log2(color.rgb + 1.0f);
}
float4 compress_rgba_to_log(float4 color)
{
	return log2(color + 1.0f);
}

float3 decompress_rgb_to_log(float3 color)
{
	return exp2(color.rgb) - 1.0f;
}

float4 decompress_rgba_to_log(float4 color)
{
	return exp2(color) - 1.0f;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  レンダリング色空間に合わせた対応
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  レンダリング色空間の白色点を取得
float2 get_rcs_white_point()
{
    if (rendering_gamut == gamut_rec709 || rendering_gamut == gamut_rec2020)
        return d65_white_point;

    //  @remainder  将来ACESを対応するときにでも・・・
    return d60_white_point;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  レンダリング色空間<>sRGB色空間変換
float3 convert_srgb_to_rcs(float3 x)
{
    x = convert_srgb_to_linear(x);
    if (rendering_gamut == gamut_rec2020)
        x = convert_linear_rec709_to_linear_rec2020(x);
    return x;
}

float3 convert_rcs_to_srgb(float3 x)
{
    if (rendering_gamut == gamut_rec2020)
        x = convert_linear_rec2020_to_linear_rec709(x);
    return convert_linear_to_srgb(x);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  レンダリング色空間<>Rec709 Linear色空間変換
float3 convert_linear_rec709_to_rcs(float3 x)
{
    if (rendering_gamut == gamut_rec2020)
        return convert_linear_rec709_to_linear_rec2020(x);

    return x;
}

float3 convert_rcs_to_linear_rec709(float3 x)
{
    if (rendering_gamut == gamut_rec2020)
        return convert_linear_rec2020_to_linear_rec709(x);

    return x;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  レンダリング色空間<>Rec2020 Linear色空間変換
float3 convert_linear_rec2020_to_rcs(float3 x)
{
    if (rendering_gamut == gamut_rec709)
        return convert_linear_rec2020_to_linear_rec709(x);

    return x;
}

float3 convert_rcs_to_linear_rec2020(float3 x)
{
    if (rendering_gamut == gamut_rec709)
        return convert_linear_rec709_to_linear_rec2020(x);

    return x;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  レンダリング色空間 > 輝度変換
float convert_rcs_to_luminance(float3 x)
{
    if (rendering_gamut == gamut_rec2020)
        return convert_linear_rec2020_to_luminance(x);

    return convert_linear_rec709_to_luminance(x);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  レンダリング色空間<>XYZ色空間変換
float3 convert_rcs_to_xyz(float3 x)
{
    if (rendering_gamut == gamut_rec2020)
        return convert_linear_rec2020_to_xyz(x);

    return convert_linear_rec709_to_xyz(x);
}

float3 convert_xyz_to_rcs(float3 x)
{
    if (rendering_gamut == gamut_rec2020)
        return convert_xyz_to_linear_rec2020(x);

    return convert_xyz_to_linear_rec709(x);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  レンダリング色空間<>xyY色空間変換
float3 convert_rcs_to_xyY(float3 x)
{
    float3 xyz = convert_rcs_to_xyz(x);
    return convert_xyz_to_xyY(xyz, get_rcs_white_point());
}

float3 convert_xyY_to_rcs(float3 x)
{
    float3 xyz = convert_xyY_to_xyz(x);
    return convert_xyz_to_rcs(xyz);
}

#endif  //  __COLOR_SPACE_HLSLI__
