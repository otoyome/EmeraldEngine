#include "Editor/Windows/sound_bank_editor_window.h"

#if defined(USE_IMGUI)
#include <algorithm>
#include <filesystem>

#include "imgui.h"

#include "System/asset_browser.h"


namespace
{
	constexpr std::string_view sound_directory = "Assets/Resources/Sounds/RawFiles";

	//	指定ディレクトリ直下の、指定拡張子のファイルを「/」区切りの相対パスで集める
	std::vector<std::string> collect_files(std::string_view directory, std::initializer_list<std::string_view> extensions)
	{
		std::vector<std::string> files;
		std::error_code error;
		for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::path(directory), error))
		{
			if (!entry.is_regular_file())
				continue;
			const std::string extension = entry.path().extension().string();
			if (std::find(extensions.begin(), extensions.end(), extension) == extensions.end())
				continue;
			files.emplace_back(std::string(directory) + "/" + entry.path().filename().string());
		}
		std::sort(files.begin(), files.end());
		return files;
	}

	//	std::string を固定長の入力欄で編集する
	bool input_string(const char* label, std::string& text)
	{
		char buffer[256] = {};
		strncpy_s(buffer, text.c_str(), _TRUNCATE);
		if (!ImGui::InputText(label, buffer, sizeof(buffer)))
			return false;
		text = buffer;
		return true;
	}

	//	拡張子がサウンドバンクか
	bool is_sound_bank_file(std::string_view file_path)
	{
		const std::string extension = std::filesystem::path(std::string(file_path)).extension().string();
		return _stricmp(extension.c_str(), std::string(audio::sound_bank_desc::extension).c_str()) == 0;
	}
}

sound_bank_editor_window::sound_bank_editor_window(std::string_view file_path)
	: file_path(file_path)
{
	scan_sound_files();
	load();
}

sound_bank_editor_window::~sound_bank_editor_window()
{
}

void sound_bank_editor_window::register_asset_handlers(std::shared_ptr<editor_items::context> context)
{
	auto* browser = asset_browser::instance();
	const std::string extension(audio::sound_bank_desc::extension);

	//	ダブルクリックでファイルごとの窓を開く
	asset_browser::open_handler open;
	open.label = "Sound Bank Editor";
	open.extensions = { extension };
	open.open_file = [context](const cache_string& file_path) mutable
	{
		editor_items::open_window<sound_bank_editor_window>(context, file_path.view());
	};
	browser->register_open_handler(std::move(open));

	//	空のバンクを作る
	asset_browser::create_handler create;
	create.label = "Sound Bank";
	create.default_name = "NewSoundBank";
	create.extension = extension;
	create.create = [](std::string_view file_path)
	{
		return audio::sound_bank_desc{}.save_file(file_path);
	};
	browser->register_create_handler(std::move(create));

	//	ゲームで読み込み中のバンクは消さない
	browser->register_delete_validator([](std::string_view file_path, std::string& out_reason)
	{
		if (!is_sound_bank_file(file_path) || !audio::sound_player::is_valid() || !audio::sound_player::instance()->is_bank_loaded(file_path))
			return true;
		out_reason = "sound bank is in use";
		return false;
	});
}

std::string sound_bank_editor_window::get_label() const
{
	const std::string name = std::filesystem::path(file_path).filename().string();
	return "Sound Bank : " + system_utility::convert_shift_jis_to_utf8(name) + (dirty ? " *" : "");
}

void sound_bank_editor_window::scan_sound_files()
{
	sound_files = collect_files(sound_directory, { ".wav", ".ogg", ".mp3", ".flac" });
}

bool sound_bank_editor_window::load()
{
	audio::sound_bank_desc loaded_bank;
	if (!loaded_bank.load_file(file_path))
	{
		message = "load failed: " + system_utility::convert_shift_jis_to_utf8(file_path);
		return false;
	}
	preview = {};
	bank = std::move(loaded_bank);
	selected_cue = bank.cues.empty() ? -1 : 0;
	dirty = false;
	loaded = true;
	message.clear();
	return true;
}

bool sound_bank_editor_window::save()
{
	const std::string file_path_utf8 = system_utility::convert_shift_jis_to_utf8(file_path);
	if (!bank.save_file(file_path))
	{
		message = "save failed: " + file_path_utf8;
		return false;
	}
	dirty = false;
	message = "saved: " + file_path_utf8;

	//	ゲームで読み込み中なら作り直して、編集をすぐ反映する
	if (audio::sound_player::is_valid() && audio::sound_player::instance()->reload_bank(file_path))
		message += " (reloaded)";
	return true;
}

void sound_bank_editor_window::on_file_moved(std::string_view new_file_path)
{
	file_path = std::string(new_file_path);
}

bool sound_bank_editor_window::select_sound_file(const char* label, std::string& target_path)
{
	bool changed = false;
	if (ImGui::BeginCombo(label, target_path.empty() ? "(none)" : target_path.c_str()))
	{
		for (const auto& candidate : sound_files)
		{
			const bool selected = candidate == target_path;
			if (ImGui::Selectable(candidate.c_str(), selected))
			{
				target_path = candidate;
				changed = true;
			}
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	return changed;
}

void sound_bank_editor_window::imgui_render_toolbar()
{
	ImGui::BeginDisabled(!dirty);
	if (ImGui::Button("save"))
		save();
	ImGui::SameLine();
	if (ImGui::Button("revert"))
		load();
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("rescan sounds"))
		scan_sound_files();
	ImGui::SameLine();

	const bool in_use = audio::sound_player::is_valid() && audio::sound_player::instance()->is_bank_loaded(file_path);
	ImGui::TextDisabled("%s%s", in_use ? "[in use] " : "", dirty ? "* modified" : "");
}

void sound_bank_editor_window::imgui_render_cue_list()
{
	auto& cues = bank.cues;
	if (ImGui::Button("add"))
	{
		audio::sound_cue_desc cue;
		cue.name = std::filesystem::path(file_path).stem().string() + ".new_cue";
		cues.push_back(std::move(cue));
		selected_cue = static_cast<int>(cues.size()) - 1;
		dirty = true;
	}
	ImGui::SameLine();
	ImGui::BeginDisabled(selected_cue < 0);
	if (ImGui::Button("duplicate"))
	{
		audio::sound_cue_desc cue = cues[selected_cue];
		cue.name += "_copy";
		cues.insert(cues.begin() + selected_cue + 1, std::move(cue));
		++selected_cue;
		dirty = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("remove"))
	{
		cues.erase(cues.begin() + selected_cue);
		selected_cue = (std::min)(selected_cue, static_cast<int>(cues.size()) - 1);
		dirty = true;
	}
	ImGui::EndDisabled();

	ImGui::Separator();
	for (int index = 0; index < static_cast<int>(cues.size()); ++index)
	{
		ImGui::PushID(index);
		if (ImGui::Selectable(cues[index].name.empty() ? "(no name)" : cues[index].name.c_str(), index == selected_cue))
			selected_cue = index;
		ImGui::PopID();
	}
}

void sound_bank_editor_window::imgui_render_cue_detail(audio::sound_cue_desc& cue)
{
	bool changed = false;
	changed |= input_string("name", cue.name);

	//	バス
	if (ImGui::BeginCombo("bus", std::string(magic_enum::enum_name(cue.target_bus)).c_str()))
	{
		for (const auto value : magic_enum::enum_values<audio::bus>())
		{
			if (ImGui::Selectable(std::string(magic_enum::enum_name(value)).c_str(), value == cue.target_bus))
			{
				cue.target_bus = value;
				changed = true;
			}
		}
		ImGui::EndCombo();
	}

	//	候補のファイル
	ImGui::SeparatorText("files (random)");
	for (size_t index = 0; index < cue.files.size(); ++index)
	{
		ImGui::PushID(static_cast<int>(index));
		ImGui::SetNextItemWidth(-60.0f);
		changed |= select_sound_file("##file", cue.files[index]);
		ImGui::SameLine();
		if (ImGui::Button("x"))
		{
			cue.files.erase(cue.files.begin() + index);
			changed = true;
			ImGui::PopID();
			break;
		}
		ImGui::PopID();
	}
	if (ImGui::Button("add file"))
	{
		cue.files.emplace_back(sound_files.empty() ? std::string() : sound_files.front());
		changed = true;
	}

	ImGui::SeparatorText("playback");
	changed |= ImGui::SliderFloat("volume", &cue.volume, 0.0f, 2.0f);
	changed |= ImGui::SliderFloat("volume random", &cue.volume_random, 0.0f, 1.0f);
	changed |= ImGui::SliderFloat("pitch", &cue.pitch, 0.1f, 3.0f);
	changed |= ImGui::SliderFloat("pitch random", &cue.pitch_random, 0.0f, 1.0f);
	if (cue.target_bus == audio::bus::bgm)
	{
		//	BGM は常にループする
		ImGui::BeginDisabled();
		bool loop = true;
		ImGui::Checkbox("loop (bgm always)", &loop);
		ImGui::EndDisabled();
	}
	else
	{
		changed |= ImGui::Checkbox("loop", &cue.loop);
	}
	changed |= ImGui::DragFloat("fade in", &cue.fade_in_time, 0.01f, 0.0f, 10.0f, "%.2f s");
	changed |= ImGui::DragFloat("fade out", &cue.fade_out_time, 0.01f, 0.0f, 10.0f, "%.2f s");

	ImGui::SeparatorText("preview");
	if (ImGui::Button("play"))
	{
		if (audio::sound_player::is_valid())
			preview = audio::sound_player::instance()->preview(cue);
	}
	ImGui::SameLine();
	if (ImGui::Button("stop"))
		preview = {};
	ImGui::SameLine();
	ImGui::TextDisabled(preview.is_playing() ? "playing" : "");

	if (changed)
		dirty = true;
}

void sound_bank_editor_window::render_window(std::shared_ptr<editor_items::context>& /*context*/, bool& open)
{
	const std::string title = get_label() + "###sound_bank:" + system_utility::convert_shift_jis_to_utf8(file_path);
	ImGui::SetNextWindowSize(ImVec2(760.0f, 480.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(title.c_str(), &open))
	{
		if (loaded)
			imgui_render_toolbar();
		if (!message.empty())
			ImGui::TextUnformatted(message.c_str());
		ImGui::Separator();

		if (loaded)
		{
			if (ImGui::BeginChild("cues", ImVec2(240.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
				imgui_render_cue_list();
			ImGui::EndChild();
			ImGui::SameLine();
			if (ImGui::BeginChild("detail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
			{
				if (selected_cue >= 0 && selected_cue < static_cast<int>(bank.cues.size()))
					imgui_render_cue_detail(bank.cues[selected_cue]);
				else
					ImGui::TextDisabled("select a cue");
			}
			ImGui::EndChild();
		}
	}
	ImGui::End();
}
#endif	//	defined(USE_IMGUI)
