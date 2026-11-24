#include "Object/Graphics/effect_object.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/common.hpp"

#include "Math/math_serialize.h"
#include "Object/engine_context.h"
#include "Object/game_object_serializer.h"
#include "Object/object_manager.h"
#include "Resource/resource_manager.h"

#if defined(USE_IMGUI)
#include "Object/asset_reference_field.h"
#include "Object/object_reference_field.h"
#endif	//	defined(USE_IMGUI)

namespace
{
	//	インスタンスが無い時に返す空のイベント
	const std::vector<effect_instance::particle_event_info> empty_particle_events;
}

effect_object::effect_object(std::string_view name)
: base(name)
, effect()
, auto_play(true)
, loop(true)
, attract_type(position_types::self)
, attract_target()
, attract_position()
, event_type(position_types::self)
, event_target()
, event_position()
, spawn_stop(false)
, play_requested(false)
, loaded(false)
, load_operation()
, loaded_resource()
, instance()
{
}

effect_object::~effect_object()
{
}

void effect_object::set_effect(const resource::asset_reference<effect_resource>& value)
{
	if (value.uuid == effect.uuid)
		return;

	const bool playing = is_playing();
	release_instance();
	effect = value;
	if (has_context())
		request_load();
	play_requested = playing && !effect.is_empty();
}

void effect_object::set_loop(bool value)
{
	loop = value;
	if (instance)
		instance->set_flag(effect_instance::flags::animation_loop, value);
}

void effect_object::play()
{
	//	読込中なら読み終わってから再生する
	if (!loaded_resource)
	{
		play_requested = !effect.is_empty();
		return;
	}

	//	時間を戻すとパーティクルシステム全体が巻き戻るので、インスタンスごと作り直す
	play_requested = false;
	release_instance();
	create_instance();
}

void effect_object::stop()
{
	play_requested = false;
	release_instance();
}

void effect_object::set_spawn_stop(bool value)
{
	spawn_stop = value;
	if (instance)
		instance->set_flag(effect_instance::flags::spawn_stop, value);
}

const std::vector<effect_instance::particle_event_info>& effect_object::get_particle_events() const
{
	return instance ? instance->get_particle_events() : empty_particle_events;
}

std::string_view effect_object::get_particle_node_name(size_t particle_index) const
{
	if (!instance)
		return std::string_view();

	std::vector<effect_instance::particle>& particles = instance->get_particles();
	if (particle_index >= particles.size())
		return std::string_view();

	std::vector<effect_instance::node>& nodes = instance->get_nodes();
	const size_t node_index = particles[particle_index].node_index;
	if (node_index >= nodes.size())
		return std::string_view();
	return nodes[node_index].name;
}

void effect_object::set_attract_target(const std::shared_ptr<spatial_object>& target, const math::vector3d& offset)
{
	attract_target = target;
	attract_position = offset;
	attract_type = target ? position_types::target : position_types::self;
}

void effect_object::set_attract_self(const math::vector3d& offset)
{
	attract_target.reset();
	attract_position = offset;
	attract_type = position_types::self;
}

void effect_object::set_attract_world(const math::vector3d& position)
{
	attract_target.reset();
	attract_position = position;
	attract_type = position_types::world;
}

math::vector3d effect_object::get_attract_world_position() const
{
	return resolve_position(attract_type, attract_target, attract_position);
}

void effect_object::set_event_target(const std::shared_ptr<spatial_object>& target, const math::vector3d& offset)
{
	event_target = target;
	event_position = offset;
	event_type = target ? position_types::target : position_types::self;
}

void effect_object::set_event_self(const math::vector3d& offset)
{
	event_target.reset();
	event_position = offset;
	event_type = position_types::self;
}

void effect_object::set_event_world(const math::vector3d& position)
{
	event_target.reset();
	event_position = position;
	event_type = position_types::world;
}

math::vector3d effect_object::get_event_world_position() const
{
	return resolve_position(event_type, event_target, event_position);
}

void effect_object::on_initialize()
{
	base::on_initialize();

	request_load();
}

void effect_object::on_finalize()
{
	if (load_operation)
	{
		load_operation->cancel();
		load_operation.reset();
	}

	release_instance();
	play_requested = false;
	loaded = false;
	loaded_resource.reset();

	base::on_finalize();
}

void effect_object::post_update(float elapsed_time)
{
	base::post_update(elapsed_time);

	update_loading();
	apply_transform();
	apply_positions();
}

template<class Archive>
void effect_object::serialize_fields(Archive& archive, std::uint32_t version)
{
	game_object_serializer::serialize_asset_reference(archive, "effect", effect.uuid);
	archive(CEREAL_NVP(auto_play),
			CEREAL_NVP(loop),
			CEREAL_NVP(attract_type),
			CEREAL_NVP(attract_position),
			CEREAL_NVP(event_type),
			CEREAL_NVP(event_position));

	std::shared_ptr<spatial_object> attract_target_object = attract_target.lock();
	std::shared_ptr<spatial_object> event_target_object = event_target.lock();
	game_object_serializer::serialize_object_reference(archive, "attract_target", attract_target_object);
	game_object_serializer::serialize_object_reference(archive, "event_target", event_target_object);
	attract_target = attract_target_object;
	event_target = event_target_object;
}

void effect_object::serialize(cereal::JSONOutputArchive& archive) const
{
	base::serialize(archive);
	game_object_serializer::save_fields(archive, "effect_object", *this);
}

void effect_object::deserialize(cereal::JSONInputArchive& archive)
{
	base::deserialize(archive);
	game_object_serializer::load_fields(archive, "effect_object", *this);
}

#if defined(USE_IMGUI)
void effect_object::imgui_render_core()
{
	base::imgui_render_core();

	resource::asset_reference<effect_resource> reference = effect;
	if (asset_reference_field::imgui_combo("effect", reference, resource::asset_types::effect))
		set_effect(reference);

	ImGui::Checkbox("auto play", &auto_play);
	bool new_loop = loop;
	if (ImGui::Checkbox("loop", &new_loop))
		set_loop(new_loop);
	bool new_spawn_stop = spawn_stop;
	if (ImGui::Checkbox("spawn stop", &new_spawn_stop))
		set_spawn_stop(new_spawn_stop);

	ImGui::SeparatorText("attract");
	ImGui::Combo("attract type", attract_type);
	if (attract_type == position_types::target)
		object_reference_field::imgui_combo("attract target", get_context(), attract_target);
	ImGui::InputScalarN("attract position", ImGuiDataType_Double, &attract_position.x, 3);

	ImGui::SeparatorText("event");
	ImGui::Combo("event type", event_type);
	if (event_type == position_types::target)
		object_reference_field::imgui_combo("event target", get_context(), event_target);
	ImGui::InputScalarN("event position", ImGuiDataType_Double, &event_position.x, 3);

	ImGui::SeparatorText("playback");
	if (ImGui::Button("play"))
		play();
	ImGui::SameLine();
	if (ImGui::Button("stop"))
		stop();
	ImGui::Text("loading : %s", load_operation ? "true" : "false");
	ImGui::Text("playing : %s", is_playing() ? "true" : "false");
	ImGui::Text("particle events : %zu", get_particle_events().size());
}
#endif	//	defined(USE_IMGUI)

void effect_object::on_active()
{
	base::on_active();

	if (instance)
		instance->set_flag(effect_instance::flags::active, true);
}

void effect_object::on_deactive()
{
	base::on_deactive();

	if (instance)
		instance->set_flag(effect_instance::flags::active, false);
}

void effect_object::request_load()
{
	if (load_operation)
	{
		load_operation->cancel();
		load_operation.reset();
	}
	loaded_resource.reset();

	loaded = effect.is_empty();
	if (loaded)
		return;

	load_operation = resource_manager::instance()->load_async(effect);
	loaded = !load_operation;
}

void effect_object::update_loading()
{
	if (!load_operation || !load_operation->is_done())
		return;

	const resource::load_operation_ptr<effect_resource> operation = std::move(load_operation);
	load_operation.reset();
	loaded = true;

	if (!operation->is_succeeded() || !operation->get_resource())
	{
		DEBUG_WARNING_CATEGORY(core::logger::categorys::system, "effect_object load failed[%s] %s",
			std::string(operation->get_file_path()).c_str(), operation->get_error_message().c_str());
		play_requested = false;
		return;
	}

	loaded_resource = operation->get_resource();
	if (auto_play || play_requested)
		play();
}

void effect_object::create_instance()
{
	if (!loaded_resource || !has_context())
		return;

	instance = std::make_shared<effect_instance>(loaded_resource);
	instance->set_flag(effect_instance::flags::animation_loop, loop);
	instance->set_flag(effect_instance::flags::spawn_stop, spawn_stop);
	instance->set_flag(effect_instance::flags::active, is_active());
	apply_transform();
	apply_positions();
	if (!get_context().get_effects().register_instance(instance))
		instance.reset();
}

void effect_object::release_instance()
{
	if (!instance)
		return;

	if (has_context())
		get_context().get_effects().unregister_instance(instance);
	instance.reset();
}

void effect_object::apply_transform()
{
	if (!instance)
		return;

	const math::vector3d world_scale = get_world_scale();
	instance->set_position(get_world_translation());
	instance->set_rotation(math::rotatorf(get_world_euler()));
	instance->set_scale({ static_cast<float>(world_scale.x), static_cast<float>(world_scale.y), static_cast<float>(world_scale.z) });
}

void effect_object::apply_positions()
{
	if (!instance)
		return;

	instance->set_attract_position(get_attract_world_position());
	instance->set_event_position(get_event_world_position());
}

math::vector3d effect_object::resolve_position(position_types type, const std::weak_ptr<spatial_object>& target, const math::vector3d& position) const
{
	switch (type)
	{
		case position_types::target:
		{
			//	追従先が消えていたら自身の位置
			if (std::shared_ptr<spatial_object> locked = target.lock())
				return locked->get_world().transform_point(position);
			return get_world_translation();
		}
		case position_types::world:
		{
			return position;
		}
		default:
		{
			return get_world().transform_point(position);
		}
	}
}
