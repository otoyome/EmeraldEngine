#include "Object/spatial_object.h"
#include "Object/game_object_registry.h"
#include "Object/game_object_serializer.h"
#include "Object/engine_context.h"

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
	if (can_move())
		spatial->set_local(local);
}

void spatial_object::set_local_scale(const math::vector3d& scale)
{
	if (can_move())
		spatial->set_local_scale(scale);
}

void spatial_object::set_local_rotation(const math::quaterniond& rotation)
{
	if (can_move())
		spatial->set_local_rotation(rotation);
}

void spatial_object::set_local_translation(const math::vector3d& translation)
{
	if (can_move())
		spatial->set_local_translation(translation);
}

math::rotatord spatial_object::get_local_euler() const
{
	return spatial->get_local_euler();
}

void spatial_object::set_local_euler(const math::rotatord& euler)
{
	if (can_move())
		spatial->set_local_euler(euler);
}

const math::transformd& spatial_object::get_world() const
{
	return spatial->get_world();
}

void spatial_object::set_world(const math::transformd& world)
{
	if (can_move())
		spatial->set_world(world);
}

math::vector3d spatial_object::get_world_scale() const
{
	return spatial->get_world_scale();
}

void spatial_object::set_world_scale(const math::vector3d& scale)
{
	if (can_move())
		spatial->set_world_scale(scale);
}

math::quaterniond spatial_object::get_world_rotation() const
{
	return spatial->get_world_rotation();
}

void spatial_object::set_world_rotation(const math::quaterniond& rotation)
{
	if (can_move())
		spatial->set_world_rotation(rotation);
}

math::rotatord spatial_object::get_world_euler() const
{
	return spatial->get_world_euler();
}

void spatial_object::set_world_euler(const math::rotatord& euler)
{
	if (can_move())
		spatial->set_world_euler(euler);
}

math::vector3d spatial_object::get_world_translation() const
{
	return spatial->get_world_translation();
}

void spatial_object::set_world_translation(const math::vector3d& translation)
{
	if (can_move())
		spatial->set_world_translation(translation);
}

bool spatial_object::set_mobility(mobilities value)
{
	//	movable の親に fixed の子は付けられない
	if (value == mobilities::fixed)
	{
		auto parent = parent_object.lock();
		if (parent && parent->get_mobility() == mobilities::movable)
		{
			DEBUG_WARNING_CATEGORY(logger::categorys::system, "spatial_object fixed object cannot be a child of movable object[%s]", std::string(get_name()).c_str());
			return false;
		}
	}
	else
	{
		for (const auto& weak_child : child_objects)
		{
			auto child = weak_child.lock();
			if (child && child->get_mobility() == mobilities::fixed)
			{
				DEBUG_WARNING_CATEGORY(logger::categorys::system, "spatial_object movable object cannot have fixed children[%s]", std::string(get_name()).c_str());
				return false;
			}
		}
	}
	mobility = value;
	return true;
}

bool spatial_object::can_move() const
{
	//	fixed でも登録前と編集中は動かせる
	if (mobility == mobilities::movable || !has_context())
		return true;
#if defined(USE_IMGUI)
	if (get_context().is_editing())
		return true;
#endif	//	defined(USE_IMGUI)

	DEBUG_WARNING_CATEGORY(logger::categorys::system, "spatial_object fixed object cannot move[%s]", std::string(get_name()).c_str());
	return false;
}

bool spatial_object::attach(const std::shared_ptr<spatial_object>& parent, attach_rule rule)
{
	if (!parent)
		return false;

	//	movable の親に fixed の子は付けられない
	if (mobility == mobilities::fixed && parent->get_mobility() == mobilities::movable)
	{
		DEBUG_WARNING_CATEGORY(logger::categorys::system, "spatial_object fixed object cannot be attached to movable object[%s] -> [%s]", std::string(get_name()).c_str(), std::string(parent->get_name()).c_str());
		return false;
	}

	if (!spatial->attach(parent->get_spatial(), rule))
		return false;

	detach_record();
	parent_object = parent;
	if (auto self = std::dynamic_pointer_cast<spatial_object>(weak_from_this().lock()))
		parent->child_objects.emplace_back(self);
	return true;
}

void spatial_object::detach(attach_rule rule)
{
	spatial->detach(rule);
	detach_record();
}

bool spatial_object::attach_object(const std::shared_ptr<game_object>& child, const std::shared_ptr<game_object>& parent)
{
	const std::shared_ptr<spatial_object> child_object = std::dynamic_pointer_cast<spatial_object>(child);
	if (!child_object)
		return false;
	if (!parent)
	{
		child_object->detach();
		return true;
	}

	const std::shared_ptr<spatial_object> parent_object = std::dynamic_pointer_cast<spatial_object>(parent);
	return parent_object && child_object->attach(parent_object);
}

void spatial_object::detach_record()
{
	if (auto parent = parent_object.lock())
	{
		std::erase_if(parent->child_objects, [this](const std::weak_ptr<spatial_object>& child)
		{
			auto locked = child.lock();
			return !locked || locked.get() == this;
		});
	}
	parent_object.reset();
}

template<class Archive>
void spatial_object::serialize_fields(Archive& archive, std::uint32_t version)
{
	archive(cereal::make_nvp("spatial", *spatial), CEREAL_NVP(mobility));
}

void spatial_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);
	game_object_serializer::save_fields(archive, "spatial_object", *this);
}

void spatial_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);
	game_object_serializer::load_fields(archive, "spatial_object", *this);
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
	detach_record();
}

#if defined(USE_IMGUI)
void spatial_object::imgui_render_core()
{
	base::imgui_render_core();

	//	ローカル姿勢の編集
	//if (ImGui::TreeNode("spatial"))
	{
		//	動き方
		static const char* mobility_names[] = { "fixed", "movable" };
		int mobility_index = static_cast<int>(mobility);
		if (ImGui::Combo("mobility", &mobility_index, mobility_names, IM_ARRAYSIZE(mobility_names)))
			set_mobility(static_cast<mobilities>(mobility_index));

		const math::transformd& local = spatial->get_local();

		math::vector3d translation = local.get_translation();
		if (ImGui::DragScalarN("translation", ImGuiDataType_Double, &translation.x, 3, 0.1f))
			set_local_translation(translation);

		const math::rotatord euler = spatial->get_local_euler();
		double degrees[3] = { math::angle::to_degree(euler.x), math::angle::to_degree(euler.y), math::angle::to_degree(euler.z) };
		if (ImGui::DragScalarN("rotation", ImGuiDataType_Double, degrees, 3, 0.5f))
		{
			const math::rotatord edited(math::angle::to_radian(degrees[0]), math::angle::to_radian(degrees[1]), math::angle::to_radian(degrees[2]));
			set_local_euler(edited);
		}

		math::vector3d scale = local.get_scale();
		if (ImGui::DragScalarN("scale", ImGuiDataType_Double, &scale.x, 3, 0.01f))
			set_local_scale(scale);

	//	const math::vector3d world_translation = spatial->get_world().get_translation();
	//	ImGui::Text("world : %.3f, %.3f, %.3f", world_translation.x, world_translation.y, world_translation.z);
	//
	//	ImGui::TreePop();
	}
}
#endif	//	defined(USE_IMGUI)

