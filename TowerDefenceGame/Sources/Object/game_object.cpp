#include "Object/game_object.h"
#include "Object/game_object_registry.h"

#include "cereal/cereal.hpp"
#include "cereal/archives/json.hpp"
#include "cereal/types/string.hpp"

size_t game_object::get_id() const
{
	return id;
}

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
, uuid(universally_unique_identifier::generate())
{
	id = ++id_counter;
	assert(id != invalid_id);
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

	universally_unique_identifier value;
	if (universally_unique_identifier::from_string(uuid_text, value))
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
void game_object::imgui_render(const std::function<void()>& render_children)
{
	if (!display_inspector)
	{
		if (render_children)
			render_children();
		return;
	}

	ImGui::PushID(0x10203040 + static_cast<int>(id));
	if (ImGui::TreeNode(name.get<utf8_cache>().data()))
	{
		if (ImGui::Checkbox("active", &active))
		{
			if (active)
				on_active();
			else
				on_deactive();
		}

		imgui_render_core();

		if (render_children)
			render_children();

		ImGui::TreePop();
	}
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

GAME_OBJECT_REGISTER(game_object);
