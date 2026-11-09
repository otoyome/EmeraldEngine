#include "Editor/Windows/debug_windows.h"

#if defined(USE_IMGUI)
#include "imgui.h"

#include "System/engine_config_data.h"
#include "Model/model_buffer_pool.h"
#include "System/memory_allocator.h"

void log_window::render_window(std::shared_ptr<editor_items::context>& /*context*/, bool& open)
{
	if (!logger::is_valid())
		return;

	if (ImGui::Begin("log", &open))
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

void engine_config_window::render_window(std::shared_ptr<editor_items::context>& /*context*/, bool& open)
{
	if (ImGui::Begin("engine_config", &open))
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

void model_buffer_pool_window::render_window(std::shared_ptr<editor_items::context>& /*context*/, bool& open)
{
	if (ImGui::Begin("model_buffer_pool", &open))
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

void memory_allocator_window::render_window(std::shared_ptr<editor_items::context>& /*context*/, bool& open)
{
	if (ImGui::Begin("memory_allocator", &open))
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
#endif	//	defined(USE_IMGUI)
