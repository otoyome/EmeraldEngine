#include "Editor/Windows/log_window.h"

#if defined(USE_IMGUI)
#include "imgui.h"

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
			static cache_string orders[] = {"~‡", "¸‡"};
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
#endif	//	defined(USE_IMGUI)
