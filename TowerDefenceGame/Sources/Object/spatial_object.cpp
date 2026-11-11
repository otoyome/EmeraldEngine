#include "Object/spatial_object.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"

const std::shared_ptr<::spatial>& spatial_object::get_spatial() const
{
	return spatial;
}

const math::transformd& spatial_object::get_local() const
{
	return spatial->get_local();
}

void spatial_object::set_local(const math::transformd& local)
{
	spatial->set_local(local);
}

void spatial_object::set_local_scale(const math::vector3d& scale)
{
	spatial->set_local_scale(scale);
}

void spatial_object::set_local_rotation(const math::quaterniond& rotation)
{
	spatial->set_local_rotation(rotation);
}

void spatial_object::set_local_translation(const math::vector3d& translation)
{
	spatial->set_local_translation(translation);
}

math::rotatord spatial_object::get_local_euler() const
{
	return spatial->get_local_euler();
}

void spatial_object::set_local_euler(const math::rotatord& euler)
{
	spatial->set_local_euler(euler);
}

const math::transformd& spatial_object::get_world() const
{
	return spatial->get_world();
}

void spatial_object::set_world(const math::transformd& world)
{
	spatial->set_world(world);
}

math::vector3d spatial_object::get_world_scale() const
{
	return spatial->get_world_scale();
}

void spatial_object::set_world_scale(const math::vector3d& scale)
{
	spatial->set_world_scale(scale);
}

math::quaterniond spatial_object::get_world_rotation() const
{
	return spatial->get_world_rotation();
}

void spatial_object::set_world_rotation(const math::quaterniond& rotation)
{
	spatial->set_world_rotation(rotation);
}

math::rotatord spatial_object::get_world_euler() const
{
	return spatial->get_world_euler();
}

void spatial_object::set_world_euler(const math::rotatord& euler)
{
	spatial->set_world_euler(euler);
}

math::vector3d spatial_object::get_world_translation() const
{
	return spatial->get_world_translation();
}

void spatial_object::set_world_translation(const math::vector3d& translation)
{
	spatial->set_world_translation(translation);
}

bool spatial_object::attach(const std::shared_ptr<spatial_object>& parent, attach_rule rule)
{
	if (!parent)
		return false;

	return spatial->attach(parent->get_spatial(), rule);
}

void spatial_object::detach(attach_rule rule)
{
	spatial->detach(rule);
}

void spatial_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);
	archive(cereal::make_nvp("spatial", *spatial));
}

void spatial_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);
	archive(cereal::make_nvp("spatial", *spatial));
}

spatial_object::spatial_object(std::string_view name)
: game_object(name)
, spatial(std::make_shared<::spatial>())
{
}

spatial_object::spatial_object(std::string_view name, const math::transformd& local)
: game_object(name)
, spatial(std::make_shared<::spatial>(local))
{
}

spatial_object::~spatial_object()
{
	if (spatial)
	{
		spatial->detach();
	}
}

#if defined(USE_IMGUI)
void spatial_object::imgui_render_core()
{
	base::imgui_render_core();

	//	ƒ[ƒJƒ‹Žp¨‚Ì•ÒW
	//if (ImGui::TreeNode("spatial"))
	{
		const math::transformd& local = spatial->get_local();

		math::vector3d translation = local.get_translation();
		if (ImGui::DragScalarN("translation", ImGuiDataType_Double, &translation.x, 3, 0.1f))
			spatial->set_local_translation(translation);

		const math::rotatord euler = spatial->get_local_euler();
		double degrees[3] = { math::angle::to_degree(euler.x), math::angle::to_degree(euler.y), math::angle::to_degree(euler.z) };
		if (ImGui::DragScalarN("rotation", ImGuiDataType_Double, degrees, 3, 0.5f))
		{
			const math::rotatord edited(math::angle::to_radian(degrees[0]), math::angle::to_radian(degrees[1]), math::angle::to_radian(degrees[2]));
			spatial->set_local_euler(edited);
		}

		math::vector3d scale = local.get_scale();
		if (ImGui::DragScalarN("scale", ImGuiDataType_Double, &scale.x, 3, 0.01f))
			spatial->set_local_scale(scale);

	//	const math::vector3d world_translation = spatial->get_world().get_translation();
	//	ImGui::Text("world : %.3f, %.3f, %.3f", world_translation.x, world_translation.y, world_translation.z);
	//
	//	ImGui::TreePop();
	}
}
#endif	//	defined(USE_IMGUI)
