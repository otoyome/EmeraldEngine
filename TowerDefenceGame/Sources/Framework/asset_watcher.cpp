#include "Framework/asset_watcher.h"

#include "Graphics/shader.h"
#include "Editor/detail_profiler.h"

asset_watcher::asset_watcher()
{
}

asset_watcher::~asset_watcher()
{
}

void asset_watcher::update()
{
#if defined(USE_IMGUI)
	//	各種データの更新チェック
	DETAIL_PROFILE_CPU_SCOPE("update.asset_hot_reload");
	if (shader_manager::is_valid())
	{
		shader_manager::instance()->check_shader_update_and_recompile();
	}
#endif //	defined(USE_IMGUI)
}
