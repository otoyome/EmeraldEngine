#pragma once

#include "scenes/scene_base.h"

#if defined(USE_IMGUI)
#include "effect_resource.h"
#include "effect_instance.h"

#include "object/camera_object.h"
#include "object/post_process_volume_object.h"

class edit_particle_scene : public scene_base
{
	SCENE_DECRLEAR(edit_particle_scene, scene_base);

public:
	edit_particle_scene();
	edit_particle_scene(const edit_particle_scene&) = delete;
	edit_particle_scene(edit_particle_scene&&) = delete;
	~edit_particle_scene() override;

protected:
	bool initialize_core() override;
	bool uninitialize_core() override;
	void update_core(float elapsed_time) override;
	void imgui_render_core() override;

private:
	void on_new();

private:
	std::shared_ptr<camera_object> main_camera;
	std::shared_ptr<post_process_volume_object> post_process_volume;

private:
	std::shared_ptr<editor_items::context> editor_context;

	effect_resource_ptr edit_resource;
	effect_instance_ptr edit_instance;
};
#endif // defined(USE_IMGUI)
