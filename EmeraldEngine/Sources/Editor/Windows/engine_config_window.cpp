#include "Editor/Windows/engine_config_window.h"

#if defined(USE_IMGUI)
#include "imgui.h"

#include "System/engine_config_data.h"
#include "Framework/display_settings.h"
#include "Object/Collision/collision_filter.h"
#include "Graphics/Core/render_data.h"

#include <algorithm>
#include <optional>

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

		//	当たり判定
		render_collision();
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

	//	HDR設定
	ImGui::Checkbox("hdr", &engine_config_data::instance()->windows.enable_hdr);
	ImGui::SameLine();
	const render_data& data = *render_data::instance();
	if (data.is_hdr_output())
		ImGui::TextDisabled("output : HDR (peak %.0f nits / sdr white %.0f nits)", data.get_display_color_space_data().peak_nits, data.get_display_color_space_data().sdr_white_nits);
	else
		ImGui::TextDisabled("output : SDR");

	ImGui::TreePop();
}

void engine_config_window::render_collision()
{
	if (!ImGui::TreeNode("collision"))
		return;

	engine_config_data::collision_data& data = engine_config_data::instance()->collision;
	bool changed = false;
	if (ImGui::TreeNode("channels"))
	{
		changed |= render_collision_channels(data);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("profiles"))
	{
		changed |= render_collision_profiles(data);
		ImGui::TreePop();
	}

	//	変えたらすぐフィルターを作り直す
	if (changed && collision_filter_manager::is_valid())
		collision_filter_manager::instance()->rebuild_filters();

	ImGui::TreePop();
}

bool engine_config_window::render_collision_channels(engine_config_data::collision_data& data)
{
	bool changed = false;
	std::optional<size_t> remove_index;
	for (size_t index = 0; index < data.channels.size(); ++index)
	{
		engine_config_data::collision_data::channel_data& channel = data.channels[index];
		ImGui::PushID(static_cast<int>(index));

		std::string name = channel.name;
		ImGui::SetNextItemWidth(160.0f);
		if (ImGui::InputText("##name", name, ImGuiInputTextFlags_EnterReturnsTrue) && !name.empty() && collision_filter_manager::find_channel(name) == collision_filter::max_channels)
		{
			for (engine_config_data::collision_data::profile_data& profile : data.profiles)
			{
				if (profile.object_type == channel.name)
					profile.object_type = name;
				for (engine_config_data::collision_data::response_data& response : profile.channel_responses)
				{
					if (response.channel == channel.name)
						response.channel = name;
				}
			}
			channel.name = name;
			changed = true;
		}
		ImGui::SameLine();
		//	最後の1つは消せない
		ImGui::BeginDisabled(data.channels.size() <= 1);
		if (ImGui::SmallButton("remove"))
			remove_index = index;
		ImGui::EndDisabled();
		ImGui::PopID();
	}

	//	消したチャンネルを使っている所も消す
	if (remove_index)
	{
		const std::string removed = data.channels[*remove_index].name;
		data.channels.erase(data.channels.begin() + static_cast<std::ptrdiff_t>(*remove_index));
		for (engine_config_data::collision_data::profile_data& profile : data.profiles)
		{
			if (profile.object_type == removed)
				profile.object_type = data.channels.front().name;
			auto& responses = profile.channel_responses;
			responses.erase(std::remove_if(responses.begin(), responses.end(), [&removed](const engine_config_data::collision_data::response_data& response) { return response.channel == removed; }), responses.end());
		}
		changed = true;
	}

	ImGui::BeginDisabled(data.channels.size() >= engine_config_data::collision_data::max_channels);
	if (ImGui::Button("add channel"))
	{
		std::vector<std::string> names;
		for (const engine_config_data::collision_data::channel_data& channel : data.channels)
			names.emplace_back(channel.name);
		engine_config_data::collision_data::channel_data& channel = data.channels.emplace_back();
		channel.name = make_unique_name(names, "channel");
		changed = true;
	}
	ImGui::EndDisabled();
	return changed;
}

bool engine_config_window::render_collision_profiles(engine_config_data::collision_data& data)
{
	bool changed = false;
	std::vector<std::string> channel_names;
	for (const engine_config_data::collision_data::channel_data& channel : data.channels)
		channel_names.emplace_back(channel.name);

	std::optional<size_t> remove_index;
	for (size_t index = 0; index < data.profiles.size(); ++index)
	{
		engine_config_data::collision_data::profile_data& profile = data.profiles[index];
		ImGui::PushID(static_cast<int>(index));
		const bool opened = ImGui::TreeNode("##profile", "%s", profile.name.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("remove"))
			remove_index = index;
		if (opened)
		{
			std::string name = profile.name;
			if (ImGui::InputText("name", name, ImGuiInputTextFlags_EnterReturnsTrue) && !name.empty() &&
				std::none_of(data.profiles.begin(), data.profiles.end(), [&name](const engine_config_data::collision_data::profile_data& other) { return other.name == name; }))
			{
				profile.name = name;
				changed = true;
			}

			if (ImGui::Combo("object type", profile.object_type, channel_names))
				changed = true;

			//	チャンネルごとの応答
			for (size_t channel_index = 0; channel_index < data.channels.size(); ++channel_index)
			{
				const engine_config_data::collision_data::channel_data& channel = data.channels[channel_index];
				auto& responses = profile.channel_responses;
				auto it = std::find_if(responses.begin(), responses.end(), [&channel](const engine_config_data::collision_data::response_data& response) { return response.channel == channel.name; });
				const bool listed = it != responses.end();
				collision_response response = listed ? it->response : collision_response::ignore;

				ImGui::PushID(static_cast<int>(channel_index));
				ImGui::SetNextItemWidth(100.0f);
				if (ImGui::Combo(channel.name.c_str(), response))
				{
					if (listed)
						responses.erase(it);
					if (response != collision_response::ignore)
					{
						engine_config_data::collision_data::response_data& added = responses.emplace_back();
						added.channel = channel.name;
						added.response = response;
					}
					changed = true;
				}
				ImGui::PopID();
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	if (remove_index)
	{
		data.profiles.erase(data.profiles.begin() + static_cast<std::ptrdiff_t>(*remove_index));
		changed = true;
	}

	if (ImGui::Button("add profile"))
	{
		std::vector<std::string> names;
		for (const engine_config_data::collision_data::profile_data& profile : data.profiles)
			names.emplace_back(profile.name);
		engine_config_data::collision_data::profile_data& profile = data.profiles.emplace_back();
		profile.name = make_unique_name(names, "profile");
		profile.object_type = data.channels.empty() ? std::string() : data.channels.front().name;
		changed = true;
	}
	return changed;
}

std::string engine_config_window::make_unique_name(const std::vector<std::string>& names, std::string_view base)
{
	for (int number = 1; ; ++number)
	{
		std::string name = std::string(base) + "_" + std::to_string(number);
		if (std::find(names.begin(), names.end(), name) == names.end())
			return name;
	}
}
#endif	//	defined(USE_IMGUI)
