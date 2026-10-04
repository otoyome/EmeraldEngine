#include "ImGuiExternal/imgui_external.h"

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
