#include "Scenes/GameScene/game_main_scene.h"

#include "custom_pipeline_state.h"

#include "object/camera_object.h"
#include "object/model_object.h"
#include "object/post_process_volume_object.h"

#include "object/gameobjects/character/person/kuroko_object.h"

#include "object/gameobjects/character/player/player_controller_object.h"
#include "object/gameobjects/character/player/player_camera_controller_object.h"

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

	//	ポストプロセス生成
	if (pipeline_state_manager* instance = pipeline_state_manager::instance())
	{
		auto post_process_volume = std::make_shared<post_process_volume_object>("ポストプロセスボリューム");
		post_process_volume->set_flag(post_process_volume::flags::unbind, true);
		register_object(post_process_volume);

		for (UINT index = 0; index < ARRAYSIZE(postprocess_material_path_list); ++index)
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>(postprocess_material_texts_list[index][0], &pso);
			pso->set_domain(pipeline_state_base::domains::post_process);

			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = postprocess_material_texts_list[index][1];
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}

			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = postprocess_material_texts_list[index][2];
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}

			std::shared_ptr<material> post_process_material = std::make_shared<material>();
			post_process_material->set_name(postprocess_material_texts_list[index][3]);
			post_process_material->set_domain(material::domains::post_process);
			post_process_material->set_flag(material::flags::two_sided, true);
			post_process_material->set_blend_mode(material::blend_modes::opaque);
			post_process_material->set_blendable_location(postprocess_material_path_list[index]);
			post_process_material->set_pipeline_state(postprocess_material_texts_list[index][0]);

			//	マテリアル登録
			post_process_volume->get_volume()->register_postprocess_materials(post_process_material);
		}
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
