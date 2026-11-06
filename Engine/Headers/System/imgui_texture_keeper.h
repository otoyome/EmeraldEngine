#pragma	once

#if defined(USE_IMGUI)
#include <memory>
#include <mutex>
#include <vector>

class texture;

//	ImGui に渡したテクスチャを ImGui の描画が終わるまで保持
class imgui_texture_keeper
{
	SINGLETON(imgui_texture_keeper)

private:
	imgui_texture_keeper() = default;
	~imgui_texture_keeper() = default;

public:
	//	テクスチャを保持して ImGui に渡す ID を返す
	ImTextureID keep(const std::shared_ptr<texture>& value);

	//	ImGui の描画が終わった後に呼んで、保持していたテクスチャを手放す
	void release();

private:
	std::mutex mutex;
	std::vector<std::shared_ptr<texture>> kept_textures;
};
#endif	//	defined(USE_IMGUI)
