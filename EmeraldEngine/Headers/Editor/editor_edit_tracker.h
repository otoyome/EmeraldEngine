#pragma once

#if defined(USE_IMGUI)
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "imgui.h"
#include "Editor/editor_transaction.h"

class engine_context;
class game_object;
class scene_system;

//	編集をトランザクションに積む
class editor_edit_tracker
{
public:
	//	追跡物
	struct target
	{
		std::string label;									//	履歴に出す名前
		std::function<std::string()> capture;				//	今の中身(取れなければ空)
		std::function<bool(const std::string&)> apply;		//	中身を戻す
	};

	//	リドゥ/アンドゥショートカット
	enum class shortcuts
	{
		none,	//	押されていない
		redo,	//	リドゥ
		undo,	//	アンドゥ
	};

public:
	//	フレームの最初(インスペクターやギズモが書き換える前)に、追跡する物の中身を取っておく
	void begin(std::vector<target> targets);

	//	フレームの最後に、操作が終わっていて中身が変わっていれば history に積む
	void end(editor_transaction_history& history);

	//	追跡中の物の編集前の中身を今の中身にする(履歴に積まずに変えた時に呼ぶ)
	void resync();
	//	追跡をやめる
	void reset();

public:
	//	オブジェクトを追跡する物
	static target make_object_target(engine_context& context, const std::shared_ptr<game_object>& object);

	//	シーンシステムの中身の保存・読み直し
	static std::string make_system_snapshot(const scene_system& system);
	static bool apply_system_snapshot(engine_context& context, scene_system& system, const std::string& snapshot);

	//	履歴に出す名前作成
	static std::string make_label(std::string_view action, const std::shared_ptr<game_object>& object);

	//	Ctrl+Z / Ctrl+Y(Ctrl+Shift+Z)を調べる
	static shortcuts check_shortcuts(ImGuiInputFlags route);

private:
	//	追跡中の物と編集前の中身
	struct tracked_target
	{
		target source;
		std::string snapshot;
	};

private:
	std::vector<tracked_target> tracked_targets;	//	追跡中の物
	bool interacting = false;						//	前のフレームで操作中だったか
};

class material_resource;

//	変更をトランザクションに積む
class editor_change_tracker
{
public:
	//	フレームの最初(編集する UI を描く前)に、追跡する物を決める。前と違う物なら変更前を取り直す
	void set_target(std::string label, editor_transaction_target target);

	//	変更があったことを知らせる(編集する UI が変更ありを返した時に呼ぶ)
	void notify_changed() { changed = true; }

	//	フレームの最後に、操作が終わっていて変更があれば history に積む
	void end(editor_transaction_history& history);

	//	変更前を今の中身で取り直す(元に戻した時など、履歴に積まずに変えた時に呼ぶ)
	void resync();

	//	追跡をやめる
	void reset();

public:
	//	マテリアルを記録する物(戻した時は on_restored を呼ぶ)
	static editor_transaction_target make_material_target(const std::shared_ptr<material_resource>& material, std::function<void()> on_restored = {});

private:
	std::string label;						//	履歴に出す名前
	editor_transaction_target target;		//	追跡している物
	editor_restore_function checkpoint;		//	変更前に戻す関数
	bool changed = false;					//	変更を知らされたか
};
#endif	//	defined(USE_IMGUI)
