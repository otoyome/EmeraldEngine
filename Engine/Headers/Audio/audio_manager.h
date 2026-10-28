#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "System/singleton.h"

#include "miniaudio/miniaudio.h"

namespace audio
{
	//	音量をまとめて調整するバス
	enum class bus : uint8_t
	{
		bgm,	//	背景音楽
		se,		//	効果音
		ui,		//	UI の操作音
	};

	//	再生中の音を指すハンドル
	struct sound_handle
	{
		static constexpr uint32_t invalid_index = ~0u;

		uint32_t index = invalid_index;
		uint32_t generation = 0;

		bool is_valid() const { return index != invalid_index; }
	};

	//	再生パラメーター
	struct play_parameters
	{
		bus target_bus = bus::se;
		float volume = 1.0f;
		float pitch = 1.0f;
		bool loop = false;
		bool stream = false;		//	ストリーミング再生
		float fade_in_time = 0.0f;	//	フェードイン時間（秒）
	};
}

//	サウンド管理クラス
class audio_manager
{
	SINGLETON(audio_manager)

	//	miniaudio のエンジンとバスごとのグループ
	struct backend
	{
		ma_engine engine{};
		std::array<ma_sound_group, magic_enum::enum_count<audio::bus>()> groups{};
		bool initialized = false;
	};

	//	再生中の音の枠
	struct voice
	{
		std::unique_ptr<ma_sound> sound;
		uint32_t generation = 0;
		audio::bus target_bus = audio::bus::se;
		bool paused = false;
		bool allocated = false;	//	枠を確保済みか
	};

	//	update() で実行する命令
	struct command
	{
		enum class types : uint8_t
		{
			play,			//	音を作って鳴らす（SE は初回に展開してキャッシュ、BGM はストリーム）
			stop,			//	止める（フェード有りは鳴り終わった後に枠を返す）
			set_paused,		//	一時停止・再開（再生位置は残る）
			set_volume,		//	音量を変える
			set_pitch,		//	ピッチを変える
			clear_cache,	//	キャッシュクリア
			preload,		//	展開してキャッシュに載せる
			release,		//	キャッシュから外す
		};

		types type = types::play;
		audio::sound_handle handle;
		std::string file_path;
		audio::play_parameters parameters;
		float value = 0.0f;
		bool flag = false;
	};

public:
	//	同時に鳴らせる音の最大数
	static constexpr uint32_t max_voices = 128;

private:
	audio_manager();
	~audio_manager();
	audio_manager(const audio_manager&) = delete;
	audio_manager(audio_manager&&) = delete;

public:
	//	初期化に成功して音を鳴らせる状態か？
	bool is_available() const;

	//	更新
	void update(float elapsed_time);

	//	再生
	audio::sound_handle play(std::string_view file_path, const audio::play_parameters& parameters = {});

	//	停止
	void stop(audio::sound_handle handle, float fade_out_time = 0.0f);
	void stop_bus(audio::bus target_bus, float fade_out_time = 0.0f);
	void stop_all(float fade_out_time = 0.0f);

	//	再生中か？
	bool is_playing(audio::sound_handle handle) const;

	void set_paused(audio::sound_handle handle, bool paused);
	void set_volume(audio::sound_handle handle, float volume);
	void set_pitch(audio::sound_handle handle, float pitch);

	//	BGM
	audio::sound_handle play_bgm(std::string_view file_path, float fade_time = 1.0f, float volume = 1.0f);
	void stop_bgm(float fade_time = 1.0f);

	//	音量調整
	float get_master_volume() const;
	void set_master_volume(float volume);
	float get_bus_volume(audio::bus target_bus) const;
	void set_bus_volume(audio::bus target_bus, float volume);

	//	キャッシュした展開済みの音を解放する
	void clear_cache();

	//	ファイル単位でキャッシュに載せる／外す
	void preload(std::string_view file_path);
	void release(std::string_view file_path);

	//	再生数を取得
	size_t get_playing_count() const;

private:
	voice* find_voice(audio::sound_handle handle);
	const voice* find_voice(audio::sound_handle handle) const;
	void release_voice(uint32_t index);
	audio::sound_handle allocate_voice(std::string_view file_path, const audio::play_parameters& parameters);
	void push_stop(audio::sound_handle handle, float fade_out_time);
	void execute_stop(audio::sound_handle handle, float fade_out_time);

	void execute_command(command& item);
	void execute_play(command& item);
	bool cache_file(const std::string& file_path);

	void release_cache();
	void apply_volumes();

private:
	std::unique_ptr<backend> impl;
	std::vector<voice> voices;
	std::vector<uint32_t> free_indices;
	std::unordered_set<std::wstring> cached_files;

	audio::sound_handle bgm_handle;
	std::string bgm_file_path;

	mutable std::mutex mutex;
	std::vector<command> commands;
	std::vector<command> executing_commands;
};
