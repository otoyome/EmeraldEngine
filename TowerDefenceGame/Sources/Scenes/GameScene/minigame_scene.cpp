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
			"after_dof_color_filter",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/Postprocess/color_filter_after_dof_ps.hlsl",
			"被写界深度後ポストプロセスマテリアル",
		},
		{
			"after_dof_translucency_color_filter",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/Postprocess/color_filter_after_dof_translucency_ps.hlsl",
			"被写界深度後半透明描画ポストプロセスマテリアル",
		},
		{
			"before_bloom_color_filter",
			"Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl",
			"Shaders/Postprocess/color_filter_before_bloom_ps.hlsl",
			"ブルーム前ポストプロセスマテリアル",
		},
		{
			"after_tone_mapping_color_filter",
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
		{
			auto bloom_constant = post_process_volume->get_volume()->get_bloom_constant();
			auto lens_ghosts_constant = post_process_volume->get_volume()->get_lens_ghosts_constant();
			auto depth_of_field_constant = post_process_volume->get_volume()->get_depth_of_field_constant();
			auto screen_space_reflection_constant = post_process_volume->get_volume()->get_screen_space_reflection_constant();
			auto motion_blur_constant = post_process_volume->get_volume()->get_motion_blur_constant();
			auto exposure_compensation_constant = post_process_volume->get_volume()->get_exposure_compensation_constant();
			post_process_volume->get_volume()->set_bloom_constant(bloom_constant);
			post_process_volume->get_volume()->set_lens_ghosts_constant(lens_ghosts_constant);
			post_process_volume->get_volume()->set_depth_of_field_constant(depth_of_field_constant);
			post_process_volume->get_volume()->set_screen_space_reflection_constant(screen_space_reflection_constant);
			post_process_volume->get_volume()->set_motion_blur_constant(motion_blur_constant);
			post_process_volume->get_volume()->set_exposure_compensation_constant(exposure_compensation_constant);
		}
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

			//	連携させるためにポストプロセスマテリアルを保持しておく
			if (index == 1)
			{
				postprocess_material = post_process_material;
			}
		}
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
	{
		font_data_ptr speed_font;
		font_data_manager::instance()->load_from_file(speed_font, "Assets/Resources/Textures/fonts/MS Gothic/MS Gothic.fnt");
		test_speed = std::make_shared<ui_text>("速度", speed_font);
		test_speed->set_screen_rect({ 1000, 500, 180, 180 });
		test_speed->set_color({ 0, 0, 1, 1 });
		test_speed->set_sort_order(0);
		register_object(test_speed);
	}

	//	エフェクト類生成
	{
		effect_resource_ptr resource;
		if (effect_resource_manager::instance()->load(resource, "Assets/Resources/Effects/Bubble.eme"))
		{
			bubble_effect = std::make_shared<effect_instance>(resource);
			effect_manager::instance()->register_instance(bubble_effect);
		}
		//if (effect_resource_manager::instance()->load(resource, "Assets/Resources/Effects/Rain.eme"))
		//{
		//	rain_effect = std::make_shared<effect_instance>(resource);
		//	effect_manager::instance()->register_instance(rain_effect);
		//}
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
	if (bubble_effect)
		effect_manager::instance()->unregister_instance(bubble_effect);
	if (rain_effect)
		effect_manager::instance()->unregister_instance(rain_effect);
	return true;
}

void minigame_scene::update_core(float elapsed_time)
{
	if (player_object)
	{
		if (bubble_effect)
		{
			//	吸引座標とイベント基準座標設定
			auto position = player_object->get_push_box()->get_position();
			position.y += player_object->get_push_box()->get_local_scale().y * 0.5f;
			bubble_effect->set_attract_position(position);
			bubble_effect->set_event_position(position);

			//	吸引範囲設定
			auto bubble_index = bubble_effect->search_node_index_by_name("Bubble");
			for (auto& particle : bubble_effect->get_particles())
			{
				if (particle.node_index != bubble_index)
					continue;

				auto attract = particle.particle.get_item<particle::item::attract>();
				if (attract)
				{
					attract->radius = player_object->get_attract_range();
					break;
				}
			}
		}

		//	1F前のパーティクルイベント情報を取得
		auto particle_events = effect_manager::instance()->get_particle_events();
		auto push_box_position = DirectX::XMLoadFloat3(&player_object->get_push_box()->get_position());

		//	bubble/rainの自エミッターIDを収集
		for (auto& item : effect_manager::instance()->get_items())
		{
			if (item.effect_instance == bubble_effect)
			{
				for (auto& emitter_item : item.emitter_items)
					bubble_emitter_ids.push_back(emitter_item.system_index);
			}
			else if (item.effect_instance == rain_effect)
			{
				for (auto& emitter_item : item.emitter_items)
					rain_emitter_ids.push_back(emitter_item.system_index);
			}
		}

		//	濡れゲージ増加処理
		UINT wet_event_count = 0;
		for (auto& event_info : particle_events)
		{
			//	泡の場合はそのまま濡れカウントとして
			bool is_bubble = false;
			for (auto emitter_id : bubble_emitter_ids)
			{
				if (emitter_id == event_info.target_emitter_id)
				{
					is_bubble = true;
					break;
				}
			}
			if (is_bubble)
			{
				++wet_event_count;
				continue;
			}

			//	雨の場合はそのまま球体判定をこちらで行う
			bool is_rain = false;
			for (auto emitter_id : rain_emitter_ids)
			{
				if (emitter_id == event_info.target_emitter_id)
				{
					is_rain = true;
					break;
				}
			}
			if (is_rain)
			{
				auto event_position = DirectX::XMLoadFloat3(&event_info.position);
				float distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(event_position, push_box_position)));
				if (distance <= 0.1f)
					++wet_event_count;
				//	範囲外のrainイベントは無視
			}
		}
		player_object->on_wet(wet_event_count * 100);

		if (test_heat)
		{
			test_heat->set_screen_rect({ 40, 40, (1200 - 40) * player_object->get_heat_rate(), 50 });
		}
		if (test_wet)
		{
			test_wet->set_screen_rect({ 40, 100, (1200 - 40) * player_object->get_wet_rate(), 50 });
		}
		if (test_speed)
		{
			DirectX::XMVECTOR Velocity = DirectX::XMLoadFloat3(&player_object->get_velocity());
			float speed_m_s = system_units::convert_unit_to_m(DirectX::XMVectorGetX(DirectX::XMVector3Length(Velocity)));
			float speed_km_h = speed_m_s * 3.6f;
			char text[64];
			sprintf_s(text, "速度: %.1f km/h", speed_km_h);
			test_speed->set_texts(text);
		}

		if (postprocess_material)
		{
			//	熱量に応じて赤く
			float heat_rate = math::saturate(player_object->get_heat_rate() * 2.0f - 1.0f);
			float wet_rate = player_object->get_wet_rate();
			postprocess_material->set_float("vg_intensity", heat_rate);
			postprocess_material->set_float("vg_radius", 0.4f);
			postprocess_material->set_float("vg_softness", 0.6f);
			postprocess_material->set_float3("vg_color", {1.0f, 0.0f, 0.0f});

			//	陽炎もだす
			postprocess_material->set_float("hz_intensity", heat_rate);
			postprocess_material->set_float("hz_scale", 20.0f);
			postprocess_material->set_float("hz_speed", 1.5f);

			//	水分量に応じて水滴を出す
			postprocess_material->set_float("wd_intensity", wet_rate);
			postprocess_material->set_float("wd_scale", 2.0f);
			postprocess_material->set_float("wd_wind_x", 0.0f);
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
