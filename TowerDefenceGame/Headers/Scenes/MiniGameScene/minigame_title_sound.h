#pragma once

#include "System/signals.h"
#include "Audio/sound_player.h"

//	@tool タイトルのサウンド。タイトルシーンの出来事を購読して鳴らす
class minigame_title_sound
{
public:
	explicit minigame_title_sound(signals::broadcaster& broadcaster);
	minigame_title_sound(const minigame_title_sound&) = delete;
	minigame_title_sound& operator=(const minigame_title_sound&) = delete;

private:
	audio::sound_bank_handle bank;			//	@tool タイトルの間だけ読み込む
	signals::connection_list connections;
};
