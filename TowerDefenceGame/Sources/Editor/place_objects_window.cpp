#include "Editor/place_objects_window.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <cctype>
#include <string>

#include "imgui.h"
#include "Editor/scene_editor.h"
#include "Object/world.h"
#include "Object/game_object_registry.h"
#include "Object/spatial_object.h"
#include "Scenes/scene_resource_utility.h"

bool place_objects_window::is_placeable_type(std::string_view type)
{
	return type != game_object::type_name();
}

void place_objects_window::imgui_render(scene_editor& editor)
{
	//	å^ÇÃàÍóó
	if (ImGui::Begin(editor.get_edit_world().get_context().make_window_name("place objects").c_str()))
	{
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##filter", "search", filter, sizeof(filter));

		//	åüçıÇÕëÂï∂éöÅEè¨ï∂éöÇãÊï ÇµÇ»Ç¢
		auto to_lower = [](std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		};
		const std::string lower_filter = to_lower(filter);

		if (game_object_registry::is_valid())
		{
			for (const std::string& type : game_object_registry::instance()->get_type_names())
			{
				if (!is_placeable_type(type))
					continue;
				if (!lower_filter.empty() && to_lower(type).find(lower_filter) == std::string::npos)
					continue;

				ImGui::Selectable(type.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					editor.place_object(type);
				if (ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(payload_type, type.c_str(), type.size() + 1);
					ImGui::TextUnformatted(type.c_str());
					ImGui::EndDragDropSource();
				}
			}
		}
	}
	ImGui::End();
}
#endif	//	defined(USE_IMGUI)
