#include "Object/builtin_types.h"
#include "Object/game_object_registry.h"
#include "Scenes/scene_system.h"

#include "Object/game_object.h"
#include "Object/spatial_object.h"
#include "Object/Graphics/camera_object.h"
#include "Object/Graphics/effect_object.h"
#include "Object/Graphics/light_object.h"
#include "Object/Graphics/light_probe_object.h"
#include "Object/Graphics/model_object.h"
#include "Object/Collision/model_collision_object.h"
#include "Object/collision_shape_object.h"
#include "Object/Graphics/post_process_volume_object.h"
#include "Object/System/locator_object.h"
#include "Object/System/group_object.h"

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
	objects->add<effect_object>("Effect");
	objects->add<model_collision_object>("Collision");
	objects->add<collision_shape_object>("Collision");
	objects->add<post_process_volume_object>("Rendering");
	objects->add<locator_object>("Game/System");
	objects->add<group_object>("Basic");

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
