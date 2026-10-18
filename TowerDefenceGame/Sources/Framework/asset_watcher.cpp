#include "Framework/asset_watcher.h"

#include "shader.h"
#include "texture.h"
#include "Editor/detail_profiler.h"

void asset_watcher::update()
{
#if defined(USE_IMGUI)
	//	各種データの更新チェック
	{
		DETAIL_PROFILE_CPU_SCOPE("update.asset_hot_reload");
		if (shader_manager::is_valid())
		{
			shader_manager::instance()->check_shader_update_and_recompile();
		}
		if (texture_manager::is_valid())
		{
			texture_manager::instance()->check_update_and_reload();
		}
	}
#endif //	defined(USE_IMGUI)
}
