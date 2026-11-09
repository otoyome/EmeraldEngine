#include "Process/input_process.h"
#include "framework.h"

#include "Input/input_action_system.h"
#include "System/console_manager.h"

bool input_process::enter()
{
	framework& self = get_owner();

	//	入力関係初期化
	{
		mouse::create_instance(self.hwnd);
		keyboard::create_instance();
		gamepad::create_instance();
		input::action_system::create_instance();
	}
	return true;
}

void input_process::exit()
{
	//	入力関係終了
	{
		input::action_system::delete_instance();
		gamepad::delete_instance();
		keyboard::delete_instance();
		mouse::delete_instance();
	}
}

void input_process::update(float elapsed_time)
{
	//	入力関係の更新
	if (mouse::is_valid())
	{
		mouse::instance()->update_state();
	}
	if (keyboard::is_valid())
	{
		keyboard::instance()->update_state();
	}
	if (gamepad::is_valid())
	{
		gamepad::instance()->update_state();
	}
#if defined(USE_IMGUI)
	//	キー割り当てされたコンソールコマンドを実行
	console_manager::instance().update_bindings();
#endif //	defined(USE_IMGUI)
}
