//	user_interface ドメインのパステンプレート(全ステージ共通の main)
//	ステージ : USER_INTERFACE_STAGE_VERTEX / USER_INTERFACE_STAGE_PIXEL
//	その他   : MATERIAL_USER_INTERFACE(PS のみ。マテリアルは user_interface() を定義する)
#ifndef __USER_INTERFACE_PASS_HLSLI__
#define __USER_INTERFACE_PASS_HLSLI__

#if defined(USER_INTERFACE_STAGE_VERTEX)

#include "sprite_vs.hlsl"

#elif defined(USER_INTERFACE_STAGE_PIXEL)

float4 main(VS_OUT pin) : SV_TARGET
{
    return user_interface(pin);
}

#else
#error "user_interface_pass.hlsli : USER_INTERFACE_STAGE_xxx is not defined"
#endif

#endif	//	__USER_INTERFACE_PASS_HLSLI__
