#include "Editor/Windows/engine_config_window.h"

#if defined(USE_IMGUI)
#include "imgui.h"

#include "System/engine_config_data.h"
#include "Framework/display_settings.h"

void engine_config_window::render_window(std::shared_ptr<editor_items::context>& /*context*/, bool& open)
{
	if (ImGui::Begin("engine_config", &open))
	{
		//	アプリケーション
		if (ImGui::TreeNode("application"))
		{
			ImGui::InputTextUtf8("name", engine_config_data::instance()->application.name);
			ImGui::InputTextUtf8("splash_image_path", engine_config_data::instance()->application.splash_image_path);
			ImGui::InputTextUtf8("icon_path", engine_config_data::instance()->application.icon_path);
			ImGui::TextDisabled("restart to apply");
			ImGui::TreePop();
		}

		//	解像度と表示の方法
		render_display();

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

void engine_config_window::render_display()
{
	if (!display_settings::is_valid())
		return;
	if (!ImGui::TreeNode("display"))
		return;

	display_settings& settings = *display_settings::instance();
	const std::vector<display_settings::resolution>& resolutions = settings.get_resolutions();
	const display_settings::resolution& current = settings.get_current_resolution();

	//	未選択なら今の値を選んでおく
	if (display_resolution_index < 0 || display_resolution_index >= static_cast<int>(resolutions.size()))
	{
		const auto it = std::find(resolutions.begin(), resolutions.end(), current);
		display_resolution_index = it != resolutions.end() ? static_cast<int>(it - resolutions.begin()) : 0;
	}
	if (display_window_mode < 0)
		display_window_mode = static_cast<int>(settings.get_window_mode());

	//	解像度
	std::vector<std::string> labels;
	labels.reserve(resolutions.size());
	for (const display_settings::resolution& resolution : resolutions)
		labels.emplace_back(std::to_string(resolution.width) + " x " + std::to_string(resolution.height));
	std::vector<const char*> label_texts;
	for (const std::string& label : labels)
		label_texts.emplace_back(label.c_str());
	ImGui::Combo("resolution", &display_resolution_index, label_texts.data(), static_cast<int>(label_texts.size()));

	//	表示の方法
	std::vector<const char*> mode_texts;
	for (auto& name : magic_enum::enum_names<display_settings::window_modes>())
		mode_texts.emplace_back(name.data());
	ImGui::Combo("window_mode", &display_window_mode, mode_texts.data(), static_cast<int>(mode_texts.size()));

	const display_settings::resolution selected = resolutions.empty() ? current : resolutions[display_resolution_index];
	const display_settings::window_modes selected_mode = static_cast<display_settings::window_modes>(display_window_mode);
	const bool changed = selected != current || selected_mode != settings.get_window_mode();

	ImGui::BeginDisabled(!changed || settings.is_changing());
	if (ImGui::Button("apply"))
		settings.request_change(selected, selected_mode);
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::TextDisabled("current : %u x %u (%s)", current.width, current.height, magic_enum::enum_name(settings.get_window_mode()).data());

	ImGui::TreePop();
}
#endif	//	defined(USE_IMGUI)
