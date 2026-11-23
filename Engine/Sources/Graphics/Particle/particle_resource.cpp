#include <cmath>
#include <algorithm>

#include "Graphics/Particle/particle_resource.h"
#include "Graphics/Core/graphics_utility.h"
#include "Graphics/Core/render_data.h"
#include "Texture/texture.h"

#include "cereal/cereal.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/unordered_map.hpp"
#include "cereal/types/variant.hpp"

#include "Animation/animation_data_version.h"

// CEREALバージョン定義
CEREAL_CLASS_VERSION(particle::item::renderer::trails, 2)
CEREAL_CLASS_VERSION(particle::item::renderer::texcoords, 1)
CEREAL_CLASS_VERSION(particle::item::renderer::colors, 3)
CEREAL_CLASS_VERSION(particle::item::renderer, 2)

CEREAL_CLASS_VERSION(particle::item::spawn, 1)
CEREAL_CLASS_VERSION(particle::item::velocity, 2)
CEREAL_CLASS_VERSION(particle::item::attract, 1)
CEREAL_CLASS_VERSION(particle::item::event, 1)
CEREAL_CLASS_VERSION(particle::item::curve, 1)
CEREAL_CLASS_VERSION(particle::item::gradient::key, 1)
CEREAL_CLASS_VERSION(particle::item::gradient, 1)

CEREAL_CLASS_VERSION(particle::data, 1);

bool particle::item::renderer::trails::is_state(states flag) const
{
	return core::is_bitflag(state, flag);
}
void particle::item::renderer::trails::set_state(states flag, bool value)
{
	core::set_bitflag(state, flag, value);
}
template<class Archive>
void particle::item::renderer::trails::serialize(Archive& archive, int version)
{
	archive(state, max, color_by_length, alpha_by_length, width_by_length);
}

bool particle::item::renderer::texcoords::is_state(states flag) const
{
	return core::is_bitflag(state, flag);
}
void particle::item::renderer::texcoords::set_state(states flag, bool value)
{
	core::set_bitflag(state, flag, value);
}
template<class Archive>
void particle::item::renderer::texcoords::serialize(Archive& archive, int version)
{
	archive(state, index, time, rect, splits);
}

bool particle::item::renderer::colors::is_state(states flag) const
{
	return core::is_bitflag(state, flag);
}
void particle::item::renderer::colors::set_state(states flag, bool value)
{
	core::set_bitflag(state, flag, value);
}
template<class Archive>
void particle::item::renderer::colors::serialize(Archive& archive, int version)
{
	archive(state, color_min, color_max, color_by_life, alpha_by_life);
}

bool particle::item::renderer::is_state(states flag) const
{
	return core::is_bitflag(state, flag);
}
void particle::item::renderer::set_state(states flag, bool value)
{
	core::set_bitflag(state, flag, value);
}
template<class Archive>
void particle::item::renderer::serialize(Archive& archive, int version)
{
	archive(state, type, total_particle_count, material, trail, uv, color, scale_by_life);
}

bool particle::item::spawn::is_state(states flag) const
{
	return core::is_bitflag(state, flag);
}
void particle::item::spawn::set_state(states flag, bool value)
{
	core::set_bitflag(state, flag, value);
}
template<class Archive>
void particle::item::spawn::serialize(Archive& archive, int version)
{
	archive(state, life_time_range, scale_range, area_min, area_max, range);
}

bool particle::item::velocity::is_state(states flag) const
{
	return core::is_bitflag(state, flag);
}
void particle::item::velocity::set_state(states flag, bool value)
{
	core::set_bitflag(state, flag, value);
}
template<class Archive>
void particle::item::velocity::serialize(Archive& archive, int version)
{
	archive(state,
			direction, angle_range, power_range,
			acceleration_min, acceleration_max,
			damping_min, damping_max,
			gravity_scale, speed_by_life);
}

template<class Archive>
void particle::item::attract::serialize(Archive& archive, int version)
{
	archive(strength,
			falloff,
			damping,
			radius,
			kill_radius);
}

bool particle::item::event::is_state(states flag) const
{
	return core::is_bitflag(state, flag);
}
void particle::item::event::set_state(states flag, bool value)
{
	core::set_bitflag(state, flag, value);
}
template<class Archive>
void particle::item::event::serialize(Archive& archive, int version)
{
	archive(state, node_name, radius);
}

float particle::item::curve::evaluate(float rate) const
{
	if (keys.empty())
		return 1.0f;

	if (rate <= keys.front().x)
		return keys.front().y;

	if (rate >= keys.back().x)
		return keys.back().y;

	//	区間を探す
	size_t key_index = 0;
	for (; key_index < keys.size() - 1; ++key_index)
	{
		if (keys[key_index].x <= rate && rate < keys[key_index + 1].x)
			break;
	}
	if (key_index >= keys.size() - 1)
		return keys.back().y;

	const math::float2& s = keys[key_index + 0];
	const math::float2& e = keys[key_index + 1];
	switch (type)
	{
		case types::discrete:	rate = 0.0f; break;
		case types::linear:		rate = math::linearstep(s.x, e.x, rate); break;
		case types::smooth:		rate = math::smoothstep(s.x, e.x, rate); break;
	}
	return math::lerp(s.y, e.y, rate);
}
template<class Archive>
void particle::item::curve::serialize(Archive& archive, int version)
{
	archive(type, keys);
}

//	寿命の割合から色を求める
math::float3 particle::item::gradient::evaluate(float rate) const
{
	if (keys.empty())
		return { 1.0f, 1.0f, 1.0f };
	if (rate <= keys.front().rate)
		return keys.front().color;
	if (rate >= keys.back().rate)
		return keys.back().color;

	for (size_t index = 0; index + 1 < keys.size(); ++index)
	{
		const key& s = keys[index + 0];
		const key& e = keys[index + 1];
		if (rate < e.rate)
		{
			const float t = math::linearstep(s.rate, e.rate, rate);
			return { math::lerp(s.color.x, e.color.x, t), math::lerp(s.color.y, e.color.y, t), math::lerp(s.color.z, e.color.z, t) };
		}
	}
	return keys.back().color;
}
template<class Archive>
void particle::item::gradient::key::serialize(Archive& archive, int version)
{
	archive(rate, color);
}
template<class Archive>
void particle::item::gradient::serialize(Archive& archive, int version)
{
	archive(keys);
}


//////////////////////////////////////////////////////////////////////////////////////////////
bool particle::data::is_valid() const
{
	return !items.empty();
}

particle_system_item::emitter_resource particle::data::constract_system_resource() const
{
	particle_system_item::emitter_resource resource; 
	auto& emitter_system = resource.system;
	auto& emitter_render = resource.render;

	//	エミッター情報登録
	emitter_system.max_update_item_size = sizeof(particle_system_item::emitter::common);
	emitter_system.particle_type_size = sizeof(particle_system_item::particle::common);
	emitter_system.total_particle_count = 0;

	//	描画系の情報設定
	if (auto renderer = get_item<particle::item::renderer>())
	{
		emitter_system.total_particle_count = renderer->total_particle_count;
		switch (renderer->type)
		{
			case particle::item::renderer::types::sprite:
			{
				emitter_render.renderer_type = particle_system_item::renderer_types::sprite;
				emitter_render.trail_count = 0;
				break;
			}
			case particle::item::renderer::types::ribon:
			{
				emitter_render.renderer_type = particle_system_item::renderer_types::ribon;
				emitter_render.trail_count = renderer->trail.max;

				emitter_system.max_update_item_size += sizeof(particle_system_item::emitter::trail);
				emitter_system.particle_type_size += sizeof(particle_system_item::particle::trail_header) +
														sizeof(particle_system_item::particle::trail_body) * renderer->trail.max;
				break;
			}
		}

		//	UV指定アイテム
		emitter_system.max_update_item_size += sizeof(particle_system_item::emitter::uv);
		emitter_system.particle_type_size += sizeof(particle_system_item::particle::uv);

		//	色指定アイテム
		emitter_system.max_update_item_size += sizeof(particle_system_item::emitter::color);
		//emitter_system.particle_type_size += sizeof(particle_system_item::particle::color);
	}

	//	速度情報の設定
	if (auto velocity = get_item<particle::item::velocity>())
	{
		emitter_system.max_update_item_size += sizeof(particle_system_item::emitter::velocity);
		emitter_system.particle_type_size += sizeof(particle_system_item::particle::velocity);
	}

	//	引き込み情報の設定
	if (auto attract = get_item<particle::item::attract>())
	{
		emitter_system.max_update_item_size += sizeof(particle_system_item::emitter::attract);
		emitter_system.particle_type_size += sizeof(particle_system_item::particle::attract);
	}

	//	イベント情報の設定
	if (auto event = get_item<particle::item::event>())
	{
		emitter_system.max_update_item_size += sizeof(particle_system_item::emitter::event);
		emitter_system.particle_type_size += sizeof(particle_system_item::particle::event);
	}

	//	各アイテムの寿命カーブを集めて、サンプル表に焼く
	//	寿命 行0:(R, G, B, A) / 行1:(大きさ, 速度, 未使用, 未使用)
	//	長さ 行2:(R, G, B, A) / 行3:(太さ, 未使用, 未使用, 未使用)
	{
		using flags = particle_system_item::curve_flags;
		const auto* renderer = get_item<particle::item::renderer>();
		const auto* velocity = get_item<particle::item::velocity>();

		const particle::item::gradient* color_curve = nullptr;
		const particle::item::curve* alpha_curve = nullptr;
		const particle::item::curve* scale_curve = nullptr;
		const particle::item::curve* speed_curve = nullptr;
		const particle::item::gradient* trail_color_curve = nullptr;
		const particle::item::curve* trail_alpha_curve = nullptr;
		const particle::item::curve* trail_width_curve = nullptr;
		if (renderer && renderer->color.is_state(particle::item::renderer::colors::states::color_by_life))
			color_curve = &renderer->color.color_by_life;
		if (renderer && renderer->color.is_state(particle::item::renderer::colors::states::alpha_by_life))
			alpha_curve = &renderer->color.alpha_by_life;
		if (renderer && renderer->is_state(particle::item::renderer::states::scale_by_life))
			scale_curve = &renderer->scale_by_life;
		if (velocity && velocity->is_state(particle::item::velocity::states::speed_by_life))
			speed_curve = &velocity->speed_by_life;
		if (renderer && renderer->type == particle::item::renderer::types::ribon)
		{
			using trail_states = particle::item::renderer::trails::states;
			if (renderer->trail.is_state(trail_states::color_by_length))
				trail_color_curve = &renderer->trail.color_by_length;
			if (renderer->trail.is_state(trail_states::alpha_by_length))
				trail_alpha_curve = &renderer->trail.alpha_by_length;
			if (renderer->trail.is_state(trail_states::width_by_length))
				trail_width_curve = &renderer->trail.width_by_length;
		}

		UINT curve_flags = 0;
		if (color_curve) curve_flags |= static_cast<UINT>(flags::color);
		if (alpha_curve) curve_flags |= static_cast<UINT>(flags::alpha);
		if (scale_curve) curve_flags |= static_cast<UINT>(flags::scale);
		if (speed_curve) curve_flags |= static_cast<UINT>(flags::speed);
		if (trail_color_curve) curve_flags |= static_cast<UINT>(flags::trail_color);
		if (trail_alpha_curve) curve_flags |= static_cast<UINT>(flags::trail_alpha);
		if (trail_width_curve) curve_flags |= static_cast<UINT>(flags::trail_width);

		if (curve_flags != 0)
		{
			constexpr UINT sample_count = particle_system_item::curve_sample_count;
			resource.curve_flags = curve_flags;
			resource.curve_samples.resize(sample_count * particle_system_item::curve_row_count);
			for (UINT index = 0; index < sample_count; ++index)
			{
				const float rate = static_cast<float>(index) / static_cast<float>(sample_count - 1);

				math::float4& color = resource.curve_samples[sample_count * 0 + index];
				color = { 1.0f, 1.0f, 1.0f, 1.0f };
				if (color_curve)
				{
					const math::float3 rgb = color_curve->evaluate(rate);
					color.x = rgb.x;
					color.y = rgb.y;
					color.z = rgb.z;
				}
				if (alpha_curve)
					color.w = alpha_curve->evaluate(rate);

				math::float4& misc = resource.curve_samples[sample_count * 1 + index];
				misc = { 1.0f, 1.0f, 1.0f, 1.0f };
				if (scale_curve)
					misc.x = scale_curve->evaluate(rate);
				if (speed_curve)
					misc.y = speed_curve->evaluate(rate);

				math::float4& trail_color = resource.curve_samples[sample_count * 2 + index];
				trail_color = { 1.0f, 1.0f, 1.0f, 1.0f };
				if (trail_color_curve)
				{
					const math::float3 rgb = trail_color_curve->evaluate(rate);
					trail_color.x = rgb.x;
					trail_color.y = rgb.y;
					trail_color.z = rgb.z;
				}
				if (trail_alpha_curve)
					trail_color.w = trail_alpha_curve->evaluate(rate);

				math::float4& trail_misc = resource.curve_samples[sample_count * 3 + index];
				trail_misc = { 1.0f, 1.0f, 1.0f, 1.0f };
				if (trail_width_curve)
					trail_misc.x = trail_width_curve->evaluate(rate);
			}
		}
	}

	return resource;
}

template<class Archive>
void particle::data::serialize(Archive& archive, int version)
{
	archive(name, items);
}
template<>
void particle::data::serialize<cereal::BinaryOutputArchive>(cereal::BinaryOutputArchive& archive, int version)
{
	archive(name, items);
}

template<>
void particle::data::serialize<cereal::BinaryInputArchive>(cereal::BinaryInputArchive& archive, int version)
{
	archive(name, items);
}