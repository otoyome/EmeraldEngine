#include "Editor/outliner_window.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "imgui.h"
#include "Editor/scene_editor.h"
#include "Editor/object_edit_operations.h"
#include "Object/prefab_resource.h"
#include "Editor/place_objects_window.h"
#include "Object/world.h"
#include "Object/object_manager.h"
#include "Object/game_object_registry.h"
#include "Object/spatial_object.h"
#include "Scenes/scene_manager.h"
#include "Scenes/scene_instance.h"
#include "Scenes/scene_resource_utility.h"
#include "System/asset_browser.h"

void outliner_window::imgui_render(scene_editor& target_editor)
{
	std::vector<scene_row> scenes;
	for (const scene_resource_ptr& resource : target_editor.get_edited_scenes())
		scenes.push_back({ resource, target_editor.get_scene_objects(resource), nullptr });

	editor = &target_editor;
	operations = &target_editor;
	render_window(target_editor.get_edit_world(), scenes);
	operations = nullptr;
	editor = nullptr;
}

void outliner_window::imgui_render_contents(object_edit_operations& target_operations, const std::vector<std::shared_ptr<game_object>>& loose_objects)
{
	operations = &target_operations;
	render_contents(target_operations.get_edit_context(), {}, &loose_objects);
	operations = nullptr;
}

void outliner_window::imgui_render(world& target)
{
	std::vector<scene_row> scenes;
	for (const auto& scene : target.get_scenes().get_scenes())
	{
		scene_row& row = scenes.emplace_back();
		row.resource = scene.second->get_resource();
		row.system = scene.second->get_system();
		for (const auto& weak_object : scene.second->get_objects())
		{
			if (auto object = weak_object.lock(); object && !object->is_transient())
				row.objects.emplace_back(std::move(object));
		}
	}
	render_window(target, scenes);
}

void outliner_window::render_window(world& target, const std::vector<scene_row>& scenes)
{
	if (ImGui::Begin("outliner"))
		render_contents(target.get_context(), scenes, nullptr);
	ImGui::End();
}

void outliner_window::render_contents(engine_context& target, const std::vector<scene_row>& scenes, const std::vector<std::shared_ptr<game_object>>* loose_objects)
{
	context = &target;
	pending = nullptr;
	collect_objects(scenes, loose_objects);


	for (const scene_row& scene : scenes)
		render_scene(scene);

	//	シーンを持たないオブジェクトは、祖先にオブジェクトを持たないものを一番上に並べる
	if (loose_objects)
	{
		for (const std::shared_ptr<game_object>& object : *loose_objects)
		{
			if (object && !has_object_parent(find_node(object)))
				render_object(object);
		}
	}
	render_transient_objects(target);

	//	一覧の下の空いている所。クリックすると選択を外す
	const ImVec2 rest_size(std::max(ImGui::GetContentRegionAvail().x, 1.0f), std::max(ImGui::GetContentRegionAvail().y, ImGui::GetFrameHeight()));
	ImGui::InvisibleButton("##outliner_root", rest_size);
	if (ImGui::IsItemClicked())
		context->set_selected_object(nullptr);
	if (operations)
	{
		//	オブジェクトを落とすと親から外し、アセットブラウザからシーンを落とすと編集中のシーンに足す
		if (ImGui::BeginDragDropTarget())
		{
			object_edit_operations* target_operations = operations;
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payload_type))
			{
				if (const std::shared_ptr<game_object> child = find_payload_object(payload->Data))
					pending = [target_operations, child]() { target_operations->attach_object(child, nullptr); };
			}
			if (editor)
			{
				scene_editor* target_editor = editor;
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(asset_browser::drag_payload_type))
				{
					const std::string file_path(static_cast<const char*>(payload->Data));
					if (std::filesystem::path(file_path).extension().string() == "." + std::string(scene_resource::extension))
					{
						pending = [target_editor, file_path]() { target_editor->add_scene(file_path); };
					}
					else if (std::filesystem::path(file_path).extension().string() == "." + std::string(prefab_resource::extension))
					{
						pending = [target_editor, file_path]() { target_editor->place_prefab(file_path); };
					}
				}
			}
			ImGui::EndDragDropTarget();
		}
		if (ImGui::BeginPopupContextItem("outliner_menu"))
		{
			if (editor)
			{
				render_add_scene_menu();
			}
			//	一番上にオブジェクトを足す
			else if (ImGui::BeginMenu("add object"))
			{
				object_edit_operations* target_operations = operations;
				engine_context* target_context = context;
				render_type_menu([&](const std::string& type)
				{
					pending = [target_operations, target_context, type]()
					{
						target_context->set_selected_object(nullptr);
						target_operations->place_object(type);
					};
				});
				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}
	}

	//	木を並べ終えてから付け替える
	if (pending)
		pending();
	pending = nullptr;

	owners.clear();
	objects.clear();
	context = nullptr;
}

void outliner_window::render_scene(const scene_row& scene)
{
	ImGui::PushID(scene.resource.get());

	//	シーン名だけを出し、パスはカーソルを乗せた時に出す。オブジェクトを選んでいない時は、置く先のシーンを強調する
	const cache_string path(scene.resource->get_file_path());
	const std::string display_name = scene_resource_utility::make_display_name(path.get<utf8_cache>());
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
	if (editor && !context->get_selected_object() && scene.resource == editor->get_current_scene_resource())
		flags |= ImGuiTreeNodeFlags_Selected;
	const bool opened = ImGui::TreeNodeEx("##scene", flags, "%s", display_name.c_str());
	if (editor && ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		editor->set_current_scene(scene.resource);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
		ImGui::SetTooltip("%s", path.get<utf8_cache>().data());

	//	オブジェクトを落とすと、親から外してこのシーンへ移す
	if (editor && ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payload_type))
		{
			if (const std::shared_ptr<game_object> object = find_payload_object(payload->Data))
			{
				scene_editor* target_editor = editor;
				const scene_resource_ptr resource = scene.resource;
				pending = [target_editor, object, resource]() { target_editor->move_object_to_scene(object, resource); };
			}
		}
		ImGui::EndDragDropTarget();
	}

	if (ImGui::BeginPopupContextItem("scene_menu"))
	{
		render_scene_menu(scene);
		ImGui::EndPopup();
	}

	if (opened)
	{
		//	祖先にオブジェクトを持たないものをシーンの直下に並べる
		for (const std::shared_ptr<game_object>& object : scene.objects)
		{
			if (!has_object_parent(find_node(object)))
				render_object(object);
		}
		ImGui::TreePop();
	}
	ImGui::PopID();
}

void outliner_window::render_scene_menu(const scene_row& scene)
{
	//	動いているシーンはシステムの窓を開けるだけ
	if (!editor)
	{
		if (ImGui::MenuItem("system", nullptr, false, scene.system != nullptr))
			scene.system->open_window();
		return;
	}

	scene_editor* target_editor = editor;
	const scene_resource_ptr resource = scene.resource;
	if (ImGui::BeginMenu("add object"))
	{
		render_type_menu([&](const std::string& type)
		{
			pending = [target_editor, type, resource]() { target_editor->place_object_in_scene(type, resource); };
		});
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("edit"))
		target_editor->open_edit_window(resource);
	if (ImGui::MenuItem("remove scene", nullptr, false, target_editor->can_remove_scene()))
		pending = [target_editor, resource]() { target_editor->remove_scene(resource); };
	ImGui::Separator();
	render_add_scene_menu();
}

void outliner_window::render_add_scene_menu()
{
	if (!ImGui::BeginMenu("add scene"))
		return;

	scene_editor* target_editor = editor;
	for (const std::string& scene_path : target_editor->get_addable_scenes())
	{
		const cache_string label(scene_path);
		if (ImGui::MenuItem(label.get<utf8_cache>().data()))
			pending = [target_editor, scene_path]() { target_editor->add_scene(scene_path); };
	}
	ImGui::EndMenu();
}

void outliner_window::render_object(const std::shared_ptr<game_object>& object)
{
	const hierarchy* node = find_node(object);
	if (!object->is_display_inspector())
	{
		if (node)
			render_children(node);
		return;
	}

	ImGui::PushID(object.get());

	const bool is_spatial = node != nullptr;
	const bool has_children = node && has_object_children(node);
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (context->get_selected_object() == object)
		flags |= ImGuiTreeNodeFlags_Selected;
	if (!has_children)
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	const cache_string name(object->get_name());
	const bool opened = ImGui::TreeNodeEx("##object", flags, "%s", name.get<utf8_cache>().data());
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		context->set_selected_object(object);

	if (operations)
	{
		object_edit_operations* target_editor = operations;

		//	行をドラッグして別の行に落とすと、落とした先を親にする
		if (ImGui::BeginDragDropSource())
		{
			const game_object* self = object.get();
			ImGui::SetDragDropPayload(payload_type, &self, sizeof(self));
			ImGui::TextUnformatted(name.get<utf8_cache>().data());
			ImGui::EndDragDropSource();
		}
		if (is_spatial && ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payload_type))
			{
				const std::shared_ptr<game_object> child = find_payload_object(payload->Data);
				if (child && child != object)
					pending = [target_editor, child, object]() { target_editor->attach_object(child, object); };
			}
			ImGui::EndDragDropTarget();
		}

		//	右クリックのメニュー
		if (ImGui::BeginPopupContextItem("object_menu"))
		{
			if (ImGui::BeginMenu("add child", is_spatial))
			{
				render_type_menu([&](const std::string& type)
				{
					pending = [target_editor, type, object]() { target_editor->place_object_as_child(type, object); };
				});
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("detach", nullptr, false, has_object_parent(node)))
				pending = [target_editor, object]() { target_editor->attach_object(object, nullptr); };
			ImGui::Separator();
			if (ImGui::MenuItem("delete", "Del", false, target_editor->can_remove_object(object)))
				pending = [target_editor, object]() { target_editor->remove_object(object); };
			ImGui::EndPopup();
		}
	}

	if (opened && has_children)
	{
		render_children(node);
		ImGui::TreePop();
	}
	ImGui::PopID();
}

void outliner_window::render_children(const hierarchy* node)
{
	for (const auto& child : node->get_children())
	{
		auto it = owners.find(child.get());
		if (it != owners.end())
			render_object(it->second);
		else
			render_children(child.get());
	}
}

void outliner_window::render_transient_objects(engine_context& target)
{
	std::vector<std::shared_ptr<game_object>> transients;
	for (const std::shared_ptr<game_object>& object : target.get_objects().search_game_objects<game_object>())
	{
		if (objects.find(object.get()) == objects.end() && object->is_display_inspector())
			transients.emplace_back(object);
	}
	if (transients.empty())
		return;

	//	シーンに保存されない一時オブジェクトは、まとめて薄く出す
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	if (ImGui::TreeNodeEx("transient objects", ImGuiTreeNodeFlags_SpanAvailWidth))
	{
		for (const std::shared_ptr<game_object>& object : transients)
		{
			ImGui::PushID(object.get());
			const cache_string name(object->get_name());
			if (ImGui::Selectable(name.get<utf8_cache>().data(), context->get_selected_object() == object))
				context->set_selected_object(object);
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
	ImGui::PopStyleColor();
}

void outliner_window::render_type_menu(const std::function<void(const std::string& type)>& on_select)
{
	place_objects_window::render_placeable_types({},
		[](const char* category) { return ImGui::BeginMenu(category); },
		[]() { ImGui::EndMenu(); },
		[&on_select](const std::string& type)
		{
			if (ImGui::MenuItem(type.c_str()))
				on_select(type);
		});
}

void outliner_window::collect_objects(const std::vector<scene_row>& scenes, const std::vector<std::shared_ptr<game_object>>* loose_objects)
{
	owners.clear();
	objects.clear();
	auto add_object = [this](const std::shared_ptr<game_object>& object)
	{
		if (!object)
			return;
		objects.emplace(object.get(), object);
		if (const hierarchy* node = find_node(object))
			owners.emplace(node, object);
	};
	for (const scene_row& scene : scenes)
	{
		for (const std::shared_ptr<game_object>& object : scene.objects)
			add_object(object);
	}
	if (loose_objects)
	{
		for (const std::shared_ptr<game_object>& object : *loose_objects)
			add_object(object);
	}
}

const hierarchy* outliner_window::find_node(const std::shared_ptr<game_object>& object)
{
	if (!object || !object->check_type(spatial_object::type_name()))
		return nullptr;
	return static_cast<spatial_object*>(object.get())->get_spatial().get();
}

bool outliner_window::has_object_children(const hierarchy* node) const
{
	for (const auto& child : node->get_children())
	{
		if (owners.find(child.get()) != owners.end() || has_object_children(child.get()))
			return true;
	}
	return false;
}

bool outliner_window::has_object_parent(const hierarchy* node) const
{
	if (!node)
		return false;
	for (auto parent = node->get_parent(); parent; parent = parent->get_parent())
	{
		if (owners.find(parent.get()) != owners.end())
			return true;
	}
	return false;
}

std::shared_ptr<game_object> outliner_window::find_payload_object(const void* data) const
{
	const game_object* target = *static_cast<const game_object* const*>(data);
	auto it = objects.find(target);
	return it != objects.end() ? it->second : nullptr;
}
#endif	//	defined(USE_IMGUI)
