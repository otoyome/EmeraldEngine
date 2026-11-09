#include "Process/core_process.h"
#include "framework.h"

#include "System/game_config_data.h"
#include "System/project_path.h"

bool core_process::enter()
{
	HRESULT hr{ S_OK };

	//	COMオブジェクト初期化
	{
		hr = CoInitializeEx(NULL, 0);
		_ASSERT_EXPR(SUCCEEDED(hr), hr_trace(hr));
		if (FAILED(hr))
		{
			return false;
		}
	}

	//	文字コード変換器所持
	system_utility::initialize_charset_converter();

	//	プロジェクトのルートディレクトリを控える
	project_path::initialize();

	//	ゲームコンフィグ初期化
	{
		//	@remainder	内部でコンソールマネージャーに情報を受け渡しているだけ。
		game_config_data::create_instance();
	}
	return true;
}

void core_process::exit()
{
	//	ゲームコンフィグ終了
	{
		game_config_data::delete_instance();
	}

	//	文字コード変換器破棄
	system_utility::uninitialize_charset_converter();

	//	COMオブジェクト破棄
	CoUninitialize();
}
