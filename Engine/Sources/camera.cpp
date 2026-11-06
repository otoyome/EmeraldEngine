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
bool view_selector::select(camera* game_source, camera* override_source, view_selection& selection)
{
	if (game_source && !game_source->is_active())
		game_source = nullptr;

	//	上書き前のゲーム側の視点を控えておく
	has_game_view = game_source != nullptr;
	if (game_source)
		game_view = game_source->parameters;

	//	上書き視点があれば優先する
	camera* source = game_source;
	if (override_source && override_source->is_active())
		source = override_source;

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
