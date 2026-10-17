#include "sky_box.hlsli"

//	@tool パス層（シェーダーリソースは t64-t95）
TextureCube specular_pmrem : register(t82);
SamplerState samplerstate : register(s0);

float4 main(VS_OUT pin) : SV_TARGET
{
    //	視線ベクトル
    float3 E = normalize(pin.world_position.xyz - camera_position.xyz);

	//	mipmap使うと崩れるのでmipmapなしになるようにしておく
    return specular_pmrem.SampleLevel(samplerstate, E, 0);
}
