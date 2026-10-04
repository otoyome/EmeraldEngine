#include "system/simple_helper.h"

void common_animation::compute_animation(float time, const float_animation* animations, size_t animation_size, std::function<void(float)> func)
{
	UINT index = 0;
	for(; index < animation_size - 1; ++index)
	{
		auto& first = animations[index + 0];
		auto& second = animations[index + 1];
		float factor = (time - first.time) / (second.time - first.time);
		if (0.0f <= factor && factor < 1.0f)
		{
			factor = math::easing::calculate(factor, first.ease_type);
			func(math::lerp(first.value, second.value, factor));
			break;
		}
	}
	if (time >= animations[animation_size - 1].time)
	{
		func(animations[animation_size - 1].value);
	}
}

void common_animation::compute_animation(float time, const float2_animation* animations, size_t animation_size, std::function<void(math::float2)> func)
{
	UINT index = 0;
	for(; index < animation_size - 1; ++index)
	{
		auto& first = animations[index + 0];
		auto& second = animations[index + 1];
		float factor = (time - first.time) / (second.time - first.time);
		if (0.0f <= factor && factor < 1.0f)
		{
			factor = math::easing::calculate(factor, first.ease_type);
			math::float2 value;
			value.x = math::lerp(first.value.x, second.value.x, factor);
			value.y = math::lerp(first.value.y, second.value.y, factor);
			func(value);
			break;
		}
	}
	if (time >= animations[animation_size - 1].time)
	{
		func(animations[animation_size - 1].value);
	}
}

void common_animation::compute_animation(float time, const float3_animation* animations, size_t animation_size, std::function<void(math::float3)> func)
{
	UINT index = 0;
	for(; index < animation_size - 1; ++index)
	{
		auto& first = animations[index + 0];
		auto& second = animations[index + 1];
		float factor = (time - first.time) / (second.time - first.time);
		if (0.0f <= factor && factor < 1.0f)
		{
			factor = math::easing::calculate(factor, first.ease_type);
			math::float3 value;
			value.x = math::lerp(first.value.x, second.value.x, factor);
			value.y = math::lerp(first.value.y, second.value.y, factor);
			value.z = math::lerp(first.value.z, second.value.z, factor);
			func(value);
			break;
		}
	}
	if (time >= animations[animation_size - 1].time)
	{
		func(animations[animation_size - 1].value);
	}
}

void common_animation::compute_animation(float time, const float4_animation* animations, size_t animation_size, std::function<void(math::float4)> func)
{
	UINT index = 0;
	for(; index < animation_size - 1; ++index)
	{
		auto& first = animations[index + 0];
		auto& second = animations[index + 1];
		float factor = (time - first.time) / (second.time - first.time);
		if (0.0f <= factor && factor < 1.0f)
		{
			factor = math::easing::calculate(factor, first.ease_type);
			math::float4 value;
			value.x = math::lerp(first.value.x, second.value.x, factor);
			value.y = math::lerp(first.value.y, second.value.y, factor);
			value.z = math::lerp(first.value.z, second.value.z, factor);
			value.w = math::lerp(first.value.w, second.value.w, factor);
			func(value);
			break;
		}
	}
	if (time >= animations[animation_size - 1].time)
	{
		func(animations[animation_size - 1].value);
	}
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
compute_fade::out_calculator::out_calculator(time_pair fade_out)
: fade_out(fade_out)
{
}

float compute_fade::out_calculator::compute(float time)
{
	float factor = math::easing::calculate((time - fade_out.start) / (fade_out.end - fade_out.start), fade_out.ease_type);
	return math::saturate(factor);
}
compute_fade::in_calculator::in_calculator(time_pair fade_in)
: fade_in(fade_in)
{
}

float compute_fade::in_calculator::compute(float time)
{
	float factor = math::easing::calculate(1.0f - (time - fade_in.start) / (fade_in.end - fade_in.start), fade_in.ease_type);
	return math::saturate(factor);
}

compute_fade::calculator::calculator(time_pair fade_out, time_pair fade_in)
: fade_in(fade_in)
, fade_out(fade_out)
{
}

float compute_fade::calculator::compute(float time, bool* end_flag)
{
	if (end_flag)
		*end_flag = false;
	float factor = 0.0f;
	if (fade_out.start <= time && time <= fade_out.end)
	{
		factor = math::easing::calculate((time - fade_out.start) / (fade_out.end - fade_out.start), fade_out.ease_type);
	}
	else if (fade_out.end <= time && time <= fade_in.start)
	{
		factor = 1.0f;
	}
	else if (fade_in.start <= time && time <= fade_in.end)
	{
		factor = math::easing::calculate(1.0f - (time - fade_in.start) / (fade_in.end - fade_in.start), fade_in.ease_type);
	}
	else if (fade_in.end <= time && end_flag)
	{
		*end_flag = true;
	}
	return factor;
}

