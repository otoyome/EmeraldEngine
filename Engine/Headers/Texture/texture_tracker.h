#pragma once

#if defined(USE_IMGUI)
#include <memory>
#include <mutex>
#include <vector>

#include "Texture/texture.h"

//	生成したテクスチャの一覧
class texture_tracker
{
	SINGLETON(texture_tracker)

public:
	texture_tracker();
	~texture_tracker();

	//	生成したテクスチャを登録する
	void add(const texture_ptr& value);

	//	生きているテクスチャを集める
	std::vector<texture_ptr> collect();

private:
	std::mutex mutex;
	std::vector<std::weak_ptr<texture>> textures;
};
#endif	//	defined(USE_IMGUI)
