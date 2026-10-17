#pragma once

#include "view_parameters.h"

#include <memory>
#include <vector>

//	カメラ情報
class camera
{
	friend class camera_manager;

public:
	uint64_t get_id() const { return id; }

	bool is_active() const { return active; }
	void set_active(bool active) { this->active = active; }

	//	シーンビュー情報
	const view_parameters& get_parameters() const { return parameters; }
	view_parameters& get_parameters() { return parameters; }

	//	カメラカット等が発生した時に
	void request_cut() { cut_requested = true; }

public:
	camera();
	~camera();

private:
	view_parameters parameters;
	uint64_t id;
	bool active = true;
	bool cut_requested = false;
};

using camera_ptr = std::shared_ptr<camera>;

//	シーンビュー情報
struct view_selection
{
	view_parameters parameters;
	uint64_t source_id = 0;
	bool camera_cut = false;
};

//	カメラマネージャー
class camera_manager
{
	WORLD_SINGLETON(camera_manager)

private:
	camera_manager();
	~camera_manager();

public:
	//	シーンビューの候補を登録/解除
	void push_view_target(const camera_ptr& source);
	void remove_view_target(const camera_ptr& source);

	//	有効なシーンビューを選ぶ
	bool select_view(view_selection& selection);

	//	シーンビューの上書き
	void set_override_view(const camera_ptr& source);

	//	直近のシーンビュー
	const view_parameters* get_game_view() const { return has_game_view ? &game_view : nullptr; }

private:
	std::vector<std::weak_ptr<camera>> view_targets;	//	視点候補（後ろほど優先）
	uint64_t current_source_id = 0;						//	前回ID
	std::weak_ptr<camera> override_view;				//	上書きシーンビュー
	view_parameters game_view;							//	直近のゲーム側のシーンビュー
	bool has_game_view = false;
};
