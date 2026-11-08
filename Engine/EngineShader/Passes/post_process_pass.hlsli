//	post_process ドメインのパステンプレート(全ステージ共通の main)
//	ステージ : POST_PROCESS_STAGE_VERTEX / POST_PROCESS_STAGE_PIXEL
//	その他   : MATERIAL_POST_PROCESS(PS のみ。マテリアルは post_process() を定義する)
#ifndef __POST_PROCESS_PASS_HLSLI__
#define __POST_PROCESS_PASS_HLSLI__

#if defined(POST_PROCESS_STAGE_VERTEX)

#include "fullscreen_quad_vs.hlsl"

#elif defined(POST_PROCESS_STAGE_PIXEL)

float4 main(VS_OUT pin) : SV_TARGET
{
    return post_process(pin);
}

#else
#error "post_process_pass.hlsli : POST_PROCESS_STAGE_xxx is not defined"
#endif

#endif	//	__POST_PROCESS_PASS_HLSLI__
