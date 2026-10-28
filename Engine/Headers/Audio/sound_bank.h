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
		std::string name;						//	「バンク名.音の名前」の形を推奨
		std::vector<std::string> files;			//	候補。鳴らすたびにランダムに 1 つ選ぶ
		bus target_bus = bus::se;				//	bgm は BGM として鳴らす（ストリーム・ループ・クロスフェード）
		float volume = 1.0f;
		float pitch = 1.0f;
		float volume_random = 0.0f;				//	鳴らすたびの音量の揺らぎ（±）
		float pitch_random = 0.0f;				//	鳴らすたびのピッチの揺らぎ（±）
		bool loop = false;
		float fade_in_time = 0.0f;				//	秒。BGM はクロスフェードの時間
		float fade_out_time = 0.0f;				//	秒。止める時

		template<class Archive>
		void save(Archive& archive, std::uint32_t const version) const;
		template<class Archive>
		void load(Archive& archive, std::uint32_t const version);
	};

	//	サウンドバンク
	struct sound_bank_desc
	{
		std::vector<sound_cue_desc> cues;

		bool load_file(std::string_view file_path);
		bool save_file(std::string_view file_path) const;
	};

	//	鳴らす音の名前
	struct sound_cue
	{
		constexpr explicit sound_cue(std::string_view name) : name(name), id(system_utility::generate_hash(name)) {}

		std::string_view name;
		system_utility::hash_key id;
	};
}
