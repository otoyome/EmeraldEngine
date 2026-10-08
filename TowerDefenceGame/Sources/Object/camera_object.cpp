#include "object/camera_object.h"

camera_object::camera_object(std::string_view name)
: spatial_object(name)
{
	camera_manager::instance()->generate_camera(camera);
	camera->set_screen_rect({0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT});
}

camera_object::~camera_object()
{
}

void camera_object::post_update(float elapsed_time)
{
	sync_from_spatial();

	camera->set_rotation(get_spatial()->get_parent()
		? math::rotatorf(get_world().get_euler())
		: math::rotatorf(get_local_euler()));
	camera->set_position(get_world().get_translation());
}

void camera_object::post_render()
{
	camera->cache_previous_view_projection_matrix();
}

void camera_object::sync_from_spatial()
{
}

#if defined(USE_IMGUI)
void camera_object::imgui_render_core()
{
	base::imgui_render_core();
	sync_from_spatial();

	auto is_hybrid_projection = camera->is_flag(camera::flags::hybrid_projection);
	if (ImGui::Checkbox("hybrid_projection", &is_hybrid_projection))
	{
		camera->set_flag(camera::flags::hybrid_projection, is_hybrid_projection);
	}

	if (is_hybrid_projection)
	{
		auto hybrid_rate = camera->get_hybrid_rate();
		ImGui::SliderFloat("hybrid_rate", &hybrid_rate, 0, 1);
		camera->set_hybrid_rate(hybrid_rate);
	}

	auto fov_y = math::angle::to_degree(camera->get_fov_y());
	ImGui::DragFloat("fov_y", &fov_y, 0.01f, 0.01f);
	camera->set_fov_y(math::angle::to_radian(fov_y));

	auto clip_distance = camera->get_clip_distance();
	ImGui::DragFloat2("clip_distance", &clip_distance.x, 0.01f);
	camera->set_clip_distance(clip_distance);
}
#endif	//	defined(USE_IMGUI)
