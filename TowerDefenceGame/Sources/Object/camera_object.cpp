#include "object/camera_object.h"

camera_object::camera_object(std::string_view name)
: spatial_object(name)
{
	camera = std::make_shared<::camera>();

	view_order = view_order_counter.fetch_add(1);
}

camera_object::~camera_object()
{
}

void camera_object::on_active()
{
	base::on_active();

	view_order = view_order_counter.fetch_add(1);
}

void camera_object::on_deactive()
{
	base::on_deactive();
}

void camera_object::post_update(float elapsed_time)
{
	sync_from_spatial();

	auto& parameters = camera->get_parameters();
	parameters.rotation = get_spatial()->get_parent()
		? math::rotatorf(get_world().get_euler())
		: math::rotatorf(get_local_euler());
	parameters.position = get_world().get_translation();
}

void camera_object::sync_from_spatial()
{
}

#if defined(USE_IMGUI)
void camera_object::imgui_render_core()
{
	base::imgui_render_core();
	sync_from_spatial();

	auto& parameters = camera->get_parameters();

	ImGui::Checkbox("hybrid_projection", &parameters.hybrid_projection);

	if (parameters.hybrid_projection)
	{
		ImGui::SliderFloat("hybrid_rate", &parameters.hybrid_rate, 0, 1);
	}

	auto fov_y = math::angle::to_degree(parameters.fov_y);
	ImGui::DragFloat("fov_y", &fov_y, 0.01f, 0.01f);
	parameters.fov_y = math::angle::to_radian(fov_y);

	ImGui::DragFloat2("clip_distance", &parameters.clip_distance.x, 0.01f);
}
#endif	//	defined(USE_IMGUI)
