#pragma once

#if defined(USE_IMGUI)
#include <filesystem>
#include <string>
#include <vector>

#include "imgui.h"
#include "Resource/asset_reference.h"
#include "Resource/asset_registry.h"
#include "Resource/asset_file_header.h"
#include "System/asset_browser.h"

//	アセットへの参照をインスペクターで選ぶ
class asset_reference_field
{
public:
	template<class Type>
	static bool imgui_combo(const char* label, resource::asset_reference<Type>& reference, resource::asset_types type)
	{
		bool changed = false;
		const std::string current_path = reference.is_empty() || !asset_registry::is_valid() ? std::string() : asset_registry::instance()->find_path(reference);
		const std::string current_label = current_path.empty() ? std::string("(none)") : core::convert_shift_jis_to_utf8(current_path);
		if (ImGui::BeginCombo(label, current_label.c_str()))
		{
			if (ImGui::Selectable("(none)", reference.is_empty()))
			{
				changed = !reference.is_empty();
				reference = {};
			}
			const std::vector<std::string> paths = asset_registry::is_valid() ? asset_registry::instance()->find_paths(type) : std::vector<std::string>();
			for (const std::string& path : paths)
			{
				const bool selected = path == current_path;
				if (ImGui::Selectable(core::convert_shift_jis_to_utf8(path).c_str(), selected))
				{
					if (set_path(reference, path))
						changed = true;
				}
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		//	アセットブラウザからドロップ
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(asset_browser::drag_payload_type))
			{
				const std::string file_path(static_cast<const char*>(payload->Data));
				const std::filesystem::path extension = std::filesystem::path(file_path).extension();
				const resource::asset_types file_type = resource::get_asset_type_from_extension(extension.string());
				if (file_type == type && set_path(reference, file_path))
					changed = true;
			}
			ImGui::EndDragDropTarget();
		}
		return changed;
	}

private:
	//	パスからUUIDを引いて設定(変化が無ければfalse)
	template<class Type>
	static bool set_path(resource::asset_reference<Type>& reference, const std::string& path)
	{
		const core::uuid uuid = asset_registry::is_valid() ? asset_registry::instance()->find_uuid(path) : core::uuid();
		if (uuid.is_empty() || uuid == reference.uuid)
			return false;
		reference.uuid = uuid;
		return true;
	}
};
#endif	//	defined(USE_IMGUI)
