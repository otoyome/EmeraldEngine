#include "fullscreen_quad.hlsli"
#include "shading_functions.hlsli"
#include "scene_constant_buffer.hlsli"
#include "filter_functions.hlsli"
#include "common_gbuffer_loader.hlsli"
#include "common.hlsli"
#include "common_post_process.hlsli"

cbuffer CINEMATIC_POST_PROCESS_CONSTANT_BUFFER
{
	//	F‘Š•â³
	float  cg_intensity;
	float3 cg_hsv_weight;
	float3 cg_color_multiplyer;
	float  cg_dummy0;

	//	ƒ‰ƒWƒAƒ‹ƒuƒ‰[
	float  rb_radius;
	int    rb_sampling_count;
	float2 rb_center;
	float  rb_mask_radius;
	float3 rb_dummy0;

	//	Fû·
	float  sc_intensity;
	float3 sc_color_min;
	float  sc_threshold;
	float3 sc_color_max;

	//	ƒeƒNƒXƒ`ƒƒã‘‚«
	float  ow_intensity;
	float  ow_texcoord_scale;
	float2 ow_texcoord_offset;
	float4 ow_color;

	//	W’†ü
	float  cl_intensity;
	float  cl_use_animation;
	float  cl_animation_speed;
	float  cl_pattern_seed;
	float  cl_noise_scale;
	float3 cl_color;
	float2 cl_edge;
	float2 cl_dummy0;

	//	˜c‚İ
	float  ds_power;
	float  ds_speed;
	float  ds_amount;
	float  ds_dummy0;
	float4 sd0;	//	x:amplitude y:angular_frequency z:posterize_count w:amount
	float4 sd1;

	//	ƒ”ƒBƒlƒbƒg
	float  vg_intensity;
	float  vg_radius;
	float  vg_softness;
	float  vg_dummy0;
	float3 vg_color;
	float  vg_dummy1;

	//	ƒJƒQƒƒE
	float  hz_intensity;
	float  hz_scale;
	float  hz_speed;
	float  hz_dummy0;

	//	…“H
	float  wd_intensity;
	float  wd_scale;
	float2 wd_dummy0;
};

Texture2D text_texture;

float2 cinematic_calc_uv_angle(float2 uv)
{
	float2 f = uv * 2.0f - 1.0f;
	return atan2(f.y, f.x) / (PI * 2.0f);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	‰J—±
//	https://www.shadertoy.com/view/ltffzl
float3 rain_hash13(float p)
{
	float3 p3 = frac(float3(p, p, p) * float3(0.1031f, 0.11369f, 0.13787f));
	p3 += dot(p3, p3.yzx + 19.19f);
	return frac(float3((p3.x + p3.y) * p3.z, (p3.x + p3.z) * p3.y, (p3.y + p3.z) * p3.x));
}

float rain_hash1(float t)
{
	return frac(sin(t * 12345.564f) * 7658.76f);
}

float rain_saw(float b, float t)
{
	return smoothstep(0.0f, b, t) * smoothstep(1.0f, b, t);
}

//	–{—¬ + ”ö‚ğˆø‚­‹OÕ
//	x:‰J—±‚Ì”Z“x y:‹OÕ‚Ì”Z“x
float2 rain_drop_layer(float2 uv, float t)
{
	float2 base_uv = uv;

	uv.y += t * 0.75f;
	float2 a = float2(6.0f, 1.0f);
	float2 grid = a * 2.0f;
	float2 id = floor(uv * grid);

	float col_shift = rain_hash1(id.x);
	uv.y += col_shift;

	id = floor(uv * grid);
	float3 n = rain_hash13(id.x * 35.2f + id.y * 2376.1f);
	float2 st = frac(uv * grid) - float2(0.5f, 0.0f);

	float x = n.x - 0.5f;

	float y = base_uv.y * 20.0f;
	float wiggle = sin(y + sin(y));
	x += wiggle * (0.5f - abs(x)) * (n.z - 0.5f);
	x *= 0.7f;
	float ti = frac(t + n.z);
	y = (rain_saw(0.85f, ti) - 0.5f) * 0.9f + 0.5f;
	float2 p = float2(x, y);

	float d = length((st - p) * a.yx);

	float main_drop = smoothstep(0.4f, 0.0f, d);

	float r = sqrt(saturate(smoothstep(1.0f, y, st.y)));
	float cd = abs(st.x - x);
	float trail = smoothstep(0.23f * r, 0.15f * r * r, cd);
	float trail_front = smoothstep(-0.02f, 0.02f, st.y - y);
	trail *= trail_front * r * r;

	y = base_uv.y;
	float trail2 = smoothstep(0.2f * r, 0.0f, cd);
	float droplets = max(0.0f, (sin(y * (1.0f - y) * 120.0f) - st.y)) * trail2 * trail_front * n.z;
	y = frac(y * 10.0f) + (st.y - 0.5f);
	float dd = length(st - float2(x, y));
	droplets = smoothstep(0.3f, 0.0f, dd);
	float m = main_drop + droplets * r * trail_front;

	return float2(m, trail);
}

//	ƒKƒ‰ƒX‚É’£‚è•t‚¢‚½¬‚³‚ÈÃ~…“H
float rain_static_drops(float2 uv, float t)
{
	uv *= 40.0f;

	float2 id = floor(uv);
	uv = frac(uv) - 0.5f;
	float3 n = rain_hash13(id.x * 107.45f + id.y * 3543.654f);
	float2 p = (n.xy - 0.5f) * 0.7f;
	float d = length(uv - p);

	float fade = rain_saw(0.025f, frac(t + n.z));
	float c = smoothstep(0.3f, 0.0f, d) * frac(n.z * 10.0f) * fade;
	return c;
}

//	Ã~…“H + 2‘w‚Ì—¬‚ê‚é‰J—±‚ğ‡¬
//	x:…“H‚Ì”Z“x y:‹OÕ‚Ì”Z“x
float2 rain_drops(float2 uv, float t, float l0, float l1, float l2)
{
	float s = rain_static_drops(uv, t) * l0;
	float2 m1 = rain_drop_layer(uv, t) * l1;
	float2 m2 = rain_drop_layer(uv * 1.85f, t) * l2;

	float c = s + m1.x + m2.x;
	c = smoothstep(0.3f, 1.0f, c);

	return float2(c, max(m1.y * l0, m2.y * l1));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float4 main(VS_OUT pin) : SV_TARGET
{
	float2 map_size;
	scene_color_map.GetDimensions(map_size.x, map_size.y);

	float2 uv = pin.texcoord;

	//	˜c‚İiUV ƒ[ƒvj
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

	//	—z‰Š
	if (hz_intensity > 0.0f)
	{
		float2 haze_seed = uv * hz_scale + time_data.x * hz_speed;
		float2 haze_offset = float2(sin(haze_seed.y * 6.28318f), cos(haze_seed.x * 6.28318f));
		uv += haze_offset * 0.01f * hz_intensity;
	}

	float4 color = scene_color_map.Sample(scene_map_sampler, uv);

	//	ƒ‰ƒWƒAƒ‹ƒuƒ‰[
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

	//	F‘Š•â³
	if (cg_intensity > 0.0f)
	{
		color.rgb = convert_rgb_to_hsv(color.rgb);
		color.rgb *= cg_intensity * cg_hsv_weight;
		color.rgb = convert_hsv_to_rgb(color.rgb);
		color.rgb *= cg_color_multiplyer;
	}

	//	‹P“x‚ÅF‚ğ•ªŠ„
	if (sc_intensity > 0.0f)
	{
		float luma = convert_rgb_to_luminance(color.rgb);
		float3 mix_color = lerp(sc_color_min, sc_color_max, step(sc_threshold, luma));
		color.rgb = lerp(color.rgb, mix_color, sc_intensity);
	}

	//	ƒeƒNƒXƒ`ƒƒã‘‚«
	if (ow_intensity > 0.0f)
	{
		float2 texcoord = (pin.texcoord - 0.5f) * ow_texcoord_scale + 0.5f;
		texcoord += ow_texcoord_offset;
		texcoord = saturate(texcoord);

		float alpha = text_texture.Sample(scene_map_sampler, texcoord).r * ow_color.a * ow_intensity;
		color.rgb = lerp(color.rgb, ow_color.rgb, saturate(alpha));
	}

	//	W’†ü
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

	//	ü•ÓŒ¸Œõ
	if (vg_intensity > 0.0f)
	{
		float2 centered_uv = pin.texcoord - 0.5f;
		float vignette_distance = length(centered_uv) * 2.0f;
		float vignette_factor = smoothstep(vg_radius - vg_softness, vg_radius, vignette_distance);
		color.rgb = lerp(color.rgb, vg_color, saturate(vignette_factor * vg_intensity));
	}
	//	…“H
	if (wd_intensity > 0.0f)
	{
		float aspect = map_size.x / map_size.y;
		float2 rain_uv = (pin.texcoord - 0.5f) * float2(aspect, 1.0f);
		float rain_zoom = -cos(time_data.x * 0.2f);
		rain_uv *= (0.7f + rain_zoom * 0.3f) * wd_scale;

		float rain_time = time_data.x * 0.2f;
		float rain_amount = saturate(wd_intensity);
		float rain_static = smoothstep(-0.5f, 1.0f, rain_amount) * 2.0f;
		float rain_layer1 = smoothstep(0.25f, 0.75f, rain_amount);
		float rain_layer2 = smoothstep(0.0f, 0.5f, rain_amount);

		float2 rain_c = rain_drops(rain_uv, rain_time, rain_static, rain_layer1, rain_layer2);

		//	ƒXƒNƒŠ[ƒ“‹óŠÔ‚Ì–@ü‚ğ‹‚ß‹üÜ‚³‚¹‚é
		float2 rain_e = float2(0.001f, 0.0f);
		float rain_cx = rain_drops(rain_uv + rain_e, rain_time, rain_static, rain_layer1, rain_layer2).x;
		float rain_cy = rain_drops(rain_uv + rain_e.yx, rain_time, rain_static, rain_layer1, rain_layer2).x;
		float2 rain_n = float2(rain_cx - rain_c.x, rain_cy - rain_c.x);

		float3 rain_refracted = scene_color_map.Sample(scene_map_sampler, saturate(pin.texcoord + rain_n)).rgb;
		color.rgb = lerp(color.rgb, rain_refracted, saturate(rain_c.x * rain_amount));
	}

	return color;
}
