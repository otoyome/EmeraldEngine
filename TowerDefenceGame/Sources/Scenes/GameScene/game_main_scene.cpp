#include "Scenes/GameScene/game_main_scene.h"

#include "custom_pipeline_state.h"

#include "object/camera_object.h"
#include "object/model_object.h"
#include "object/post_process_volume_object.h"

#include "object/gameobjects/character/person/kuroko_object.h"

#include "object/gameobjects/character/player/player_controller_object.h"
#include "object/gameobjects/character/player/player_camera_controller_object.h"

#include "object/gameobjects/character/person/boss_seraphim_object.h"
#include "object/gameobjects/character/enemy/BossSeraphim/boss_seraphim_controller_object.h"

#include "object/gameobjects/cinematics/cinematics_object.h"

namespace
{
	//	ポストプロセスマテリアル
	constexpr material::blendable_locations postprocess_material_path_list[] =
	{
		material::blendable_locations::before_dof,
		material::blendable_locations::after_dof,
		material::blendable_locations::after_dof_translucency,
		material::blendable_locations::before_bloom,
		material::blendable_locations::after_tone_mapping,
	};
	constexpr std::string_view postprocess_material_texts_list[ARRAYSIZE(postprocess_material_path_list)][4] =
	{
		{
			"before_translucent_color_filter",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/Postprocess/color_filter_before_dof_ps.hlsl",
			"被写界深度前ポストプロセスマテリアル",
		},
		{
			"before_tonemapping_color_filter",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/Postprocess/color_filter_after_dof_ps.hlsl",
			"被写界深度後ポストプロセスマテリアル",
		},
		{
			"before_tonemapping_color_filter",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/Postprocess/color_filter_after_dof_translucency_ps.hlsl",
			"被写界深度後半透明描画ポストプロセスマテリアル",
		},
		{
			"after_tonemapping_color_filter",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/Postprocess/color_filter_before_bloom_ps.hlsl",
			"ブルーム前ポストプロセスマテリアル",
		},
		{
			"after_tonemapping_color_filter",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/Postprocess/color_filter_after_tone_mapping_ps.hlsl",
			"トーンマッピング後ポストプロセスマテリアル",
		}
	};
}

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
		main_camera->get_camera()->set_fov_y(DirectX::XMConvertToRadians(30.0f));
		main_camera->set_position({0, 10, 40});
		register_object(main_camera);
	}

	//	演出オブジェクト生成
	std::shared_ptr<cinematics_object> main_cinematics;
	{
		main_cinematics = std::make_shared<cinematics_object>("演出管理オブジェクト", main_camera);
		register_object(main_cinematics);
	}

	//	主人公モデル生成
	std::shared_ptr<player_camera_controller_object> player_camera_controller;
	{
		//	モデルオブジェクト生成
		std::shared_ptr<model_object> hero_model = kuroko_object::generate_character_model_object();
		std::shared_ptr<model_object> weapon_model = kuroko_object::generate_weapon_model_object();
		register_object(hero_model);
		register_object(weapon_model);

		//	足IKはONにしておく(アニメーション側で制御できるに越したことはない)
		size_t ik_behavior_index = hero_model->search_ik_behavior_index("FootIK");
		if (ik_behavior_index != ik_resource::invalid_id)
		{
			model_object::ik_data ik_data = hero_model->get_ik_behavior_datas()[ik_behavior_index];
			ik_data.visible = true;
			hero_model->set_ik_behavior_datas(ik_behavior_index, ik_data);
		}

		auto player1 = std::make_shared<kuroko_object>(hero_model, weapon_model, "player");
		player1->set_position(system_units::convert_m_to_unit({0, 0, -50}));
		player1->set_rotation({0.0f, 0.0f, 0.0f});
		register_object(player1);

		auto controller_object = std::make_shared<player_controller_object>("プレイヤー入力", player1, main_camera);
		register_object(controller_object);

		player_camera_controller = std::make_shared<player_camera_controller_object>("プレイヤーカメラ", controller_object, main_camera);
		register_object(player_camera_controller);
	}

	//	敵モデル生成
	{
		//	モデルオブジェクト生成
		std::shared_ptr<model_object> boss_model = boss_seraphim_object::generate_character_model_object();
		register_object(boss_model);

		auto enemy1 = std::make_shared<boss_seraphim_object>(boss_model, "enemy");
		enemy1->set_active(false);
		enemy1->set_position(system_units::convert_m_to_unit({0, -1000, 0}));
		enemy1->set_rotation({ 0.0f, DirectX::XM_PI, 0.0f });
		register_object(enemy1);

		auto controller = std::make_shared<boss_seraphim_controller_object>("ボスセラフィム制御用クラス", enemy1);
		controller->set_active(false);

		register_object(controller);

		main_cinematics->register_effects_function(enemy1);
		player_camera_controller->register_character_object(enemy1);
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
