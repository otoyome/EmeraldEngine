#include "object/gameobjects/MiniGame/minigame000_stage_object.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"

#include "Object/game_object_registry.h"
#include "Object/game_object_serializer.h"
#include "Object/Collision/collision_filter.h"
#include "Object/Collision/collision_world.h"
#include "Object/engine_context.h"
#include "Scenes/scene_manager.h"
#include "Scenes/scene_instance.h"
#if defined(USE_IMGUI)
#include "Object/object_reference_field.h"
#endif	//	defined(USE_IMGUI)

namespace
{
	//	ゴミ/泡のパーティクルを持つノード名
	constexpr std::string_view trash_node_name = "Dust";
	constexpr std::string_view bubble_node_name = "Bubble";
}

template<class Archive>
void minigame000::stage_settings::serialize(Archive& archive, std::uint32_t version)
{
	archive(CEREAL_NVP(world_gravity),
			CEREAL_NVP(world_air_density));
}

template void minigame000::stage_settings::serialize<cereal::JSONOutputArchive>(cereal::JSONOutputArchive& archive, std::uint32_t version);
template void minigame000::stage_settings::serialize<cereal::JSONInputArchive>(cereal::JSONInputArchive& archive, std::uint32_t version);

////////////////////////////////////////////////////////////////////////////////////
minigame000::minigame_stage_object::minigame_stage_object(std::string_view name)
: base(name)
, model()
, collision()
, state(states::setup_wait)
, effects()
, trash_effects()
, bubble_effects()
, trash_max(0)
{
}

minigame000::minigame_stage_object::~minigame_stage_object()
{
}

void minigame000::minigame_stage_object::on_initialize()
{
	base::on_initialize();
}

void minigame000::minigame_stage_object::on_finalize()
{
	base::on_finalize();
}

void minigame000::minigame_stage_object::fixed_update(float elapsed_time)
{
	switch (state)
	{
		case states::setup_wait:
		{
			break;
		}
		case states::collect_effects:
		{
			effects.clear();
			trash_effects.clear();
			bubble_effects.clear();
			trash_max = 0;

			//	シーン全体から探す
			for (const std::pair<std::string, std::unique_ptr<scene_instance>>& scene : get_context().get_scenes().get_scenes())
			{
				if (!scene.second)
					continue;

				for (const std::weak_ptr<game_object>& weak_object : scene.second->get_objects())
				{
					game_object_ptr object = weak_object.lock();
					if (object && object->check_type(effect_object::type_name()))
						effects.emplace_back(std::static_pointer_cast<effect_object>(object));
				}
			}
			state = states::effects_load_wait;
			break;
		}
		case states::effects_load_wait:
		{
			//	エフェクト読込待機
			bool is_all_loaded = true;
			for (const std::weak_ptr<effect_object>& weak_effect : effects)
			{
				const std::shared_ptr<effect_object> effect = weak_effect.lock();
				if (effect && !effect->is_loaded())
				{
					is_all_loaded = false;
					break;
				}
			}
			if (!is_all_loaded)
				break;

			state = states::classify_effects;
			break;
		}
		case states::classify_effects:
		{
			trash_effects.clear();
			bubble_effects.clear();
			trash_max = 0;

			for (const std::weak_ptr<effect_object>& weak_effect : effects)
			{
				const std::shared_ptr<effect_object> effect = weak_effect.lock();
				if (!effect || !effect->get_loaded_resource())
					continue;

				//	ノード名で判断
				const effect_resource_ptr& resource = effect->get_loaded_resource();
				const std::vector<effect_resource::node>& nodes = resource->get_nodes();
				bool is_trash = false;
				bool is_bubble = false;
				for (const effect_resource::particle& particle : resource->get_particles())
				{
					if (particle.node_index >= nodes.size())
						continue;

					const std::string_view node_name = nodes[particle.node_index].name.view();
					if (node_name == trash_node_name)
					{
						is_trash = true;
						if (const ::particle::item::renderer* renderer = particle.particle.get_item<::particle::item::renderer>())
							trash_max += static_cast<int>(renderer->total_particle_count);
					}
					else if (node_name == bubble_node_name)
					{
						is_bubble = true;
					}
				}

				if (is_trash)
					trash_effects.emplace_back(effect);
				if (is_bubble)
					bubble_effects.emplace_back(effect);
			}
			state = states::idle;
			break;
		}
		case states::idle:
		{
			break;
		}
	}
}

#if defined(USE_IMGUI)
void minigame000::minigame_stage_object::imgui_render_core()
{
	base::imgui_render_core();

	object_reference_field::imgui_combo("model", get_context(), model);
	object_reference_field::imgui_combo("collision", get_context(), collision);

	ImGui::SeparatorText("state");
	ImGui::Text("completed : %s", is_completed() ? "true" : "false");
	ImGui::Text("effects : %zu", effects.size());
	ImGui::Text("trash effects : %zu", trash_effects.size());
	ImGui::Text("bubble effects : %zu", bubble_effects.size());
	ImGui::Text("trash max : %d", trash_max);
}
#endif	//	defined(USE_IMGUI)

template<class Archive>
void minigame000::minigame_stage_object::serialize_fields(Archive& archive, std::uint32_t version)
{
	switch (version)
	{
		case 0:
		case 1:
		{
			game_object_serializer::serialize_object_reference(archive, "model", model);
			game_object_serializer::serialize_object_reference(archive, "collision", collision);
			archive(CEREAL_NVP(setting));
			break;
		}
	}
}
void minigame000::minigame_stage_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);
	game_object_serializer::save_fields(archive, "minigame_stage_object", *this);
}

void minigame000::minigame_stage_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);
	game_object_serializer::load_fields(archive, "minigame_stage_object", *this);
}

namespace minigame000
{
	GAME_OBJECT_REGISTER(minigame_stage_object, "minigame000/Roomba");
}	//	namespace minigame000
