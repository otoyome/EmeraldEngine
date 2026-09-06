#include "object/gameobjects/character/person/boss_seraphim_object.h"

#include "collision/capsule.h"

namespace setup_data
{
	constexpr std::string_view character_model_file_path = "Assets/Resources/Models/Seraphim/Seraphim.emo";
	constexpr std::string_view character_model_object_name = "セラフィム モデル";
	constexpr std::string_view model_control_object_name = "セラフィム 制御";
}

std::shared_ptr<model_object> boss_seraphim_object::generate_character_model_object()
{
	std::filesystem::path path(setup_data::character_model_file_path);
	assert (std::filesystem::exists(path));
	return std::make_shared<model_object>(setup_data::character_model_object_name, path.string());
}

void boss_seraphim_object::register_effects()
{
	register_effect(static_cast<size_t>(custom_effect_id::omen), "Assets/Resources/effect/Pierre01/omen.efk");
	register_effect(static_cast<size_t>(custom_effect_id::monster_roar), "Assets/Resources/effect/Pierre01/monster_roar.efk");
	register_effect(static_cast<size_t>(custom_effect_id::special_attack_convergence), "Assets/Resources/effect/MAGICALxSPIRAL/special_attack_convergence.efk");
	
	//	エフェクトリソース読込
//	register_effect()
//	effect_manager::instance()->load_resource(effect_resources[static_cast<int>(effect_id::dash)], "Assets/Resources/effect/Effekseer01/sonic_boom.efk");

	//	攻撃用デカールテクスチャ読込
//	texture_manager::instance()->load_texture_from_file(attack_6_decal, "Assets/Resources/ground_crack.png");
}

