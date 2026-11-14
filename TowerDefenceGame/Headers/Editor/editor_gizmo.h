#pragma once

#if defined(USE_IMGUI)
#include "imgui.h"

class engine_context;

//	エディターのギズモ
class editor_gizmo
{
public:
	//	操作の種類
	enum class operations
	{
		translate,	//	移動
		rotate,		//	回転
		scale,		//	拡縮
	};

public:
	//	ギズモのメニュー
	void imgui_menu();

	//	選んでいるオブジェクトにギズモを描いて操作する
	bool manipulate(engine_context& context, const ImVec2& position, const ImVec2& size);

private:
	//	ショートカット
	void handle_shortcuts();

private:
	operations operation = operations::translate;	//	操作の種類
	bool local = false;								//	ローカル軸で操作するか（拡縮は常にローカル）
	bool snap = false;								//	スナップするか
	float snap_translate = 10.0f;					//	移動のスナップ量（1 = 10cm）
	float snap_rotate = 15.0f;						//	回転のスナップ量（度）
	float snap_scale = 0.1f;						//	拡縮のスナップ量
};
#endif	//	defined(USE_IMGUI)
