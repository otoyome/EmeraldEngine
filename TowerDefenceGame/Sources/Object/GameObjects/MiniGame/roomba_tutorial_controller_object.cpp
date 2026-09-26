#include "object/gameobjects/minigame/roomba_tutorial_controller_object.h"

namespace
{
	enum ui_id
	{
		base,
		text,
	};
}

roomba_tutorial_controller_object::roomba_tutorial_controller_object(const object_descs& object_desc)
: game_object(object_desc.name)
{
	base_image = object_desc.base_image;
	if (base_image)
		origin_colors[ui_id::base] = base_image->get_color();
	text = object_desc.text;
	if (text)
		origin_colors[ui_id::text] = text->get_color();
}

roomba_tutorial_controller_object::~roomba_tutorial_controller_object()
{
}

void roomba_tutorial_controller_object::set_alpha(float alpha)
{
	if (base_image)
	{
		DirectX::XMFLOAT4 color = origin_colors[ui_id::base];
		color.w *= alpha;
		base_image->set_color(color);
	}
	if (text)
	{
		DirectX::XMFLOAT4 color = origin_colors[ui_id::text];
		color.w *= alpha;
		text->set_color(color);
	}
}
