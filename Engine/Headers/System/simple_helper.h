#pragma	once

#include "Math/math_all.h"

//	簡易的なアニメーション計算用構造体
namespace common_animation
{
	struct float_animation
	{
		float time;
		math::easing::type ease_type;
		float value;
	};

	struct float2_animation
	{
		float time;
		math::easing::type ease_type;
		DirectX::XMFLOAT2 value;
	};

	struct float3_animation
	{
		float time;
		math::easing::type ease_type;
		DirectX::XMFLOAT3 value;
	};

	struct float4_animation
	{
		float time;
		math::easing::type ease_type;
		DirectX::XMFLOAT4 value;
	};

	void compute_animation(float time, const float_animation* animations, size_t animation_size, std::function<void(float)> func);
	void compute_animation(float time, const float2_animation* animations, size_t animation_size, std::function<void(DirectX::XMFLOAT2)> func);
	void compute_animation(float time, const float3_animation* animations, size_t animation_size, std::function<void(DirectX::XMFLOAT3)> func);
	void compute_animation(float time, const float4_animation* animations, size_t animation_size, std::function<void(DirectX::XMFLOAT4)> func);
}	//	namespace common_animation

//	簡易的なフェード係数計算用構造体
namespace compute_fade
{
	struct time_pair
	{
		float start;
		float end;
		math::easing::type ease_type = math::easing::type::lerp;
	};

	struct out_calculator
	{
		time_pair fade_out;
		out_calculator(time_pair fade_out);
		float compute(float time);
	};

	struct in_calculator
	{
		time_pair fade_in;
		in_calculator(time_pair fade_in);
		float compute(float time);
	};

	struct calculator
	{
		time_pair fade_in;
		time_pair fade_out;
		calculator(time_pair fade_out, time_pair fade_in);
		float compute(float time, bool* end_flag);
	};
}	//	namespace compute_fade



