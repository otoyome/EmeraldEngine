#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "System/singleton.h"

namespace audio
{
	//	@tool 音量をまとめて調整するバス（全体の音量はマスター音量で別に持つ）
	enum class bus : uint8_t
	{
		bgm,	//	@tool 背景音楽
		se,		//	@tool 効果音
		ui,		//	@tool UI の操作音

		max,
	};
	static constexpr size_t bus_count = static_cast<size_t>(bus::max);

	//	@tool 再生中の音を指すハンドル
	//	@tool 世代付きなので、音が終わって枠が再利用された後に古いハンドルで操作しても何も起きない
	struct sound_handle
	{
		static constexpr uint32_t invalid_index = ~0u;

		uint32_t index = invalid_index;
		uint32_t generation = 0;

		bool is_valid() const { return index != invalid_index; }
	};

	//	@tool 再生パラメーター
	struct play_parameters
	{
		bus target_bus = bus::se;
		float volume = 1.0f;
		float pitch = 1.0f;
		bool loop = false;
		bool stream = false;		//	@tool ファイルから少しずつ読みながら再生する（BGM など長い音向け）。false なら全体を展開してキャッシュする
		float fade_in_time = 0.0f;	//	@tool フェードイン時間（秒）
	};
}

//	@tool サウンド管理クラス
//	@tool ・バックエンドは miniaudio。ミキシングは miniaudio のオーディオスレッドで行う（miniaudio の型はこのヘッダーに出さない）
//	@tool ・関数はすべてメインスレッド専用
//	@tool ・出力デバイスが無いなどで初期化に失敗した場合は、何も鳴らさずに全ての呼び出しを無視する
//	@tool ・展開して再生した音（stream=false）はファイルパス単位でキャッシュし、clear_cache() まで保持する
class audio_manager
{
	SINGLETON(audio_manager)

public:
	//	@tool 同時に鳴らせる音の最大数
	static constexpr uint32_t max_voices = 128;

private:
	audio_manager();
	~audio_manager();
	audio_manager(const audio_manager&) = delete;
	audio_manager(audio_manager&&) = delete;

public:
	//	@tool 初期化に成功して音を鳴らせる状態か
	bool is_available() const;

	//	@tool 毎フレームメインスレッドから呼ぶ
	//	@tool バス音量の反映と、再生が終わった音の片付けを行う
	void update(float elapsed_time);

	//	@tool 再生
	//	@tool 失敗した場合（ファイルが無い/同時発音数の上限など）は無効なハンドルを返す
	audio::sound_handle play(std::string_view file_path, const audio::play_parameters& parameters = {});

	//	@tool 停止（fade_out_time 秒かけて音量を下げてから止める）
	void stop(audio::sound_handle handle, float fade_out_time = 0.0f);
	void stop_bus(audio::bus target_bus, float fade_out_time = 0.0f);
	void stop_all(float fade_out_time = 0.0f);

	bool is_playing(audio::sound_handle handle) const;

	void set_paused(audio::sound_handle handle, bool paused);
	void set_volume(audio::sound_handle handle, float volume);
	void set_pitch(audio::sound_handle handle, float pitch);

	//	@tool BGM（同時に 1 曲。別の曲を指定するとクロスフェードで切り替える。同じ曲が再生中なら何もしない）
	void play_bgm(std::string_view file_path, float fade_time = 1.0f);
	void stop_bgm(float fade_time = 1.0f);

	//	@tool 音量（0～1）
	//	@tool コンソール変数 audio.volume.* と同じ値で、ゲーム設定に保存される
	float get_master_volume() const;
	void set_master_volume(float volume);
	float get_bus_volume(audio::bus target_bus) const;
	void set_bus_volume(audio::bus target_bus, float volume);

	//	@tool キャッシュした展開済みの音を解放する（再生中の音はそのまま鳴り続ける）
	void clear_cache();

	size_t get_playing_count() const;

private:
	struct backend;
	struct voice;

	voice* find_voice(audio::sound_handle handle);
	const voice* find_voice(audio::sound_handle handle) const;
	void release_voice(uint32_t index);
	void apply_volumes();

private:
	std::unique_ptr<backend> impl;
	std::vector<voice> voices;
	std::vector<uint32_t> free_indices;
	std::unordered_set<std::wstring> cached_files;

	audio::sound_handle bgm_handle;
	std::string bgm_file_path;
};
