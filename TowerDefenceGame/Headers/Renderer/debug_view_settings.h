#pragma once

#if defined(USE_IMGUI)
#include <map>
#include <string>
#include <string_view>

#include "System/singleton.h"

//	デバッグ表示の設定
struct debug_view_settings
{
	bool grid_xz = true;		//	XZ平面のグリッド表示
	bool grid_xy = false;		//	XY平面のグリッド表示
	bool grid_yz = false;		//	YZ平面のグリッド表示
	int grid_length = 100;		//	グリッドの半径（メートル）
	bool mesh_aabb = false;		//	モデルAABBの表示
	bool collision = false;		//	衝突判定の表示

	//	メニューバーに「View」メニューを描く
	void imgui_menu();
};

//	ウィンドウ毎のデバッグ表示マネージャー
class debug_view_settings_manager
{
	SINGLETON(debug_view_settings_manager)

public:
	debug_view_settings& get(std::string_view key);

private:
	debug_view_settings_manager();
	~debug_view_settings_manager() = default;

	void register_settings_handler();

private:
	std::map<std::string, debug_view_settings, std::less<>> settings;
};
#endif	//	defined(USE_IMGUI)
