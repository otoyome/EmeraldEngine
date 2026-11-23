#pragma once

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

#include <magic_enum.hpp>

#include <functional>
#include <vector>

#include "Core/cache_string.h"
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
    bool InputTextUtf8(const char* label, core::cache_string& str, ImGuiInputTextFlags flags = 0);

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

    bool ColorEditGamut3(const char* label, float color[3], ImGuiColorEditFlags flags = 0);

    //	カーブ編集の設定
    //	カーブ編集(ImPlot で描画)
    //	keys は x = 0～1 の昇順。両端のキーは x=0 / x=1 に固定し、削除しない
    //	evaluate は補間関数
    //	操作:ドラッグで移動 / 空いている所をダブルクリックで追加 / キーを右クリックで削除
    struct CurveEditSettings
    {
        const char* x_label = "life";	//	横軸の名前
        const char* y_label = "value";	//	縦軸の名前
        float value_min = 0.0f;	//	値の下限
        float value_max = 1.0f;	//	値の上限
        bool auto_range = false;	//	キーに合わせて縦の表示範囲を広げる
        bool show_presets = true;	//	プリセットボタンを表示
        float height = 0.0f;	//	グラフの高さ
    };

    bool CurveEdit(const char* label, std::vector<math::float2>& keys, const std::function<float(float)>& evaluate, const CurveEditSettings& settings = {});

    //	グラデーション編集でキーを操作するための関数群
    struct GradientEditAccessor
    {
        std::function<int()> count;	//	キー数
        std::function<float&(int)> rate;	//	キーの位置(0～1)
        std::function<float*(int)> color;	//	キーの色
        std::function<void(int, float)> insert;	//	指定番号の位置に、指定位置のキーを追加
        std::function<void(int)> erase;	//	キーを削除
        std::function<math::float3(float)> evaluate;	//	補間関数
    };

    //	カラーグラデーション編集
    //	両端のキーは 0 / 1 に固定し、削除しない
    //	操作:三角をクリックで選択 / ドラッグで移動 / バーをダブルクリックで追加 / 三角を右クリックで削除
    bool GradientEdit(const char* label, const GradientEditAccessor& accessor);

    //	rate(float) と color(math::float3) を持つキー配列用
    template<class Key, class Evaluate>
    bool GradientEdit(const char* label, std::vector<Key>& keys, Evaluate&& evaluate)
    {
        GradientEditAccessor accessor;
        accessor.count = [&]() { return static_cast<int>(keys.size()); };
        accessor.rate = [&](int index) -> float& { return keys[index].rate; };
        accessor.color = [&](int index) { return &keys[index].color.x; };
        accessor.insert = [&](int index, float rate)
        {
            Key key{};
            key.rate = rate;
            key.color = evaluate(rate);
            keys.insert(keys.begin() + index, key);
        };
        accessor.erase = [&](int index) { keys.erase(keys.begin() + index); };
        accessor.evaluate = [&](float rate) { return evaluate(rate); };
        return GradientEdit(label, accessor);
    }

    template<class StatesEnum, class T>
    bool EditFlags(T& obj)
    {
        bool edited_flag = false;
        for (auto flag : magic_enum::enum_values<StatesEnum>())
        {
            bool checked = core::is_bitflag(obj, flag);
            std::string label(magic_enum::enum_name(flag));
            if (Checkbox(label.c_str(), &checked))
            {
	            core::set_bitflag(obj, flag, checked);
                edited_flag = true;
            }
        }

        return edited_flag;
    }
}
