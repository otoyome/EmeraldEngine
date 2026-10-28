#pragma once

#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Audio/sound_bank.h"
#include "System/singleton.h"

namespace audio
{
	//	@tool 読み込んだサウンドバンクを持つ。破棄されると（他に持ち主がいなければ）バンクを破棄する
	class sound_bank_handle
	{
	public:
		sound_bank_handle() = default;
		explicit sound_bank_handle(system_utility::hash_key key);
		sound_bank_handle(const sound_bank_handle&) = delete;
		sound_bank_handle& operator=(const sound_bank_handle&) = delete;
		sound_bank_handle(sound_bank_handle&& other) noexcept;
		sound_bank_handle& operator=(sound_bank_handle&& other) noexcept;
		~sound_bank_handle();

		void release();
		bool is_valid() const { return key != 0; }

	private:
		system_utility::hash_key key = 0;
	};

	//	@tool 鳴っている音を持つ。破棄されると音を止める
	class sound_instance
	{
	public:
		sound_instance() = default;
		sound_instance(sound_handle handle, std::shared_ptr<const sound_cue_desc> cue);
		sound_instance(const sound_instance&) = delete;
		sound_instance& operator=(const sound_instance&) = delete;
		sound_instance(sound_instance&& other) noexcept;
		sound_instance& operator=(sound_instance&& other) noexcept;
		~sound_instance();

		//	@tool 定義の fade_out_time で止める
		void stop();
		bool is_playing() const;

	private:
		sound_handle handle;
		std::shared_ptr<const sound_cue_desc> cue;	//	バンクが先に破棄されても定義は残る
	};

	//	@tool サウンドバンクの読み込み・破棄と、音の名前（cue）での再生
	class sound_player
	{
		SINGLETON(sound_player)

		friend class sound_bank_handle;

	private:
		sound_player();
		~sound_player();
		sound_player(const sound_player&) = delete;
		sound_player(sound_player&&) = delete;

	public:
		//	@tool 読み込む。同じファイルは数を数えて共有する
		[[nodiscard]] sound_bank_handle load_bank(std::string_view file_path);
		//	@tool 読み込み済みのバンクをファイルから作り直す（エディターで保存した時）
		bool reload_bank(std::string_view file_path);
		bool is_bank_loaded(std::string_view file_path) const;

		//	@tool 鳴らしっぱなしにする（BGM はそのまま切り替わる）
		void play(const sound_cue& cue);
		//	@tool 持ち主が止める音（ループ音など）。sound_instance を破棄すると止まる
		[[nodiscard]] sound_instance start(const sound_cue& cue);
		//	@tool 鳴らした BGM の定義の fade_out_time で止める
		void stop_bgm();

		//	@tool 定義を直接鳴らす（エディターの試し再生）
		[[nodiscard]] sound_instance preview(const sound_cue_desc& cue);

	private:
		struct bank_entry
		{
			std::string file_path;
			uint32_t references = 0;
			std::vector<std::shared_ptr<const sound_cue_desc>> cues;
		};

		void release_bank(system_utility::hash_key key);
		void add_cues(bank_entry& entry, sound_bank_desc&& desc);
		void remove_cues(const std::vector<std::shared_ptr<const sound_cue_desc>>& targets);
		void add_file_references(const sound_cue_desc& cue);
		void remove_file_references(const sound_cue_desc& cue);
		std::shared_ptr<const sound_cue_desc> find_cue(const sound_cue& cue) const;
		sound_handle start_core(const std::shared_ptr<const sound_cue_desc>& cue);

	private:
		std::unordered_map<system_utility::hash_key, bank_entry> banks;
		std::unordered_map<system_utility::hash_key, std::shared_ptr<const sound_cue_desc>> cues;
		std::unordered_map<std::string, uint32_t> file_references;
		std::shared_ptr<const sound_cue_desc> bgm_cue;
		std::mt19937 random;
	};
}
