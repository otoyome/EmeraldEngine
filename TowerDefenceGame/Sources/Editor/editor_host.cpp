#include "Editor/editor_host.h"

//	@tool ImGui の現在のコンテキストはスレッドごとに持つ（External/imgui/imconfig.h で GImGui をこれに置き換えている）
//	@tool メインスレッドはエディター用の別ウィンドウごとに切り替え、提出スレッドは常にメインのコンテキストで描く
struct ImGuiContext;
thread_local ImGuiContext* imgui_thread_context = nullptr;

#if defined(USE_IMGUI)
#include <algorithm>
#include <optional>

#include "System/engine_config_data.h"
#include "model_buffer_pool.h"
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
#include "Editor/editor_camera.h"
#include "System/console_manager.h"


#include "scenes/editscene/edit_model_scene.h"
#include "scenes/editscene/edit_particle_scene.h"
#include "scenes/editscene/edit_timeline_scene.h"
#include "scenes/DummyScene/dummy_scene.h"
#include "scenes/TestScene/collision_test_scene.h"
#include "scenes/TestScene/clustered_lighting_test_scene.h"
#include "scenes/TestScene/light_shadow_test_scene.h"
#include "Scenes/setup_scene.h"

#include "imPlot/imPlot.h"
#include "imtimeline/ImTimeline.h"
#include "render_data.h"
#include "System/world_viewport.h"
#include "Object/object_manager.h"

namespace
{
	template<typename Type>
	void change_scene()
	{
		if (ImGui::Button(Type::type_name().data()))
		{
			scene_manager::instance()->change_scene_for_imgui<Type>();
		}
	}

	//	デバッグ表示の設定
	console_variable<bool> grid_xz_variable("debug.grid.xz", true, "XZ平面のグリッド表示", console_flags::persist);
	console_variable<bool> grid_xy_variable("debug.grid.xy", false, "XY平面のグリッド表示", console_flags::persist);
	console_variable<bool> grid_yz_variable("debug.grid.yz", false, "YZ平面のグリッド表示", console_flags::persist);
	console_variable<int> grid_length_variable("debug.grid.length", 100, "グリッドの半径（メートル）", console_flags::persist);
	console_variable<bool> draw_mesh_aabb_variable("debug.draw.mesh_aabb", false, "モデルAABBの表示", console_flags::persist);
	console_variable<bool> draw_collision_variable("debug.draw.collision", false, "衝突判定の表示", console_flags::persist);

	//	@tool エディター用の別ウィンドウを開く
	console_command editor_open_command("editor.open", "エディター用の別ウィンドウを開く（editor.open model|particle）",
		[](const console_command::arguments& args)
		{
			if (args.empty() || !editor_host::request_open_by_name(args[0]))
				console_manager::instance().print("editor.open : usage editor.open model|particle");
		});
}

editor_host::editor_host(HWND hwnd, render_system& renderer) : hwnd(hwnd), renderer(renderer)
{
	renderer.set_debug_primitive_builder(&editor_host::build_debug_primitives);
	active_host = this;
}

editor_host::~editor_host()
{
	if (active_host == this)
		active_host = nullptr;
	renderer.set_debug_primitive_builder(nullptr);
}

bool editor_host::request_open_by_name(std::string_view kind_name)
{
	if (!active_host)
		return false;
	if (kind_name == "model")
		active_host->request_open(editor_kinds::model);
	else if (kind_name == "particle")
		active_host->request_open(editor_kinds::particle);
	else
		return false;
	return true;
}

bool editor_host::handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	//	@tool メインのウィンドウのメッセージは、メインのコンテキストで処理する
	if (!main_imgui_context)
		return false;
	ImGuiContext* previous_context = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(main_imgui_context);
	const bool handled = ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam) != 0;
	ImGui::SetCurrentContext(previous_context);
	return handled;
}

void editor_host::imgui_initialize()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImPlot::CreateContext();
	ImGui::GetIO().Fonts->AddFontFromFileTTF("Assets/Resources/Textures/Fonts/Noto_Sans_JP/NotoSansJP-VariableFont_wght.ttf", 24.0f, nullptr, glyphRangesJapanese);
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(render_data::instance()->get_device().Get(), render_data::instance()->get_immediate_context().Get());

	//	@tool 別ウィンドウ（ビューポート）の表示先は自前で持つ（描画と表示を提出スレッドで行うため）
	{
		ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
		platform_io.Renderer_CreateWindow = &editor_host::imgui_create_viewport_target;
		platform_io.Renderer_DestroyWindow = &editor_host::imgui_destroy_viewport_target;
		platform_io.Renderer_SetWindowSize = &editor_host::imgui_resize_viewport_target;
		platform_io.Renderer_RenderWindow = nullptr;
		platform_io.Renderer_SwapBuffers = nullptr;
	}
	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	//	ImGui
	{
		DXGI_SWAP_CHAIN_DESC desc;
		render_data::instance()->get_swap_chain()->GetDesc(&desc);

		// ビューポートの設定
		imgui_viewport.TopLeftX = 0.0f;
		imgui_viewport.TopLeftY = 0.0f;
		imgui_viewport.Width = static_cast<float>(desc.BufferDesc.Width);
		imgui_viewport.Height = static_cast<float>(desc.BufferDesc.Height);
		imgui_viewport.MinDepth = 0.0f;
		imgui_viewport.MaxDepth = 1.0f;
	}

	main_imgui_context = ImGui::GetCurrentContext();
	main_plot_context = ImPlot::GetCurrentContext();
}

void editor_host::imgui_terminate()
{
	//	@tool エディター用の別ウィンドウ（メインのフォントを共有しているので先に破棄する。描画スレッドは止まっている）
	open_requests.clear();
	editor_windows.clear();

	ImGui::ShutdownNeoSequencer();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImPlot::DestroyContext();
	ImGui::DestroyContext();

	main_imgui_context = nullptr;
	main_plot_context = nullptr;
}

void editor_host::imgui_create_viewport_target(ImGuiViewport* viewport)
{
	HWND window = static_cast<HWND>(viewport->PlatformHandleRaw ? viewport->PlatformHandleRaw : viewport->PlatformHandle);
	viewport->RendererUserData = new imgui_present::present_target_ptr(imgui_present::present_target::create(window, static_cast<UINT>(viewport->Size.x), static_cast<UINT>(viewport->Size.y)));
}

void editor_host::imgui_destroy_viewport_target(ImGuiViewport* viewport)
{
	//	@tool メインのウィンドウは表示先を持たない
	auto* holder = static_cast<imgui_present::present_target_ptr*>(viewport->RendererUserData);
	if (!holder)
		return;

	//	@tool 提出スレッドが描き終わってから解放する
	render_data::instance()->flush_rendering();
	(*holder)->release();
	delete holder;
	viewport->RendererUserData = nullptr;
}

void editor_host::imgui_resize_viewport_target(ImGuiViewport* viewport, ImVec2 size)
{
	auto* holder = static_cast<imgui_present::present_target_ptr*>(viewport->RendererUserData);
	if (!holder || !*holder)
		return;

	//	@tool 提出スレッドが描き終わってから作り直す（バックバッファへの参照が残っているとリサイズできない）
	render_data::instance()->flush_rendering();
	(*holder)->resize(static_cast<UINT>(size.x), static_cast<UINT>(size.y));
}

void editor_host::imgui_engine_config_window()
{
	if (ImGui::Begin("engine_config"))
	{
		//	垂直同期
		ImGui::Checkbox("vertical_sync", &engine_config_data::instance()->windows.vertical_sync);

		//	相対座標描画フラグ
		ImGui::Checkbox("camera_relative_rendering", &engine_config_data::instance()->rendering.camera_relative_rendering);

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

void editor_host::imgui_log_window()
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

void editor_host::imgui_model_buffer_pool_window()
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

void editor_host::imgui_memory_allocator_window()
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

void editor_host::imgui_textures_window()
{
	if (ImGui::Begin("textures"))
	{
		auto sorted_textures = texture_manager::instance()->get_textures();
		std::vector<std::shared_ptr<texture>> texture_list;
		for (auto& texture_ptr : sorted_textures)
		{
			auto texture = texture_ptr.second.lock();
			if (!texture)
				continue;

			if (texture->get_dimension() != D3D11_RESOURCE_DIMENSION::D3D11_RESOURCE_DIMENSION_TEXTURE2D ||
				!texture->get_shader_resource_view())
				continue;

			texture2d* p = static_cast<texture2d*>(texture.get());
			auto desc = p->get_desc();

			if (graphics_utility::is_int_format(desc.Format))
				continue;

			texture_list.emplace_back(texture);
		}

		std::sort(texture_list.begin(), texture_list.end(),
			[](const std::shared_ptr<texture>& l, const std::shared_ptr<texture>& r)
			{
				return l->get_file_path_or_key() < r->get_file_path_or_key();
			});

		for (auto& texture_ptr : texture_list)
		{
			ImGui::Separator();
			ImGui::PushID(texture_ptr->get_file_path_or_key().data());
			ImGui::Text(texture_ptr->get_file_path_or_key().data());
			if (texture_ptr->is_file_path_or_key())
			{
				if (ImGui::Button("Save"))
				{
					texture_manager::instance()->save_texture(texture_ptr);
				}
				bool cube_map = texture_ptr->is_flag(texture::flags::cube_map);
				if (ImGui::Checkbox("cube_map", &cube_map))
				{
					texture_ptr->set_flag(texture::flags::cube_map, cube_map);
				}
				bool srgb = texture_ptr->is_flag(texture::flags::srgb);
				if (ImGui::Checkbox("srgb", &srgb))
				{
					texture_ptr->set_flag(texture::flags::srgb, srgb);
				}

				if (ImGui::TreeNode("sampler"))
				{
					static constexpr const char* filter_texts[] = { "point", "linear", "anisotropic" };
					static constexpr const char* address_texts[] = { "wrap", "mirror", "clamp", "border", "mirror_once" };
					static_assert(ARRAYSIZE(filter_texts) == static_cast<size_t>(texture_sampler_desc::filters::max));
					static_assert(ARRAYSIZE(address_texts) == static_cast<size_t>(texture_sampler_desc::address_modes::max));

					texture_sampler_desc sampler_desc = texture_ptr->get_sampler_desc();
					bool changed = false;

					int filter = static_cast<int>(sampler_desc.filter);
					if (ImGui::Combo("filter", &filter, filter_texts, ARRAYSIZE(filter_texts)))
					{
						sampler_desc.filter = static_cast<texture_sampler_desc::filters>(filter);
						changed = true;
					}

					auto address_combo = [&](const char* label, texture_sampler_desc::address_modes& mode)
					{
						int value = static_cast<int>(mode);
						if (ImGui::Combo(label, &value, address_texts, ARRAYSIZE(address_texts)))
						{
							mode = static_cast<texture_sampler_desc::address_modes>(value);
							changed = true;
						}
					};
					address_combo("address_u", sampler_desc.address_u);
					address_combo("address_v", sampler_desc.address_v);
					address_combo("address_w", sampler_desc.address_w);

					if (sampler_desc.filter == texture_sampler_desc::filters::anisotropic)
					{
						int max_anisotropy = static_cast<int>(sampler_desc.max_anisotropy);
						if (ImGui::SliderInt("max_anisotropy", &max_anisotropy, 1, D3D11_REQ_MAXANISOTROPY))
						{
							sampler_desc.max_anisotropy = static_cast<UINT>(max_anisotropy);
							changed = true;
						}
					}
					changed |= ImGui::DragFloat("mip_lod_bias", &sampler_desc.mip_lod_bias, 0.01f, -16.0f, 15.99f);
					changed |= ImGui::DragFloat("min_lod", &sampler_desc.min_lod, 0.1f, 0.0f, D3D11_FLOAT32_MAX);
					changed |= ImGui::DragFloat("max_lod", &sampler_desc.max_lod, 0.1f, 0.0f, D3D11_FLOAT32_MAX);
					changed |= ImGui::ColorEdit4("border_color", sampler_desc.border_color.data());

					if (changed)
					{
						texture_ptr->set_sampler_desc(sampler_desc);
					}
					ImGui::TreePop();
				}
			}
			if (!texture_ptr->is_flag(texture::flags::cube_map))
			{
				ImGui::Image(texture_ptr->get_shader_resource_view().Get(), { 256, 256 });
			}
			else
			{
				ImGui::Text("Not Display Image");
			}
			ImGui::PopID();
		}
	}
	ImGui::End();
}

void editor_host::imgui_scene_change()
{
	if (ImGui::Begin("Scene Change Window"))
	{
		change_scene<edit_model_scene>();
		change_scene<dummy_scene>();
		change_scene<collision_test_scene>();
		change_scene<clustered_lighting_test_scene>();
		change_scene<light_shadow_test_scene>();
		change_scene<edit_particle_scene>();
		change_scene<edit_timeline_scene>();
		change_scene<setup_scene>();

		//	@tool モデル編集・パーティクル編集を別のウィンドウ（別のワールド）で開く（メイン画面はゲームのワールドを映し続ける）
		ImGui::Separator();
		if (ImGui::Button("open model editor window"))
			request_open(editor_kinds::model);
		if (ImGui::Button("open particle editor window"))
			request_open(editor_kinds::particle);
	}
	ImGui::End();
}

////////////////////////////////////////////////////////////////////////////////////////////
void editor_host::imgui_profiler_window()
{
	//	@tool レンダーグラフ（直前のフレームの描画パス一覧）
	if (renderer.get_frame_graph())
	{
		if (ImGui::Begin("render graph"))
		{
			renderer.get_frame_graph()->imgui_render();
		}
		ImGui::End();
	}

	//	詳細プロファイラー(オブジェクト関数/シーンレンダラー/GPUスコープ単位)
	if (detail_profiler::is_valid())
	{
		detail_profiler::instance()->imgui_render();
	}
}

void editor_host::build_debug_primitives(const view_render_data& render_data)
{
	//	衝突判定のデバッグ表示
	if (draw_collision_variable.get() && object_manager::is_valid())
	{
		object_manager::instance()->get_world().debug_primitive_draw();
	}

	//	モデルAABBのデバッグ表示
	if (draw_mesh_aabb_variable.get())
	{
		math::float4 rotation = {0.0f, 0.0f, 0.0f, 1.0f};
		math::float4 color = {1.0f, 0.0f, 0.0f, 1.0f};
		for (auto& draw_item : render_data.mesh_container.draw_items)
		{
			math::float3 scale((draw_item.bounds_max.x - draw_item.bounds_min.x) * 0.5f, (draw_item.bounds_max.y - draw_item.bounds_min.y) * 0.5f, (draw_item.bounds_max.z - draw_item.bounds_min.z) * 0.5f);
			math::float3 center(draw_item.bounds_min.x + scale.x, draw_item.bounds_min.y + scale.y, draw_item.bounds_min.z + scale.z);
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

void editor_host::add_views(render_system& renderer)
{
	for (const auto& window : editor_windows)
		window->add_view(renderer);
}

render_system::overlay_function editor_host::end_frame()
{
	imgui_present::draw_snapshot_ptr main_draw_data;
	std::vector<imgui_present::window_draw> windows;
	{
		ImGui::Render();
		main_draw_data = imgui_present::copy_draw_data(*ImGui::GetDrawData());

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();

			//	@tool 先頭はメインのウィンドウなので飛ばす
			const ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
			for (int index = 1; index < platform_io.Viewports.Size; ++index)
			{
				const ImGuiViewport* viewport = platform_io.Viewports[index];
				if ((viewport->Flags & ImGuiViewportFlags_IsMinimized) || !viewport->DrawData)
					continue;
				const auto* holder = static_cast<const imgui_present::present_target_ptr*>(viewport->RendererUserData);
				if (!holder || !*holder || !(*holder)->swap_chain || !(*holder)->render_target_view)
					continue;

				imgui_present::window_draw& draw = windows.emplace_back();
				draw.target = *holder;
				draw.draw_data = imgui_present::copy_draw_data(*viewport->DrawData);
				draw.clear = !(viewport->Flags & ImGuiViewportFlags_NoRendererClear);
			}
		}
	}

	//	@tool エディター用の別ウィンドウ
	for (const auto& window : editor_windows)
	{
		if (std::optional<imgui_present::window_draw> draw = window->end_frame())
			windows.emplace_back(std::move(*draw));
	}

	//	@tool 提出スレッドで描く（複製した描画データと表示先だけを使う。描画バックエンドはメインのコンテキストが持つ）
	return [main_context = main_imgui_context, main_draw_data, windows = std::move(windows), viewport_rect = imgui_viewport](ID3D11DeviceContext* immediate_context, ID3D11RenderTargetView* back_buffer)
	{
		ImGui::SetCurrentContext(main_context);

		//	ImGui描画
		if (main_draw_data)
		{
			DETAIL_PROFILE_CPU_SCOPE("render.imgui_and_present");
			//	@tool バックバッファへクリアせずに重ねて描き、描き終わったら描画先を外す
			immediate_context->OMSetRenderTargets(1, &back_buffer, nullptr);
			immediate_context->RSSetViewports(1, &viewport_rect);
			ImGui_ImplDX11_RenderDrawData(&main_draw_data->draw_data);
			ID3D11RenderTargetView* null_views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
			immediate_context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, null_views, nullptr);
		}

		//	@tool ImGui の別ウィンドウ（ビューポート）とエディター用の別ウィンドウを描いて表示する
		if (!windows.empty())
		{
			DETAIL_PROFILE_CPU_SCOPE("render.imgui_viewports");
			imgui_present::draw_windows(immediate_context, windows);
		}
	};
}

void editor_host::request_open(editor_kinds kind)
{
	open_requests.emplace_back(kind);
}

std::unique_ptr<editor_window> editor_host::create_editor_window(editor_kinds kind)
{
	const std::string number = std::to_string(++opened_count);
	ImFontAtlas* shared_fonts = ImGui::GetIO().Fonts;

	std::unique_ptr<editor_window> window;
	bool created = false;
	switch (kind)
	{
	case editor_kinds::model:
		window = std::make_unique<editor_window>("model_editor#" + number, "imgui_model_editor.ini", renderer, shared_fonts);
		created = window->create(L"Model Editor", 1280, 720, [] { scene_manager::instance()->change_scene<edit_model_scene>(); });
		break;
	case editor_kinds::particle:
		window = std::make_unique<editor_window>("particle_editor#" + number, "imgui_particle_editor.ini", renderer, shared_fonts);
		created = window->create(L"Particle Editor", 1280, 720, [] { scene_manager::instance()->change_scene<edit_particle_scene>(); });
		break;
	}
	if (!created)
		return nullptr;
	return window;
}

void editor_host::update_editor_windows()
{
	//	@tool 閉じる（描画中のフレームがワールドの管理クラスや表示先を参照しているので、終わるのを待ってから破棄する）
	const bool any_closed = std::any_of(editor_windows.begin(), editor_windows.end(), [](const auto& window) { return window->is_close_requested(); });
	if (any_closed)
	{
		renderer.flush_rendering();
		editor_windows.erase(std::remove_if(editor_windows.begin(), editor_windows.end(), [](const auto& window) { return window->is_close_requested(); }), editor_windows.end());
	}

	//	@tool 開く（シーンを読み込んだワールドを持つウィンドウを作る）
	for (editor_kinds kind : open_requests)
	{
		if (auto window = create_editor_window(kind))
			editor_windows.emplace_back(std::move(window));
	}
	open_requests.clear();

	//	@tool ウィンドウの大きさに合わせて表示先・描画先を作り直す
	for (const auto& window : editor_windows)
		window->update_targets();
}

void editor_host::update_world_viewports()
{
	//	@tool メイン画面は ImGui のウィンドウの上ではマウスを受け付けない（エディター用の別ウィンドウの入力はウィンドウごとに分かれている）
	world_viewport::instance()->set_accepts_input(!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow), true);

	for (const auto& window : editor_windows)
		window->update_world_viewport();
}

void editor_host::build_windows()
{
	//	ImGuiウィンドウ構築の計測
	DETAIL_PROFILE_CPU_SCOPE("update.imgui");

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	editor_items::begin_contexts();

	//	シーン情報表示
	if (scene_manager::is_valid())
	{
		scene_manager::instance()->imgui_render();
	}

	//	オブジェクト情報表示
	object_manager::instance()->imgui_render();

	//	ログ情報
	imgui_log_window();

	//	エンジンの設定情報
	imgui_engine_config_window();

	//	GPU計測時間表示
	imgui_profiler_window();

	//	モデルバッファプール使用量表示
	imgui_model_buffer_pool_window();

	//	メモリアロケータ使用量表示
	imgui_memory_allocator_window();

	//	テクスチャリスト情報
	imgui_textures_window();

	//	シーン変更
	imgui_scene_change();

	//	コンソール
	imgui_console_window();
	imgui_console_commands_window();

	//	@tool エディター用の別ウィンドウ（それぞれのコンテキストでフレームを組み立てる）
	for (const auto& window : editor_windows)
		window->build_windows();
}

#endif // defined(USE_IMGUI)
