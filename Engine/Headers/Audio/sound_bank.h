#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Audio/audio_manager.h"
#include "System/system_utility.h"

namespace audio
{
	//	サウンドキュー
	struct sound_cue_desc
	{
		std::string name;				//	バンク名
		std::vector<std::string> files;	//	再生候補リスト
		bus target_bus = bus::se;		//	バス指定
		float volume = 1.0f;			//	音量
		float pitch = 1.0f;				//	ピッチ
		float volume_random = 0.0f;		//	音量の揺らぎ
		float pitch_random = 0.0f;		//	ピッチの揺らぎ
		bool loop = false;				//	ループ再生
		float fade_in_time = 0.0f;		//	フェードイン時間
		float fade_out_time = 0.0f;		//	フェードアウト時間

		template<class Archive>
		void save(Archive& archive, std::uint32_t const version) const;
		template<class Archive>
		void load(Archive& archive, std::uint32_t const version);
	};

	//	サウンドバンク
	struct sound_bank_desc
	{
		static constexpr std::string_view extension = ".esb";

		std::vector<sound_cue_desc> cues;

		bool load_file(std::string_view file_path);
		bool save_file(std::string_view file_path) const;
	};

	//	サウンドキュー
	struct sound_cue
	{
		std::string_view name;
		system_utility::hash_key id;

		constexpr explicit sound_cue(std::string_view name) : name(name), id(system_utility::generate_hash(name)) {}
	};
}
