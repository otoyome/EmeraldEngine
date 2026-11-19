#include "Process/core_process.h"
#include "framework.h"

#include "System/game_config_data.h"
#include "System/project_path.h"
#include "Object/game_object_registry.h"
#include "Scenes/scene_system.h"
#include "Object/Collision/collision_filter.h"
#include "Object/builtin_types.h"
#include "Framework/application_desc.h"

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
	game_config_data::create_instance();

	//	オブジェクトの型登録表
	game_object_registry::create_instance();
	//	シーンシステムの型登録表
	scene_system_registry::create_instance();

	//	組み込み型を登録してからアプリケーション側の型を登録
	register_builtin_types();
	if (get_owner().desc.register_types)
		get_owner().desc.register_types();

	//	当たり判定フィルター
	collision_filter_manager::create_instance();
	return true;
}

void core_process::exit()
{
	//	当たり判定フィルター破棄
	collision_filter_manager::delete_instance();

	//	オブジェクトの型登録表解除
	scene_system_registry::delete_instance();
	game_object_registry::delete_instance();

	//	ゲームコンフィグ終了
	{
		game_config_data::delete_instance();
	}

	//	文字コード変換器破棄
	system_utility::uninitialize_charset_converter();

	//	COMオブジェクト破棄
	CoUninitialize();
}
