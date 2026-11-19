#pragma once

#include <functional>

//	アプリケーション起動情報
struct application_desc
{
	const TCHAR* window_title = _T("Emerald Engine");	//	設定読込前に使う仮のウィンドウ名
	std::function<void()> register_types;				//	組み込み型の後に呼ぶ型登録(任意)
};

//	アプリケーション実行
int run_application(HINSTANCE instance, const application_desc& desc);
