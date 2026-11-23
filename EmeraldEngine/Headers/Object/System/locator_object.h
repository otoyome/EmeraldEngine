#pragma once

#include "object/spatial_object.h"

#include <memory>
#include <vector>

class scene_instance;

//	ロケーターオブジェクト
class locator_object : public spatial_object
{
	GAME_OBJECT_DECRLEAR(locator_object, spatial_object)

public:
	std::string_view get_tag() const;
	void set_tag(std::string_view tag);

public:
	locator_object(std::string_view name);
	locator_object(std::string_view name, std::string_view tag);
	~locator_object() override;

	void serialize(cereal::JSONOutputArchive& archive) const override;
	void deserialize(cereal::JSONInputArchive& archive) override;

#if defined(USE_IMGUI)
	void debug_update(float elapsed_time) override;
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

public:
	//	シーン内から tag が一致するロケーターを収集
	static void collect(const scene_instance& scene, std::string_view tag, std::vector<std::shared_ptr<locator_object>>& out);

private:
	core::cache_string tag;
};
