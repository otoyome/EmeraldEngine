#include "System/imgui_texture_keeper.h"

#if defined(USE_IMGUI)
#include "Texture/texture.h"

ImTextureID imgui_texture_keeper::keep(const std::shared_ptr<texture>& value)
{
	if (!value)
		return nullptr;

	std::lock_guard<std::mutex> lock(mutex);
	kept_textures.push_back(value);
	return static_cast<ImTextureID>(value->get_shader_resource_view().Get());
}

void imgui_texture_keeper::release()
{
	std::vector<std::shared_ptr<texture>> released;
	{
		std::lock_guard<std::mutex> lock(mutex);
		released.swap(kept_textures);
	}
}
#endif	//	defined(USE_IMGUI)
