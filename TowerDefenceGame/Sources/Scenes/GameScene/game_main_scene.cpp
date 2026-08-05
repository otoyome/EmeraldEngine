#include "Scenes/GameScene/game_main_scene.h"

#include "object/camera_object.h"
#include "object/model_object.h"

#include "object/gameobjects/character/person/kuroko_object.h"

#include "object/gameobjects/character/player/player_controller_object.h"
#include "object/gameobjects/character/player/player_camera_controller_object.h"

game_main_scene::game_main_scene()
: scene_base()
{
	//	カーソル関係の非表示
	mouse::instance()->set_center_lock(true);
	mouse::instance()->set_cursol_hide(true);

	//	メインカメラ生成
	std::shared_ptr<camera_object> main_camera;
	{
		main_camera = std::make_shared<camera_object>("メインカメラ");
		main_camera->set_position({0, 10, 40});
		register_object(main_camera);
	}

	//	主人公モデル生成
	{
		auto model = std::make_shared<model_object>("プレイヤー", "Assets/Resources/Models/Kuroko/Kuroko.emo");
		size_t ik_behavior_index = model->search_ik_behavior_index("FootIK");
		if (ik_behavior_index != ik_resource::invalid_id)
		{
			model_object::ik_data ik_data = model->get_ik_behavior_datas()[ik_behavior_index];
			ik_data.visible = true;
			model->set_ik_behavior_datas(ik_behavior_index, ik_data);
		}

		register_object(model);
	}

	//	主人公モデル生成
	std::shared_ptr<player_camera_controller_object> player_camera_controller;
	{
		//	モデルオブジェクト生成
		std::shared_ptr<model_object> hero_model = kuroko_object::generate_character_model_object();
		std::shared_ptr<model_object> weapon_model = kuroko_object::generate_weapon_model_object();
		register_object(hero_model);
		register_object(weapon_model);

		auto player1 = std::make_shared<kuroko_object>(hero_model, weapon_model, "player");
		player1->set_position(system_units::convert_m_to_unit({0, 0, -50}));
		player1->set_rotation({0.0f, 0.0f, 0.0f});
		register_object(player1);

		auto controller_object = std::make_shared<player_controller_object>("プレイヤー入力", player1, main_camera);
		register_object(controller_object);

		player_camera_controller = std::make_shared<player_camera_controller_object>("プレイヤーカメラ", controller_object, main_camera);
		player_camera_controller->set_input_purmit_flag(false);
		register_object(player_camera_controller);

		//auto shadow_light = std::make_shared<shadow_light_object>("影ライト：クロコ");
		//shadow_light->set_color({0.6f, 0.6f, 0.6f, 1.0f});
		//shadow_light_to_model_connector_object::setup_datas setup_data;
		//setup_data.shadow_light = shadow_light;
		//setup_data.main_model = hero_model;
		//setup_data.sub_models.emplace_back(weapon_model);
		//setup_data.shadow_category = common_pipeline_states::shadow_category::mask_1;
		//register_object(shadow_light);
		//register_object(std::make_shared<shadow_light_to_model_connector_object>("影ライトコネクター：クロコ", setup_data));

		//	演出オブジェクトに登録
		//main_cinematics->register_effects_function(controller_object);
	}
}

game_main_scene::~game_main_scene()
{
}

bool game_main_scene::initialize_core()
{
	return true;
}

bool game_main_scene::uninitialize_core()
{
	return true;
}

void game_main_scene::update_core(float elapsed_time)
{
}

#if defined(USE_IMGUI)
void game_main_scene::imgui_render_core()
{
	if (ImGui::TreeNode("game_main_scene"))
	{
		ImGui::TreePop();
	}
}
#endif	//	defined(USE_IMGUI)
