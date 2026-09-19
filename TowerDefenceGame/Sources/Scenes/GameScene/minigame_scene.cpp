#include "Scenes/GameScene/minigame_scene.h"

#include "custom_pipeline_state.h"

#include "object/camera_object.h"
#include "object/model_object.h"
#include "object/post_process_volume_object.h"

#include "object/gameobjects/minigame/roomba_object.h"
#include "object/gameobjects/minigame/roomba_controller_object.h"
#include "object/gameobjects/minigame/roomba_camera_controller_object.h"

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

minigame_scene::minigame_scene()
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

	//	ルンバモデル生成
	{
		//	モデルオブジェクト生成
		auto roomba_model = std::make_shared<model_object>("ルンバ モデル", "Assets/Resources/Models/Roomba/Roomba.emo");
		register_object(roomba_model);

		//	ルンバオブジェクト生成
		player_object = std::make_shared<roomba_object>("プレイヤー 制御", roomba_model, "player");
		register_object(player_object);
		player_object->set_position(system_units::convert_m_to_unit({0, 0, -60}));
		player_object->set_rotation({0.0f, 0.0f, 0.0f});

		//	ルンバ制御オブジェクト生成
		auto roomba_controller = std::make_shared<roomba_controller_object>("プレイヤー 入力", player_object, main_camera);
		register_object(roomba_controller);

		//	ルンバカメラ制御オブジェクト
		auto camera_controller = std::make_shared<roomba_camera_controller_object>("プレイヤー カメラ", roomba_controller, main_camera);
		register_object(camera_controller);
	}

	//	UIオブジェクト設置
	{
		test_heat = std::make_shared<ui_object>("熱バー");
		test_heat->set_screen_rect({ 40, 40, 180, 50 });
		test_heat->set_color({ 1, 0, 0, 1 });
		test_heat->set_sort_order(0);
		register_object(test_heat);
	}
	{
		test_wet = std::make_shared<ui_object>("濡れバー");
		test_wet->set_screen_rect({ 40, 100, 180, 50 });
		test_wet->set_color({ 0, 0, 1, 1 });
		test_wet->set_sort_order(0);
		register_object(test_wet);
	}

	//	星粒生成
	{
		effect_resource_ptr resource;
		if (effect_resource_manager::instance()->load(resource, "Assets/Resources/Effects/StarDust.eme"))
		{
			star_dust = std::make_shared<effect_instance>(resource);
			effect_manager::instance()->register_instance(star_dust);
		}
	}
}

minigame_scene::~minigame_scene()
{
}

bool minigame_scene::initialize_core()
{
	return true;
}

bool minigame_scene::uninitialize_core()
{
	player_object = nullptr;
	if (star_dust)
		effect_manager::instance()->unregister_instance(star_dust);
	return true;
}

void minigame_scene::update_core(float elapsed_time)
{
	if (player_object)
	{
		if (star_dust)
		{
			//	吸引座標とイベント基準座標設定
			auto position = player_object->get_push_box()->get_position();
			position.y += player_object->get_push_box()->get_local_scale().y * 0.5f;
			star_dust->set_attract_position(position);
			star_dust->set_event_position(position);
		}

		//	1F前のパーティクルイベント情報を取得してライフを増減
		auto particle_events = effect_manager::instance()->get_particle_events();
		player_object->on_wet(particle_events.size() * 100);

		if (test_heat)
		{
			test_heat->set_screen_rect({ 40, 40, (1200 - 40) * player_object->get_heat_rate(), 50 });
		}
		if (test_wet)
		{
			test_wet->set_screen_rect({ 40, 100, (1200 - 40) * player_object->get_wet_rate(), 50 });
		}
	}
}

#if defined(USE_IMGUI)
void minigame_scene::imgui_render_core()
{
	if (ImGui::TreeNode("minigame_scene"))
	{
		ImGui::TreePop();
	}
}
#endif	//	defined(USE_IMGUI)
