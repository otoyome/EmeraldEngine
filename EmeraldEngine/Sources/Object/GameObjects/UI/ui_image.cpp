#include "object/gameobjects/ui/ui_image.h"
#include "Texture/texture_resource_utility.h"

namespace
{
	const math::float2 fill_ring_points[8] =
	{
		{ 0.5f, 0.0f },	//	上
		{ 1.0f, 0.0f },	//	右上
		{ 1.0f, 0.5f },	//	右
		{ 1.0f, 1.0f },	//	右下
		{ 0.5f, 1.0f },	//	下
		{ 0.0f, 1.0f },	//	左下
		{ 0.0f, 0.5f },	//	左
		{ 0.0f, 0.0f },	//	左上
	};

	math::float2 calculate_corner_size(float border_near, float border_far, float texture_size, float screen_size)
	{
		float rate[2] = { border_near, border_far };
		float total_rate = rate[0] + rate[1];
		if (total_rate <= 0.0f)
		{
			return { 0.0f, 1.0f };
		}

		{
			float w = std::max(total_rate - 1.0f, 0.0f);
			float w0 = rate[0] / total_rate;
			float w1 = rate[1] / total_rate;
			rate[0] -= w * w0;
			rate[1] -= w * w1;
			total_rate = rate[0] + rate[1];
		}
		float near_size = rate[0] * texture_size;
		float far_size = rate[1] * texture_size;
		float size_rate = screen_size / texture_size;
		float remain = texture_size * (std::min(size_rate, total_rate) - total_rate);
		near_size = std::max(0.0f, near_size + remain * (rate[0] / total_rate));
		far_size = std::max(0.0f, far_size + remain * (rate[1] / total_rate));

		return { std::max(0.0f, near_size / screen_size), std::max(0.0f, (screen_size - far_size) / screen_size) };
	}
}

ui_image::draw_modes ui_image::get_draw_mode() const
{
	return draw_mode;
}

void ui_image::set_draw_mode(draw_modes draw_mode)
{
	this->draw_mode = draw_mode;
	set_dirty();
}

math::float4 ui_image::get_border() const
{
	return border;
}

void ui_image::set_border(math::float4 border)
{
	this->border = math::saturate(math::vector4f(border)).to_float4();
	set_dirty();
}

float ui_image::get_fill_amount() const
{
	return fill_amount;
}

void ui_image::set_fill_amount(float fill_amount)
{
	this->fill_amount = math::saturate(fill_amount);
	set_dirty();
}

ui_image::fill_methods ui_image::get_fill_method() const
{
	return fill_method;
}

void ui_image::set_fill_method(fill_methods fill_method)
{
	this->fill_method = fill_method;
	set_dirty();
}

ui_image::fill_origins ui_image::get_fill_origin() const
{
	return fill_origin;
}

void ui_image::set_fill_origin(fill_origins fill_origin)
{
	this->fill_origin = fill_origin;
	set_dirty();
}

bool ui_image::get_fill_clockwise() const
{
	return fill_clockwise;
}

void ui_image::set_fill_clockwise(bool fill_clockwise)
{
	this->fill_clockwise = fill_clockwise;
	set_dirty();
}

texture_ptr ui_image::get_texture() const
{
	texture_ptr texture;
	get_sprite().get_material().get_texture(sprite::base_texture_name, texture);
	return texture;
}

void ui_image::set_texture(std::string_view texture_path)
{
	const texture_resource_ptr texture = texture_resource_utility::load_texture_resource(texture_path);
	get_sprite().get_material().set_texture(sprite::base_texture_name, texture ? texture->get_texture() : nullptr);
}

void ui_image::set_texture(texture_ptr texture)
{
	get_sprite().get_material().set_texture(sprite::base_texture_name, texture);
}

ui_image::ui_image(std::string_view name)
: ui_object(name)
, draw_mode(draw_modes::single)
, border(0.25f, 0.25f, 0.25f, 0.25f)
, fill_amount(1.0f)
, fill_origin(fill_origins::top)
, fill_clockwise(true)
{
}

ui_image::~ui_image()
{
}

void ui_image::build_primitive_single(std::vector<sprite::vertex>& vertices, std::vector<UINT>& indices)
{
	math::float4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

	vertices.push_back({{ 0.0f, 0.0f, 0.0f }, color, { 0.0f, 0.0f }});
	vertices.push_back({{ 1.0f, 0.0f, 0.0f }, color, { 1.0f, 0.0f }});
	vertices.push_back({{ 0.0f, 1.0f, 0.0f }, color, { 0.0f, 1.0f }});
	vertices.push_back({{ 1.0f, 1.0f, 0.0f }, color, { 1.0f, 1.0f }});
	indices.emplace_back(0);
	indices.emplace_back(1);
	indices.emplace_back(2);
	indices.emplace_back(1);
	indices.emplace_back(3);
	indices.emplace_back(2);
}

void ui_image::build_primitive_sliced(std::vector<sprite::vertex>& vertices, std::vector<UINT>& indices)
{
	math::float4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

	//	テクスチャ空間での境界を算出
	texture_ptr texture;
	get_sprite().get_material().get_texture(sprite::base_texture_name, texture);
	math::float2 texture_size = { ui::virtual_screen_width, ui::virtual_screen_height };
	if (texture2d_ptr texture_2d = std::static_pointer_cast<texture2d>(texture))
	{
		texture_size.x = static_cast<float>(texture_2d->get_width());
		texture_size.y = static_cast<float>(texture_2d->get_height());
	}

	math::float2 screen_rect_size = { get_screen_rect().z, get_screen_rect().w };
	math::float2 size_rate = { screen_rect_size.x / texture_size.x, screen_rect_size.y / texture_size.y };
	//	座標
	math::float2 corner_size[2];
	{
		math::float2 x_corners = calculate_corner_size(border.x, border.z, texture_size.x, screen_rect_size.x);
		math::float2 y_corners = calculate_corner_size(border.y, border.w, texture_size.y, screen_rect_size.y);
		corner_size[0] = { x_corners.x, y_corners.x };
		corner_size[1] = { x_corners.y, y_corners.y };
	}
	//	UV
	float llx = 0.0f;
	float lrx = 1.0f;
	float lly = 0.0f;
	float lry = 1.0f;
	float clx = corner_size[0].x;
	float crx = corner_size[1].x;
	float cly = corner_size[0].y;
	float cry = corner_size[1].y;
	vertices.push_back({{ llx, lly, 0.0f }, color, {                0.0f, 0.0f }});
	vertices.push_back({{ clx, lly, 0.0f }, color, {            border.x, 0.0f }});
	vertices.push_back({{ crx, lly, 0.0f }, color, { 1.0f - border.z, 0.0f }});
	vertices.push_back({{ lrx, lly, 0.0f }, color, {                1.0f, 0.0f }});

	vertices.push_back({{ llx, cly, 0.0f }, color, {                0.0f, border.y }});
	vertices.push_back({{ clx, cly, 0.0f }, color, {            border.x, border.y }});
	vertices.push_back({{ crx, cly, 0.0f }, color, { 1.0f - border.z, border.y }});
	vertices.push_back({{ lrx, cly, 0.0f }, color, {                1.0f, border.y }});

	vertices.push_back({{ llx, cry, 0.0f }, color, {                0.0f, 1.0f - border.w }});
	vertices.push_back({{ clx, cry, 0.0f }, color, {            border.x, 1.0f - border.w }});
	vertices.push_back({{ crx, cry, 0.0f }, color, { 1.0f - border.z, 1.0f - border.w }});
	vertices.push_back({{ lrx, cry, 0.0f }, color, {                1.0f, 1.0f - border.w }});

	vertices.push_back({{ llx, lry, 0.0f }, color, {                0.0f, 1.0f }});
	vertices.push_back({{ clx, lry, 0.0f }, color, {            border.x, 1.0f }});
	vertices.push_back({{ crx, lry, 0.0f }, color, { 1.0f - border.z, 1.0f }});
	vertices.push_back({{ lrx, lry, 0.0f }, color, {                1.0f, 1.0f }});

	indices.emplace_back(0);	indices.emplace_back(1);	indices.emplace_back(4);
	indices.emplace_back(1);	indices.emplace_back(5);	indices.emplace_back(4);

	indices.emplace_back(1);	indices.emplace_back(2);	indices.emplace_back(5);
	indices.emplace_back(2);	indices.emplace_back(6);	indices.emplace_back(5);

	indices.emplace_back(2);	indices.emplace_back(3);	indices.emplace_back(6);
	indices.emplace_back(3);	indices.emplace_back(7);	indices.emplace_back(6);

	indices.emplace_back(4);	indices.emplace_back(5);	indices.emplace_back(8);
	indices.emplace_back(5);	indices.emplace_back(9);	indices.emplace_back(8);

	indices.emplace_back(5);	indices.emplace_back(6);	indices.emplace_back(9);
	indices.emplace_back(6);	indices.emplace_back(10);	indices.emplace_back(9);

	indices.emplace_back(6);	indices.emplace_back(7);	indices.emplace_back(10);
	indices.emplace_back(7);	indices.emplace_back(11);	indices.emplace_back(10);

	indices.emplace_back(8);	indices.emplace_back(9);	indices.emplace_back(12);
	indices.emplace_back(9);	indices.emplace_back(13);	indices.emplace_back(12);

	indices.emplace_back(9);	indices.emplace_back(10);	indices.emplace_back(13);
	indices.emplace_back(10);	indices.emplace_back(14);	indices.emplace_back(13);

	indices.emplace_back(10);	indices.emplace_back(11);	indices.emplace_back(14);
	indices.emplace_back(11);	indices.emplace_back(15);	indices.emplace_back(14);
}

void ui_image::build_primitive_filled(std::vector<sprite::vertex>& vertices, std::vector<UINT>& indices)
{
	float amount = math::saturate(fill_amount);
	if (amount <= FLT_MIN)
		return;

	math::float4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
	switch (fill_method)
	{
		case fill_methods::axis:
		{
			auto build_rect = [&](float x0, float y0, float x1, float y1)
			{
				vertices.push_back({{ x0, y0, 0.0f }, color, { x0, y0 }});
				vertices.push_back({{ x1, y0, 0.0f }, color, { x1, y0 }});
				vertices.push_back({{ x0, y1, 0.0f }, color, { x0, y1 }});
				vertices.push_back({{ x1, y1, 0.0f }, color, { x1, y1 }});

				indices.emplace_back(0);
				indices.emplace_back(1);
				indices.emplace_back(2);
				indices.emplace_back(1);
				indices.emplace_back(3);
				indices.emplace_back(2);
			};

			switch (fill_origin)
			{
				case fill_origins::left:	build_rect(0.0f, 0.0f, amount, 1.0f);			break;
				case fill_origins::right:	build_rect(1.0f - amount, 0.0f, 1.0f, 1.0f);	break;
				case fill_origins::top:		build_rect(0.0f, 0.0f, 1.0f, amount);			break;
				case fill_origins::bottom:	build_rect(0.0f, 1.0f - amount, 1.0f, 1.0f);	break;
			}
			break;
		}
		case fill_methods::radial:
		{
			int origin_index = 0;
			switch (fill_origin)
			{
				case fill_origins::top:		origin_index = 0;	break;
				case fill_origins::right:	origin_index = 2;	break;
				case fill_origins::bottom:	origin_index = 4;	break;
				case fill_origins::left:	origin_index = 6;	break;
			}
			int direction = fill_clockwise ? 1 : -1;

			auto ring_point = [&](int step)
			{
				int wrapped = ((origin_index + step * direction) % 8 + 8) % 8;
				return fill_ring_points[wrapped];
			};

			float total = amount * 8.0f;
			int full_slices = std::min(8, static_cast<int>(total));
			float fraction = total - static_cast<float>(full_slices);

			vertices.push_back({{ 0.5f, 0.5f, 0.0f }, color, { 0.5f, 0.5f }});

			math::float2 first_point = ring_point(0);
			vertices.push_back({{ first_point.x, first_point.y, 0.0f }, color, { first_point.x, first_point.y }});

			for (int step = 1; step <= full_slices; ++step)
			{
				math::float2 point = ring_point(step);
				vertices.push_back({{ point.x, point.y, 0.0f }, color, { point.x, point.y }});
			}

			if (fraction > 0.0f)
			{
				math::float2 from = ring_point(full_slices);
				math::float2 to = ring_point(full_slices + 1);
				math::float2 point = { math::lerp(from.x, to.x, fraction), math::lerp(from.y, to.y, fraction) };
				vertices.push_back({{ point.x, point.y, 0.0f }, color, { point.x, point.y }});
			}

			for (UINT index = 1; index < vertices.size() - 1; ++index)
			{
				indices.emplace_back(0);
				indices.emplace_back(index);
				indices.emplace_back(index + 1);
			}
			break;
		}
	}
}

void ui_image::pre_render()
{
	//	頂点修正
	if (is_dirty())
	{
		std::vector<sprite::vertex> vertices;
		std::vector<UINT> indices;
		switch (draw_mode)
		{
			case draw_modes::single:
			{
				build_primitive_single(vertices, indices);
				break;
			}
			case draw_modes::sliced:
			{
				build_primitive_sliced(vertices, indices);
				break;
			}
			case draw_modes::filled:
			{
				build_primitive_filled(vertices, indices);
				break;
			}
		}
		get_sprite().update_vertices(vertices, indices);
	}

	base::pre_render();
}

#if defined(USE_IMGUI)
void ui_image::imgui_render_core()
{
	base::imgui_render_core();

	if (ImGui::Combo("draw_mode", draw_mode))
	{
		set_dirty();
	}

	auto rect = get_border();
	if (ImGui::DragFloat4("border", &rect.x, 0.01f, 0.0f, 1.0f))
	{
		set_border(rect);
	}

	if (draw_mode == draw_modes::filled)
	{
		float amount = get_fill_amount();
		if (ImGui::DragFloat("fill_amount", &amount, 0.01f, 0.0f, 1.0f))
		{
			set_fill_amount(amount);
		}

		auto method = get_fill_method();
		if (ImGui::Combo("fill_method", method))
		{
			set_fill_method(method);
		}

		if (ImGui::Combo("fill_origin", fill_origin))
		{
			set_dirty();
		}

		bool clockwise = get_fill_clockwise();
		if (ImGui::Checkbox("fill_clockwise", &clockwise))
		{
			set_fill_clockwise(clockwise);
		}
	}
}
#endif	//	defined(USE_IMGUI)
