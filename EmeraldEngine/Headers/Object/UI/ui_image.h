#pragma once

#include "Graphics/UI/sprite.h"
#include "object/ui/ui_object.h"

//	UI制御用オブジェクト
class ui_image : public ui_object
{
	GAME_OBJECT_DECRLEAR(ui_image, ui_object)

public:
	enum class draw_modes
	{
		single,
		sliced,
		filled,
	};

	enum class fill_methods
	{
		axis,
		radial,
	};

	enum class fill_origins
	{
		top,
		right,
		bottom,
		left,
	};

public:
	draw_modes get_draw_mode() const;
	void set_draw_mode(draw_modes draw_mode);

	math::float4 get_border() const;
	void set_border(math::float4 border);

	float get_fill_amount() const;
	void set_fill_amount(float fill_amount);

	fill_methods get_fill_method() const;
	void set_fill_method(fill_methods fill_method);

	fill_origins get_fill_origin() const;
	void set_fill_origin(fill_origins fill_origin);

	bool get_fill_clockwise() const;
	void set_fill_clockwise(bool fill_clockwise);

	texture_ptr get_texture() const;
	void set_texture(std::string_view texture_path);
	void set_texture(texture_ptr texture);

public:
	ui_image(std::string_view name);
	~ui_image() override;

	void pre_render() override;

#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

private:
	void build_primitive_single(std::vector<sprite::vertex>& vertices, std::vector<UINT>& indices);
	void build_primitive_sliced(std::vector<sprite::vertex>& vertices, std::vector<UINT>& indices);
	void build_primitive_filled(std::vector<sprite::vertex>& vertices, std::vector<UINT>& indices);

private:
	draw_modes draw_mode;

	math::float4 border;

	float fill_amount = 1.0f;
	fill_methods fill_method = fill_methods::radial;
	fill_origins fill_origin = fill_origins::top;
	bool fill_clockwise = true;
};

