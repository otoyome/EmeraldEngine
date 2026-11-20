#include "Object/builtin_types.h"
#include "Object/game_object_registry.h"
#include "Scenes/scene_system.h"

#include "Object/game_object.h"
#include "Object/spatial_object.h"
#include "Object/camera_object.h"
#include "Object/light_object.h"
#include "Object/model_object.h"
#include "Object/model_collision_object.h"
#include "Object/collision_shape_object.h"
#include "Object/post_process_volume_object.h"
#include "Object/System/start_point_object.h"

#include "SceneSystems/Editor/edit_material_system.h"
#include "SceneSystems/Editor/edit_model_system.h"
#include "SceneSystems/Editor/edit_particle_system.h"
#include "SceneSystems/Editor/edit_prefab_system.h"
#include "SceneSystems/Editor/edit_timeline_system.h"
#include "SceneSystems/Network/network_sandbox_system.h"
#include "SceneSystems/Test/clustered_lighting_test_system.h"
#include "SceneSystems/Test/collision_test_system.h"
#include "SceneSystems/Test/light_shadow_test_system.h"

void register_builtin_types()
{
	//	ゲームオブジェクト
	game_object_registry* objects = game_object_registry::instance();
	objects->add<game_object>("");
	objects->add<spatial_object>("");
	objects->add<camera_object>("Camera");
	objects->add<directional_light_object>("Light");
	objects->add<point_light_object>("Light");
	objects->add<spot_light_object>("Light");
	objects->add<sky_light_object>("Light/Probe");
	objects->add<area_light_probe_object>("Light/Probe");
	objects->add<model_object>("Model");
	objects->add<model_collision_object>("Collision");
	objects->add<collision_shape_object>("Collision");
	objects->add<post_process_volume_object>("Rendering");
	objects->add<start_point_object>("Game/System");

	//	シーンシステム
	scene_system_registry* systems = scene_system_registry::instance();
#if defined(USE_IMGUI)
	systems->add<edit_material_system>();
	systems->add<edit_model_system>();
	systems->add<edit_particle_system>();
	systems->add<edit_prefab_system>();
	systems->add<edit_timeline_system>();
#endif	//	defined(USE_IMGUI)
	systems->add<network_sandbox_system>();
	systems->add<clustered_lighting_test_system>();
	systems->add<collision_test_system>();
	systems->add<light_shadow_test_system>();
}
