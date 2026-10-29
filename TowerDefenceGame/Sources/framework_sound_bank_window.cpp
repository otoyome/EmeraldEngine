#include "framework.h"

#if defined(USE_IMGUI)
#include "imgui.h"

#include "System/console_manager.h"
#include "Audio/sound_player.h"

namespace
{
	//	サウンドバンク編集ウィンドウの状態
	console_variable<bool> sound_bank_window_variable("editor.window.sound_bank", true, "サウンドバンク編集ウィンドウの表示", console_flags::persist);

	constexpr std::string_view bank_directory = "Assets/Resources/Sounds/Banks";
	constexpr std::string_view sound_directory = "Assets/Resources/Sounds";

	//	編集中のバンク
	struct sound_bank_editor
	{
		std::vector<std::string> bank_files;
		std::vector<std::string> sound_files;
		bool scanned = false;

		std::string file_path;
		audio::sound_bank_desc bank;
		int selected_cue = -1;
		bool dirty = false;

		char new_bank_name[64] = {};
		std::string message;
		audio::sound_instance preview;
	};
	sound_bank_editor editor;

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

	void scan_files()
	{
		editor.bank_files = collect_files(bank_directory, { ".json" });
		editor.sound_files = collect_files(sound_directory, { ".wav", ".ogg", ".mp3", ".flac" });
		editor.scanned = true;
	}

	void open_bank(const std::string& file_path)
	{
		audio::sound_bank_desc bank;
		if (!bank.load_file(file_path))
		{
			editor.message = "load failed: " + file_path;
			return;
		}
		editor.preview = {};
		editor.file_path = file_path;
		editor.bank = std::move(bank);
		editor.selected_cue = editor.bank.cues.empty() ? -1 : 0;
		editor.dirty = false;
		editor.message.clear();
	}

	void save_bank()
	{
		if (editor.file_path.empty())
			return;
		if (!editor.bank.save_file(editor.file_path))
		{
			editor.message = "save failed: " + editor.file_path;
			return;
		}
		editor.dirty = false;
		editor.message = "saved: " + editor.file_path;

		//	ゲームで読み込み中なら作り直して、編集をすぐ反映する
		if (audio::sound_player::is_valid() && audio::sound_player::instance()->reload_bank(editor.file_path))
			editor.message += " (reloaded)";
	}

	void create_bank()
	{
		const std::string name = editor.new_bank_name;
		if (name.empty())
			return;

		const std::string file_path = std::string(bank_directory) + "/" + name + ".json";
		if (std::filesystem::exists(file_path))
		{
			editor.message = "already exists: " + file_path;
			return;
		}
		std::error_code error;
		std::filesystem::create_directories(std::filesystem::path(bank_directory), error);

		editor.preview = {};
		editor.file_path = file_path;
		editor.bank = {};
		editor.selected_cue = -1;
		editor.dirty = false;
		save_bank();
		editor.new_bank_name[0] = '\0';
		scan_files();
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

	//	音のファイルを一覧から選ぶ
	bool select_sound_file(const char* label, std::string& file_path)
	{
		bool changed = false;
		if (ImGui::BeginCombo(label, file_path.empty() ? "(none)" : file_path.c_str()))
		{
			for (const auto& candidate : editor.sound_files)
			{
				const bool selected = candidate == file_path;
				if (ImGui::Selectable(candidate.c_str(), selected))
				{
					file_path = candidate;
					changed = true;
				}
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	void bank_toolbar()
	{
		if (ImGui::BeginCombo("bank", editor.file_path.empty() ? "(select)" : editor.file_path.c_str()))
		{
			for (const auto& file_path : editor.bank_files)
			{
				if (ImGui::Selectable(file_path.c_str(), file_path == editor.file_path))
					open_bank(file_path);
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::Button("rescan"))
			scan_files();

		ImGui::SetNextItemWidth(200.0f);
		ImGui::InputText("##new_bank", editor.new_bank_name, sizeof(editor.new_bank_name));
		ImGui::SameLine();
		if (ImGui::Button("new bank"))
			create_bank();

		if (editor.file_path.empty())
			return;

		ImGui::BeginDisabled(!editor.dirty);
		if (ImGui::Button("save"))
			save_bank();
		ImGui::SameLine();
		if (ImGui::Button("revert"))
			open_bank(std::string(editor.file_path));
		ImGui::EndDisabled();
		ImGui::SameLine();

		const bool loaded = audio::sound_player::is_valid() && audio::sound_player::instance()->is_bank_loaded(editor.file_path);
		ImGui::TextDisabled("%s%s", loaded ? "[in use] " : "", editor.dirty ? "* modified" : "");
	}

	void cue_list()
	{
		auto& cues = editor.bank.cues;
		if (ImGui::Button("add"))
		{
			audio::sound_cue_desc cue;
			cue.name = std::filesystem::path(editor.file_path).stem().string() + ".new_cue";
			cues.push_back(std::move(cue));
			editor.selected_cue = static_cast<int>(cues.size()) - 1;
			editor.dirty = true;
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(editor.selected_cue < 0);
		if (ImGui::Button("duplicate"))
		{
			audio::sound_cue_desc cue = cues[editor.selected_cue];
			cue.name += "_copy";
			cues.insert(cues.begin() + editor.selected_cue + 1, std::move(cue));
			++editor.selected_cue;
			editor.dirty = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("remove"))
		{
			cues.erase(cues.begin() + editor.selected_cue);
			editor.selected_cue = (std::min)(editor.selected_cue, static_cast<int>(cues.size()) - 1);
			editor.dirty = true;
		}
		ImGui::EndDisabled();

		ImGui::Separator();
		for (int index = 0; index < static_cast<int>(cues.size()); ++index)
		{
			ImGui::PushID(index);
			if (ImGui::Selectable(cues[index].name.empty() ? "(no name)" : cues[index].name.c_str(), index == editor.selected_cue))
				editor.selected_cue = index;
			ImGui::PopID();
		}
	}

	void cue_detail(audio::sound_cue_desc& cue)
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
			cue.files.emplace_back(editor.sound_files.empty() ? std::string() : editor.sound_files.front());
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
				editor.preview = audio::sound_player::instance()->preview(cue);
		}
		ImGui::SameLine();
		if (ImGui::Button("stop"))
			editor.preview = {};
		ImGui::SameLine();
		ImGui::TextDisabled(editor.preview.is_playing() ? "playing" : "");

		if (changed)
			editor.dirty = true;
	}
}

void framework::imgui_sound_bank_window()
{
	if (!sound_bank_window_variable.get())
	{
		//	閉じたら試し再生を止める
		editor.preview = {};
		return;
	}

	if (!editor.scanned)
		scan_files();

	ImGui::SetNextWindowSize(ImVec2(760.0f, 480.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("sound bank", &sound_bank_window_variable.ref()))
	{
		bank_toolbar();
		if (!editor.message.empty())
			ImGui::TextUnformatted(editor.message.c_str());
		ImGui::Separator();

		if (!editor.file_path.empty())
		{
			if (ImGui::BeginChild("cues", ImVec2(240.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
				cue_list();
			ImGui::EndChild();
			ImGui::SameLine();
			if (ImGui::BeginChild("detail", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
			{
				if (editor.selected_cue >= 0 && editor.selected_cue < static_cast<int>(editor.bank.cues.size()))
					cue_detail(editor.bank.cues[editor.selected_cue]);
				else
					ImGui::TextDisabled("select a cue");
			}
			ImGui::EndChild();
		}
	}
	ImGui::End();
}
#endif	//	defined(USE_IMGUI)
