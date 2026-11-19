#pragma once

#include "minigame000_roomba_movement.h"

namespace minigame000
{
	//	ルンバへの入力ベース
	class roomba_input_source
	{
	public:
		virtual ~roomba_input_source() = default;

		//	入力コマンド生成
		virtual bool make_command(std::uint32_t tick, roomba_input_command& out_command) = 0;
	};
}	//	namespace minigame000