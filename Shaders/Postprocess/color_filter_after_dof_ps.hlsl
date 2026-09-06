#include "fullscreen_quad.hlsli"
#include "shading_functions.hlsli"
#include "scene_constant_buffer.hlsli"
#include "filter_functions.hlsli"
#include "common_gbuffer_loader.hlsli"
#include "common.hlsli"
#include "common_post_process.hlsli"

cbuffer CINEMATIC_POST_PROCESS_CONSTANT_BUFFER
{
	//	色相補正
	float  cg_intensity;
	float3 cg_hsv_weight;
	float3 cg_color_multiplyer;
	float  cg_dummy0;

	//	ラジアルブラー
	float  rb_radius;
	int    rb_sampling_count;
	float2 rb_center;
	float  rb_mask_radius;
	float3 rb_dummy0;

	//	色収差
	float  sc_intensity;
	float3 sc_color_min;
	float  sc_threshold;
	float3 sc_color_max;

	//	テクスチャ上書き
	float  ow_intensity;
	float  ow_texcoord_scale;
	float2 ow_texcoord_offset;
	float4 ow_color;

	//	集中線
	float  cl_intensity;
	float  cl_use_animation;
	float  cl_animation_speed;
	float  cl_pattern_seed;
	float  cl_noise_scale;
	float3 cl_color;
	float2 cl_edge;
	float2 cl_dummy0;

	//	歪み
	float  ds_power;
	float  ds_speed;
	float  ds_amount;
	float  ds_dummy0;
	float4 sd0;	//	x:amplitude y:angular_frequency z:posterize_count w:amount
	float4 sd1;
};

Texture2D text_texture;

float2 cinematic_calc_uv_angle(float2 uv)
{
	float2 f = uv * 2.0f - 1.0f;
	return atan2(f.y, f.x) / (PI * 2.0f);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float4 main(VS_OUT pin) : SV_TARGET
{
	float2 map_size;
	scene_color_map.GetDimensions(map_size.x, map_size.y);

	float2 uv = pin.texcoord;

	//	歪み（UV ワープ）
	if (sd0.w > 0.0f)
		uv.x += sin(uv.y * sd0.y + time_data.x * 6.28318f) * sd0.x * sd0.w;
	if (sd1.w > 0.0f)
		uv.x += sin(uv.y * sd1.y + time_data.x * 6.28318f) * sd1.x * sd1.w;
	if (ds_amount > 0.0f)
	{
		float2 d = uv - 0.5f;
		float r = length(d);
		uv += d * sin(r * ds_power - time_data.x * ds_speed * 6.28318f) * 0.02f * ds_amount;
	}

	float4 color = scene_color_map.Sample(scene_map_sampler, uv);

	//	ラジアルブラー
	if (rb_radius > 0.0f && rb_sampling_count > 1)
	{
		float4 result_color = color;
		float2 blur_vector = (rb_center - uv);

		float pixel_angle = atan2(blur_vector.y, blur_vector.x);
		float wave_length = lerp(50.0f, 100.0f, random(float2(pixel_angle + time_data.y, pixel_angle)));
		float amplitude = lerp(20.0f, 50.0f, random(float2(pixel_angle + time_data.x, pixel_angle)));
		float value = sin(pixel_angle * wave_length + amplitude);
		blur_vector *= abs(value);

		blur_vector *= (rb_radius.xx / map_size) / (float)rb_sampling_count;
		float total_alpha = 1.0f;
		[loop]
		for (int index = 1; index < rb_sampling_count; ++index)
		{
			float alpha = (float)(rb_sampling_count - index) / (float)rb_sampling_count;
			result_color += scene_color_map.Sample(scene_map_sampler, uv + blur_vector * index) * alpha;
			total_alpha += alpha;
		}

		float mask_value = 1.0f;
		if (rb_mask_radius > 0.0f)
		{
			float mask_radius = rb_mask_radius / min(map_size.x, map_size.y);
			mask_value = saturate(length(pin.texcoord - rb_center) / mask_radius);
		}

		color = lerp(color, result_color / total_alpha, mask_value);
	}

	//	色相補正
	if (cg_intensity > 0.0f)
	{
		color.rgb = convert_rgb_to_hsv(color.rgb);
		color.rgb *= cg_intensity * cg_hsv_weight;
		color.rgb = convert_hsv_to_rgb(color.rgb);
		color.rgb *= cg_color_multiplyer;
	}

	//	輝度で色を分割
	if (sc_intensity > 0.0f)
	{
		float luma = convert_rgb_to_luminance(color.rgb);
		float3 mix_color = lerp(sc_color_min, sc_color_max, step(sc_threshold, luma));
		color.rgb = lerp(color.rgb, mix_color, sc_intensity);
	}

	//	テクスチャ上書き
	if (ow_intensity > 0.0f)
	{
		float2 texcoord = (pin.texcoord - 0.5f) * ow_texcoord_scale + 0.5f;
		texcoord += ow_texcoord_offset;
		texcoord = saturate(texcoord);

		float alpha = text_texture.Sample(scene_map_sampler, texcoord).r * ow_color.a * ow_intensity;
		color.rgb = lerp(color.rgb, ow_color.rgb, saturate(alpha));
	}

	//	集中線
	if (cl_intensity > 0.0f)
	{
		float angle = cinematic_calc_uv_angle(pin.texcoord);
		float2 line_seed = floor(float2(angle, angle) * cl_noise_scale);
		float line_random = random(line_seed);
		float line_value = lerp(sin(line_random * cl_pattern_seed),
								sin(line_random * time_data.x * cl_animation_speed),
								cl_use_animation);
		float2 fixed_uv = pin.texcoord * 2.0f - 1.0f;
		float circle = length(fixed_uv);
		float result_line = saturate(line_value * circle);
		float smooth_alpha = smoothstep(cl_edge.x, cl_edge.y, result_line) * cl_intensity;

		color.rgb = lerp(color.rgb, cl_color, smooth_alpha);
	}

	return color;
}
