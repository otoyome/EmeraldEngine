#pragma once

#include "System/signals.h"

//	@tool タイトルシーンが知らせる出来事（受け手はタイトルシーンを知らずに済む）
namespace minigame_title_topics
{
	constexpr signals::topic<> opened{ "minigame.title.opened" };		//	@tool タイトルが開いた
	constexpr signals::topic<> decided{ "minigame.title.decided" };		//	@tool ゲーム開始が決まった
	constexpr signals::topic<> closed{ "minigame.title.closed" };		//	@tool タイトルが閉じた
}
