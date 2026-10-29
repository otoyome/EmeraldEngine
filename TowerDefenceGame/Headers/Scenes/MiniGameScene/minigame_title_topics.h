#pragma once

#include "System/signals.h"

namespace minigame_title_topics
{
	constexpr signals::topic<> opened{ "minigame.title.opened" };		//	タイトルが開いた
	constexpr signals::topic<> decided{ "minigame.title.decided" };		//	ゲーム開始が決まった
	constexpr signals::topic<> closed{ "minigame.title.closed" };		//	タイトルが閉じた
}
