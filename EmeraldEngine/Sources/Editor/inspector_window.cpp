#include "Editor/inspector_window.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <memory>
#include <vector>

#include "imgui.h"
#include "Editor/scene_editor.h"
#include "Object/world.h"
#include "Object/object_manager.h"
#include "Scenes/scene_resource_utility.h"

void inspector_window::imgui_render(world& target)
{
	if (ImGui::Begin("inspector"))
	{
		if (!render_selected_object(target))
			ImGui::TextDisabled("(no selection)");
	}
	ImGui::End();
}

void inspector_window::imgui_render(scene_editor& editor)
{
	if (ImGui::Begin("inspector"))
	{
		if (!render_selected_object(editor.get_edit_world()))
		{
			const scene_resource_ptr& resource = editor.get_current_scene_resource();
			if (resource)
			{
				const core::cache_string path(resource->get_file_path());
				ImGui::TextUnformatted(scene_resource_utility::make_display_name(path.get<core::utf8_cache>()).c_str());
				editor.imgui_render_scene_settings(resource);
			}
			else
			{
				ImGui::TextDisabled("(no selection)");
			}
		}
	}
	ImGui::End();
}

void inspector_window::imgui_render_contents(engine_context& target)
{
	if (!render_selected_object(target))
		ImGui::TextDisabled("(no selection)");
}

bool inspector_window::render_selected_object(world& target)
{
	return render_selected_object(target.get_context());
}

bool inspector_window::render_selected_object(engine_context& target)
{
	const std::shared_ptr<game_object> selected = target.get_editor().get_selected_object();
	const std::vector<std::shared_ptr<game_object>> world_objects = target.get_objects().search_game_objects<game_object>();
	if (!selected || std::find(world_objects.begin(), world_objects.end(), selected) == world_objects.end())
		return false;
	selected->imgui_render_inspector();
	return true;
}
#endif	//	defined(USE_IMGUI)
