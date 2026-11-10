#include "Renderer/debug_view_settings.h"

#if defined(USE_IMGUI)
#include "imgui.h"

void debug_view_settings::imgui_menu()
{
	if (ImGui::BeginMenu("View"))
	{
		ImGui::MenuItem("Grid XZ", nullptr, &grid_xz);
		ImGui::MenuItem("Grid XY", nullptr, &grid_xy);
		ImGui::MenuItem("Grid YZ", nullptr, &grid_yz);
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::DragInt("Grid length (m)", &grid_length, 1.0f, 1, 10000))
			grid_length = grid_length < 1 ? 1 : grid_length;
		ImGui::Separator();
		ImGui::MenuItem("Mesh AABB", nullptr, &mesh_aabb);
		ImGui::MenuItem("Collision", nullptr, &collision);
		ImGui::EndMenu();
	}
}
#endif	//	defined(USE_IMGUI)
