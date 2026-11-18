#include "Process/audio_process.h"
#include "framework.h"

#include "Audio/audio_manager.h"
#include "Audio/sound_player.h"
#include "Editor/detail_profiler.h"

bool audio_process::enter()
{
	//	サウンド関係
	{
		audio_manager::create_instance();
		audio::sound_player::create_instance();
	}
	return true;
}

void audio_process::exit()
{
	//	サウンド関係
	{
		audio::sound_player::delete_instance();
		audio_manager::delete_instance();
	}
}

void audio_process::update(float elapsed_time)
{
	//	サウンド更新
	if (audio_manager::is_valid())
	{
		DETAIL_PROFILE_CPU_SCOPE("update.audio_manager");
		audio_manager::instance()->update(elapsed_time);
	}
}
