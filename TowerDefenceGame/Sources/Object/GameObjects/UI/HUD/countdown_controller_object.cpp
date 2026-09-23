#include "object/gameobjects/ui/hud/countdown_controller_object.h"

countdown_controller_object::countdown_controller_object(const object_descs& object_desc)
: game_object(object_desc.name)
, text(object_desc.text)
, start_image(object_desc.start_image)
{
}

countdown_controller_object::~countdown_controller_object()
{
}

void countdown_controller_object::set_count(int count)
{
	if (text)
	{
		auto color = text->get_color();
		color.w = 1.0f;
		text->set_color(color);
		text->set_texts(std::to_string(count));
	}
	if (start_image)
	{
		auto color = start_image->get_color();
		color.w = 0.0f;
		start_image->set_color(color);
	}
}

void countdown_controller_object::set_start()
{
	if (text)
	{
		auto color = text->get_color();
		color.w = 0.0f;
		text->set_color(color);
	}
	if (start_image)
	{
		auto color = start_image->get_color();
		color.w = 1.0f;
		start_image->set_color(color);
	}
}

void countdown_controller_object::hide()
{
	if (text)
	{
		auto color = text->get_color();
		color.w = 0.0f;
		text->set_color(color);
	}
	if (start_image)
	{
		auto color = start_image->get_color();
		color.w = 0.0f;
		start_image->set_color(color);
	}
}
