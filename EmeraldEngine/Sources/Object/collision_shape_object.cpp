#include "Object/collision_shape_object.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"

#include "Collision/sphere.h"
#include "Collision/capsule.h"
#include "Collision/box.h"

#include "Object/game_object_registry.h"
#include "Object/game_object_serializer.h"

collision_shape_object::collision_shape_object(std::string_view name)
: collision_placement_object(name)
{
	const float half = system_units::convert_m_to_unit(0.5f);
	shape_scale = { half, half, half };
}

collision_shape_object::~collision_shape_object()
{
}

void collision_shape_object::set_shape(shapes value)
{
	shape = value;
	rebuild_collisions();
}

void collision_shape_object::set_shape_scale(const math::float3& value)
{
	shape_scale = value;
	rebuild_collisions();
}

void collision_shape_object::create_collisions(std::vector<collision_object_ptr>& out_collisions)
{
	collision_object_ptr collision = std::make_shared<collision_object>();
	switch (shape)
	{
		case shapes::sphere:
			collision->set_bounding_object(std::make_shared<collision::bounding_sphere>());
			break;
		case shapes::capsule:
			collision->set_bounding_object(std::make_shared<collision::bounding_capsule>());
			break;
		case shapes::box:
			collision->set_bounding_object(std::make_shared<collision::bounding_box>());
			break;
	}
	collision->set_local_scale(shape_scale);
#if defined(USE_IMGUI)
	collision->set_name(get_name());
#endif	//	defined(USE_IMGUI)
	out_collisions.emplace_back(std::move(collision));
}

template<class Archive>
void collision_shape_object::serialize_fields(Archive& archive, std::uint32_t version)
{
	archive(CEREAL_NVP(shape), CEREAL_NVP(shape_scale));
}

void collision_shape_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);
	game_object_serializer::save_fields(archive, "collision_shape_object", *this);
}

void collision_shape_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);
	game_object_serializer::load_fields(archive, "collision_shape_object", *this);
}

#if defined(USE_IMGUI)
void collision_shape_object::imgui_render_core()
{
	base::imgui_render_core();

	static const char* shape_names[] = { "sphere", "capsule", "box" };
	int shape_index = static_cast<int>(shape);
	if (ImGui::Combo("shape", &shape_index, shape_names, IM_ARRAYSIZE(shape_names)))
		set_shape(static_cast<shapes>(shape_index));
	const float unit = system_units::convert_m_to_unit(1.0f);
	math::float3 meter = { shape_scale.x / unit, shape_scale.y / unit, shape_scale.z / unit };
	int component_count = 3;
	const char* label = "half extent [m]";
	switch (shape)
	{
		case shapes::sphere:
			component_count = 1;
			label = "radius [m]";
			break;
		case shapes::capsule:
			component_count = 2;
			label = "radius / length [m]";
			break;
		case shapes::box:
			break;
	}
	if (ImGui::DragScalarN(label, ImGuiDataType_Float, &meter.x, component_count, 0.01f))
		set_shape_scale({ meter.x * unit, meter.y * unit, meter.z * unit });
}
#endif	//	defined(USE_IMGUI)

GAME_OBJECT_REGISTER(collision_shape_object, "Collision");
