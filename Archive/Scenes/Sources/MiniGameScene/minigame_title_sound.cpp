#include "Scenes/MiniGameScene/minigame_title_sound.h"

#include "Scenes/MiniGameScene/minigame_title_topics.h"

#include "Sound/common_sounds.h"

namespace
{
	//	サウンド
	constexpr std::string_view bank_path = "Assets/Resources/Sounds/minigame_title.esb";
	constexpr audio::sound_cue bgm{ "minigame_title.bgm" };
}

minigame_title_sound::minigame_title_sound(signals::broadcaster& broadcaster)
: bank()
, connections()
{
	if (!audio::sound_player::is_valid())
		return;

	auto* player = audio::sound_player::instance();
	bank = player->load_bank(bank_path);

	//	BGM 開始
	connections += broadcaster.subscribe(minigame_title_topics::opened, [player]()
	{
		player->play(bgm);
	});

	//	決定音を鳴らし、フェードアウトに合わせて BGM も止める
	connections += broadcaster.subscribe(minigame_title_topics::decided, [player]()
	{
		player->play(common_sounds::decide);
		player->stop_bgm();
	});

	connections += broadcaster.subscribe(minigame_title_topics::closed, [player]()
	{
		player->stop_bgm();
	});
}
