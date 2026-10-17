#include "subpixel_morphological_anti_aliasing.hlsli"

//	@tool パス層（シェーダーリソースは t64-t95）
Texture2D color_map : register(t71);
Texture2D blend_map : register(t73);

Texture2D edge_map : register(t74);

float4 main(NEIGHBORHOOD_BLENDING_OUT pin) : SV_TARGET0
{
    float4 color = SMAANeighborhoodBlendingPS(
        pin.uv,
        pin.offset,
        color_map,
        blend_map
    );
    
    //  検出したエッジ表示
    //if (smaa_draw_edge)
    //    color += edge_map.Load(int3(pin.position.xy, 0));

    return color;
}