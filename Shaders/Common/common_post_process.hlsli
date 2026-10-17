#ifndef __COMMON_POST_PROCESS_HLSLI__
#define __COMMON_POST_PROCESS_HLSLI__

#include "gbuffer.hlsli"

//	@tool パス層（シェーダーリソースは t64-t95）
Texture2D scene_color_map : register(t71);
Texture2D scene_depth_map : register(t72);
SamplerState scene_map_sampler;

#endif  //  __COMMON_POST_PROCESS_HLSLI__
