#pragma once

#include "System/world_singleton.h"
#include "render_data.h"
#include "view_parameters.h"

//	ワールドを映している画面情報
class world_viewport
{
	WORLD_SINGLETON(world_viewport)

private:
	world_viewport() = default;
	~world_viewport() = default;

public:
	//	画面矩形
	math::float4 get_rect() const { return rect; }
	void set_rect(const math::float4& value) { rect = value; }
	float get_width() const { return rect.z; }
	float get_height() const { return rect.w; }

	//	ImGui のウィンドウの中に描いているか
	bool is_in_window() const { return in_window; }
	void set_in_window(bool value) { in_window = value; }

	//	@tool マウス・キーボードの入力を受け付けるか
	bool accepts_mouse() const { return accept_mouse; }
	bool accepts_keyboard() const { return accept_keyboard; }
	void set_accepts_input(bool mouse, bool keyboard) { accept_mouse = mouse; accept_keyboard = keyboard; }

	//	@tool クライアント座標をビュー内の座標にする
	math::float2 to_view_position(const math::float2& client_position) const
	{
		return { client_position.x - rect.x, client_position.y - rect.y };
	}

	//	@tool 視点パラメーターの画面矩形（投影のアスペクト比）をこの画面に合わせる
	void apply_screen_rect(view_parameters& parameters) const
	{
		parameters.screen_rect = { 0.0f, 0.0f, rect.z, rect.w };
	}

private:
	math::float4 rect = { 0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT) };
	bool in_window = false;
	bool accept_mouse = true;
	bool accept_keyboard = true;
};
