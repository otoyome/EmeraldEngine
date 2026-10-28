#pragma once

#include "Audio/sound_bank.h"

//	@tool どのシーンでも使う音（common バンク。framework が常に読み込んでおく）
namespace common_sounds
{
	constexpr std::string_view bank_path = "Assets/Resources/Sounds/Banks/common.json";

	constexpr audio::sound_cue decide{ "common.decide" };	//	@tool 決定
}
