#pragma once

#include "view_parameters.h"

#include <memory>
#include <vector>

//	カメラ情報
class camera
{
	friend class view_selector;

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

//	シーンビューセレクター
class view_selector
{
public:
	//	game_source はゲーム側の視点、override_source はエディターカメラなどの上書き視点（無ければ nullptr）
	bool select(camera* game_source, camera* override_source, view_selection& selection);

	//	直近のシーンビュー
	const view_parameters* get_game_view() const { return has_game_view ? &game_view : nullptr; }

private:
	uint64_t current_source_id = 0;	//	前回ID
	view_parameters game_view;		//	直近のゲーム側のシーンビュー
	bool has_game_view = false;
};
