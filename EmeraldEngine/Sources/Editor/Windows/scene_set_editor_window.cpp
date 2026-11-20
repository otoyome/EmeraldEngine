#include "Editor/Windows/scene_set_editor_window.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <filesystem>

#include "imgui.h"

#include "System/asset_browser.h"
#include "Resource/asset_file_header.h"
#include "Resource/asset_registry.h"
#include "Resource/resource_manager.h"
#include "Scenes/scene_resource_utility.h"
#include "Scenes/scene_set_resource.h"

namespace
{
	//	シーンのファイルか
	bool is_scene_path(std::string_view file_path)
	{
		return resource::get_asset_type_from_extension(std::filesystem::path(std::string(file_path)).extension().string()) == resource::asset_types::scene;
	}

	//	一覧に出す名前
	std::string make_label(std::string_view file_path)
	{
		const cache_string path(file_path);
		return scene_resource_utility::make_display_name(path.get<utf8_cache>());
	}
}

scene_set_editor_window::scene_set_editor_window(std::string_view file_path, open_scenes_function open_scenes)
	: file_path(file_path)
	, open_scenes(std::move(open_scenes))
{
	load();
}

scene_set_editor_window::~scene_set_editor_window()
{
}

void scene_set_editor_window::register_asset_handlers(std::shared_ptr<editor_items::context> context, open_scenes_function open_scenes)
{
	asset_browser::open_handler open;
	open.label = "Scene Set Editor";
	open.extensions = { std::string(".") + std::string(scene_set_resource::extension) };
	open.open_file = [context, open_scenes](const cache_string& file_path) mutable
	{
		editor_items::open_window<scene_set_editor_window>(context, file_path.view(), open_scenes);
	};
	asset_browser::instance()->register_open_handler(std::move(open));
}

std::string scene_set_editor_window::get_label() const
{
	const std::string name = std::filesystem::path(file_path).filename().string();
	return "Scene Set : " + system_utility::convert_shift_jis_to_utf8(name) + (dirty ? " *" : "");
}

std::string scene_set_editor_window::resolve_path(const resource::asset_reference<scene_resource>& reference)
{
	if (!asset_registry::is_valid())
		return std::string();
	return asset_registry::instance()->find_path(reference);
}

bool scene_set_editor_window::load()
{
	//	キャッシュに載せずに読む（保存前の編集が他から見えないようにする）
	const scene_set_resource_ptr resource = resource_manager::instance()->load_uncached<scene_set_resource>(file_path);
	if (!resource)
	{
		message = "load failed: " + system_utility::convert_shift_jis_to_utf8(file_path);
		return false;
	}
	scenes = resource->get_scenes();
	dirty = false;
	loaded = true;
	message.clear();
	return true;
}

bool scene_set_editor_window::save()
{
	const std::string file_path_utf8 = system_utility::convert_shift_jis_to_utf8(file_path);

	//	UUID を引き継ぐためファイルから読み直し、シーンの一覧だけ差し替えて保存する
	const scene_set_resource_ptr resource = resource_manager::instance()->load_uncached<scene_set_resource>(file_path);
	if (!resource)
	{
		message = "save failed: " + file_path_utf8;
		return false;
	}
	resource->set_scene_references(scenes);
	if (!resource_manager::instance()->save(resource, file_path))
	{
		message = "save failed: " + file_path_utf8;
		return false;
	}
	dirty = false;
	message = "saved: " + file_path_utf8;
	return true;
}

void scene_set_editor_window::on_file_moved(std::string_view new_file_path)
{
	file_path = std::string(new_file_path);
}

bool scene_set_editor_window::add_scene(std::string_view scene_path)
{
	if (!is_scene_path(scene_path))
		return false;

	resource::asset_reference<scene_resource> reference;
	if (asset_registry::is_valid())
		reference.uuid = asset_registry::instance()->find_uuid(scene_path);
	if (reference.uuid.is_empty())
	{
		message = "scene not registered: " + system_utility::convert_shift_jis_to_utf8(scene_path);
		return false;
	}

	const auto found = std::find_if(scenes.begin(), scenes.end(), [&](const resource::asset_reference<scene_resource>& scene) { return scene.uuid == reference.uuid; });
	if (found != scenes.end())
		return false;

	scenes.push_back(std::move(reference));
	dirty = true;
	return true;
}

void scene_set_editor_window::imgui_render_toolbar()
{
	ImGui::BeginDisabled(!dirty);
	if (ImGui::Button("save"))
		save();
	ImGui::SameLine();
	if (ImGui::Button("revert"))
		load();
	ImGui::EndDisabled();
	ImGui::SameLine();

	//	保存した内容で開くので、変更中は押せない
	ImGui::BeginDisabled(dirty || scenes.empty() || !open_scenes);
	if (ImGui::Button("open scenes"))
		open_scenes(file_path);
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("%s", dirty ? "* modified" : "");
}

void scene_set_editor_window::imgui_render_scene_list()
{
	//	並べ替え・削除はループの後でまとめて行う
	int move_from = -1;
	int move_to = -1;
	int remove_index = -1;

	const int count = static_cast<int>(scenes.size());
	for (int index = 0; index < count; ++index)
	{
		ImGui::PushID(index);
		ImGui::BeginDisabled(index == 0);
		if (ImGui::ArrowButton("up", ImGuiDir_Up))
		{
			move_from = index;
			move_to = index - 1;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(index == count - 1);
		if (ImGui::ArrowButton("down", ImGuiDir_Down))
		{
			move_from = index;
			move_to = index + 1;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::SmallButton("remove"))
			remove_index = index;
		ImGui::SameLine();

		const std::string reference_path = resolve_path(scenes[index]);
		if (reference_path.empty())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "(missing)");
		}
		else
		{
			ImGui::TextUnformatted(make_label(reference_path).c_str());
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", system_utility::convert_shift_jis_to_utf8(reference_path).c_str());
		}
		ImGui::PopID();
	}

	if (move_from >= 0)
	{
		std::swap(scenes[move_from], scenes[move_to]);
		dirty = true;
	}
	else if (remove_index >= 0)
	{
		scenes.erase(scenes.begin() + remove_index);
		dirty = true;
	}

	if (scenes.empty())
		ImGui::TextDisabled("drop scenes here from the asset browser");
}

void scene_set_editor_window::imgui_render_add_scene()
{
	if (!ImGui::BeginCombo("add scene", "(select)"))
		return;

	std::vector<std::string> candidates = asset_registry::is_valid() ? asset_registry::instance()->find_paths(resource::asset_types::scene) : std::vector<std::string>();
	std::sort(candidates.begin(), candidates.end());
	std::string selected;
	for (const std::string& candidate : candidates)
	{
		const universally_unique_identifier uuid = asset_registry::instance()->find_uuid(candidate);
		const bool contained = std::any_of(scenes.begin(), scenes.end(), [&](const resource::asset_reference<scene_resource>& scene) { return scene.uuid == uuid; });
		if (contained)
			continue;
		if (ImGui::Selectable(system_utility::convert_shift_jis_to_utf8(candidate).c_str()))
			selected = candidate;
	}
	ImGui::EndCombo();

	if (!selected.empty())
		add_scene(selected);
}

void scene_set_editor_window::render_window(std::shared_ptr<editor_items::context>& /*context*/, bool& open)
{
	const std::string title = get_label() + "###scene_set:" + system_utility::convert_shift_jis_to_utf8(file_path);
	ImGui::SetNextWindowSize(ImVec2(520.0f, 360.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(title.c_str(), &open))
	{
		if (loaded)
			imgui_render_toolbar();
		if (!message.empty())
			ImGui::TextUnformatted(message.c_str());
		ImGui::Separator();

		if (loaded)
		{
			imgui_render_add_scene();
			ImGui::TextDisabled("the first scene is the main scene in the scene editor");

			if (ImGui::BeginChild("scenes", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
				imgui_render_scene_list();
			ImGui::EndChild();

			//	アセットブラウザーからシーンを落とすと足す
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(asset_browser::drag_payload_type))
					add_scene(static_cast<const char*>(payload->Data));
				ImGui::EndDragDropTarget();
			}
		}
	}
	ImGui::End();
}
#endif	//	defined(USE_IMGUI)
