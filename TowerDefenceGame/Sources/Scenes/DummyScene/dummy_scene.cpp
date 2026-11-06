#include "scenes/DummyScene/dummy_scene.h"
#include "texture_resource_utility.h"
#include "camera.h"

#include "object/light_object.h"
#include "object/model_object.h"
#include "object/camera_object.h"
#include "object/post_process_volume_object.h"

#include "custom_pipeline_state.h"

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
		main_camera->get_camera()->get_parameters().fov_y = math::angle::to_radian(45.0f);
	//	main_camera->get_camera()->set_flag(camera::flags::hybrid_projection, true);
	//	main_camera->get_camera()->set_hybrid_rate(0.3f);	//	透視投影

		main_camera->set_world_translation(math::vector3d(0.0, 10.0, 100.0));
		main_camera->set_world_euler(math::rotatord(0.1, math::constants<double>::pi, 0.0));
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
			ibl_texture = texture_resource_utility::load_texture("Assets/Resources/Textures/hilly_terrain_01_puresky_4k/diffuse_iem.etx");
			sky_light->set_diffuse_ibl_texture(ibl_texture);
			ibl_texture = texture_resource_utility::load_texture("Assets/Resources/Textures/hilly_terrain_01_puresky_4k/specular_pmrem.etx");
			sky_light->set_specular_ibl_texture(ibl_texture);
			ibl_texture = texture_resource_utility::load_texture("Assets/Resources/Textures/lut_ggx.etx");
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
			obj->set_world_translation(math::vector3d(0.0, 25.0, 0.0));
			obj->set_world_scale(math::vector3d(25.0, 25.0, 25.0));
		}
		if (auto obj = load_model("Assets/Resources/Models/Cartoon_City/Cartoon_City.emo"))
		{
			obj->set_world_translation(math::vector3d(22.0, 0.0, 0.0));
			obj->set_world_scale(math::vector3d(3.0, 3.0, 3.0));
		}
	}

	//	デフォルトでポストプロセスを適応する
	if (pipeline_state_manager* instance = pipeline_state_manager::instance())
	{
		auto post_process_volume = std::make_shared<post_process_volume_object>("ポストプロセスボリューム");
		post_process_volume->set_flag(post_process_volume::flags::unbind, true);
		register_object(post_process_volume);

		//	マテリアル登録
		static constexpr std::string_view texts_list[][2] =
		{
			{
				"Assets/Resources/Shaders/PostProcess/color_filter_before_dof.hlsl",
				"被写界深度前ポストプロセスマテリアル",
			},
			{
				"Assets/Resources/Shaders/PostProcess/color_filter_after_dof.hlsl",
				"被写界深度後ポストプロセスマテリアル",
			},
			{
				"Assets/Resources/Shaders/PostProcess/color_filter_after_dof_translucency.hlsl",
				"被写界深度後半透明描画ポストプロセスマテリアル",
			},
			{
				"Assets/Resources/Shaders/PostProcess/color_filter_before_bloom.hlsl",
				"ブルーム前ポストプロセスマテリアル",
			},
			{
				"Assets/Resources/Shaders/PostProcess/color_filter_after_tone_mapping.hlsl",
				"トーンマッピング後ポストプロセスマテリアル",
			}
		};
		material_resource::blendable_locations path_list[] =
		{
			material_resource::blendable_locations::before_dof,
			material_resource::blendable_locations::after_dof,
			material_resource::blendable_locations::after_dof_translucency,
			material_resource::blendable_locations::before_bloom,
			material_resource::blendable_locations::after_tone_mapping,
		};
		for (UINT index = 0; index < ARRAYSIZE(path_list); ++index)
		{
			std::shared_ptr<material_resource> post_process_material = std::make_shared<material_resource>();
			post_process_material->set_name(texts_list[index][1]);
			post_process_material->set_domain(material_resource::domains::post_process);
			post_process_material->set_flag(material_resource::flags::two_sided, true);
			post_process_material->set_blend_mode(material_resource::blend_modes::opaque);
			post_process_material->set_blendable_location(path_list[index]);
			post_process_material->set_shader_path(texts_list[index][0]);

			//	マテリアル登録
			post_process_volume->get_volume()->register_postprocess_materials(post_process_material);
		}
	}

	return true;
}

void dummy_scene::update_core(float elapsed_time)
{
}

#if defined(USE_IMGUI)
void dummy_scene::imgui_render_core()
{
}
#endif	//	defined(USE_IMGUI)
