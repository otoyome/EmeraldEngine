#include "object/gameobjects/ui/ui_button.h"

math::float4 ui_button::get_color(color_type type) const
{
	return color[static_cast<int>(type)];
}

void ui_button::set_color(color_type type, const math::float4& color)
{
	this->color[static_cast<int>(type)] = color;
}

void ui_button::set_color(color_type type, const math::float3& color)
{
	auto& c = this->color[static_cast<int>(type)];
	c.x = color.x;
	c.y = color.y;
	c.z = color.z;
}

void ui_button::set_alpha(float alpha)
{
	this->alpha = alpha;
}

bool ui_button::get_enabled()
{
	return enable;
}

void ui_button::set_enabled(bool enable)
{
	this->enable = enable;
}

ui_button::ui_button(std::string_view name, std::shared_ptr<ui_image> image)
: game_object(name)
, image(image)
{
	set_color(color_type::normal, {1, 1, 1, 1});
	set_color(color_type::pressed, {0.5f, 0.5f, 0.5f, 1});
	set_color(color_type::disabled, {0.5f, 0.5f, 0.5f, 1});
}

ui_button::~ui_button()
{
}

void ui_button::fixed_update(float elapsed_time)
{
	math::float4 apply_color = image->get_color();
	if (enable)
	{
		apply_color = color[static_cast<int>(color_type::normal)];
	}
	else
	{
		apply_color = color[static_cast<int>(color_type::disabled)];
	}

	if (pressed)
	{
		if (math::nearly_equal(math::vector4f(image->get_color()), math::vector4f(color[static_cast<int>(color_type::pressed)])))
			pressed = false;
		apply_color = color[static_cast<int>(color_type::pressed)];
	}

	if (image)
	{
		apply_color.w *= alpha;
		math::float4 col = math::lerp(math::vector4f(image->get_color()), math::vector4f(apply_color), fade_duration).to_float4();
		image->set_color(col);
	}
}

void ui_button::press()
{
	pressed = true;
}

#if defined(USE_IMGUI)
void ui_button::imgui_render_core()
{
	base::imgui_render_core();

	ImGui::ColorEdit4(magic_enum::enum_name(color_type::normal).data(), &color[static_cast<int>(color_type::normal)].x);
	ImGui::ColorEdit4(magic_enum::enum_name(color_type::pressed).data(), &color[static_cast<int>(color_type::pressed)].x);
	ImGui::ColorEdit4(magic_enum::enum_name(color_type::disabled).data(), &color[static_cast<int>(color_type::disabled)].x);
}
#endif	//	defined(USE_IMGUI)

