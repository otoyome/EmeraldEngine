#include "Object/game_object.h"

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
{
	id = ++id_counter;
	assert(id != invalid_id);
}

game_object::~game_object()
{
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
void game_object::imgui_render()
{
	if (!display_inspector)
		return;

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
