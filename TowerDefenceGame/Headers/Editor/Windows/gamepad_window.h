#pragma once

#if defined(USE_IMGUI)
#include "Editor/editor_items.h"

//	ゲームパッドの入力状況
class gamepad_window : public editor_items::ibase_editor
{
	static constexpr size_t key_count = magic_enum::enum_count<gamepad::key_states>();

	//	down/release は1フレームで見逃しやすいので回数を数える
	struct counter
	{
		uint32_t down[key_count] = {};
		uint32_t release[key_count] = {};
	};

public:
	std::string get_label() const override { return "gamepad"; }

	void update() override;
	void render_window(std::shared_ptr<editor_items::context>& context, bool& open) override;

	void on_closed() override;

private:
	void imgui_render_keys(size_t pad_index, const gamepad::states& state);
	void imgui_render_vibration(size_t pad_index);

private:
	counter counters[gamepad::max_count];

	//	振動の確認用
	float vibrations[gamepad::max_count][2] = {};
};
#endif	//	defined(USE_IMGUI)
