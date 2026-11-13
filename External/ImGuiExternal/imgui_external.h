#pragma once

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <magic_enum.hpp>

#include "system/cache_string.h"
#include "Graphics/Core/color_space.h"

namespace ImGui
{
    bool Combo(const char* label, int& current_item_index, const std::vector<std::string_view>& items);
    bool Combo(const char* label, std::size_t& current_item_index, const std::vector<std::string_view>& items);
    bool Combo(const char* label, std::string& current_item, const std::vector<std::string_view>& items);

    bool Combo(const char* label, int& current_item_index, const std::vector<std::string>& items);
    bool Combo(const char* label, std::size_t& current_item_index, const std::vector<std::string>& items);
    bool Combo(const char* label, std::string& current_item, const std::vector<std::string>& items);

    template<typename type>
    bool Combo(const char* label, type& current_item_index)
    {
		auto rv = magic_enum::enum_names<type>();
		std::vector<std::string_view> names(rv.begin(), rv.end());
        int id = static_cast<int>(current_item_index);
		if (ImGui::Combo(label, id, names))
        {
            current_item_index = static_cast<type>(id);
            return true;
        }
        return false;
    }
    template<typename type>
    bool Combo(const char* label, int& current_item_index)
    {
		auto rv = magic_enum::enum_names<type>();
		std::vector<std::string_view> names(rv.begin(), rv.end());
		return ImGui::Combo(label, current_item_index, names);
    }
    template<typename type>
    bool Combo(const char* label, std::size_t& current_item_index)
    {
		auto rv = magic_enum::enum_names<type>();
		std::vector<std::string_view> names(rv.begin(), rv.end());
		return ImGui::Combo(label, current_item_index, names);
    }
    template<typename type>
    bool Combo(const char* label, std::string& current_item)
    {
		auto rv = magic_enum::enum_names<type>();
		std::vector<std::string_view> names(rv.begin(), rv.end());
		return ImGui::Combo(label, current_item, names);
    }

    bool InputText(const char* label, std::string& str, ImGuiInputTextFlags flags = 0);
    bool InputText(const char* label, std::string& str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data = nullptr);
    bool InputTextWithHint(const char* label, const char* hint, std::string& str, ImGuiInputTextFlags flags = 0);

    //  SJIS>UTF8に内部で変換するタイプ
    bool InputTextUtf8(const char* label, std::string& str, ImGuiInputTextFlags flags = 0);
    bool InputTextUtf8(const char* label, cache_string& str, ImGuiInputTextFlags flags = 0);

    bool TreeNodeUtf8(const char* label);

    //  角度用。内部でラジアン角をディグリー角に変えてる
//    bool SliderAngle(const char* label, float* radian, float v_min = -360, float v_max = +360, const char* format = "%.3f", ImGuiSliderFlags flags = 0); <= 既にあった
    bool SliderAngle2(const char* label, float* radians, float v_min = -360, float v_max = +360, const char* format = "%.0f deg", ImGuiSliderFlags flags = 0);
    bool SliderAngle3(const char* label, float* radians, float v_min = -360, float v_max = +360, const char* format = "%.0f deg", ImGuiSliderFlags flags = 0);
    bool SliderAngle4(const char* label, float* radians, float v_min = -360, float v_max = +360, const char* format = "%.0f deg", ImGuiSliderFlags flags = 0);
    bool DragAngle(const char* label, float* radian, float v_speed = 0.01f, float v_min = -360, float v_max = 360, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
    bool DragAngle2(const char* label, float* radians, float v_speed = 0.01f, float v_min = -360, float v_max = 360, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
    bool DragAngle3(const char* label, float* radians, float v_speed = 0.01f, float v_min = -360, float v_max = 360, const char* format = "%.3f", ImGuiSliderFlags flags = 0);
    bool DragAngle4(const char* label, float* radians, float v_speed = 0.01f, float v_min = -360, float v_max = 360, const char* format = "%.3f", ImGuiSliderFlags flags = 0);

    bool DragUInt(const char* label, UINT* v, float v_speed = 1.0f, UINT v_min = 0, UINT v_max = 0, ImGuiSliderFlags flags = 0);
    bool DragUInt2(const char* label, UINT v[2], float v_speed = 1.0f, UINT v_min = 0, UINT v_max = 0, ImGuiSliderFlags flags = 0);
    bool DragUInt3(const char* label, UINT v[3], float v_speed = 1.0f, UINT v_min = 0, UINT v_max = 0, ImGuiSliderFlags flags = 0);
    bool DragUInt4(const char* label, UINT v[4], float v_speed = 1.0f, UINT v_min = 0, UINT v_max = 0, ImGuiSliderFlags flags = 0);

    //  色域を指定して色を編集する
    bool ColorEditGamut3(const char* label, float color[3], color_space::gamuts value_gamut, ImGuiColorEditFlags flags = 0);

    template<class StatesEnum, class T>
    bool EditFlags(T& obj)
    {
        bool edited_flag = false;
        for (auto flag : magic_enum::enum_values<StatesEnum>())
        {
            bool checked = system_utility::is_bitflag(obj, flag);
            std::string label(magic_enum::enum_name(flag));
            if (Checkbox(label.c_str(), &checked))
            {
	            system_utility::set_bitflag(obj, flag, checked);
                edited_flag = true;
            }
        }

        return edited_flag;
    }
}
