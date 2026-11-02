//	スプライト(UI)の既定のマテリアルシェーダー
//	MATERIAL_USER_INTERFACE	: user_interface()
#ifndef __SPRITE_DEFAULT_HLSL__
#define __SPRITE_DEFAULT_HLSL__

Texture2D color_map : register(t0);
SamplerState color_mapState : register(s0);

#if defined(MATERIAL_USER_INTERFACE)
float4 user_interface(VS_OUT pin)
{
	return color_map.Sample(color_mapState, pin.texcoord) * pin.color;
}
#endif	//	defined(MATERIAL_USER_INTERFACE)

#endif	//	__SPRITE_DEFAULT_HLSL__
