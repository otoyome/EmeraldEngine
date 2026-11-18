#include "SceneSystems/Network/network_sandbox_system.h"

network_sandbox_system::network_sandbox_system()
{
}

network_sandbox_system::~network_sandbox_system()
{
}

void network_sandbox_system::on_begin()
{
	mouse::instance()->set_center_lock(false);
	mouse::instance()->set_cursol_hide(false);
}

void network_sandbox_system::pre_update(float elapsed_time)
{
}

#if defined(USE_IMGUI)
void network_sandbox_system::imgui_render()
{
}
#endif	//	defined(USE_IMGUI)

SCENE_SYSTEM_REGISTER(network_sandbox_system);