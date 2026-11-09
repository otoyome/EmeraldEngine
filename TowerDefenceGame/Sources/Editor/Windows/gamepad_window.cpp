#include "Editor/Windows/gamepad_window.h"

#if defined(USE_IMGUI)
#include "imgui.h"
#include "imgui_internal.h"

namespace
{
	constexpr size_t gamepad_axis_count = magic_enum::enum_count<gamepad::axis_states>();

	ImVec4 state_color(input::state state)
	{
		switch (state)
		{
			case input::state::down:	return ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
			case input::state::hold:	return ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
			case input::state::release:	return ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
			default:					return ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
		}
	}

	//	スティックの位置を円の中の点で描く
	void draw_stick(const char* label, float x, float y)
	{
		constexpr float size = 96.0f;
		ImGui::BeginGroup();
		ImGui::TextUnformatted(label);
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		ImGui::Dummy(ImVec2(size, size));
		const ImVec2 center(origin.x + size * 0.5f, origin.y + size * 0.5f);
		const float radius = size * 0.5f - 2.0f;
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		draw_list->AddCircle(center, radius, ImGui::GetColorU32(ImGuiCol_Border), 32);
		draw_list->AddLine(ImVec2(center.x - radius, center.y), ImVec2(center.x + radius, center.y), ImGui::GetColorU32(ImGuiCol_Separator));
		draw_list->AddLine(ImVec2(center.x, center.y - radius), ImVec2(center.x, center.y + radius), ImGui::GetColorU32(ImGuiCol_Separator));
		draw_list->AddCircleFilled(ImVec2(center.x + x * radius, center.y - y * radius), 5.0f, ImGui::GetColorU32(ImVec4(0.3f, 0.8f, 1.0f, 1.0f)));
		ImGui::Text("%+.2f, %+.2f", x, y);
		ImGui::EndGroup();
	}

	void gamepad_axes(const gamepad::states& state)
	{
		using axis = gamepad::axis_states;
		draw_stick("left stick", state.get_axis(axis::left_x), state.get_axis(axis::left_y));
		ImGui::SameLine(0.0f, 32.0f);
		draw_stick("right stick", state.get_axis(axis::right_x), state.get_axis(axis::right_y));
		ImGui::SameLine(0.0f, 32.0f);

		ImGui::BeginGroup();
		ImGui::TextUnformatted("triggers");
		char overlay[32];
		sprintf_s(overlay, "lt %.2f", state.get_axis(axis::left_trigger));
		ImGui::ProgressBar(state.get_axis(axis::left_trigger), ImVec2(160.0f, 0.0f), overlay);
		sprintf_s(overlay, "rt %.2f", state.get_axis(axis::right_trigger));
		ImGui::ProgressBar(state.get_axis(axis::right_trigger), ImVec2(160.0f, 0.0f), overlay);
		ImGui::EndGroup();

		//	軸を押下とみなした状態
		constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
		if (ImGui::BeginTable("axes", 3, table_flags))
		{
			ImGui::TableSetupColumn("axis");
			ImGui::TableSetupColumn("value");
			ImGui::TableSetupColumn("state");
			ImGui::TableHeadersRow();
			for (size_t index = 0; index < gamepad_axis_count; ++index)
			{
				const auto axis_state = static_cast<axis>(index);
				const input::state key_state = state.get_axis_state(axis_state);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(magic_enum::enum_name(axis_state).data());
				ImGui::TableNextColumn();
				ImGui::Text("%+.3f", state.get_axis(axis_state));
				ImGui::TableNextColumn();
				ImGui::TextColored(state_color(key_state), "%s", magic_enum::enum_name(key_state).data());
			}
			ImGui::EndTable();
		}
	}
}

void gamepad_window::update()
{
	if (!gamepad::is_valid())
		return;

	auto* pad = gamepad::instance();
	for (size_t pad_index = 0; pad_index < gamepad::max_count; ++pad_index)
	{
		const gamepad::states& state = pad->get_state(pad_index);
		for (size_t index = 0; index < key_count; ++index)
		{
			const input::state key_state = state.key_state[index];
			counters[pad_index].down[index] += key_state == input::state::down ? 1 : 0;
			counters[pad_index].release[index] += key_state == input::state::release ? 1 : 0;
		}
	}
}

void gamepad_window::on_closed()
{
	if (!gamepad::is_valid())
		return;

	auto* pad = gamepad::instance();
	for (size_t pad_index = 0; pad_index < gamepad::max_count; ++pad_index)
	{
		float* vibration = vibrations[pad_index];
		if (vibration[0] == 0.0f && vibration[1] == 0.0f)
			continue;
		vibration[0] = 0.0f;
		vibration[1] = 0.0f;
		pad->set_vibration(pad_index, 0.0f, 0.0f);
	}
}

void gamepad_window::imgui_render_keys(size_t pad_index, const gamepad::states& state)
{
	counter& target = counters[pad_index];
	if (ImGui::SmallButton("reset counts"))
		target = counter();

	constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
	if (!ImGui::BeginTable("keys", 4, table_flags))
		return;

	ImGui::TableSetupColumn("key");
	ImGui::TableSetupColumn("state");
	ImGui::TableSetupColumn("down");
	ImGui::TableSetupColumn("release");
	ImGui::TableHeadersRow();
	for (size_t index = 0; index < key_count; ++index)
	{
		const auto key = static_cast<gamepad::key_states>(index);
		const input::state key_state = state.is_key_state(key);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(magic_enum::enum_name(key).data());
		ImGui::TableNextColumn();
		ImGui::TextColored(state_color(key_state), "%s", magic_enum::enum_name(key_state).data());
		ImGui::TableNextColumn();
		ImGui::Text("%u", target.down[index]);
		ImGui::TableNextColumn();
		ImGui::Text("%u", target.release[index]);
	}
	ImGui::EndTable();
}

void gamepad_window::imgui_render_vibration(size_t pad_index)
{
	float* vibration = vibrations[pad_index];
	ImGui::SetNextItemWidth(160.0f);
	bool changed = ImGui::SliderFloat("low frequency", &vibration[0], 0.0f, 1.0f);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(160.0f);
	changed = ImGui::SliderFloat("high frequency", &vibration[1], 0.0f, 1.0f) || changed;
	ImGui::SameLine();
	if (ImGui::Button("stop"))
	{
		vibration[0] = 0.0f;
		vibration[1] = 0.0f;
		changed = true;
	}
	if (changed)
		gamepad::instance()->set_vibration(pad_index, vibration[0], vibration[1]);
}

void gamepad_window::render_window(std::shared_ptr<editor_items::context>& /*context*/, bool& open)
{
	if (!gamepad::is_valid())
		return;

	auto* pad = gamepad::instance();
	ImGui::SetNextWindowSize(ImVec2(560.0f, 640.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("gamepad", &open))
	{
		if (ImGui::BeginTabBar("pads"))
		{
			for (size_t pad_index = 0; pad_index < gamepad::max_count; ++pad_index)
			{
				const gamepad::states& state = pad->get_state(pad_index);
				char label[64];
				sprintf_s(label, "pad%zu%s###pad%zu", pad_index, state.connected ? "" : " (disconnected)", pad_index);
				if (!ImGui::BeginTabItem(label))
					continue;

				ImGui::PushID(static_cast<int>(pad_index));
				ImGui::TextColored(state.connected ? ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
					state.connected ? "connected" : "disconnected");
				if (state.connected)
				{
					ImGui::SameLine();
					const std::string name(pad->get_name(pad_index));
					ImGui::TextUnformatted(name.c_str());
				}
				ImGui::BeginDisabled(!state.connected);
				imgui_render_vibration(pad_index);
				ImGui::EndDisabled();

				if (ImGui::CollapsingHeader("axes", ImGuiTreeNodeFlags_DefaultOpen))
					gamepad_axes(state);
				if (ImGui::CollapsingHeader("keys", ImGuiTreeNodeFlags_DefaultOpen))
					imgui_render_keys(pad_index, state);
				ImGui::PopID();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
}
#endif	//	defined(USE_IMGUI)
