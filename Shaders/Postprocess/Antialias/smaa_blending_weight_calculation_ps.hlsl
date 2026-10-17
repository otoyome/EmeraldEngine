#include "subpixel_morphological_anti_aliasing.hlsli"

//	@tool パス層（シェーダーリソースは t64-t95）
Texture2D edge_map : register(t71);
Texture2D area_map : register(t1);
Texture2D search_map : register(t2);

float4 main(BLENDING_WEIGHT_OUT pin) : SV_TARGET0
{
    return SMAABlendingWeightCalculationPS(
        pin.uv,
        pin.texcoord,
        pin.offset,
        edge_map,
        area_map,
        search_map,
        0.0f    //  ジッタリング係数
    );
}