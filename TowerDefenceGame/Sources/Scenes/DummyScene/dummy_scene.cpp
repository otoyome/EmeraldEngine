#include "scenes/DummyScene/dummy_scene.h"
#include "camera.h"

#include "object/light_object.h"
#include "object/model_object.h"
#include "object/camera_object.h"
#include "object/post_process_volume_object.h"

#include "custom_pipeline_state.h"

#if defined(USE_IMGUI)
#include "Editor/editor_camera.h"
#endif // defined(USE_IMGUI)

dummy_scene::dummy_scene()
: scene_base()
{
}

dummy_scene::~dummy_scene()
{
}

bool dummy_scene::initialize_core()
{
	//	メインカメラ生成
	std::shared_ptr<camera_object> main_camera;
	{
		main_camera = std::make_shared<camera_object>("メインカメラ");
		main_camera->get_camera()->set_fov_y(DirectX::XMConvertToRadians(45.0f));
	//	main_camera->get_camera()->set_flag(camera::flags::hybrid_projection, true);
	//	main_camera->get_camera()->set_hybrid_rate(0.3f);	//	透視投影

		main_camera->set_position({0, 10, 100.0f});
		main_camera->set_rotation({0.1f, DirectX::XM_PI, 0});
		register_object(main_camera);
		mouse::instance()->set_center_lock(false);
		mouse::instance()->set_cursol_hide(false);
	}

#if defined(USE_IMGUI)
	//	エディターカメラ生成
	{
		auto camera = std::make_shared<editor_camera>(main_camera);
		camera->set_position(system_units::convert_m_to_unit({ 0, 1, 8 }));
		camera->set_rotation({ 0, DirectX::XM_PI, 0 });
		camera->set_rotation_speed(camera->get_rotation_speed() * 0.25f);
		camera->set_move_speed(camera->get_move_speed() * 0.25f);
		register_object(camera);
	}
#endif // defined(USE_IMGUI)

	//	ライト生成
	{
		//	平行光源生成
		{
			auto light = std::make_shared<directional_light_object>("平行光源");
			auto color = light->get_color();
			color.w = 1.0f;
			light->set_color(color);
			light->set_flag(light::flags::use_shadow, false);
			register_object(light);
		}

		//	間接光生成
		{
			auto sky_light = std::make_shared<sky_light_object>("スカイライト");
			auto color = sky_light->get_color();
			color.w = 1.0f;
			sky_light->set_color(color);

			texture_ptr ibl_texture;
			texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/hilly_terrain_01_puresky_4k/diffuse_iem.dds");
			sky_light->set_diffuse_ibl_texture(ibl_texture);
			texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/hilly_terrain_01_puresky_4k/specular_pmrem.dds");
			sky_light->set_specular_ibl_texture(ibl_texture);
			texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/lut_ggx.dds");
			sky_light->set_lut_ggx_texture(ibl_texture);

			register_object(sky_light);
		}
	}

	//	モデル生成
	{
		auto load_model = [&](std::string_view file_path)
		{
			std::shared_ptr<model_object> obj;
			model_resource_ptr resource;
			if (model_resource_manager::instance()->load(resource, file_path))
			{
				obj = std::make_shared<model_object>(file_path, resource);
				register_object(obj);
			}
			return obj;
		};
		if (auto obj = load_model("Assets/Resources/Models/DamagedHelmet/glTF/DamagedHelmet.emo"))
		{
			obj->set_position({0, 25, 0});
			obj->set_scale({25,25,25});
		}
		if (auto obj = load_model("Assets/Resources/Models/Cartoon_City/Cartoon_City.emo"))
		{
			obj->set_position({22, 0, 0});
			obj->set_scale({3,3,3});
		}
	}

	//	デフォルトでポストプロセスを適応する
	if (pipeline_state_manager* instance = pipeline_state_manager::instance())
	{
		auto post_process_volume = std::make_shared<post_process_volume_object>("ポストプロセスボリューム");
		post_process_volume->set_flag(post_process_volume::flags::unbind, true);
		register_object(post_process_volume);

		//	マテリアル登録
		static constexpr std::string_view texts_list[][4] =
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
		material::blendable_locations path_list[] =
		{
			material::blendable_locations::before_dof,
			material::blendable_locations::after_dof,
			material::blendable_locations::after_dof_translucency,
			material::blendable_locations::before_bloom,
			material::blendable_locations::after_tone_mapping,
		};
		for (UINT index = 0; index < ARRAYSIZE(path_list); ++index)
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>(texts_list[index][0], &pso);
			pso->set_domain(pipeline_state_base::domains::post_process);

			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = texts_list[index][1];
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
				desc.file_path = texts_list[index][2];
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}

			std::shared_ptr<material> post_process_material = std::make_shared<material>();
			post_process_material->set_name(texts_list[index][3]);
			post_process_material->set_domain(material::domains::post_process);
			post_process_material->set_flag(material::flags::two_sided, true);
			post_process_material->set_blend_mode(material::blend_modes::opaque);
			post_process_material->set_blendable_location(path_list[index]);
			post_process_material->set_pipeline_state(texts_list[index][0]);

			//	マテリアル登録
			post_process_volume->get_volume()->register_postprocess_materials(post_process_material);
		}
	}

	return true;
}

void dummy_scene::update_core(float elapsed_time)
{
}

void dummy_scene::imgui_render_core()
{
}
