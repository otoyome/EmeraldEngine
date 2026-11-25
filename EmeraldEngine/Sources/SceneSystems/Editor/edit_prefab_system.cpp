#include "SceneSystems/Editor/edit_prefab_system.h"

#if defined(USE_IMGUI)
#include <filesystem>
#include <functional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "imgui.h"
#include "imgui_internal.h"
#include "Object/engine_context.h"
#include "Object/object_manager.h"
#include "Object/game_object_registry.h"
#include "Object/game_object_serializer.h"
#include "Object/spatial_object.h"
#include "Resource/resource_manager.h"
#include "Texture/texture_resource_utility.h"
#include "Graphics/View/camera.h"
#include "Editor/editor_picker.h"
#include "Object/Graphics/light_object.h"
#include "Object/Graphics/light_probe_object.h"

namespace
{
	//	原点に置いたプレハブを正面から見るカメラの位置と向き
	const math::vector3d preview_camera_position = system_units::convert_m_to_unit(math::vector3d(0.0, 1.0, 5.0));
	const math::rotatord preview_camera_rotation(0.0, math::constants<double>::pi, 0.0);

	//	ファイルパスを ImGui 表示用の UTF-8 にする
	std::string to_display(std::string_view text)
	{
		const core::cache_string path(text);
		return std::string(path.get<core::utf8_cache>());
	}

	//	空間の親子の親
	std::shared_ptr<game_object> get_parent_object(const std::shared_ptr<game_object>& object)
	{
		if (!object || !object->check_type(spatial_object::type_name()))
			return nullptr;
		return static_cast<spatial_object*>(object.get())->get_parent();
	}
}

edit_prefab_system::edit_prefab_system()
: scene_system()
{
}

edit_prefab_system::~edit_prefab_system()
{
}

void edit_prefab_system::on_begin()
{
	get_context().get_objects().set_pause(true);
	get_context().get_editor().set_editing(true);

	//	メインカメラ生成
	{
		main_camera = std::make_shared<camera_object>("メインカメラ");
		main_camera->get_camera()->get_parameters().fov_y = math::angle::to_radian(30.0f);
		main_camera->set_local_translation(preview_camera_position);
		main_camera->set_local_euler(preview_camera_rotation);
		main_camera->get_camera()->get_parameters().position = preview_camera_position;
		main_camera->get_camera()->get_parameters().rotation = math::rotatorf(preview_camera_rotation);
		register_object(main_camera);
		mouse::instance()->set_center_lock(false);
		mouse::instance()->set_cursol_hide(false);
	}

	//	ライト生成
	{
		auto light = std::make_shared<directional_light_object>("平行光源");
		register_object(light);

		auto sky_light = std::make_shared<sky_light_object>("スカイライト");
		texture_resource_ptr ibl_texture;
		ibl_texture = texture_resource_utility::load_texture_resource("Assets/Resources/Textures/hilly_terrain_01_puresky_4k/diffuse_iem.etx");
		sky_light->set_diffuse_ibl_texture(ibl_texture);
		ibl_texture = texture_resource_utility::load_texture_resource("Assets/Resources/Textures/hilly_terrain_01_puresky_4k/specular_pmrem.etx");
		sky_light->set_specular_ibl_texture(ibl_texture);
		ibl_texture = texture_resource_utility::load_texture_resource("Assets/Resources/Textures/lut_ggx.etx");
		sky_light->set_lut_ggx_texture(ibl_texture);
		register_object(sky_light);
	}

	//	エディターカメラ生成
	{
		editor_camera = std::make_shared<editor_camera_object>("エディターカメラ", preview_camera_position, math::rotatorf(preview_camera_rotation), 0.25f);
		register_object(editor_camera);
	}
}

void edit_prefab_system::on_end()
{
	clear_objects();
	prefab.reset();
}

void edit_prefab_system::pre_update(float elapsed_time)
{
	//	窓に映しているワールドではアセットプレビューから渡されたファイルを開く
	std::string file_path;
	if (get_context().get_editor().take_open_file(file_path))
		open_prefab(file_path);

	//	選んでいるオブジェクトの形を線で出す
	icons.queue_selected_shape(get_context());
}

bool edit_prefab_system::open_prefab(const std::string& file_path)
{
	auto operation = resource_manager::instance()->load<prefab_resource>(file_path);
	if (!operation || !operation->is_succeeded())
	{
		message = "load failed: " + to_display(file_path);
		return false;
	}

	clear_objects();
	prefab = operation->get_resource();
	if (!load_objects())
	{
		message = "load failed: " + to_display(file_path);
		return false;
	}
	message.clear();
	return true;
}

void edit_prefab_system::clear_objects()
{
	object_manager& manager = get_context().get_objects();
	for (const std::shared_ptr<game_object>& object : objects)
		manager.unregister_object(object);
	objects.clear();
	get_context().get_editor().set_selected_object(nullptr);

	//	別の中身の操作は戻せないので捨てる
	history.clear();
	edit_tracker.reset();
}

bool edit_prefab_system::load_objects()
{
	if (!prefab)
		return false;

	//	編集ではファイルの UUID のまま読む
	std::istringstream stream(prefab->get_body());
	std::vector<game_object_ptr> loaded;
	if (!game_object_serializer::load(stream, loaded))
		return false;

	object_manager& manager = get_context().get_objects();
	for (const game_object_ptr& object : loaded)
	{
		objects.emplace_back(object);
		manager.register_object(object);
	}
	saved_body = serialize_objects();
	return true;
}

bool edit_prefab_system::save_prefab()
{
	if (!prefab)
		return false;

	const std::string file_path(prefab->get_file_path());
	if (!prefab->set_contents(objects) || !resource_manager::instance()->save(prefab))
	{
		message = "save failed: " + to_display(file_path);
		return false;
	}
	saved_body = serialize_objects();
	message = "saved: " + to_display(file_path);
	return true;
}

void edit_prefab_system::revert_prefab()
{
	if (!prefab)
		return;

	clear_objects();
	load_objects();
	message = "reverted: " + to_display(prefab->get_file_path());
}

std::string edit_prefab_system::serialize_objects() const
{
	std::ostringstream stream;
	if (!game_object_serializer::save(stream, objects))
		return {};
	return stream.str();
}

bool edit_prefab_system::is_dirty() const
{
	return prefab && serialize_objects() != saved_body;
}

std::shared_ptr<game_object> edit_prefab_system::place_object(std::string_view type, const std::optional<math::vector3d>& position)
{
	if (!prefab)
		return nullptr;

	//	位置の指定が無く、プレハブのオブジェクトを選んでいればその子にする
	const std::shared_ptr<game_object> selected = get_context().get_editor().get_selected_object();
	if (!position && is_prefab_object(selected))
		return place_object_as_child(type, selected);

	//	プレハブは原点を基準に作るので、位置の指定が無ければ原点に置く
	const game_object_ptr object = add_object(type, position ? *position : math::vector3d(0.0, 0.0, 0.0));
	push_place_command(object);
	return object;
}

std::shared_ptr<game_object> edit_prefab_system::place_object_as_child(std::string_view type, const std::shared_ptr<game_object>& parent)
{
	if (!is_prefab_object(parent))
		return nullptr;

	//	親の位置に置いて子にする
	math::vector3d position(0.0, 0.0, 0.0);
	if (parent->check_type(spatial_object::type_name()))
		position = static_cast<spatial_object*>(parent.get())->get_world_translation();
	const game_object_ptr object = add_object(type, position);
	if (object && parent->check_type(spatial_object::type_name()))
		spatial_object::attach_object(object, parent);
	push_place_command(object);
	return object;
}

std::shared_ptr<game_object> edit_prefab_system::add_object(std::string_view type, const math::vector3d& position)
{
	if (!game_object_registry::is_valid())
		return nullptr;

	game_object_ptr object = game_object_registry::instance()->create(type);
	if (!object)
	{
		message = "add object failed: " + std::string(type);
		return nullptr;
	}

	object->set_name(make_unique_object_name(type));
	if (object->check_type(spatial_object::type_name()))
		static_cast<spatial_object*>(object.get())->set_world_translation(position);

	objects.emplace_back(object);
	get_context().get_objects().register_object(object);
	get_context().get_editor().set_selected_object(object);
	return object;
}

bool edit_prefab_system::can_remove_object(const std::shared_ptr<game_object>& target) const
{
	return is_prefab_object(target);
}

bool edit_prefab_system::remove_object(const std::shared_ptr<game_object>& target)
{
	if (!can_remove_object(target))
		return false;

	//	子孫も一緒に消す
	const std::shared_ptr<object_tree_state> state = std::make_shared<object_tree_state>(capture_object_tree(target));
	detach_object_tree(*state);
	get_context().get_editor().set_selected_object(nullptr);

	history.push(editor_edit_tracker::make_label("Delete", target),
		[this, state]()
		{
			attach_object_tree(*state);
			get_context().get_editor().set_selected_object(state->objects.front());
			return true;
		},
		[this, state]()
		{
			detach_object_tree(*state);
			get_context().get_editor().set_selected_object(nullptr);
			return true;
		});

	//	親から外して姿勢が変わっても、インスペクターの編集として積まないようにする
	edit_tracker.resync();
	return true;
}

bool edit_prefab_system::attach_object(const std::shared_ptr<game_object>& child, const std::shared_ptr<game_object>& parent)
{
	if (!is_prefab_object(child) || (parent && !is_prefab_object(parent)))
		return false;

	//	元に戻せるよう、付け替える前の親を覚えておく
	const game_object_ptr before_parent = get_parent_object(child);
	if (!spatial_object::attach_object(child, parent))
		return false;
	const game_object_ptr after_parent = get_parent_object(child);
	if (before_parent != after_parent)
	{
		history.push(editor_edit_tracker::make_label("Attach", child),
			[child, before_parent]() { return spatial_object::attach_object(child, before_parent); },
			[child, after_parent]() { return spatial_object::attach_object(child, after_parent); });

		//	付け替えで姿勢が変わっても、インスペクターの編集として積まないようにする
		edit_tracker.resync();
	}
	return true;
}

edit_prefab_system::object_tree_state edit_prefab_system::capture_object_tree(const std::shared_ptr<game_object>& root)
{
	object_tree_state state;
	state.objects = collect_object_tree(root);
	state.parent = get_parent_object(root);
	return state;
}

void edit_prefab_system::detach_object_tree(const object_tree_state& state)
{
	std::unordered_set<const game_object*> removes;
	for (const game_object_ptr& object : state.objects)
		removes.insert(object.get());

	object_manager& manager = get_context().get_objects();
	for (const game_object_ptr& object : state.objects)
		manager.unregister_object(object);
	std::erase_if(objects, [&](const game_object_ptr& object) { return removes.find(object.get()) != removes.end(); });

	//	履歴が持っている間も親の子として残らないよう、ルートを親から外す
	if (state.parent && !state.objects.empty())
		spatial_object::attach_object(state.objects.front(), nullptr);

	//	外したオブジェクトを選んでいたら選択を外す
	const game_object_ptr selected = get_context().get_editor().get_selected_object();
	if (selected && removes.find(selected.get()) != removes.end())
		get_context().get_editor().set_selected_object(nullptr);
}

void edit_prefab_system::attach_object_tree(const object_tree_state& state)
{
	object_manager& manager = get_context().get_objects();
	for (const game_object_ptr& object : state.objects)
	{
		objects.emplace_back(object);
		manager.register_object(object);
	}

	//	ルートを元の親に付け直す(外す時にワールド姿勢を維持しているので元の位置に戻る)
	if (state.parent && !state.objects.empty())
		spatial_object::attach_object(state.objects.front(), state.parent);
}

void edit_prefab_system::push_place_command(const std::shared_ptr<game_object>& root)
{
	if (!root)
		return;

	const std::shared_ptr<object_tree_state> state = std::make_shared<object_tree_state>(capture_object_tree(root));
	history.push(editor_edit_tracker::make_label("Place", root),
		[this, state]()
		{
			detach_object_tree(*state);
			return true;
		},
		[this, state]()
		{
			attach_object_tree(*state);
			get_context().get_editor().set_selected_object(state->objects.front());
			return true;
		});
	edit_tracker.resync();
}

void edit_prefab_system::undo()
{
	if (!history.can_undo())
		return;
	history.undo();

	//	戻した変更をインスペクターの編集として積まないようにする
	edit_tracker.resync();
}

void edit_prefab_system::redo()
{
	if (!history.can_redo())
		return;
	history.redo();
	edit_tracker.resync();
}

bool edit_prefab_system::is_prefab_object(const std::shared_ptr<game_object>& object) const
{
	return object && std::find(objects.begin(), objects.end(), object) != objects.end();
}

std::vector<std::shared_ptr<game_object>> edit_prefab_system::collect_object_tree(const std::shared_ptr<game_object>& target) const
{
	//	空間の親子をたどって、プレハブの子孫を集める
	std::unordered_map<const hierarchy*, game_object_ptr> owners;
	for (const game_object_ptr& object : objects)
	{
		if (object->check_type(spatial_object::type_name()))
			owners.emplace(static_cast<spatial_object*>(object.get())->get_spatial().get(), object);
	}

	std::unordered_set<const game_object*> visited;
	std::vector<game_object_ptr> result;
	auto add_result = [&](const game_object_ptr& object)
	{
		if (object && visited.insert(object.get()).second)
			result.emplace_back(object);
	};
	std::function<void(const hierarchy*)> collect_children = [&](const hierarchy* node)
	{
		for (const auto& child : node->get_children())
		{
			auto it = owners.find(child.get());
			if (it != owners.end())
				add_result(it->second);
			collect_children(child.get());
		}
	};
	add_result(target);
	if (target && target->check_type(spatial_object::type_name()))
		collect_children(static_cast<spatial_object*>(target.get())->get_spatial().get());
	return result;
}

std::string edit_prefab_system::make_unique_object_name(std::string_view base) const
{
	std::unordered_set<std::string> names;
	for (const game_object_ptr& object : objects)
		names.emplace(object->get_name());

	std::string name(base);
	for (int number = 1; names.find(name) != names.end(); ++number)
		name = std::string(base) + "_" + std::to_string(number);
	return name;
}

bool edit_prefab_system::has_panel(panel_regions region) const
{
	switch (region)
	{
	case panel_regions::menu_bar:
	case panel_regions::left:
	case panel_regions::right:
	case panel_regions::bottom:
		{
			return true;
		}
	}
	return false;
}

void edit_prefab_system::imgui_render_panel(panel_regions region)
{
	switch (region)
	{
	case panel_regions::menu_bar:
		{
			imgui_render_menu_bar();
			break;
		}
	case panel_regions::left:
		{
			outliner.imgui_render_contents(*this, objects);
			break;
		}
	case panel_regions::right:
		{
			inspector.imgui_render_contents(get_context());
			break;
		}
	case panel_regions::bottom:
		{
			place_window.imgui_render_contents(*this);
			break;
		}
	}
}

void edit_prefab_system::imgui_render_menu_bar()
{
	//	メニューバーはこの窓で最初に描かれるので、インスペクターやギズモが書き換える前にここで中身を取っておく
	{
		std::vector<editor_edit_tracker::target> targets;
		const game_object_ptr selected = get_context().get_editor().get_selected_object();
		if (is_prefab_object(selected))
			targets.emplace_back(editor_edit_tracker::make_object_target(get_context(), selected));
		edit_tracker.begin(std::move(targets));
	}

	//	Ctrl+Z / Ctrl+Y はこの窓がフォーカスしている時だけ受ける
	switch (editor_edit_tracker::check_shortcuts(ImGuiInputFlags_RouteFocused))
	{
		case editor_edit_tracker::shortcuts::undo:
		{
			undo();
			break;
		}
		case editor_edit_tracker::shortcuts::redo:
		{
			redo();
			break;
		}
		default:
		{
			break;
		}
	}

	//	Delete キーで選んでいるオブジェクトを削除
	if (ImGui::Shortcut(ImGuiKey_Delete, ImGuiInputFlags_RouteFocused) && !ImGui::GetIO().WantTextInput)
		remove_object(get_context().get_editor().get_selected_object());

	const bool dirty = is_dirty();
	if (ImGui::MenuItem("Save", nullptr, false, prefab && dirty))
		save_prefab();
	if (ImGui::MenuItem("Revert", nullptr, false, prefab && dirty))
		revert_prefab();

	//	元に戻す・やり直す(名前が変わっても同じ項目になるよう ### で ID を固定)
	const std::string undo_label = history.can_undo() ? "Undo (" + std::string(history.get_undo_label()) + ")###prefab_undo" : std::string("Undo###prefab_undo");
	if (ImGui::MenuItem(undo_label.c_str(), "Ctrl+Z", false, history.can_undo()))
		undo();
	const std::string redo_label = history.can_redo() ? "Redo (" + std::string(history.get_redo_label()) + ")###prefab_redo" : std::string("Redo###prefab_redo");
	if (ImGui::MenuItem(redo_label.c_str(), "Ctrl+Y", false, history.can_redo()))
		redo();
	if (dirty)
		ImGui::TextDisabled("*");
	if (!message.empty())
		ImGui::TextDisabled("%s", message.c_str());
}

bool edit_prefab_system::imgui_render_viewport_overlay(const ImVec2& image_position, const ImVec2& image_size)
{
	engine_context& context = get_context();

	//	配置窓からドロップされた型を、落とした位置の物の表面に置く
	if (ImGui::BeginDragDropTargetCustom(ImRect(image_position, ImVec2(image_position.x + image_size.x, image_position.y + image_size.y)), ImGui::GetID("##place_drop")))
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(place_objects_window::payload_type))
		{
			const ImVec2 mouse(ImGui::GetMousePos().x - image_position.x, ImGui::GetMousePos().y - image_position.y);
			std::shared_ptr<game_object> hit_object;
			math::vector3d hit_position;
			if (editor_picker::raycast(context, mouse, image_size, false, hit_object, hit_position))
				place_object(static_cast<const char*>(payload->Data), hit_position);
			else
				place_object(static_cast<const char*>(payload->Data));
		}
		ImGui::EndDragDropTarget();
	}

	//	ライトなど形の無いオブジェクトのアイコン
	icons.draw(context, image_position, image_size);

	if (gizmo.manipulate(context, image_position, image_size))
		return true;

	//	ギズモを触っていない時に左クリックしたら、その位置のオブジェクトを選ぶ
	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		const ImVec2 mouse(ImGui::GetMousePos().x - image_position.x, ImGui::GetMousePos().y - image_position.y);
		if (mouse.x >= 0.0f && mouse.y >= 0.0f && mouse.x < image_size.x && mouse.y < image_size.y)
		{
			std::shared_ptr<game_object> picked = icons.pick(context, mouse, image_size);
			if (!picked)
				picked = editor_picker::pick(context, mouse, image_size);
			context.get_editor().set_selected_object(picked);
		}
	}
	return false;
}

bool edit_prefab_system::request_close()
{
	if (!is_dirty())
		return true;
	open_close_popup = true;
	return false;
}

void edit_prefab_system::imgui_render()
{
	imgui_render_close_popup();

	//	この窓の描画が全部終わった後なので、操作が終わっていたら選んでいるオブジェクトの変更を履歴に積む
	edit_tracker.end(history);
}

void edit_prefab_system::imgui_render_close_popup()
{
	const std::string popup_name = get_context().get_editor().make_window_name("Unsaved Prefab");
	if (open_close_popup)
	{
		ImGui::OpenPopup(popup_name.c_str());
		open_close_popup = false;
	}

	if (!ImGui::BeginPopupModal(popup_name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	ImGui::TextUnformatted("保存していない変更があります。");
	if (prefab)
		ImGui::TextDisabled("%s", to_display(prefab->get_file_path()).c_str());
	ImGui::Separator();
	if (ImGui::Button("Save"))
	{
		if (save_prefab())
			get_context().get_editor().request_close_window();
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Discard"))
	{
		get_context().get_editor().request_close_window();
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel"))
		ImGui::CloseCurrentPopup();
	ImGui::EndPopup();
}

#endif	//	defined(USE_IMGUI)
