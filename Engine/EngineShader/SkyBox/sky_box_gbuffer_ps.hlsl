#include "sky_box.hlsli"

#include "deferred_light_probe_data.hlsli"
#include "common_gbuffer_writer.hlsli"

TextureCube specular_pmrem : register(t0);
SamplerState samplerstate : register(s0);

PS_GBUFFER_OUT main(VS_OUT pin)
{
    //  ライトプローブ
    light_probes light_probe = light_probe_data;

    //	視線ベクトル
    float3 E = normalize(pin.world_position.xyz - camera_position.xyz);

	//	mipmap使うと崩れるのでmipmapなしになるようにしておく
    float3 base_color = specular_pmrem.SampleLevel(samplerstate, E, 0);
    base_color *= light_probe.color.rgb * light_probe.color.a;

    //  GBufferDataに出力情報をまとめる
    GBufferData data = (GBufferData)0;
    data.shading_model = shading_model_unlit;
    data.w_position = pin.world_position.xyz;
    data.velocity = calculate_uvspace_velocity(pin.current_clip_position, pin.previous_clip_position);
    encode_g_buffer_data encode_data = encode_g_buffer(data, view_projection_transform, z_buffer_parameteres);

    PS_GBUFFER_OUT pout;
    pout.base_color = encode_data.base_color;
    pout.normal_tangent = encode_data.normal_tangent;
    pout.parameter = encode_data.parameter;
    pout.velocity = encode_data.velocity;
    pout.custom_data0 = encode_data.custom_data0;
    pout.custom_data1 = encode_data.custom_data1;
    pout.custom_data2 = encode_data.custom_data2;
    pout.scene_color = float4(base_color.rgb, 1); //  自己発光は直接シーンに書き込む
    return pout;
}
