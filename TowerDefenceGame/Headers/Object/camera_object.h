#pragma once

#include "camera.h"
#include "object/game_object.h"
#include "object/spatial_object.h"

//	カメラ制御用オブジェクト
class camera_object : public spatial_object
{
	GAME_OBJECT_DECRLEAR(camera_object, spatial_object)

public:
	camera_ptr get_camera() const {return camera;}

public:
	camera_object(std::string_view name);
	~camera_object() override;

	void post_update(float elapsed_time) override;

#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

protected:
	void on_active() override;
	void on_deactive() override;

private:
	void sync_from_spatial();

private:
	camera_ptr camera;
};
