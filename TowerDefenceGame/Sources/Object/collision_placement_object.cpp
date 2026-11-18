#include "Object/collision_placement_object.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"

#include "Object/engine_context.h"
#include "Object/object_manager.h"
#include "Object/game_object_serializer.h"

collision_placement_object::collision_placement_object(std::string_view name)
: spatial_object(name)
{
}

collision_placement_object::~collision_placement_object()
{
}

void collision_placement_object::on_initialize()
{
	base::on_initialize();
	build_collisions();
}

void collision_placement_object::on_finalize()
{
	release_collisions();
	base::on_finalize();
}

void collision_placement_object::fixed_update(float elapsed_time)
{
	base::fixed_update(elapsed_time);

	if (get_mobility() == mobilities::movable)
		sync_transform();
}

void collision_placement_object::pre_render()
{
	base::pre_render();

#if defined(USE_IMGUI)
	if (has_context() && get_context().is_editing())
		sync_transform();
#endif	//	defined(USE_IMGUI)
}

void collision_placement_object::set_filter_name(std::string_view name)
{
	filter_name = name;
	for (auto& collision : collisions)
		collision->set_filter(filter_name);
}

void collision_placement_object::rebuild_collisions()
{
	if (!has_context())
		return;
	release_collisions();
	build_collisions();
}

void collision_placement_object::build_collisions()
{
	std::vector<collision_object_ptr> created;
	create_collisions(created);

	object_manager& objects = get_context().get_objects();
	for (auto& collision : created)
	{
		collision->set_filter(filter_name);
		collision->set_body_type(collision_object::body_types::static_body);
		objects.register_object(collision);
		collisions.emplace_back(std::move(collision));
	}
	sync_transform();
}

void collision_placement_object::release_collisions()
{
	if (has_context())
	{
		object_manager& objects = get_context().get_objects();
		for (auto& collision : collisions)
			objects.unregister_object(collision);
	}
	collisions.clear();
	release_resources();
}

void collision_placement_object::sync_transform()
{
	const math::rigid_transformd transform = get_world().to_rigid();
	for (auto& collision : collisions)
		collision->set_transform(transform);
}

template<class Archive>
void collision_placement_object::serialize_fields(Archive& archive, std::uint32_t version)
{
	switch (version)
	{
		case 0:
		case 1:
		{
			bool detect_only = false;
			archive(CEREAL_NVP(filter_name), CEREAL_NVP(detect_only));
			break;
		}
		case 2:
		{ 
			archive(CEREAL_NVP(filter_name));
			break;
		}
	}
}

void collision_placement_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);
	game_object_serializer::save_fields(archive, "collision_placement_object", *this);
}

void collision_placement_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);
	game_object_serializer::load_fields(archive, "collision_placement_object", *this);
}

#if defined(USE_IMGUI)
void collision_placement_object::imgui_render_core()
{
	base::imgui_render_core();

	char buffer[64] = {};
	filter_name.copy(buffer, sizeof(buffer) - 1);
	if (ImGui::InputText("filter", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
		set_filter_name(buffer);
	ImGui::Text("collisions : %d", static_cast<int>(collisions.size()));
}
#endif	//	defined(USE_IMGUI)
