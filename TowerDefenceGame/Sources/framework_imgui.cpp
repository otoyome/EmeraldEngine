#include "framework.h"

#if defined(USE_IMGUI)
#include "System/engine_config_data.h"
#include "model_buffer_pool.h"
#include "material_resource_factory.h"
#include "System/memory_allocator.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
#include "imPlot/imPlot.h"
#include "ImGuizmo/ImCurveEdit.h"
#include "imtimeline/ImTimeline.h"
#include "im-neo-sequencer/imgui_neo_sequencer.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern ImWchar glyphRangesJapanese[];

#include "Editor/editor_items.h"
#include "Editor/detail_profiler.h"
#include "Editor/texture_editor.h"
#include "Object/world.h"
#include "Editor/editor_camera.h"
#include "System/console_manager.h"
#include "System/asset_browser.h"
#include "System/imgui_texture_keeper.h"
#include "Resource/resource_manager.h"
#include "material_resource.h"


#include "scenes/editscene/edit_model_scene.h"
#include "scenes/editscene/edit_particle_scene.h"
#include "scenes/editscene/edit_material_scene.h"
#include "scenes/editscene/edit_timeline_scene.h"
#include "scenes/DummyScene/dummy_scene.h"
#include "scenes/TestScene/collision_test_scene.h"
#include "scenes/TestScene/clustered_lighting_test_scene.h"
#include "scenes/TestScene/light_shadow_test_scene.h"
#include "scenes/NetworkScene/network_sandbox_scene.h"
#include "Scenes/setup_scene.h"

#include "imPlot/imPlot.h"
#include "imtimeline/ImTimeline.h"

namespace
{
	template<typename Type>
	void change_scene(scene_manager& scenes)
	{
		if (ImGui::Button(Type::type_name().data()))
		{
			scenes.change_scene_for_imgui<Type>();
		}
	}

	//	アセットブラウザでダブルクリックしたファイルを、シーンを切り替えて開く
	template<typename Type>
	void register_asset_open_handler(scene_manager& scenes, std::string_view label, std::vector<cache_string> extensions)
	{
		asset_browser::open_handler handler;
		handler.name = Type::type_name();
		handler.label = label;
		handler.extensions = std::move(extensions);
		handler.activate = [&scenes]()
		{
			if (!scenes.valid_scene<Type>())
				scenes.change_scene_for_imgui<Type>();
		};
		asset_browser::instance()->register_open_handler(std::move(handler));
	}

	//	アセットブラウザの新規作成でドメインの既定のマテリアルリソースを保存する
	void register_material_create_handler(std::string_view label, material_resource::domains domain)
	{
		asset_browser::create_handler handler;
		handler.label = label;
		handler.default_name = "NewMaterial";
		handler.extension = std::string(".") + std::string(material_resource::extension);
		handler.create = [domain](std::string_view file_path)
		{
			const std::string name = std::filesystem::path(std::string(file_path)).stem().string();
			auto material = material_resource_factory::create_new(name, domain);
			return resource_manager::instance()->save(material, file_path);
		};
		asset_browser::instance()->register_create_handler(std::move(handler));
	}

	//	デバッグ表示の設定
	console_variable<bool> grid_xz_variable("debug.grid.xz", true, "XZ平面のグリッド表示", console_flags::persist);
	console_variable<bool> grid_xy_variable("debug.grid.xy", false, "XY平面のグリッド表示", console_flags::persist);
	console_variable<bool> grid_yz_variable("debug.grid.yz", false, "YZ平面のグリッド表示", console_flags::persist);
	console_variable<int> grid_length_variable("debug.grid.length", 100, "グリッドの半径（メートル）", console_flags::persist);
	console_variable<bool> draw_mesh_aabb_variable("debug.draw.mesh_aabb", false, "モデルAABBの表示", console_flags::persist);
	console_variable<bool> draw_collision_variable("debug.draw.collision", false, "衝突判定の表示", console_flags::persist);
}

void framework::imgui_initialize()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGui::GetIO().Fonts->AddFontFromFileTTF("Assets/Resources/Textures/Fonts/Noto_Sans_JP/NotoSansJP-VariableFont_wght.ttf", 24.0f, nullptr, glyphRangesJapanese);
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(render_data::instance()->get_device().Get(), render_data::instance()->get_immediate_context().Get());
	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	//	アセットブラウザの開き先
	imgui_texture_keeper::create_instance();
	asset_browser::create_instance();
	register_asset_open_handler<edit_model_scene>(main_world->get_scenes(), "Model Editor", { ".emo", ".pmx", ".gltf", ".glb" });
	register_asset_open_handler<edit_particle_scene>(main_world->get_scenes(), "Particle Editor", { ".eme" });
	register_asset_open_handler<edit_timeline_scene>(main_world->get_scenes(), "Timeline Editor", { ".etl" });
	register_asset_open_handler<edit_material_scene>(main_world->get_scenes(), "Material Editor", { ".emo", ".eme", ".emr" });

	//	テクスチャリソースは別窓で編集する
	texture_editor::create_instance();
	texture_editor::instance()->register_open_handler();

	//	アセットブラウザの新規作成
	register_material_create_handler("Material (Surface)", material_resource::domains::surface);
	register_material_create_handler("Material (Particle)", material_resource::domains::particle);
	register_material_create_handler("Material (Deferred Decal)", material_resource::domains::deferred_decal);
	register_material_create_handler("Material (Post Process)", material_resource::domains::post_process);
	register_material_create_handler("Material (User Interface)", material_resource::domains::user_interface);
}

void framework::imgui_terminate()
{
	texture_editor::delete_instance();
	asset_browser::delete_instance();
	imgui_texture_keeper::delete_instance();

	ImGui::ShutdownNeoSequencer();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();
}

void framework::imgui_engine_config_window()
{
	if (ImGui::Begin("engine_config"))
	{
		//	垂直同期
		ImGui::Checkbox("vertical_sync", &engine_config_data::instance()->windows.vertical_sync);

		//	相対座標描画フラグ
		ImGui::Checkbox("camera_relative_rendering", &engine_config_data::instance()->rendering.camera_relative_rendering);

		//	描画色域
		ImGui::Combo("rendering_gamut", engine_config_data::instance()->rendering.rendering_gamut);
		if (engine_config_data::instance()->rendering.rendering_gamut != engine_config_data::instance()->get_active_rendering_gamut())
			ImGui::TextDisabled("restart to apply (current : %s)", magic_enum::enum_name(engine_config_data::instance()->get_active_rendering_gamut()).data());

		//	アンチエイリアス
		if (ImGui::TreeNode("antialias"))
		{
			std::vector<const char*> combo_texts;
			for (auto& name : magic_enum::enum_names<engine_config_data::antialias_data::modes>())
			{
				combo_texts.emplace_back(name.data());
			}
			int mode = static_cast<int>(engine_config_data::instance()->antialias.mode);
			if (ImGui::Combo("mode", &mode, combo_texts.data(), static_cast<int>(combo_texts.size())))
			{
				engine_config_data::instance()->antialias.mode = static_cast<engine_config_data::antialias_data::modes>(mode);
			}
			ImGui::TreePop();
		}
	}
	ImGui::End();
}

void framework::imgui_log_window()
{
	if (!logger::is_valid())
		return;

	if (ImGui::Begin("log"))
	{
		{
			auto category_texts = logger::instance()->get_category_texts();
			int category = static_cast<int>(display_log_category);
			if (ImGui::Combo("Category", &category, category_texts.data(), static_cast<INT>(category_texts.size())))
			{
				display_log_category = static_cast<logger::categorys>(category);
			}
			ImGui::SameLine();
			static cache_string orders[] = {"降順", "昇順"};
			if (ImGui::Button(is_log_descending_order ? orders[0].c_str() : orders[1].c_str()))
				is_log_descending_order = !is_log_descending_order;
		}
		{
			auto category_texts = logger::instance()->get_levels_texts();
			for (UINT index = 0; index < static_cast<UINT>(logger::levels::max); ++index)
			{
				auto level = magic_enum::enum_value<logger::levels>(index);
				bool b = system_utility::is_bitflag(display_log_flags, 1 << static_cast<UINT>(level));
				ImGui::Checkbox(magic_enum::enum_name(level).data(), &b);
				//if (index + 1 != static_cast<UINT>(logger::levels::max))
				ImGui::SameLine();
				system_utility::set_bitflag(display_log_flags, 1 << static_cast<UINT>(level), b);
			}
			if (ImGui::Button("Log Clear"))
			{
				logger::instance()->clear();
			}
		}
		ImGui::Separator();
		if (ImGui::BeginChild("##Log"))
		{
			auto display_log = [&](const logger::data& data)
			{
				if ((display_log_category != logger::categorys::max && display_log_category != data.category) ||
					!system_utility::is_bitflag(display_log_flags, 1 << static_cast<UINT>(data.level)))
					return;

				ImVec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
				switch (data.level)
				{
					case logger::levels::warning: color = { 1.0f, 1.0f, 0.0f, 1.0f }; break;
					case logger::levels::error: color = { 1.0f, 0.0f, 0.0f, 1.0f }; break;
				}
				ImGui::TextColored(color, data.text.data());
				ImGui::Separator();
			};

			if (is_log_descending_order)
			{
				for (auto& log : logger::instance()->get_logs())
				{
					display_log(log);
				}
			}
			else
			{
				std::list<logger::data>::const_reverse_iterator it = logger::instance()->get_logs().rbegin();
				std::list<logger::data>::const_reverse_iterator it_end = logger::instance()->get_logs().rend();
				while (it != it_end)
				{
					display_log(*it);
					it++;
				}
			}

			ImGui::EndChild();
		}
	}
	ImGui::End();
}

void framework::imgui_model_buffer_pool_window()
{
	if (ImGui::Begin("model_buffer_pool"))
	{
		//	モデル頂点/インデックスバッファプールの使用量表示
		if (model_buffer_pool_manager::is_valid())
		{
			for (auto& usage : model_buffer_pool_manager::instance()->get_pool_usages())
			{
				float fraction = usage.capacity_in_bytes > 0 ?
					static_cast<float>(usage.used_in_bytes) / static_cast<float>(usage.capacity_in_bytes) : 0.0f;

				std::string text = std::to_string(usage.used_in_bytes / 1024);
				text += " / ";
				text += std::to_string(usage.capacity_in_bytes / 1024);
				text += " [KB]";

				ImGui::Text("%s", usage.label.c_str());
				ImGui::ProgressBar(fraction, ImVec2(-1, 0), text.c_str());
			}
		}
	}
	ImGui::End();
}

void framework::imgui_memory_allocator_window()
{
	if (ImGui::Begin("memory_allocator"))
	{
		//	固定長プール
		for (auto& usage : memory_allocator::get_size_class_usages())
		{
			float fraction = usage.pooled_piece_count > 0 ?
				static_cast<float>(usage.used_piece_count) / static_cast<float>(usage.pooled_piece_count) : 0.0f;

			std::string text = std::to_string(usage.used_piece_count);
			text += " / ";
			text += std::to_string(usage.pooled_piece_count);
			text += " pieces";

			std::string label = std::to_string(usage.block_size_in_bytes) + " bytes";
			ImGui::Text("%s", label.c_str());
			ImGui::ProgressBar(fraction, ImVec2(-1, 0), text.c_str());
		}

		ImGui::Separator();

		//	可変長プール
		memory_allocator::large_pool_usage large_usage = memory_allocator::get_large_pool_usage();
		float large_fraction = large_usage.segment_capacity_bytes > 0 ?
			static_cast<float>(large_usage.used_bytes) / static_cast<float>(large_usage.segment_capacity_bytes) : 0.0f;

		std::string large_text = std::to_string(large_usage.used_bytes / 1024);
		large_text += " / ";
		large_text += std::to_string(large_usage.segment_capacity_bytes / 1024);
		large_text += " [KB]";

		ImGui::Text("large pool (segments : %zu)", large_usage.segment_count);
		ImGui::ProgressBar(large_fraction, ImVec2(-1, 0), large_text.c_str());
	}
	ImGui::End();
}

void framework::imgui_scene_change()
{
	if (ImGui::Begin("Scene Change Window"))
	{
		change_scene<edit_model_scene>(main_world->get_scenes());
		change_scene<dummy_scene>(main_world->get_scenes());
		change_scene<collision_test_scene>(main_world->get_scenes());
		change_scene<clustered_lighting_test_scene>(main_world->get_scenes());
		change_scene<light_shadow_test_scene>(main_world->get_scenes());
		change_scene<network_sandbox_scene>(main_world->get_scenes());
		change_scene<edit_particle_scene>(main_world->get_scenes());
		change_scene<edit_material_scene>(main_world->get_scenes());
		change_scene<edit_timeline_scene>(main_world->get_scenes());
		change_scene<setup_scene>(main_world->get_scenes());
	}
	ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////////////////
void framework::imgui_profiler_window()
{
	//	詳細プロファイラー(オブジェクト関数/シーンレンダラー/GPUスコープ単位)
	if (detail_profiler::is_valid())
	{
		detail_profiler::instance()->imgui_render();
	}
}

void framework::build_debug_primitives(const render_data_context& render_data)
{
	//	衝突判定のデバッグ表示
	if (draw_collision_variable.get() && main_world)
	{
		main_world->get_objects().get_world().debug_primitive_draw();
	}

	//	モデルAABBのデバッグ表示
	if (draw_mesh_aabb_variable.get())
	{
		math::float4 rotation = {0.0f, 0.0f, 0.0f, 1.0f};
		math::float4 color = {1.0f, 0.0f, 0.0f, 1.0f};
		for (auto& buffer_data : render_data.mesh_container.buffer_datas)
		{
			math::float3 scale((buffer_data.bounds_max.x - buffer_data.bounds_min.x) * 0.5f, (buffer_data.bounds_max.y - buffer_data.bounds_min.y) * 0.5f, (buffer_data.bounds_max.z - buffer_data.bounds_min.z) * 0.5f);
			math::float3 center(buffer_data.bounds_min.x + scale.x, buffer_data.bounds_min.y + scale.y, buffer_data.bounds_min.z + scale.z);
			QUEUE_DEBUG_PRIMITIVE_BOX(center, rotation, scale, color, true);
		}
	}

	const bool display_grid_xz = grid_xz_variable.get();
	const bool display_grid_xy = grid_xy_variable.get();
	const bool display_grid_yz = grid_yz_variable.get();
	const int display_grid_length = grid_length_variable.get();
	if (!display_grid_xz && !display_grid_xy && !display_grid_yz)
		return;

	float grid_length = system_units::convert_m_to_unit(static_cast<float>(display_grid_length));
	for (int index = -display_grid_length; index <= display_grid_length; ++index)
	{
		float len = system_units::convert_m_to_unit(static_cast<float>(index));
		if (display_grid_xz)
		{
			math::float4 line_color = math::float4(1.0f, 0.0f, 1.0f, 1);
			QUEUE_DEBUG_PRIMITIVE_LINE(math::float3(len, 0.0f, -grid_length), math::float3(len, 0.0f, +grid_length), line_color, true);
			QUEUE_DEBUG_PRIMITIVE_LINE(math::float3(-grid_length, 0.0f, len), math::float3(+grid_length, 0.0f, len), line_color, true);
		}
		if (display_grid_xy)
		{
			math::float4 line_color = math::float4(1.0f, 1.0f, 0.0f, 1);
			QUEUE_DEBUG_PRIMITIVE_LINE(math::float3(len, -grid_length, 0.0f), math::float3(len, +grid_length, 0.0f), line_color, true);
			QUEUE_DEBUG_PRIMITIVE_LINE(math::float3(-grid_length, len, 0.0f), math::float3(+grid_length, len, 0.0f), line_color, true);
		}
		if (display_grid_yz)
		{
			math::float4 line_color = math::float4(0.0f, 1.0f, 1.0f, 1);
			QUEUE_DEBUG_PRIMITIVE_LINE(math::float3(0.0f, len, -grid_length), math::float3(0.0f, len, +grid_length), line_color, true);
			QUEUE_DEBUG_PRIMITIVE_LINE(math::float3(0.0f, -grid_length, len), math::float3(0.0f, +grid_length, len), line_color, true);
		}
	}
}

#endif // defined(USE_IMGUI)
