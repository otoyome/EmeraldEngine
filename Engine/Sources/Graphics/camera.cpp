#include "Graphics/camera.h"

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
bool view_selector::select(camera* source, view_selection& selection)
{
	if (source && !source->is_active())
		source = nullptr;

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
