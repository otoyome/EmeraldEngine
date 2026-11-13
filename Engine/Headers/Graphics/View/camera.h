#pragma once

#include "Graphics/View/view_parameters.h"

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
	bool select(camera* source, view_selection& selection);

private:
	uint64_t current_source_id = 0;	//	前回ID
};
