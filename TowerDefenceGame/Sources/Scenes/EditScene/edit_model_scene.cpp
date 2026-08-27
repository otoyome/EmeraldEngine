#include "scenes/editscene/edit_model_scene.h"

#include "model_instance.h"
#include "camera.h"

#include "object/light_object.h"

#include "object/model_object.h"

#include "custom_pipeline_state.h"

#include "effect_instance.h"

#if defined(USE_IMGUI)

edit_model_scene::edit_model_scene()
: scene_base()
{
}

edit_model_scene::~edit_model_scene()
{
}

bool edit_model_scene::initialize_core()
{
	//	メインカメラ生成
	{
		main_camera = std::make_shared<camera_object>("メインカメラ");
		main_camera->get_camera()->set_fov_y(DirectX::XMConvertToRadians(15.0f));
		register_object(main_camera);
		mouse::instance()->set_center_lock(false);
		mouse::instance()->set_cursol_hide(false);
	}

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
		load_model_path = "Assets/Resources/Models/Umber/Umber.emo";
		convert_option_data.kind = convert_option_data::kinds::emo;
	//	load_model_path = "Assets/Resources/Models/Umber/Umber.glb";
	//	convert_option_data.kind = convert_option_data::kinds::gltf;
	//	load_model_path = "Assets/Resources/Models/Cartoon_City/Cartoon_City.emo";
	//	convert_option_data.kind = convert_option_data::kinds::emo;
	}

	//	デフォルトでポストプロセスを適応する
	if (pipeline_state_manager* instance = pipeline_state_manager::instance())
	{
		post_process_volume = std::make_shared<post_process_volume_object>("ポストプロセスボリューム");
		post_process_volume->set_flag(post_process_volume::flags::unbind, true);
		{
			auto bloom_constant = post_process_volume->get_volume()->get_bloom_constant();
			auto lens_ghosts_constant = post_process_volume->get_volume()->get_lens_ghosts_constant();
			auto depth_of_field_constant = post_process_volume->get_volume()->get_depth_of_field_constant();
			auto screen_space_reflection_constant = post_process_volume->get_volume()->get_screen_space_reflection_constant();
			auto motion_blur_constant = post_process_volume->get_volume()->get_motion_blur_constant();
			auto exposure_compensation_constant = post_process_volume->get_volume()->get_exposure_compensation_constant();
			bloom_constant.set_active(false);
			lens_ghosts_constant.set_active(false);
			depth_of_field_constant.set_active(false);
			screen_space_reflection_constant.set_active(false);
			motion_blur_constant.set_active(false);
			exposure_compensation_constant.set_active(false);
			post_process_volume->get_volume()->set_bloom_constant(bloom_constant);
			post_process_volume->get_volume()->set_lens_ghosts_constant(lens_ghosts_constant);
			post_process_volume->get_volume()->set_depth_of_field_constant(depth_of_field_constant);
			post_process_volume->get_volume()->set_screen_space_reflection_constant(screen_space_reflection_constant);
			post_process_volume->get_volume()->set_motion_blur_constant(motion_blur_constant);
			post_process_volume->get_volume()->set_exposure_compensation_constant(exposure_compensation_constant);
		}
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

	return initialize_edit_core();
}

bool edit_model_scene::uninitialize_core()
{
	object_manager::instance()->unregister_object(edit_model);
	object_manager::instance()->unregister_object(post_process_volume);
	return uninitialize_edit_core();
}

void edit_model_scene::update_core(float elapsed_time)
{
	if (convert_option_data.kind != convert_option_data::kinds::none)
	{
		if (edit_model)
		{
			on_model_release();
		}

		//	モデルデータ生成
		{
			bool is_load = false;
			std::shared_ptr<model_resource> resource = std::make_shared<model_resource>();
			switch (convert_option_data.kind)
			{
				case convert_option_data::kinds::mmd:
				{
					data_converter::mmd::convert_model_data data;
					convert_option_data.mmd.file_path = load_model_path;
					is_load = data_converter::mmd::convert(*resource, convert_option_data.mmd);
					break;
				}
				case convert_option_data::kinds::gltf:
				{
					convert_option_data.gltf.file_path = load_model_path;
					is_load = data_converter::gltf::convert(*resource, convert_option_data.gltf);
					break;
				}
				case convert_option_data::kinds::emo:
				{
					is_load = model_resource_manager::instance()->load(resource, load_model_path);
					break;
				}
			}
			if (is_load && resource)
			{
				edit_model = std::make_shared<model_object>(load_model_path, resource);
			}
		}

		if (edit_model)
			on_model_setup();

		convert_option_data.kind = convert_option_data::kinds::none;
		load_model_path.clear();
	}

	update_edit_core(elapsed_time);
}

void edit_model_scene::imgui_render_core()
{
	//	ライトのプリセット設定
	imgui_render_preset_light();

	//	ロードウィンドウ
	imgui_model_loader_window();

	//	エディターウィンドウ関連
	imgui_render_edit_core();
}

void edit_model_scene::save_model()
{
	if (!edit_model)
		return;

	//	EMO形式で吐き出す
	auto& resource = edit_model->get_model_instance()->get_resource();

	//	マテリアルの上書き
	auto& resource_materials = resource->get_materials_for_editor();
	auto& instance_materials = edit_model->get_model_instance()->get_materials();
	for (UINT material_index = 0; material_index < resource_materials.size(); ++material_index)
	{
		resource_materials[material_index] = instance_materials[material_index];
	}

	//	保存
	resource->serialize(resource->get_file_path());
}

#endif	//	defined(USE_IMGUI)
