#include "Editor/scenes_window.h"

#if defined(USE_IMGUI)
#include <string>
#include <vector>

#include "imgui.h"
#include "Editor/scene_editor.h"
#include "Object/world.h"
#include "Scenes/scene_resource_utility.h"

void scenes_window::imgui_render(scene_editor& editor)
{
	//	編集中は scenes 窓に編集中のシーンを出す
	if (ImGui::Begin(editor.get_edit_world().get_context().make_window_name("scenes").c_str()))
	{
		const std::vector<scene_resource_ptr> scenes = editor.get_edited_scenes();
		const scene_resource_ptr current_scene = editor.get_current_scene_resource();
		for (size_t index = 0; index < scenes.size(); ++index)
		{
			const scene_resource_ptr& scene = scenes[index];
			ImGui::PushID(scene.get());
			if (index > 0)
				ImGui::Indent();
			//	シーン名だけを出し、パスはカーソルを乗せた時に出す。クリックするとオブジェクトを置く先のシーンにする
			const cache_string path(scene->get_file_path());
			const std::string display_name = scene_resource_utility::make_display_name(path.get<utf8_cache>());
			const bool current = (scene == current_scene);
			if (ImGui::Selectable(display_name.c_str(), current, ImGuiSelectableFlags_None, ImVec2(ImGui::CalcTextSize(display_name.c_str()).x, 0.0f)))
				editor.set_current_scene(scene);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", path.get<utf8_cache>().data());
			ImGui::SameLine();
			if (ImGui::SmallButton("edit"))
				editor.open_edit_window(scene);
			if (index > 0)
				ImGui::Unindent();
			ImGui::PopID();
		}

		//	読み込めなかったサブシーンはパスだけ出す
		for (const std::string& unloaded_path : editor.get_unloaded_sub_scenes())
		{
			const cache_string path(unloaded_path);
			ImGui::Indent();
			ImGui::TextDisabled("%s (not loaded)", scene_resource_utility::make_display_name(path.get<utf8_cache>()).c_str());
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", path.get<utf8_cache>().data());
			ImGui::Unindent();
		}
	}
	ImGui::End();
}
#endif	//	defined(USE_IMGUI)
