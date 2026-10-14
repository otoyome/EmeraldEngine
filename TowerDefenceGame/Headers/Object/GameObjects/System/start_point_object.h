#pragma once

#include "object/spatial_object.h"

#include "collision/mesh.h"

//	開始位置指定オブジェクト
class start_point_object : public spatial_object
{
	GAME_OBJECT_DECRLEAR(start_point_object, spatial_object)

public:
	std::string_view get_tag() const;
	void set_tag(std::string_view tag);

public:
	start_point_object(std::string_view name, std::string_view tag);
	~start_point_object() override;

#if defined(USE_IMGUI)
	void debug_update(float elapsed_time) override;
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

private:
	cache_string tag;
};
