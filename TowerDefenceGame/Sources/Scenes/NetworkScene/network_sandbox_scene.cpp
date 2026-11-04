#include "scenes/NetworkScene/network_sandbox_scene.h"
#include "camera.h"

#include "object/light_object.h"
#include "object/camera_object.h"

network_sandbox_scene::network_sandbox_scene()
: scene_base()
{
}

network_sandbox_scene::~network_sandbox_scene()
{
}

bool network_sandbox_scene::initialize_core()
{
	//	メインカメラ生成
	{
		auto main_camera = std::make_shared<camera_object>("メインカメラ");
		main_camera->get_camera()->get_parameters().fov_y = math::angle::to_radian(45.0f);
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
			texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/hilly_terrain_01_puresky_4k/diffuse_iem.dds");
			sky_light->set_diffuse_ibl_texture(ibl_texture);
			texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/hilly_terrain_01_puresky_4k/specular_pmrem.dds");
			sky_light->set_specular_ibl_texture(ibl_texture);
			texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/lut_ggx.dds");
			sky_light->set_lut_ggx_texture(ibl_texture);

			register_object(sky_light);
		}
	}

	return true;
}

void network_sandbox_scene::update_core(float elapsed_time)
{
}

#if defined(USE_IMGUI)
void network_sandbox_scene::imgui_render_core()
{
}
#endif	//	defined(USE_IMGUI)
