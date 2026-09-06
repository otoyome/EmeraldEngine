#include "object/gameobjects/character/person/kuroko_object.h"

#include "collision/capsule.h"

#include "collision/sphere.h"

namespace setup_data
{
	constexpr std::string_view character_model_file_path = "Assets/Resources/Models/Kuroko/Kuroko.emo";
	constexpr std::string_view character_model_object_name = "モノ・クロコ モデル";

	//	武器モデル
	constexpr std::string_view weapon_model_file_path = "Assets/Resources/Models/Kuroko/Weapon.emo";
	constexpr std::string_view weapon_model_object_name = "モノ・クロコ 武器 モデル";
}

std::shared_ptr<model_object> kuroko_object::generate_character_model_object()
{
	std::filesystem::path path(setup_data::character_model_file_path);
	assert (std::filesystem::exists(path));
	return std::make_shared<model_object>(setup_data::character_model_object_name, path.string());
}

std::shared_ptr<model_object> kuroko_object::generate_weapon_model_object()
{
	std::filesystem::path path(setup_data::weapon_model_file_path);
	assert (std::filesystem::exists(path));
	return std::make_shared<model_object>(setup_data::weapon_model_object_name, path.string());
}


void kuroko_object::register_effects()
{
	//	この辺りは今の所置いておく

	//	エフェクトリソース読込
	register_effect(static_cast<size_t>(custom_effect_id::grand_viper_frame), "Assets/Resources/effect/tktk02/grand_viperr_frame.efk");
	register_effect(static_cast<size_t>(custom_effect_id::attack6_3), "Assets/Resources/effect/tktk02/attack6_3.efk");
	register_effect(static_cast<size_t>(custom_effect_id::ground_break_effect), "Assets/Resources/effect/Pierre01/ground_break_effect.efk");

//	register_effect(static_cast<size_t>(custom_effect_id::break_attack_impact), "Assets/Resources/effect/tktk02/break_attack2.efk");
	register_effect(static_cast<size_t>(custom_effect_id::break_attack_impact), "Assets/Resources/effect/tktk01/Blow13.efk");

	//	攻撃用デカールテクスチャ読込
//	texture_manager::instance()->load_texture_from_file(attack_6_decal, "Assets/Resources/ground_crack.png");
}
