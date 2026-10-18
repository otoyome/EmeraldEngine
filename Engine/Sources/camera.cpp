#include "camera.h"

#include <atomic>

namespace
{
	std::atomic<uint64_t> next_camera_id{ 1 };
}

camera::camera()
: id(next_camera_id.fetch_add(1))
{
}

camera::~camera()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
camera_manager::camera_manager()
{
}

camera_manager::~camera_manager()
{
}

void camera_manager::push_view_target(const camera_ptr& source)
{
	if (!source)
		return;

	//	既に登録済みなら一度外して最後尾に積み直す
	remove_view_target(source);
	view_targets.emplace_back(source);
}

void camera_manager::remove_view_target(const camera_ptr& source)
{
	view_targets.erase(std::remove_if(view_targets.begin(), view_targets.end(),
		[&source](const std::weak_ptr<camera>& target) { return target.expired() || target.lock() == source; }), view_targets.end());
}

void camera_manager::set_override_view(const camera_ptr& source)
{
	override_view = source;
}

bool camera_manager::select_view(view_selection& selection)
{
	//	最後尾の有効な物を視点にする
	view_targets.erase(std::remove_if(view_targets.begin(), view_targets.end(),
		[](const std::weak_ptr<camera>& target) { return target.expired(); }), view_targets.end());

	camera_ptr source;
	for (auto it = view_targets.rbegin(); it != view_targets.rend(); ++it)
	{
		auto target = it->lock();
		if (target && target->is_active())
		{
			source = target;
			break;
		}
	}

	//	上書き前のゲーム側の視点を控えておく
	has_game_view = source != nullptr;
	if (source)
		game_view = source->parameters;

	//	上書き視点があれば優先する
	{
		auto override_source = override_view.lock();
		if (override_source && override_source->is_active())
			source = override_source;
	}
	if (!source)
		return false;

	selection.parameters = source->parameters;
	selection.source_id = source->id;

	//	視点が切り替わった場合は前フレームの行列を使わない
	selection.camera_cut = source->id != current_source_id || source->cut_requested;
	current_source_id = source->id;
	source->cut_requested = false;
	return true;
}
