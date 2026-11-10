#include "Renderer/debug_view_settings.h"

#if defined(USE_IMGUI)
#include <stdio.h>

#include "imgui.h"
#include "imgui_internal.h"

void debug_view_settings::imgui_menu()
{
	if (ImGui::BeginMenu("View"))
	{
		bool changed = false;
		changed |= ImGui::MenuItem("Grid XZ", nullptr, &grid_xz);
		changed |= ImGui::MenuItem("Grid XY", nullptr, &grid_xy);
		changed |= ImGui::MenuItem("Grid YZ", nullptr, &grid_yz);
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::DragInt("Grid length (m)", &grid_length, 1.0f, 1, 10000))
		{
			grid_length = grid_length < 1 ? 1 : grid_length;
			changed = true;
		}
		ImGui::Separator();
		changed |= ImGui::MenuItem("Mesh AABB", nullptr, &mesh_aabb);
		changed |= ImGui::MenuItem("Collision", nullptr, &collision);
		ImGui::EndMenu();

		if (changed)
			ImGui::MarkIniSettingsDirty();
	}
}

debug_view_settings_manager::debug_view_settings_manager()
{
	register_settings_handler();
}

debug_view_settings& debug_view_settings_manager::get(std::string_view key)
{
	auto it = settings.find(key);
	if (it == settings.end())
		it = settings.emplace(std::string(key), debug_view_settings{}).first;
	return it->second;
}

void debug_view_settings_manager::register_settings_handler()
{
	ImGuiSettingsHandler handler;
	handler.TypeName = "DebugView";
	handler.TypeHash = ImHashStr(handler.TypeName);
	handler.UserData = this;

	//	[DebugView][キー] の項目を開く
	handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler* self, const char* name) -> void*
	{
		auto* manager = static_cast<debug_view_settings_manager*>(self->UserData);
		return &manager->get(name);
	};

	//	行単位で処理
	handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line)
	{
		auto& target = *static_cast<debug_view_settings*>(entry);
		int value = 0;
		if (sscanf_s(line, "GridXZ=%d", &value) == 1)			target.grid_xz = value != 0;
		else if (sscanf_s(line, "GridXY=%d", &value) == 1)		target.grid_xy = value != 0;
		else if (sscanf_s(line, "GridYZ=%d", &value) == 1)		target.grid_yz = value != 0;
		else if (sscanf_s(line, "GridLength=%d", &value) == 1)	target.grid_length = value < 1 ? 1 : value;
		else if (sscanf_s(line, "MeshAABB=%d", &value) == 1)		target.mesh_aabb = value != 0;
		else if (sscanf_s(line, "Collision=%d", &value) == 1)	target.collision = value != 0;
	};

	//	すべてのキーを書き出す
	handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* self, ImGuiTextBuffer* buffer)
	{
		auto* manager = static_cast<debug_view_settings_manager*>(self->UserData);
		for (const auto& [key, value] : manager->settings)
		{
			buffer->appendf("[%s][%s]\n", self->TypeName, key.c_str());
			buffer->appendf("GridXZ=%d\n", value.grid_xz ? 1 : 0);
			buffer->appendf("GridXY=%d\n", value.grid_xy ? 1 : 0);
			buffer->appendf("GridYZ=%d\n", value.grid_yz ? 1 : 0);
			buffer->appendf("GridLength=%d\n", value.grid_length);
			buffer->appendf("MeshAABB=%d\n", value.mesh_aabb ? 1 : 0);
			buffer->appendf("Collision=%d\n", value.collision ? 1 : 0);
			buffer->append("\n");
		}
	};

	ImGui::AddSettingsHandler(&handler);
}
#endif	//	defined(USE_IMGUI)
