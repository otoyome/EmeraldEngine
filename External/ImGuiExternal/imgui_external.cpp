#include "ImGuiExternal/imgui_external.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>

#include "implot/implot.h"

namespace
{
	static inline int input_text_callback(ImGuiInputTextCallbackData* data)
	{
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			auto* str = reinterpret_cast<std::string*>(data->UserData);
			str->resize(data->BufTextLen);
			data->Buf = str->data();
		}
		return 0;
	}

	struct input_text_callback_chain
	{
		std::string* str;
		ImGuiInputTextCallback callback;
		void* user_data;
	};
	int input_text_chain_callback(ImGuiInputTextCallbackData* data)
	{
		auto* chain = reinterpret_cast<input_text_callback_chain*>(data->UserData);
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			chain->str->resize(data->BufTextLen);
			data->Buf = chain->str->data();
			return 0;
		}
		if (!chain->callback)
			return 0;
		data->UserData = chain->user_data;
		const int result = chain->callback(data);
		data->UserData = chain;
		return result;
	}
}

bool ImGui::Combo(const char* label, int& current_item_index, const std::vector<std::string_view>& items)
{
	std::string name = std::string(items[current_item_index]);
	if (ImGui::Combo(label, name, items))
	{
		for (UINT index = 0; index < items.size(); ++index)
		{
			if (items[index] == name)
			{
				current_item_index = index;
				break;
			}
		}
		return true;
	}
	return false;
}

bool ImGui::Combo(const char* label, std::size_t& current_item_index, const std::vector<std::string_view>& items)
{
	std::string name = std::string(items[current_item_index]);
	if (ImGui::Combo(label, name, items))
	{
		for (UINT index = 0; index < items.size(); ++index)
		{
			if (items[index] == name)
			{
				current_item_index = index;
				break;
			}
		}
		return true;
	}
	return false;
}

bool ImGui::Combo(const char* label, std::string& current_item, const std::vector<std::string_view>& items)
{
	bool changed = false;

	if (ImGui::BeginCombo(label, system_utility::convert_utf8(current_item).c_str()))
	{
		for (int n = 0; n < items.size(); n++)
		{
			bool is_selected = (current_item == items[n]);	
			if (ImGui::Selectable(system_utility::convert_utf8(items[n]).c_str(), is_selected))
			{
				current_item = items[n];
				changed = true;
			}
			if (is_selected)
			{
				// Set the initial focus when opening the combo (scrolling + for
				// keyboard navigation support in the upcoming navigation branch)
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (changed)
	{
		ImGuiWindow* window = GetCurrentWindow();
		const ImGuiID id = window->GetID(label);
		MarkItemEdited(id);
	}
	return changed;
}

bool ImGui::Combo(const char* label, int& current_item_index, const std::vector<std::string>& items)
{
	std::string name = items[current_item_index];
	if (ImGui::Combo(label, name, items))
	{
		for (UINT index = 0; index < items.size(); ++index)
		{
			if (items[index] == name)
			{
				current_item_index = index;
				break;
			}
		}
		return true;
	}
	return false;
}

bool ImGui::Combo(const char* label, std::size_t& current_item_index, const std::vector<std::string>& items)
{
	std::string name = items[current_item_index];
	if (ImGui::Combo(label, name, items))
	{
		for (UINT index = 0; index < items.size(); ++index)
		{
			if (items[index] == name)
			{
				current_item_index = index;
				break;
			}
		}
		return true;
	}
	return false;
}

bool ImGui::Combo(const char* label, std::string& current_item, const std::vector<std::string>& items)
{
	bool changed = false;

	if (ImGui::BeginCombo(label, system_utility::convert_utf8(current_item).data()))
	{
		for (int n = 0; n < items.size(); n++)
		{
			bool is_selected = (current_item == items[n]);
			if (ImGui::Selectable(system_utility::convert_utf8(items[n]).data(), is_selected))
			{
				current_item = items[n];
				changed = true;
			}
			if (is_selected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	if (changed)
	{
		ImGuiWindow* window = GetCurrentWindow();
		const ImGuiID id = window->GetID(label);
		MarkItemEdited(id);
	}
	return changed;
}

bool ImGui::InputText(const char* label, std::string& str, ImGuiInputTextFlags flags)
{
	flags |= ImGuiInputTextFlags_CallbackResize;

	return ImGui::InputText(
		label,
		str.data(),
		str.capacity() + 1,
		flags,
		input_text_callback,
		&str
	);
}

bool ImGui::InputText(const char* label, std::string& str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
	flags |= ImGuiInputTextFlags_CallbackResize;

	input_text_callback_chain chain{ &str, callback, user_data };
	return ImGui::InputText(label, str.data(), str.capacity() + 1, flags, input_text_chain_callback, &chain);
}

bool ImGui::InputTextWithHint(const char* label, const char* hint, std::string& str, ImGuiInputTextFlags flags)
{
	flags |= ImGuiInputTextFlags_CallbackResize;

	return ImGui::InputTextWithHint(label, hint, str.data(), str.capacity() + 1, flags, input_text_callback, &str);
}

bool ImGui::InputTextUtf8(const char* label, std::string& str, ImGuiInputTextFlags flags)
{
	flags |= ImGuiInputTextFlags_CallbackResize;

	std::string text = system_utility::convert_utf8(str);
	if (ImGui::InputText(label, text.data(), text.capacity() + 1, flags, input_text_callback, &text))
	{
		str = system_utility::convert_shift_jis(text);
		return true;
	}
	return false;
}

bool ImGui::InputTextUtf8(const char* label, cache_string& str, ImGuiInputTextFlags flags)
{
	flags |= ImGuiInputTextFlags_CallbackResize;

	std::string text = str.get<utf8_cache>();
	if (ImGui::InputText(label, text.data(), text.capacity() + 1, flags, input_text_callback, &text))
	{
		str = system_utility::convert_shift_jis(text);
		return true;
	}
	return false;
}

bool ImGui::TreeNodeUtf8(const char* label)
{
	return ImGui::TreeNode(system_utility::convert_utf8(label).data());
}

//bool ImGui::SliderAngle(const char* label, float* radian, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
//{
//	float angle = DirectX::XMConvertToDegrees(*radian);
//	if (ImGui::SliderFloat(label, &angle, v_min, v_max, format, flags))
//	{
//		*radian = DirectX::XMConvertToRadians(angle);
//		return true;
//	}
//	return false;
//}

bool ImGui::SliderAngle2(const char* label, float* radians, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
	float angles[] =
	{
		math::angle::to_degree(radians[0]),
		math::angle::to_degree(radians[1]),
	};
	if (ImGui::SliderFloat2(label, angles, v_min, v_max, format, flags))
	{
		radians[0] = math::angle::to_radian(angles[0]);
		radians[1] = math::angle::to_radian(angles[1]);
		return true;
	}
	return false;
}

bool ImGui::SliderAngle3(const char* label, float* radians, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
	float angles[] =
	{
		math::angle::to_degree(radians[0]),
		math::angle::to_degree(radians[1]),
		math::angle::to_degree(radians[2]),
	};
	if (ImGui::SliderFloat3(label, angles, v_min, v_max, format, flags))
	{
		radians[0] = math::angle::to_radian(angles[0]);
		radians[1] = math::angle::to_radian(angles[1]);
		radians[2] = math::angle::to_radian(angles[2]);
		return true;
	}
	return false;
}

bool ImGui::SliderAngle4(const char* label, float* radians, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
	float angles[] =
	{
		math::angle::to_degree(radians[0]),
		math::angle::to_degree(radians[1]),
		math::angle::to_degree(radians[2]),
		math::angle::to_degree(radians[3]),
	};
	if (ImGui::SliderFloat4(label, angles, v_min, v_max, format, flags))
	{
		radians[0] = math::angle::to_radian(angles[0]);
		radians[1] = math::angle::to_radian(angles[1]);
		radians[2] = math::angle::to_radian(angles[2]);
		radians[3] = math::angle::to_radian(angles[3]);
		return true;
	}
	return false;
}

bool ImGui::DragAngle(const char* label, float* radian, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
	float angle = math::angle::to_degree(*radian);
	if (ImGui::DragFloat(label, &angle, v_speed, v_min, v_max, format, flags))
	{
		*radian = math::angle::to_radian(angle);
		return true;
	}
	return false;
}

bool ImGui::DragAngle2(const char* label, float* radians, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
	float angles[] =
	{
		math::angle::to_degree(radians[0]),
		math::angle::to_degree(radians[1]),
	};
	if (ImGui::DragFloat2(label, angles, v_speed, v_min, v_max, format, flags))
	{
		radians[0] = math::angle::to_radian(angles[0]);
		radians[1] = math::angle::to_radian(angles[1]);
		return true;
	}
	return false;
}

bool ImGui::DragAngle3(const char* label, float* radians, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
	float angles[] =
	{
		math::angle::to_degree(radians[0]),
		math::angle::to_degree(radians[1]),
		math::angle::to_degree(radians[2]),
	};
	if (ImGui::DragFloat3(label, angles, v_speed, v_min, v_max, format, flags))
	{
		radians[0] = math::angle::to_radian(angles[0]);
		radians[1] = math::angle::to_radian(angles[1]);
		radians[2] = math::angle::to_radian(angles[2]);
		return true;
	}
	return false;
}

bool ImGui::DragAngle4(const char* label, float* radians, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags)
{
	float angles[] =
	{
		math::angle::to_degree(radians[0]),
		math::angle::to_degree(radians[1]),
		math::angle::to_degree(radians[2]),
		math::angle::to_degree(radians[3]),
	};
	if (ImGui::DragFloat4(label, angles, v_speed, v_min, v_max, format, flags))
	{
		radians[0] = math::angle::to_radian(angles[0]);
		radians[1] = math::angle::to_radian(angles[1]);
		radians[2] = math::angle::to_radian(angles[2]);
		radians[3] = math::angle::to_radian(angles[3]);
		return true;
	}
	return false;
}

bool ImGui::DragUInt(const char* label, UINT* v, float v_speed, UINT v_min, UINT v_max, ImGuiSliderFlags flags)
{
	int value = *v;
	if (DragInt(label, &value, v_speed, v_min, v_max, "%d", flags))
	{
		*v = value;
		return true;
	}
	return false;
}

bool ImGui::DragUInt2(const char* label, UINT v[2], float v_speed, UINT v_min, UINT v_max, ImGuiSliderFlags flags)
{
	int value[] =
	{
		static_cast<int>(v[0]),
		static_cast<int>(v[1])
	};
	if (DragInt2(label, value, v_speed, v_min, v_max, "%d", flags))
	{
		v[0] = static_cast<UINT>(value[0]);
		v[1] = static_cast<UINT>(value[1]);
		return true;
	}
	return false;
}

bool ImGui::DragUInt3(const char* label, UINT v[3], float v_speed, UINT v_min, UINT v_max, ImGuiSliderFlags flags)
{
	int value[] =
	{
		static_cast<int>(v[0]),
		static_cast<int>(v[1]),
		static_cast<int>(v[2])
	};
	if (DragInt3(label, value, v_speed, v_min, v_max, "%d", flags))
	{
		v[0] = static_cast<UINT>(value[0]);
		v[1] = static_cast<UINT>(value[1]);
		v[2] = static_cast<UINT>(value[2]);
		return true;
	}
	return false;
}

bool ImGui::DragUInt4(const char* label, UINT v[4], float v_speed, UINT v_min, UINT v_max, ImGuiSliderFlags flags)
{
	int value[] =
	{
		static_cast<int>(v[0]),
		static_cast<int>(v[1]),
		static_cast<int>(v[2]),
		static_cast<int>(v[3])
	};
	if (DragInt4(label, value, v_speed, v_min, v_max, "%d", flags))
	{
		v[0] = static_cast<UINT>(value[0]);
		v[1] = static_cast<UINT>(value[1]);
		v[2] = static_cast<UINT>(value[2]);
		v[3] = static_cast<UINT>(value[3]);
		return true;
	}
	return false;
}

bool ImGui::ColorEditGamut3(const char* label, float color[3], ImGuiColorEditFlags flags)
{
	constexpr color_space::gamuts value_gamut = color_space::rendering_gamut;

	bool edited = false;
	PushID(label);
	BeginGroup();

	const math::vector3f value(color[0], color[1], color[2]);
	auto write = [&](const math::vector3f& result)
	{
		color[0] = result[0];
		color[1] = result[1];
		color[2] = result[2];
		edited = true;
	};

	const ImGuiStyle& style = GetStyle();
	const float values_width = (std::max)(CalcItemWidth() - GetFrameHeight() - style.ItemInnerSpacing.x, GetFontSize() * 6.0f);

	//	見本とピッカー(表示は sRGB。表示色域の外は切り詰めて見せる)
	const math::vector3f display_linear = color_space::convert_gamut(value, value_gamut, color_space::gamuts::rec709);
	const bool out_of_display = display_linear[0] < 0.0f || display_linear[1] < 0.0f || display_linear[2] < 0.0f;
	const math::vector3f display = color_space::convert_linear_to_srgb(display_linear);
	float display_color[3] = { display[0], display[1], display[2] };
	if (ColorEdit3("##display", display_color, flags | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel))
	{
		//	ピッカーで触った時だけ書き戻す(触っていない時に往復で値が変わらないように)
		const math::vector3f edited_linear = color_space::convert_srgb_to_linear(math::vector3f(display_color[0], display_color[1], display_color[2]));
		write(color_space::convert_gamut(edited_linear, color_space::gamuts::rec709, value_gamut));
	}
	if (out_of_display && IsItemHovered())
		SetTooltip("outside the display gamut (sRGB): editing with the picker clips it");

	//	数値
	float input_values[3] = { color[0], color[1], color[2] };
	SameLine(0.0f, style.ItemInnerSpacing.x);
	SetNextItemWidth(values_width);
	if (DragFloat3("##values", input_values, 0.001f, 0.0f, 0.0f, "%.4f"))
		write(math::vector3f(input_values[0], input_values[1], input_values[2]));

	//	表示色域の外なら印を出す
	if (out_of_display)
	{
		SameLine(0.0f, style.ItemInnerSpacing.x);
		TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "!");
		if (IsItemHovered())
			SetTooltip("outside the display gamut (sRGB)");
	}

	//	ラベル
	const char* label_end = FindRenderedTextEnd(label);
	if (label != label_end)
	{
		SameLine(0.0f, style.ItemInnerSpacing.x);
		TextUnformatted(label, label_end);
	}

	EndGroup();
	PopID();
	return edited;
}

namespace
{
	//	横軸(0～1)を % で表示する
	int format_percent(double value, char* buffer, int size, void*)
	{
		return snprintf(buffer, size, "%.0f%%", value * 100.0);
	}
}

bool ImGui::CurveEdit(const char* label, std::vector<math::float2>& keys, const std::function<float(float)>& evaluate, const CurveEditSettings& settings)
{
	bool modified = false;
	PushID(label);
	ImGuiStorage* storage = GetStateStorage();
	const ImGuiID held_id = GetID("held");
	const ImGuiID range_id = GetID("range");

	TextUnformatted(label);

	//	縦の範囲(ドラッグ中は固定して、範囲が動かないようにする)
	const float value_min = settings.value_min;
	const float value_limit = settings.auto_range ? FLT_MAX : settings.value_max;
	float value_max = settings.value_max;
	if (settings.auto_range)
	{
		if (storage->GetBool(held_id, false))
		{
			value_max = storage->GetFloat(range_id, value_max);
		}
		else
		{
			for (const auto& key : keys)
				value_max = std::max(value_max, key.y * 1.1f);
			storage->SetFloat(range_id, value_max);
		}
	}
	value_max = std::max(value_max, value_min + 0.001f);

	const float line = GetTextLineHeight();
	const float height = settings.height > 0.0f ? settings.height : line * 7.0f;
	bool any_held = false;
	if (ImPlot::BeginPlot("##curve", ImVec2(-1.0f, height), ImPlotFlags_NoLegend | ImPlotFlags_NoMenus | ImPlotFlags_NoBoxSelect))
	{
		ImPlot::SetupAxes(settings.x_label, settings.y_label, ImPlotAxisFlags_NoMenus, ImPlotAxisFlags_NoMenus);
		ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, 1.0, ImPlotCond_Always);
		ImPlot::SetupAxisLimits(ImAxis_Y1, value_min, value_max, ImPlotCond_Always);
		ImPlot::SetupAxisFormat(ImAxis_X1, format_percent);
		ImPlot::SetupAxisFormat(ImAxis_Y1, "%.2f");

		//	変化なし(1.0)の基準線
		if (value_min <= 1.0f && 1.0f <= value_max)
		{
			const float base_xs[2] = { 0.0f, 1.0f };
			const float base_ys[2] = { 1.0f, 1.0f };
			ImPlot::SetNextLineStyle(ImVec4(0.8f, 0.8f, 0.5f, 0.5f), 1.0f);
			ImPlot::PlotLine("##base", base_xs, base_ys, 2);
		}

		//	カーブ
		constexpr int sample_count = 65;
		float xs[sample_count];
		float ys[sample_count];
		for (int index = 0; index < sample_count; ++index)
		{
			xs[index] = static_cast<float>(index) / static_cast<float>(sample_count - 1);
			ys[index] = evaluate(xs[index]);
		}
		ImPlot::SetNextLineStyle(ImVec4(0.47f, 0.78f, 1.0f, 1.0f), 2.0f);
		ImPlot::PlotLine("##curve", xs, ys, sample_count);

		//	キー(両端は x を固定し、隣のキーは追い越さない)
		const int last = static_cast<int>(keys.size()) - 1;
		int hovered_key = -1;
		int remove_key = -1;
		for (int index = 0; index <= last; ++index)
		{
			double x = keys[index].x;
			double y = keys[index].y;
			bool hovered = false;
			bool held = false;
			if (ImPlot::DragPoint(index, &x, &y, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 5.0f, ImPlotDragToolFlags_Delayed, nullptr, &hovered, &held))
			{
				float new_x = static_cast<float>(x);
				if (index == 0)
					new_x = 0.0f;
				else if (index == last)
					new_x = 1.0f;
				else
					new_x = std::clamp(new_x, keys[index - 1].x + 0.001f, keys[index + 1].x - 0.001f);
				keys[index] = math::float2{ new_x, std::clamp(static_cast<float>(y), value_min, value_limit) };
				modified = true;
			}
			if (hovered)
				hovered_key = index;
			any_held |= held;
			if (hovered && index != 0 && index != last && IsMouseClicked(ImGuiMouseButton_Right))
				remove_key = index;
		}

		if (remove_key >= 0)
		{
			keys.erase(keys.begin() + remove_key);
			modified = true;
		}
		//	空いている所をダブルクリックで追加
		else if (hovered_key < 0 && ImPlot::IsPlotHovered() && IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			const ImPlotPoint position = ImPlot::GetPlotMousePos();
			const float x = std::clamp(static_cast<float>(position.x), 0.001f, 0.999f);
			const float y = std::clamp(static_cast<float>(position.y), value_min, value_limit);
			auto it = std::upper_bound(keys.begin(), keys.end(), x, [](float value, const math::float2& key) { return value < key.x; });
			keys.insert(it, math::float2{ x, y });
			modified = true;
		}
		ImPlot::EndPlot();
	}
	storage->SetBool(held_id, any_held);
	TextDisabled("drag: move / double-click: add / right-click: remove");

	//	プリセット
	if (settings.show_presets)
	{
		auto preset = [&](const char* name, std::initializer_list<math::float2> values)
		{
			if (Button(name))
			{
				keys.assign(values);
				modified = true;
			}
		};
		preset("constant", { {0.0f, 1.0f}, {1.0f, 1.0f} });
		SameLine();
		preset("fade in", { {0.0f, 0.0f}, {1.0f, 1.0f} });
		SameLine();
		preset("fade out", { {0.0f, 1.0f}, {1.0f, 0.0f} });
		SameLine();
		preset("peak", { {0.0f, 0.0f}, {0.5f, 1.0f}, {1.0f, 0.0f} });
	}

	//	数値で直接編集
	if (TreeNode("keys"))
	{
		const float width = GetContentRegionAvail().x * 0.4f;
		for (int index = 0; index < static_cast<int>(keys.size()); ++index)
		{
			const int last = static_cast<int>(keys.size()) - 1;
			const bool is_end = index == 0 || index == last;
			PushID(index);

			BeginDisabled(is_end);
			float percent = keys[index].x * 100.0f;
			const float lower = is_end ? percent : keys[index - 1].x * 100.0f + 0.1f;
			const float upper = is_end ? percent : keys[index + 1].x * 100.0f - 0.1f;
			SetNextItemWidth(width);
			if (DragFloat("##x", &percent, 0.5f, lower, upper, "%.1f%%"))
			{
				keys[index].x = std::clamp(percent, lower, upper) * 0.01f;
				modified = true;
			}
			EndDisabled();

			SameLine();
			SetNextItemWidth(width);
			if (DragFloat("##y", &keys[index].y, 0.01f, value_min, value_limit, "%.2f"))
			{
				keys[index].y = std::clamp(keys[index].y, value_min, value_limit);
				modified = true;
			}

			bool removed = false;
			if (!is_end)
			{
				SameLine();
				if (SmallButton("x"))
				{
					keys.erase(keys.begin() + index);
					modified = true;
					removed = true;
				}
			}
			PopID();
			if (removed)
				break;
		}
		TreePop();
	}

	PopID();
	return modified;
}

bool ImGui::GradientEdit(const char* label, const GradientEditAccessor& accessor)
{
	bool modified = false;
	PushID(label);
	ImGuiStorage* storage = GetStateStorage();
	const ImGuiID selected_id = GetID("selected");
	const ImGuiID drag_id = GetID("drag");

	TextUnformatted(label);

	const float line = GetTextLineHeight();
	const float marker = line * 0.4f;
	const ImVec2 origin = GetCursorScreenPos();
	const float width = GetContentRegionAvail().x;
	const float bar_bottom = origin.y + line;
	InvisibleButton("##gradient", ImVec2(width, line + marker * 2.0f), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
	const bool hovered = IsItemHovered();
	const bool active = IsItemActive();

	const float left = origin.x + marker;
	const float right = origin.x + width - marker;
	auto to_x = [&](float rate) { return left + (right - left) * rate; };
	auto to_rate = [&](float x) { return std::clamp((x - left) / (right - left), 0.0f, 1.0f); };
	auto to_display_color = [](float r, float g, float b)
	{
		//	表示色に変換する。値は描画用の色域のリニア値なので、ColorEditGamut3 の見本と同じく sRGB に変換して表示する
		//	表示用の変換なので、保存している値や頂点カラーには影響しない。表示色域の外は切り詰める
		const math::vector3f linear = color_space::convert_gamut(math::vector3f(r, g, b), color_space::rendering_gamut, color_space::gamuts::rec709);
		const math::vector3f display = color_space::convert_linear_to_srgb(math::vector3f(std::max(linear[0], 0.0f), std::max(linear[1], 0.0f), std::max(linear[2], 0.0f)));
		return ColorConvertFloat4ToU32(ImVec4(std::clamp(display[0], 0.0f, 1.0f), std::clamp(display[1], 0.0f, 1.0f), std::clamp(display[2], 0.0f, 1.0f), 1.0f));
	};
	auto to_color = [&](const float* color) { return to_display_color(color[0], color[1], color[2]); };

	int selected = std::clamp(storage->GetInt(selected_id, 0), 0, accessor.count() - 1);
	int drag_index = active ? storage->GetInt(drag_id, -1) : -1;

	//	マウス下のキー(三角の範囲)
	const ImVec2 mouse = GetIO().MousePos;
	int hovered_key = -1;
	if ((hovered || active) && mouse.y >= bar_bottom)
	{
		for (int index = 0; index < accessor.count(); ++index)
		{
			if (fabsf(mouse.x - to_x(accessor.rate(index))) <= marker)
			{
				hovered_key = index;
				break;
			}
		}
	}

	//	選択とドラッグ(両端は 0 / 1 に固定し、隣のキーは追い越さない)
	if (IsItemClicked(ImGuiMouseButton_Left) && hovered_key >= 0)
		selected = drag_index = hovered_key;
	if (drag_index > 0 && drag_index < accessor.count() - 1 && IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
	{
		const float rate = std::clamp(to_rate(mouse.x), accessor.rate(drag_index - 1) + 0.001f, accessor.rate(drag_index + 1) - 0.001f);
		if (accessor.rate(drag_index) != rate)
		{
			accessor.rate(drag_index) = rate;
			modified = true;
		}
	}
	storage->SetInt(drag_id, drag_index);

	//	バーをダブルクリックで追加(その位置の色で作る)
	if (hovered && hovered_key < 0 && IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		const float rate = std::clamp(to_rate(mouse.x), 0.001f, 0.999f);
		int insert_index = accessor.count();
		for (int index = 0; index < accessor.count(); ++index)
		{
			if (rate < accessor.rate(index))
			{
				insert_index = index;
				break;
			}
		}
		accessor.insert(insert_index, rate);
		selected = insert_index;
		modified = true;
	}
	//	三角を右クリックで削除(両端は削除しない)
	else if (hovered_key > 0 && hovered_key < accessor.count() - 1 && IsItemClicked(ImGuiMouseButton_Right))
	{
		accessor.erase(hovered_key);
		selected = std::min(selected, accessor.count() - 1);
		hovered_key = -1;
		modified = true;
	}
	storage->SetInt(selected_id, selected);

	//	描画
	ImDrawList* draw_list = GetWindowDrawList();
	//	キーの間を分割し、リニア値で補間した色を変換してから描く(表示色のまま補間すると実際の補間と見え方がずれるため)
	constexpr int segment_divisions = 8;
	for (int index = 0; index + 1 < accessor.count(); ++index)
	{
		const float start_rate = accessor.rate(index + 0);
		const float end_rate = accessor.rate(index + 1);
		for (int division = 0; division < segment_divisions; ++division)
		{
			const float rate0 = start_rate + (end_rate - start_rate) * static_cast<float>(division + 0) / segment_divisions;
			const float rate1 = start_rate + (end_rate - start_rate) * static_cast<float>(division + 1) / segment_divisions;
			const math::float3 color0 = accessor.evaluate(rate0);
			const math::float3 color1 = accessor.evaluate(rate1);
			const ImU32 s = to_display_color(color0.x, color0.y, color0.z);
			const ImU32 e = to_display_color(color1.x, color1.y, color1.z);
			draw_list->AddRectFilledMultiColor(ImVec2(to_x(rate0), origin.y), ImVec2(to_x(rate1), bar_bottom), s, e, e, s);
		}
	}
	draw_list->AddRect(ImVec2(left, origin.y), ImVec2(right, bar_bottom), IM_COL32(90, 90, 90, 255));
	for (int index = 0; index < accessor.count(); ++index)
	{
		const float x = to_x(accessor.rate(index));
		const ImVec2 top(x, bar_bottom);
		const ImVec2 bottom_left(x - marker, bar_bottom + marker * 2.0f);
		const ImVec2 bottom_right(x + marker, bar_bottom + marker * 2.0f);
		draw_list->AddTriangleFilled(top, bottom_left, bottom_right, to_color(accessor.color(index)));
		const ImU32 outline = index == selected ? IM_COL32_WHITE : (index == hovered_key ? IM_COL32(200, 200, 200, 255) : IM_COL32(20, 20, 20, 255));
		draw_list->AddTriangle(top, bottom_left, bottom_right, outline, 1.5f);
	}
	if (hovered)
		SetTooltip("%.0f%%", (hovered_key >= 0 ? accessor.rate(hovered_key) : to_rate(mouse.x)) * 100.0f);
	TextDisabled("click: select / drag: move / double-click: add / right-click: remove");

	//	選択中のキーの編集
	const int last = accessor.count() - 1;
	const bool is_end = selected == 0 || selected == last;
	BeginDisabled(is_end);
	float percent = accessor.rate(selected) * 100.0f;
	const float lower = is_end ? percent : accessor.rate(selected - 1) * 100.0f + 0.1f;
	const float upper = is_end ? percent : accessor.rate(selected + 1) * 100.0f - 0.1f;
	if (DragFloat("position", &percent, 0.5f, lower, upper, "%.1f%%"))
	{
		accessor.rate(selected) = std::clamp(percent, lower, upper) * 0.01f;
		modified = true;
	}
	EndDisabled();
	if (ColorEditGamut3("color", accessor.color(selected)))
		modified = true;

	PopID();
	return modified;
}
