//	@tool パス層（シェーダーリソースは t64-t95）
Texture2D<float4> gbuffer_normal_depth : register(t65);
Texture2D<uint4> gbuffer_unique_data : register(t68);

PSDecalOut main(VS_OUT pin)
{
    //  スクリーン情報からGBuffer参照用UV座標を算出
    float2 screen_size = float2(viewport_rect.z - viewport_rect.x, viewport_rect.w - viewport_rect.y);
    float2 gbuffer_texcoord = pin.position.xy / screen_size;

    float4 normal_depth;
    uint stencil;
    {
        uint2 texture_size;
        gbuffer_normal_depth.GetDimensions(texture_size.x, texture_size.y);
        uint3 tex = uint3((uint2) (gbuffer_texcoord * texture_size), 0);
        normal_depth = gbuffer_normal_depth.Load(tex);
        stencil = gbuffer_unique_data.Load(tex).x;
    }

    //  デカール適応外はクリップ
    clip(IsGBufferFlags(stencil, material_attribute_decal_off) ? -1.0f : 1.0f);

    //  デカール適応結果を取得
    DecalIn din = convert_decal_in(normal_depth, gbuffer_texcoord.xy);
    DecalData data = decal_ps(din);

    //	マスク処理
    if (material_blend_mode == material_blend_mode_mask)
    {
        float alpha_threshold = material_alpha_clip_threshold;
        alpha_threshold = lerp(alpha_threshold, 1.0f, calculate_alpha_dithering(pin.position.xy));
        clip(data.base_color.a - alpha_threshold);
    }

    //  出力
    return convert_decal_out(data);
}
