#include <cmath>

#include "graphics/particle_resource.h"
#include "graphics_utility.h"
#include "render_data.h"
#include "texture.h"

#include "cereal/cereal.hpp"
#include "cereal/types/memory.hpp"
#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/unordered_map.hpp"
#include "cereal/types/variant.hpp"

#include "animation_data_version.h"

// CEREALバージョン定義
CEREAL_CLASS_VERSION(particle::item::renderer::trails, 3)
CEREAL_CLASS_VERSION(particle::item::renderer::texcoords, 1)
CEREAL_CLASS_VERSION(particle::item::renderer::colors, 2)
CEREAL_CLASS_VERSION(particle::item::renderer, 2)

CEREAL_CLASS_VERSION(particle::item::spawn, 1)
CEREAL_CLASS_VERSION(particle::item::velocity, 2)
CEREAL_CLASS_VERSION(particle::item::attract, 1)
CEREAL_CLASS_VERSION(particle::item::event, 2)

CEREAL_CLASS_VERSION(particle::data, 1);

bool particle::item::renderer::trails::is_state(states flag) const
{
	return system_utility::is_bitflag(state, flag);
}
void particle::item::renderer::trails::set_state(states flag, bool value)
{
	system_utility::set_bitflag(state, flag, value);
}
template<class Archive>
void particle::item::renderer::trails::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 1:
		{
			archive(max);
			break;
		}
		case 2:
		{
			math::float2 unused_time_fade_rate;
			archive(state, max, unused_time_fade_rate);
			break;
		}
		case 3:
		{
			archive(state, max);
			break;
		}
	}
}

bool particle::item::renderer::texcoords::is_state(states flag) const
{
	return system_utility::is_bitflag(state, flag);
}
void particle::item::renderer::texcoords::set_state(states flag, bool value)
{
	system_utility::set_bitflag(state, flag, value);
}
template<class Archive>
void particle::item::renderer::texcoords::serialize(Archive& archive, int version)
{
	archive(state, index, time, rect, splits);
}

bool particle::item::renderer::colors::is_state(states flag) const
{
	return system_utility::is_bitflag(state, flag);
}
void particle::item::renderer::colors::set_state(states flag, bool value)
{
	system_utility::set_bitflag(state, flag, value);
}
template<class Archive>
void particle::item::renderer::colors::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 1:
		{
			archive(state, color_min, color_max);
			break;
		}
		case 2:
		{
			archive(state, color_min, color_max, fade_range);
			break;
		}
	}
}

bool particle::item::renderer::is_state(states flag) const
{
	return system_utility::is_bitflag(state, flag);
}
void particle::item::renderer::set_state(states flag, bool value)
{
	system_utility::set_bitflag(state, flag, value);
}
template<class Archive>
void particle::item::renderer::serialize(Archive& archive, int version)
{
	if (version < 2)
	{
		archive(state, type, total_particle_count, material, trail, uv, color);
		return;
	}

	if constexpr (Archive::is_saving::value)
	{
		std::string path = material ? std::string(material->get_file_path()) : material_path;
		archive(state, type, total_particle_count, path, trail, uv, color);
	}
	else
	{
		archive(state, type, total_particle_count, material_path, trail, uv, color);
	}
}

bool particle::item::spawn::is_state(states flag) const
{
	return system_utility::is_bitflag(state, flag);
}
void particle::item::spawn::set_state(states flag, bool value)
{
	system_utility::set_bitflag(state, flag, value);
}
template<class Archive>
void particle::item::spawn::serialize(Archive& archive, int version)
{
	archive(state, life_time_range, scale_range, area_min, area_max, range);
}

bool particle::item::velocity::is_state(states flag) const
{
	return system_utility::is_bitflag(state, flag);
}
void particle::item::velocity::set_state(states flag, bool value)
{
	system_utility::set_bitflag(state, flag, value);
}
template<class Archive>
void particle::item::velocity::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 1:
		{
			math::float3 unused_deceleration_min, unused_deceleration_max;
			archive(state,
					direction, angle_range, power_range,
					acceleration_min, acceleration_max,
					unused_deceleration_min, unused_deceleration_max,
					gravity_scale);
			break;
		}
		case 2:
		{
			archive(state,
					direction, angle_range, power_range,
					acceleration_min, acceleration_max,
					damping_min, damping_max,
					gravity_scale);
			break;
		}
	}
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
	return system_utility::is_bitflag(state, flag);
}
void particle::item::event::set_state(states flag, bool value)
{
	system_utility::set_bitflag(state, flag, value);
}
template<class Archive>
void particle::item::event::serialize(Archive& archive, int version)
{
	switch (version)
	{
		case 1:
		{
			archive(node_name, radius);
			break;
		}
		case 2:
		{
			archive(state, node_name, radius);
			break;
		}
	}
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