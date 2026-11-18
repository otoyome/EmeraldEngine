#include "object/gameobjects/minigame/roomba_gauge_controller_object.h"

namespace
{
}

roomba_gauge_controller_object::roomba_gauge_controller_object(const object_descs& object_desc)
: game_object(object_desc.name)
{
	for (int index = 0; index < magic_enum::enum_count<ui_id>(); ++index)
	{
		ui[index] = object_desc.objects[index];
		if (ui[index])
		{
			origin_rects[index] = ui[index]->get_screen_rect();
			origin_colors[index] = ui[index]->get_color();
		}
	}
}

roomba_gauge_controller_object::~roomba_gauge_controller_object()
{
}

void roomba_gauge_controller_object::fixed_update(float elapsed_time)
{
	//	•Ï“®ŒŸ’m
	int max_value = get_max_value();
	if (value != get_value())
	{
		main_value_inner = get_value();
		value = get_value();
	}
	this->value = value;

	//	UI•Ï“®
	int ui_index = static_cast<int>(ui_id::main);
	int old_value_inner = main_value_inner;
	main_value_inner = math::lerp(main_value_inner, value, math::saturate(animation_speed));
	if (abs(main_value_inner - old_value_inner) <= 10)
	{
		main_value_inner = value;
	}

	float main_rate = static_cast<float>(main_value_inner) / max_value;
	if (main_value_inner <= value)
	{
		main_value_inner = value;
		main_rate = static_cast<float>(value) / max_value;
	}

	ui[ui_index]->set_fill_amount(main_rate);
}

void roomba_gauge_controller_object::post_update(float elapsed_time)
{
	shake_desc.time -= elapsed_time;
	if (shake_desc.time <= 0.0f)
	{
		if (shake_desc.time > -10000.0f)
		{
			for (int index = 0; index < magic_enum::enum_count<ui_id>(); ++index)
			{
				if (ui[index])
				{
					auto rect = ui[index]->get_screen_rect();
					rect.x -= shake_offset.x;
					rect.y -= shake_offset.y;
					ui[index]->set_screen_rect(rect);
				}
			}
			shake_offset.x = 0;
			shake_offset.y = 0;
		}
		shake_desc.time = -10000.0f;
		return;
	}

	float direction_x = shake_desc.power * static_cast<float>(rand() % 3 - 1);
	float direction_y = shake_desc.power * static_cast<float>(rand() % 3 - 1);
	for (int index = 0; index < magic_enum::enum_count<ui_id>(); ++index)
	{
		if (ui[index])
		{
			auto rect = ui[index]->get_screen_rect();
			rect.x -= shake_offset.x;
			rect.y -= shake_offset.y;
			rect.x += direction_x;
			rect.y += direction_y;
			ui[index]->set_screen_rect(rect);
		}
	}
	shake_offset.x = direction_x;
	shake_offset.y = direction_y;
}

void roomba_gauge_controller_object::set_getter(std::function<int()> get_value, std::function<int()> get_max_value)
{
	this->get_value = get_value;
	this->get_max_value = get_max_value;
}

void roomba_gauge_controller_object::call_shake(const shake_descs& shake_desc)
{
	this->shake_desc = shake_desc;
	this->shake_desc.power = (this->shake_desc.power * 1.0f);
}

void roomba_gauge_controller_object::set_alpha(float alpha)
{
	for (int index = 0; index < magic_enum::enum_count<ui_id>(); ++index)
	{
		if (!ui[index])
			continue;

		math::float4 color = origin_colors[index];
		color.w *= alpha;
		ui[index]->set_color(color);
	}
}


