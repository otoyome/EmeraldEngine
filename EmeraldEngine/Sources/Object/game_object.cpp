#include "Object/game_object.h"
#include "Object/game_object_registry.h"
#include "Object/engine_context.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"


std::string_view game_object::get_name() const
{
	return name.view();
}

void game_object::set_name(std::string_view name)
{
	this->name = name;
}

engine_context& game_object::get_context() const
{
	assert(context && "登録前のオブジェクトからワールドを参照した");
	return *context;
}

bool game_object::is_active() const
{
	return active;
}

void game_object::set_active(bool active)
{
	if (this->active != active)
	{
		this->active = active;
		if (this->active)
			on_active();
		else
			on_deactive();
	}
}

game_object::game_object(std::string_view name)
: name(name)
, active(true)
, uuid(core::uuid::generate())
{
}

game_object::~game_object()
{
}

void game_object::serialize(cereal::JSONOutputArchive& archive) const
{
	std::string uuid_text = uuid.to_string();
	std::string name_text(name.view());
	bool active_value = active;
	archive(cereal::make_nvp("uuid", uuid_text),
		cereal::make_nvp("name", name_text),
		cereal::make_nvp("active", active_value));
}

void game_object::deserialize(cereal::JSONInputArchive& archive)
{
	std::string uuid_text;
	std::string name_text;
	bool active_value = true;
	archive(cereal::make_nvp("uuid", uuid_text),
		cereal::make_nvp("name", name_text),
		cereal::make_nvp("active", active_value));

	core::uuid value;
	if (core::uuid::from_string(uuid_text, value))
		uuid = value;
	set_name(name_text);

	//	登録前なので on_active/on_deactive は呼ばない
	active = active_value;
}

void game_object::fixed_update(float elapsed_time)
{
}

void game_object::update(float elapsed_time)
{
}

void game_object::post_update(float elapsed_time)
{
}

void game_object::pre_render()
{
}

void game_object::post_render()
{
}

#if defined(USE_IMGUI)

void game_object::imgui_render_inspector()
{
	ImGui::PushID(static_cast<int>(std::hash<core::uuid>()(uuid)));
	ImGui::TextUnformatted(name.get<core::utf8_cache>().data());
	if (transient)
	{
		ImGui::SameLine();
		ImGui::TextDisabled("(transient)");
	}
	ImGui::Separator();

	if (ImGui::Checkbox("active", &active))
	{
		if (active)
			on_active();
		else
			on_deactive();
	}

	imgui_render_core();
	ImGui::PopID();
}

void game_object::imgui_render_core()
{
}
#endif	//	defined(USE_IMGUI)

void game_object::on_initialize()
{
}

void game_object::on_finalize()
{
}

void game_object::on_active()
{
}

void game_object::on_deactive()
{
}

