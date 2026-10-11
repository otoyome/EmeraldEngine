#include "framework.h"

#if defined(USE_IMGUI)
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

	//	名前で探したコンソール変数をチェックボックスで編集する
	bool checkbox_variable(const char* label, std::string_view name)
	{
		auto variable = dynamic_cast<console_variable<bool>*>(console_manager::instance().find_variable(name));
		return variable ? ImGui::Checkbox(label, &variable->ref()) : false;
	}

	//	コンソールウィンドウの状態
	bool console_visible = false;
	bool console_focus_input = false;
	bool console_scroll_to_bottom = false;
	std::string console_input;	//	入力中の文字列
	std::vector<std::string> console_history;
	int console_history_index = -1;

	//	入力候補
	std::vector<std::string> console_suggestions;
	size_t console_suggestion_word_begin = 0;	//	候補で置き換える語の開始位置
	int console_suggestion_index = -1;			//	選択中の候補
	bool console_suggestions_hovered = false;

	console_command console_toggle_command("console.toggle", "コンソールウィンドウの表示切り替え",
		[](const console_command::arguments&)
		{
			console_visible = !console_visible;
			console_focus_input = console_visible;

			//	「@」キーで開閉したときに入力された「@」を取り除く
			if (!console_input.empty() && console_input.back() == '@')
				console_input.pop_back();
		});

	//	コンソール要素リストウィンドウの状態
	console_variable<bool> console_commands_window_variable("editor.window.console_commands", true, "コンソール要素リストウィンドウの表示", console_flags::persist);
	std::string console_commands_filter;
	bool console_commands_changed_only = false;
	std::unordered_map<std::string, std::string> console_command_arguments;

	//	変数の値を型に合わせたウィジェットで編集する
	void edit_console_variable(console_variable_base& variable)
	{
		ImGui::BeginDisabled(variable.is_flag(console_flags::read_only));
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (auto value = dynamic_cast<console_variable<bool>*>(&variable))
		{
			ImGui::Checkbox("##value", &value->ref());
		}
		else if (auto value = dynamic_cast<console_variable<int>*>(&variable))
		{
			ImGui::DragInt("##value", &value->ref());
		}
		else if (auto value = dynamic_cast<console_variable<float>*>(&variable))
		{
			ImGui::DragFloat("##value", &value->ref(), 0.01f, 0.0f, 0.0f, "%.4f");
		}
		else if (auto value = dynamic_cast<console_variable<std::string>*>(&variable))
		{
			std::string utf8 = system_utility::convert_shift_jis_to_utf8(value->get());
			if (ImGui::InputText("##value", utf8, ImGuiInputTextFlags_EnterReturnsTrue))
				value->set(system_utility::convert_utf8_to_shift_jis(utf8));
		}
		else
		{
			ImGui::TextUnformatted(variable.to_string().c_str());
		}
		ImGui::EndDisabled();
	}

	//	入力中の語に合う候補を集める
	//	対象は先頭の語と、toggle / reset / help / list の引数
	void update_console_suggestions()
	{
		static constexpr size_t max_suggestions = 16;

		console_suggestions.clear();
		const std::string_view input(console_input);
		const size_t space = input.find(' ');
		const std::string_view first_word = input.substr(0, space);

		std::string_view word = input;
		console_suggestion_word_begin = 0;
		std::function<bool(const console_object&)> accept = [](const console_object&) { return true; };
		if (space != std::string_view::npos)
		{
			//	2語目だけを対象にする
			const std::string_view rest = input.substr(space + 1);
			if (rest.find(' ') != std::string_view::npos)
				return;
			if (first_word == "toggle")
				accept = [](const console_object& object) { return dynamic_cast<const console_variable<bool>*>(&object) != nullptr; };
			else if (first_word == "reset")
				accept = [](const console_object& object) { return object.is_variable(); };
			else if (first_word != "help" && first_word != "list")
				return;
			word = rest;
			console_suggestion_word_begin = space + 1;
		}
		else if (word.empty())
		{
			return;
		}

		std::vector<std::string> contains;
		for (auto* object : console_manager::instance().collect(""))
		{
			if (!accept(*object))
				continue;
			const std::string& name = object->get_name();
			if (name.starts_with(word))
				console_suggestions.emplace_back(name);
			else if (!word.empty() && name.find(word) != std::string::npos)
				contains.emplace_back(name);
		}
		console_suggestions.insert(console_suggestions.end(), contains.begin(), contains.end());
		if (console_suggestions.size() > max_suggestions)
			console_suggestions.resize(max_suggestions);

		//	入力した語と完全に一致する候補しかなければ表示しない
		if (console_suggestions.size() == 1 && console_suggestions.front() == word)
			console_suggestions.clear();
		if (console_suggestion_index >= static_cast<int>(console_suggestions.size()))
			console_suggestion_index = -1;
	}

	//	候補で入力中の語を置き換える
	std::string apply_console_suggestion(const std::string& input, const std::string& suggestion)
	{
		return input.substr(0, std::min(console_suggestion_word_begin, input.size())) + suggestion + " ";
	}

	//	入力欄の補完と、候補の選択・履歴
	int console_input_callback(ImGuiInputTextCallbackData* data)
	{
		auto replace_text = [data](const std::string& text)
		{
			data->DeleteChars(0, data->BufTextLen);
			data->InsertChars(0, text.c_str());
		};

		switch (data->EventFlag)
		{
			case ImGuiInputTextFlags_CallbackCompletion:
			{
				if (console_suggestions.empty())
					break;
				const std::string input(data->Buf, data->BufTextLen);

				//	選択中か候補が一つならそれで確定、複数なら共通する接頭辞まで埋める
				if (console_suggestion_index >= 0 || console_suggestions.size() == 1)
				{
					replace_text(apply_console_suggestion(input, console_suggestions[std::max(console_suggestion_index, 0)]));
				}
				else
				{
					std::string common = console_suggestions.front();
					for (auto& suggestion : console_suggestions)
					{
						size_t length = 0;
						while (length < common.size() && length < suggestion.size() && common[length] == suggestion[length])
							++length;
						common.resize(length);
					}
					const std::string word = input.substr(std::min(console_suggestion_word_begin, input.size()));
					if (common.size() > word.size() && common.starts_with(word))
						replace_text(input.substr(0, console_suggestion_word_begin) + common);
				}
				console_suggestion_index = -1;
				break;
			}
			case ImGuiInputTextFlags_CallbackHistory:
			{
				//	候補があるときは候補を選ぶ
				if (!console_suggestions.empty())
				{
					const int count = static_cast<int>(console_suggestions.size());
					if (data->EventKey == ImGuiKey_UpArrow)
						console_suggestion_index = std::min(console_suggestion_index + 1, count - 1);
					else if (data->EventKey == ImGuiKey_DownArrow)
						console_suggestion_index = std::max(console_suggestion_index - 1, -1);
					break;
				}

				if (console_history.empty())
					break;
				const int previous_index = console_history_index;
				const int history_count = static_cast<int>(console_history.size());
				if (data->EventKey == ImGuiKey_UpArrow)
				{
					if (console_history_index == -1)
						console_history_index = history_count - 1;
					else if (console_history_index > 0)
						--console_history_index;
				}
				else if (data->EventKey == ImGuiKey_DownArrow && console_history_index != -1)
				{
					if (++console_history_index >= history_count)
						console_history_index = -1;
				}
				if (previous_index != console_history_index)
					replace_text(console_history_index >= 0 ? console_history[console_history_index] : std::string());
				break;
			}
		}
		return 0;
	}
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
}

void framework::imgui_terminate()
{
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

void framework::imgui_main_window()
{
	if (ImGui::Begin("main"))
	{
		bool pause = object_manager::instance()->get_pause();
		ImGui::Checkbox("pause", &pause);
		object_manager::instance()->set_pause(pause);
		ImGui::SameLine();
		if (ImGui::Button("step"))
		{
			object_manager::instance()->called_step();
		}

		//	エディターカメラ
		checkbox_variable("editor_camera", "editor.camera.enable");
		ImGui::SameLine();
		checkbox_variable("console_commands", "editor.window.console_commands");
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

void framework::imgui_render_infos_window()
{
	if (ImGui::Begin("render infos"))
	{
		ImGui::Text("fixed_update_count : %d", object_manager::instance()->get_fixed_update_count());

		checkbox_variable("is_single_thread_rendering", "r.single_thread");

		ImGui::Checkbox("xz", &grid_xz_variable.ref()); ImGui::SameLine();
		ImGui::Checkbox("xy", &grid_xy_variable.ref()); ImGui::SameLine();
		ImGui::Checkbox("yz", &grid_yz_variable.ref());
		ImGui::DragInt("display_grid_length", &grid_length_variable.ref(), 1.0f, 0, 1000);

		ImGui::Checkbox("draw_mesh_aabb", &draw_mesh_aabb_variable.ref());

		ImGui::Checkbox("draw_collision", &draw_collision_variable.ref());

		checkbox_variable("freeze_culling", "r.culling.freeze");
		checkbox_variable("debug_visualize_meshlets", "r.meshlet.visualize");
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

void framework::imgui_textures_window()
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

void framework::imgui_scene_change()
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
	if (draw_collision_variable.get() && object_manager::is_valid())
	{
		object_manager::instance()->get_world().debug_primitive_draw();
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

void framework::imgui_console_window()
{
	if (!console_visible)
		return;

	ImGui::SetNextWindowSize(ImVec2(720.0f, 320.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("console", &console_visible))
	{
		auto& manager = console_manager::instance();

		//	出力
		const float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		if (ImGui::BeginChild("console_output", ImVec2(0.0f, -footer_height), false, ImGuiWindowFlags_HorizontalScrollbar))
		{
			const auto& output = manager.get_output();
			ImGuiListClipper clipper;
			clipper.Begin(static_cast<int>(output.size()));
			while (clipper.Step())
			{
				for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
				{
					const std::string utf8 = system_utility::convert_shift_jis_to_utf8(output[index]);
					ImGui::TextUnformatted(utf8.c_str());
				}
			}
			if (console_scroll_to_bottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
				ImGui::SetScrollHereY(1.0f);
			console_scroll_to_bottom = false;
		}
		ImGui::EndChild();
		ImGui::Separator();

		//	入力
		update_console_suggestions();
		if (console_focus_input)
		{
			ImGui::SetKeyboardFocusHere();
			console_focus_input = false;
		}
		ImGui::SetNextItemWidth(-1.0f);
		const ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
		if (ImGui::InputText("##console_input", console_input, flags, console_input_callback))
		{
			if (console_suggestion_index >= 0 && console_suggestion_index < static_cast<int>(console_suggestions.size()))
			{
				//	候補を選んでいる場合は確定だけ行う
				console_input = apply_console_suggestion(console_input, console_suggestions[console_suggestion_index]);
			}
			else
			{
				if (console_input.find_first_not_of(" \t") != std::string::npos)
				{
					if (console_history.empty() || console_history.back() != console_input)
						console_history.emplace_back(console_input);
					manager.execute(system_utility::convert_utf8_to_shift_jis(console_input));
				}
				console_input.clear();
				console_history_index = -1;
				console_scroll_to_bottom = true;
			}
			console_suggestion_index = -1;
			console_focus_input = true;
		}
		const bool input_active = ImGui::IsItemActive();
		const ImVec2 input_position = ImGui::GetItemRectMin();
		const float input_width = ImGui::GetItemRectSize().x;

		//	予測変換の候補
		const bool show_suggestions = !console_suggestions.empty() && (input_active || console_suggestions_hovered);
		console_suggestions_hovered = false;
		if (show_suggestions)
		{
			ImGui::SetNextWindowPos(input_position, ImGuiCond_Always, ImVec2(0.0f, 1.0f));
			ImGui::SetNextWindowSizeConstraints(ImVec2(input_width, 0.0f), ImVec2(FLT_MAX, 400.0f));
			const ImGuiWindowFlags suggestion_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize;
			if (ImGui::Begin("##console_suggestions", nullptr, suggestion_flags))
			{
				ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
				for (int index = static_cast<int>(console_suggestions.size()) - 1; index >= 0; --index)
				{
					const std::string& name = console_suggestions[index];
					const auto* object = manager.find(name);

					ImGui::PushID(index);
					if (ImGui::Selectable(name.c_str(), index == console_suggestion_index, ImGuiSelectableFlags_None, ImVec2(220.0f, 0.0f)))
					{
						console_input = apply_console_suggestion(console_input, name);
						console_suggestion_index = -1;
						console_focus_input = true;
					}
					if (object)
					{
						//	変数は現在値、コマンドは説明を添える
						ImGui::SameLine();
						const std::string detail = object->is_variable()
							? static_cast<const console_variable_base*>(object)->to_string()
							: system_utility::convert_shift_jis_to_utf8(object->get_help());
						ImGui::TextDisabled("%s", detail.c_str());
					}
					ImGui::PopID();
				}
				console_suggestions_hovered = ImGui::IsWindowHovered();
			}
			ImGui::End();
		}
	}
	ImGui::End();
}

void framework::imgui_console_commands_window()
{
	if (!console_commands_window_variable.get())
		return;

	ImGui::SetNextWindowSize(ImVec2(800.0f, 440.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("console commands", &console_commands_window_variable.ref()))
	{
		auto& manager = console_manager::instance();

		//	絞り込み
		ImGui::SetNextItemWidth(240.0f);
		ImGui::InputTextWithHint("##filter", "filter", console_commands_filter);
		ImGui::SameLine();
		ImGui::Checkbox("changed only", &console_commands_changed_only);
		ImGui::SameLine();
		if (ImGui::Button("console"))
			manager.execute("console.toggle");
		const std::string_view filter = console_commands_filter;

		//	コマンドごとの割り当てキー
		std::unordered_map<std::string, std::string> bound_keys;
		for (auto& [key, command] : manager.get_bindings())
		{
			auto& keys = bound_keys[command];
			if (!keys.empty())
				keys += ", ";
			keys += key;
		}

		//	名前の先頭ごとにまとめて表示する
		std::string current_category;
		bool table_open = false;
		bool category_open = false;
		for (auto* listed : manager.collect(""))
		{
			auto object = manager.find(listed->get_name());
			if (!object)
				continue;

			const std::string& name = object->get_name();
			if (!filter.empty() && name.find(filter) == std::string::npos)
				continue;

			auto variable = object->is_variable() ? static_cast<console_variable_base*>(object) : nullptr;
			const bool changed = variable && variable->to_string() != variable->default_to_string();
			if (console_commands_changed_only && !changed)
				continue;

			const size_t dot = name.find('.');
			const std::string category = dot != std::string::npos ? name.substr(0, dot) : std::string("(builtin)");
			if (category != current_category)
			{
				if (table_open)
					ImGui::EndTable();
				table_open = false;

				current_category = category;
				category_open = ImGui::CollapsingHeader(category.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
				if (category_open)
				{
					table_open = ImGui::BeginTable(category.c_str(), 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable);
					if (table_open)
					{
						ImGui::TableSetupColumn("name", ImGuiTableColumnFlags_WidthFixed, 200.0f);
						ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, 220.0f);
						ImGui::TableSetupColumn("key", ImGuiTableColumnFlags_WidthFixed, 50.0f);
						ImGui::TableSetupColumn("help", ImGuiTableColumnFlags_WidthStretch);
						ImGui::TableHeadersRow();
					}
				}
			}
			if (!table_open)
				continue;

			ImGui::TableNextRow();
			ImGui::PushID(name.c_str());

			//	名前
			ImGui::TableSetColumnIndex(0);
			const std::string label = name + (object->is_flag(console_flags::persist) ? " *" : "");
			if (changed)
				ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "%s", label.c_str());
			else
				ImGui::TextUnformatted(label.c_str());

			//	値の編集、またはコマンドの実行
			ImGui::TableSetColumnIndex(1);
			if (variable)
			{
				if (changed)
				{
					if (ImGui::SmallButton("reset"))
						variable->reset();
					ImGui::SameLine();
				}
				edit_console_variable(*variable);
			}
			else
			{
				auto& arguments = console_command_arguments[name];
				const bool run = ImGui::SmallButton("run");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(-FLT_MIN);
				const bool entered = ImGui::InputTextWithHint("##args", "args", arguments, ImGuiInputTextFlags_EnterReturnsTrue);
				if (run || entered)
					manager.execute(name + " " + system_utility::convert_utf8_to_shift_jis(arguments));
			}

			//	割り当てキー
			ImGui::TableSetColumnIndex(2);
			if (auto it = bound_keys.find(name); it != bound_keys.end())
				ImGui::TextUnformatted(it->second.c_str());

			//	説明
			ImGui::TableSetColumnIndex(3);
			const std::string help = system_utility::convert_shift_jis_to_utf8(object->get_help());
			ImGui::TextWrapped("%s", help.c_str());

			ImGui::PopID();
		}
		if (table_open)
			ImGui::EndTable();
	}
	ImGui::End();
}

#endif // defined(USE_IMGUI)
