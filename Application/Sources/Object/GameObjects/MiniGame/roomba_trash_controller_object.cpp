#include "object/gameobjects/minigame/roomba_trash_controller_object.h"

namespace
{
	enum ui_id
	{
		base,
		text,
	};
}

roomba_trash_controller_object::roomba_trash_controller_object(const object_descs& object_desc)
: game_object(object_desc.name)
{
	image = object_desc.image;
	if (image)
		origin_colors[ui_id::base] = image->get_color();
	text = object_desc.text;
	if (text)
		origin_colors[ui_id::text] = text->get_color();
}

roomba_trash_controller_object::~roomba_trash_controller_object()
{
}

void roomba_trash_controller_object::fixed_update(float elapsed_time)
{
	if (!get_value || !get_max_value)
		return;

	int value = get_value();
	int max_value = get_max_value();
	float rate = max_value > 0 ? math::saturate(static_cast<float>(value) / static_cast<float>(max_value)) : 0.0f;

	if (image)
	{
		image->set_fill_amount(rate);
	}
	if (text)
	{
		char buffer[64];
		sprintf_s(buffer, "%d%%", static_cast<int>(rate * 100.0f));
		text->set_texts(buffer);
	}
}

void roomba_trash_controller_object::set_getter(std::function<int()> get_value, std::function<int()> get_max_value)
{
	this->get_value = get_value;
	this->get_max_value = get_max_value;
}

void roomba_trash_controller_object::set_alpha(float alpha)
{
	if (image)
	{
		math::float4 color = origin_colors[ui_id::base];
		color.w *= alpha;
		image->set_color(color);
	}
	if (text)
	{
		math::float4 color = origin_colors[ui_id::text];
		color.w *= alpha;
		text->set_color(color);
	}
}
