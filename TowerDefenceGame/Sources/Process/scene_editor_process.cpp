#include "Process/scene_editor_process.h"

#if defined(USE_IMGUI)
#include <filesystem>

#include "framework.h"
#include "Framework/app_window.h"
#include "Process/game_process.h"
#include "Process/asset_preview_process.h"
#include "Graphics/Core/render_data.h"
#include "Graphics/Pipeline/pipeline_state.h"
#include "Renderer/render_system.h"
#include "Renderer/debug_view_settings.h"
#include "Object/world.h"
#include "Object/object_manager.h"
#include "Object/Collision/collision_filter.h"
#include "Scenes/scene_manager.h"
#include "Scenes/scene_paths.h"
#include "Input/input_action_system.h"
#include "Effect/effect_instance.h"
#include "System/asset_browser.h"
#include "System/console_manager.h"
#include "Editor/editor_host.h"
#include "Editor/editor_items.h"
#include "Editor/scene_editor.h"
#include "Editor/editor_viewport.h"
#include "Editor/editor_picker.h"
#include "imgui_internal.h"
#include "Editor/detail_profiler.h"

namespace
{
	//	起動時に読むシーン
	console_variable<std::string> editor_startup_scene_variable("editor.startup_scene", "", "エディターで起動時に読むシーン", console_flags::persist);
	//	エディターのビューポートの画面比率
	console_variable<int> editor_viewport_aspect_variable("editor.viewport.aspect", 0, "エディターのビューポートの画面比率（0:ゲームの解像度 1:16:9 2:16:10 3:4:3 4:21:9 5:固定しない）", console_flags::persist);
}

scene_editor_process::scene_editor_process(framework& owner)
: framework_process(owner)
{
}

scene_editor_process::~scene_editor_process()
{
}

std::string scene_editor_process::get_startup_scene_path()
{
	std::string path = editor_startup_scene_variable.get();
	std::error_code error;
	if (path.empty() || !std::filesystem::exists(std::filesystem::path(path), error))
		path = std::string(scene_paths::default_scene);
	return path;
}

bool scene_editor_process::enter()
{
	framework& self = get_owner();

	//	当たり判定フィルター生成
	if (!collision_filter_manager::is_valid())
	{
		collision_filter_manager::create_instance();
		owns_collision_filters = true;
		if (!game_process::create_collision_filters())
			return false;
	}

	//	編集用のワールドは scene_editor が持つ
	scene_edit = std::make_unique<scene_editor>();

	//	再生の開始・停止はこのプロセスが受け持つ
	{
		scene_editor::play_handlers handlers;
		handlers.start = [this](const scene_resource_ptr& resource, std::string_view open_file) { start_play(resource, open_file); };
		handlers.stop = [this]() { stop_play(); };
		handlers.open_file = [this](std::string_view open_file)
		{
			if (session)
				session->open_file = std::string(open_file);
		};
		scene_edit->set_play_handlers(std::move(handlers));
	}

	//	エディターではウィンドウに直接描かず、ビューポートの窓に映す
	viewport = std::make_unique<editor_viewport>(self.renderer->get_executor());
	if (!viewport->initialize(1280, 720))
		return false;
	viewport->set_aspect_variable(&editor_viewport_aspect_variable);

	//	編集中は Windows のウィンドウを隠す
	//	再生中に閉じる操作（閉じるボタン・ESC）をしたら停止、それ以外（Exit）は終了
	self.window.set_visible(false);
	self.window.set_close_handler([this, &self]()
	{
		if (scene_edit && scene_edit->is_playing() && !exit_requested)
		{
			scene_edit->request_stop();
			return;
		}
		DestroyWindow(self.window.get_handle());
	});

	register_open_handlers();

	//	エディターでは編集用に開く
	scene_edit->request_edit(get_startup_scene_path());
	return true;
}

void scene_editor_process::register_open_handlers()
{
	framework& self = get_owner();

	//	アセットブラウザの開き先
	if (self.editor)
	{
		self.editor->register_asset_open_handlers(
			[this](std::string_view file_path) { scene_edit->request_edit(file_path); },
			[this](std::string_view scene_path, std::string_view open_file) { scene_edit->request_play(scene_path, open_file); });
	}


	//	モデルエディター
	if (asset_browser::is_valid())
	{
		asset_browser::open_handler handler;
		handler.label = "Model Editor (Window)";
		handler.extensions = { ".emo" };
		handler.open_file = [&self](const cache_string& file_path)
		{
			if (!self.has_process(asset_preview_process::make_key("model_editor", file_path)))
				self.request_process(std::make_unique<asset_preview_process>(self, file_path, "model_editor",
					[](scene_manager& scenes) { scenes.add_scene(scene_paths::model_editor); }));
		};
		asset_browser::instance()->register_open_handler(std::move(handler));
	}

	//	パーティクルエディター
	if (asset_browser::is_valid())
	{
		asset_browser::open_handler handler;
		handler.label = "Particle Editor (Window)";
		handler.extensions = { ".eme" };
		handler.open_file = [&self](const cache_string& file_path)
		{
			if (!self.has_process(asset_preview_process::make_key("particle_editor", file_path)))
				self.request_process(std::make_unique<asset_preview_process>(self, file_path, "particle_editor",
					[](scene_manager& scenes) { scenes.add_scene(scene_paths::particle_editor); }));
		};
		asset_browser::instance()->register_open_handler(std::move(handler));
	}

	//	マテリアルエディター
	if (asset_browser::is_valid())
	{
		asset_browser::open_handler handler;
		handler.label = "Material Editor (Window)";
		handler.extensions = { ".emr", ".emo", ".eme" };
		handler.open_file = [&self](const cache_string& file_path)
		{
			if (!self.has_process(asset_preview_process::make_key("material_editor", file_path)))
				self.request_process(std::make_unique<asset_preview_process>(self, file_path, "material_editor",
					[](scene_manager& scenes) { scenes.add_scene(scene_paths::material_editor); }));
		};
		asset_browser::instance()->register_open_handler(std::move(handler));
	}
}

void scene_editor_process::exit()
{
	framework& self = get_owner();

	stop_play();
	self.window.set_close_handler(nullptr);
	viewport.reset();
	scene_edit.reset();

	if (owns_collision_filters)
	{
		collision_filter_manager::delete_instance();
		owns_collision_filters = false;
	}
}

void scene_editor_process::start_play(const scene_resource_ptr& resource, std::string_view open_file)
{
	framework& self = get_owner();

	//	ゲームのプロセスは次のフレームの頭で足される（止めたプロセスが外れた後）
	session = std::make_shared<play_session>();
	session->scene = resource;
	session->open_file = std::string(open_file);
	self.request_process(std::make_unique<game_process>(self, session));
}

void scene_editor_process::stop_play()
{
	//	ゲームのプロセスは次のフレームの頭で外れる
	if (session)
		session->stop_requested = true;
	session.reset();
}

void scene_editor_process::request_exit()
{
	exit_requested = true;
	PostMessage(get_owner().window.get_handle(), WM_CLOSE, 0, 0);
}

void scene_editor_process::update(float elapsed_time)
{
	//	シーンエディターのリクエストを対応
	scene_edit->apply_requests();

	//	ゲームのプロセスが自分で終わった時（始められなかった時など）は再生をやめる
	if (session && session->finished)
	{
		session.reset();
		scene_edit->notify_play_finished();
	}

	//	再生中は編集用のワールドを進めない（描画は続ける）
	if (scene_edit->is_playing())
		return;

	world& edit_world = scene_edit->get_edit_world();
	if (input::action_system::is_valid())
	{
		input::action_system::instance()->update(edit_world.get_scenes().get_broadcaster());
	}

	//	シーンの更新
	{
		DETAIL_PROFILE_CPU_SCOPE("update.edit.scene_manager");
		edit_world.get_scenes().updates(elapsed_time);
	}

	//	オブジェクト更新
	{
		DETAIL_PROFILE_CPU_SCOPE("update.edit.object_manager");
		edit_world.get_objects().updates(elapsed_time);
	}

	//	シーンの更新
	{
		DETAIL_PROFILE_CPU_SCOPE("update.edit.scene_manager.post");
		edit_world.get_scenes().post_updates(elapsed_time);
	}

	//	編集中のシステムの更新
	{
		DETAIL_PROFILE_CPU_SCOPE("update.scene_edit.editor_update");
		scene_edit->editor_update(elapsed_time);
	}

	//	選んでいるオブジェクトの形を線で出す
	icons.queue_selected_shape(edit_world.get_context());

	//	エフェクト関係更新
	{
		DETAIL_PROFILE_CPU_SCOPE("update.edit.effect_manager");
		edit_world.get_effects().update(elapsed_time);
	}
}

void scene_editor_process::render(float elapsed_time)
{
	//	編集用のワールドはビューポートへ描く
	world& edit_world = scene_edit->get_edit_world();
	edit_world.get_objects().pre_render();
	viewport->collect(edit_world, debug_view_settings_manager::instance()->get("game"));

	{
		DETAIL_PROFILE_CPU_SCOPE("render.rebuild_pipeline_bindings");
		pipeline_state_manager::instance()->rebuild_pipeline_bindings();
	}
}

void scene_editor_process::submit_render_jobs(float elapsed_time)
{
	viewport->submit_render_jobs(scene_edit->get_edit_world(), elapsed_time);
}

void scene_editor_process::post_render()
{
	scene_edit->get_edit_world().get_objects().post_render();
}

void scene_editor_process::imgui_render()
{
	//	メインの画面のツールバー
	if (editor_items::begin_editor_menu_bar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit"))
				request_exit();
			ImGui::EndMenu();
		}
		debug_view_settings_manager::instance()->get("game").imgui_menu();
		scene_edit->imgui_menu();
		editor_items::end_editor_menu_bar();
	}

	//	閉じるボタン
	if (editor_items::consume_editor_close_request())
		request_exit();

	//	ビューポートの窓
	{
		//	ビューポートには常に編集用のワールドを映す
		engine_context& context = scene_edit->get_edit_world().get_context();
		bool hovered = false;

		//	ビューポートは「editor」窓の中にドッキングする
		editor_items::editor_dock_space("viewport");
		ImGui::SetNextWindowSize(ImVec2(1280.0f, 720.0f), ImGuiCond_FirstUseEver);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		if (ImGui::Begin("viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_MenuBar))
		{
			//	画面比率を選ぶメニュー
			if (ImGui::BeginMenuBar())
			{
				viewport->imgui_menu();
				gizmo.imgui_menu();
				ImGui::EndMenuBar();
			}
			//	編集中だけギズモを重ねる
			hovered = viewport->imgui_render(context, ImGui::GetCursorScreenPos(), ImGui::GetContentRegionAvail(),
				[this, &context](const ImVec2& image_position, const ImVec2& image_size)
				{
					if (scene_edit->is_playing())
						return false;

					//	objects_windowからドロップされた型を、落とした位置の物の表面に置く
					if (ImGui::BeginDragDropTargetCustom(ImRect(image_position, ImVec2(image_position.x + image_size.x, image_position.y + image_size.y)), ImGui::GetID("##place_drop")))
					{
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(place_objects_window::payload_type))
						{
							const ImVec2 mouse(ImGui::GetMousePos().x - image_position.x, ImGui::GetMousePos().y - image_position.y);
							std::shared_ptr<game_object> hit_object;
							math::vector3d hit_position;
							if (editor_picker::raycast(context, mouse, image_size, false, hit_object, hit_position))
								scene_edit->place_object(static_cast<const char*>(payload->Data), hit_position);
							else
								scene_edit->place_object(static_cast<const char*>(payload->Data));
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
							//	アイコンを先に調べ、無ければモデルを調べる
							std::shared_ptr<game_object> picked = icons.pick(context, mouse, image_size);
							if (!picked)
								picked = editor_picker::pick(context, mouse, image_size);
							context.set_selected_object(picked);
						}
					}
					return false;
				});
		}
		ImGui::End();
		ImGui::PopStyleVar();
		context.set_viewport_hovered(hovered);
	}

	//	オブジェクト情報表示
	if (!scene_edit->is_playing())
		scene_edit->get_edit_world().get_objects().imgui_render();

	//	編集中のシーンの窓
	scene_edit->imgui_render();

	//	シーンの一覧と置くオブジェクトの一覧の窓
	if (scene_edit->is_editing() && !scene_edit->is_playing())
	{
		//	Delete キーで選んでいるオブジェクトを削除
		if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			scene_edit->remove_object(scene_edit->get_edit_world().get_context().get_selected_object());

		scene_list_window.imgui_render(*scene_edit);
		place_window.imgui_render(*scene_edit);
	}
}
#endif	//	defined(USE_IMGUI)
