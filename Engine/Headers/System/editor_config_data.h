#pragma	once

#if defined(USE_IMGUI)
#include <map>
#include <string>

//	エディター用の設定情報（個人ごとの設定。editor_config.ini に保存する）
//	persist 属性のコンソール変数の値と、キー割り当てを保存する
struct editor_config_data
{
	SINGLETON(editor_config_data)

private:
	editor_config_data();
	~editor_config_data();
	editor_config_data(const editor_config_data&) = delete;
	editor_config_data(editor_config_data&&) = delete;

	//	設定ファイルが無いときのキー割り当て
	static void apply_default_bindings();
};
#endif	//	defined(USE_IMGUI)
