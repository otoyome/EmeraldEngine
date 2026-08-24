#include "compute_particle_default.hlsli"
#include "gbuffer.hlsli"
#include "common_gbuffer_writer.hlsli"

PS_GBUFFER_OUT main(PS_IN pin)
{
    float4 color = pin.color;
    if (HAS_RESOURCE(BaseColorMap))
    {
        float4 sampled = TEXTURE2D_SAMPLE(BaseColorMap, linear_sampler_state, pin.texcoord);
        color *= sampled;
    }

    //	マスク処理
    if (material_blend_mode == material_blend_mode_mask)
    {
        float alpha_threshold = material_alpha_clip_threshold;
        alpha_threshold = lerp(alpha_threshold, 1.0f, calculate_alpha_dithering(pin.position.xy));
        clip(color.a - alpha_threshold);
    }

    //  GBufferDataに出力情報をまとめる
    GBufferData data = (GBufferData)0;
    //data.base_color.rgb = color.rgb;
    data.w_position = pin.w_position.xyz;
    data.w_normal = normalize(camera_position.xyz - pin.w_position.xyz);
    data.velocity = calculate_uvspace_velocity(pin.current_clip_position, pin.previous_clip_position);
    data.shading_model = material_shading_model;
    data.stencil = material_flags;
    data.unique_data = 0;
    data.unique_parameter = 0;

    encode_g_buffer_data encode_data = encode_g_buffer(data, view_projection_transform, z_buffer_parameteres);

    PS_GBUFFER_OUT pout;
    pout.base_color = encode_data.base_color;
    pout.normal_depth = encode_data.normal_depth;
    pout.parameter = encode_data.parameter;
    pout.velocity = encode_data.velocity;
    pout.unique_data = encode_data.unique_data;
    pout.scene_color = float4(color.rgb, 1);    //  ライティングしないのでそのまま

    pout.shade_color = encode_data.shade_color; //  [エンジン改造:G-Buffer]
    pout.ilm_color = encode_data.ilm_color; //  [エンジン改造:G-Buffer]
    return pout;
}
