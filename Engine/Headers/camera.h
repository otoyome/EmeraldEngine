#pragma once

#include "view_parameters.h"

#include <memory>
#include <vector>

//	カメラ情報
class camera
{
public:
	uint64_t get_id() const { return id; }

	bool is_active() const { return active; }
	void set_active(bool active) { this->active = active; }

	//	視点情報
	const view_parameters& get_parameters() const { return parameters; }
	view_parameters& get_parameters() { return parameters; }

	//	カメラカット等が発生した時に
	void request_cut() { cut_requested = true; }

public:
	camera();
	~camera();

private:
	friend class camera_manager;

	view_parameters parameters;
	uint64_t id;
	bool active = true;
	bool cut_requested = false;
};

using camera_ptr = std::shared_ptr<camera>;

//	視点情報
struct view_selection
{
	view_parameters parameters;
	uint64_t source_id = 0;
	bool camera_cut = false;
};

//	カメラマネージャー
class camera_manager
{
	SINGLETON(camera_manager)

private:
	camera_manager();
	~camera_manager();

public:
	//	視点の候補を登録/解除
	void push_view_target(const camera_ptr& source);
	void remove_view_target(const camera_ptr& source);

	//	有効な視点を選ぶ
	bool select_view(view_selection& selection);

private:
	std::vector<std::weak_ptr<camera>> view_targets;	//	視点候補（後ろほど優先）
	uint64_t current_source_id = 0;						//	前回ID
};
