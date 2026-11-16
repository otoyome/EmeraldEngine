#include "Editor/inspector_window.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <memory>
#include <vector>

#include "imgui.h"
#include "Object/world.h"
#include "Object/object_manager.h"

void inspector_window::imgui_render(world& target)
{
	if (ImGui::Begin("inspector"))
	{
		const std::shared_ptr<game_object> selected = target.get_context().get_selected_object();
		const std::vector<std::shared_ptr<game_object>> world_objects = target.get_objects().search_game_objects<game_object>();
		if (selected && std::find(world_objects.begin(), world_objects.end(), selected) != world_objects.end())
			selected->imgui_render_inspector();
		else
			ImGui::TextDisabled("(no selection)");
	}
	ImGui::End();
}
#endif	//	defined(USE_IMGUI)
