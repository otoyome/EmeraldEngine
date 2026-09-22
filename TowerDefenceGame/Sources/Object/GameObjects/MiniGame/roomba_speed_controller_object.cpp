#include "object/gameobjects/minigame/roomba_speed_controller_object.h"

namespace
{
	enum ui_id
	{
		base,
		text,
	};
}

roomba_speed_controller_object::roomba_speed_controller_object(const object_descs& object_desc)
: game_object(object_desc.name)
{
	image = object_desc.image;
	if (image)
		origin_colors[ui_id::base] = image->get_color();
	text = object_desc.text;
	if (text)
		origin_colors[ui_id::text] = text->get_color();
}

roomba_speed_controller_object::~roomba_speed_controller_object()
{
}

void roomba_speed_controller_object::fixed_update(float elapsed_time)
{
	if (!get_velocity || !text)
		return;

	DirectX::XMFLOAT3 velocity = get_velocity();
	DirectX::XMVECTOR Velocity = DirectX::XMLoadFloat3(&velocity);
	float speed_m_s = system_units::convert_unit_to_m(DirectX::XMVectorGetX(DirectX::XMVector3Length(Velocity)));
	UINT speed_km_h = static_cast<UINT>(speed_m_s * 3.6f);

	char buffer[64];
	sprintf_s(buffer, "%u", speed_km_h);
	text->set_texts(buffer);
}

void roomba_speed_controller_object::set_getter(std::function<DirectX::XMFLOAT3()> get_velocity)
{
	this->get_velocity = get_velocity;
}

void roomba_speed_controller_object::set_alpha(float alpha)
{
	if (image)
	{
		DirectX::XMFLOAT4 color = origin_colors[ui_id::base];
		color.w *= alpha;
		image->set_color(color);
	}
	if (text)
	{
		DirectX::XMFLOAT4 color = origin_colors[ui_id::text];
		color.w *= alpha;
		text->set_color(color);
	}
}

