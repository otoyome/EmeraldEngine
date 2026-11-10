#pragma once

#if defined(USE_IMGUI)
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
#endif	//	defined(USE_IMGUI)
