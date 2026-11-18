#pragma once

#include "Graphics/UI/sprite.h"
#include "object/game_object.h"

namespace ui
{
	//	UI の仮想画面の大きさ
	constexpr float virtual_screen_width = 1280.0f;
	constexpr float virtual_screen_height = 720.0f;

	constexpr math::float2 get_pixel_size()
	{
		return { 1.0f / virtual_screen_width, 1.0f / virtual_screen_height };
	}
	constexpr math::float4 calculate_screen_rect_rate(math::float4 rect)
	{
		//return rect;
		return { rect.x / virtual_screen_width, rect.y / virtual_screen_height, rect.z / virtual_screen_width, rect.w / virtual_screen_height };
	}
	constexpr math::float4 calculate_screen_rect(math::float4 rect_rate)
	{
		//return rect;
		return { rect_rate.x * virtual_screen_width, rect_rate.y * virtual_screen_height, rect_rate.z * virtual_screen_width, rect_rate.w * virtual_screen_height };
	}
}	//	namespace ui

//	UI制御用オブジェクト
class ui_object_base : public game_object
{
	GAME_OBJECT_DECRLEAR(ui_object_base, game_object)

public:
	int get_sort_order() const;
	void set_sort_order(int sort_order);

	float get_rotation() const;
	void set_rotation(float rotation);

	math::float4 get_screen_rect() const;
	void set_screen_rect(const math::float4& screen_rect);

	const math::float2& get_pivot() const;
	void set_pivot(const math::float2& pivot);

	const math::float4& get_uv_rect() const;
	void set_uv_rect(const math::float4& uv_rect);

	const math::float4& get_color() const;
	void set_color(const math::float4& color);
	void set_color(const math::float3& color);

protected:
	bool is_dirty() const;
	void set_dirty();

	virtual math::float4 get_screen_rect_inner() const;

public:
	ui_object_base(std::string_view name);
	~ui_object_base() override;

	void post_render() override;

#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

private:
	bool dirty;

	int sort_order;
	float rotation;
	math::float2 pivot;
	math::float4 screen_rect;
	math::float4 uv_rect;
	math::float4 color;
};

//	UI制御用オブジェクト
class ui_object : public ui_object_base
{
	GAME_OBJECT_DECRLEAR(ui_object, ui_object_base)

public:
	const sprite& get_sprite() const { return sprite; }
	sprite& get_sprite() { return sprite; }

private:
	math::float4 get_screen_rect_inner() const override { return base::get_screen_rect_inner(); }

public:
	ui_object(std::string_view name);
	~ui_object() override;

	void pre_render() override;

private:
	sprite sprite;
};

//	UI制御用オブジェクト
class ui_objects : public ui_object_base
{
	GAME_OBJECT_DECRLEAR(ui_objects, ui_object_base)

public:
	const std::vector<sprite>& get_sprites() const { return sprites; }
	std::vector<sprite>& get_sprites() { return sprites; }

private:
	math::float4 get_screen_rect_inner() const override { return base::get_screen_rect_inner(); }

public:
	ui_objects(std::string_view name);
	~ui_objects() override;

	void pre_render() override;

private:
	std::vector<sprite> sprites;
};
