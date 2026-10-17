#include "subpixel_morphological_anti_aliasing.hlsli"

//	@tool パス層（シェーダーリソースは t64-t95）
Texture2D color_map : register(t71);

float2 main(EDGE_DETECTION_OUT pin) : SV_TARGET0
{
    return SMAAColorEdgeDetectionPS(pin.uv.xy, pin.offset, color_map);
}
