#include "object/gameobjects/character/character_object.h"

namespace
{
	constexpr std::string_view common_effect_paths[] =
	{
		"Assets/Resources/effect/Effekseer01/sonic_boom.efk",	// shock_wave
		"Assets/Resources/effect/tktk01/parry.efk",	// parry
	};
}

void character_object::register_common_effects()
{
	//register_effect(static_cast<size_t>(effect_ids::shock_wave),
	//				common_effect_paths[static_cast<int>(effect_ids::shock_wave)]);
	//register_effect(static_cast<size_t>(effect_ids::parry),
	//				common_effect_paths[static_cast<int>(effect_ids::parry)]);
}

bool character_object::register_effect(size_t effect_id, std::string_view file_path)
{
	if (effect_resources.find(effect_id) != effect_resources.end())
		return false;

	effect_resource_ptr resource;
	if (effect_resource_manager::instance()->load(resource, file_path))
	{
		effect_resources.insert_or_assign(effect_id, resource);
		return true;
	}
	return false;
}

std::weak_ptr<effect_instance> character_object::play_effect(size_t effect_id)
{
	auto it = effect_resources.find(effect_id);
	if (it == effect_resources.end())
		return std::weak_ptr<effect_instance>();

	auto instance = std::make_shared<effect_instance>(it->second);
	if (effect_manager::instance()->register_instance(instance))
	{
		effect_instances.emplace_back(instance);
	}
	return instance;
}


