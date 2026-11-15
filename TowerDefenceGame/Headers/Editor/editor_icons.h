#pragma once

#if defined(USE_IMGUI)
#include <memory>
#include <vector>

#include "imgui.h"
#include "Texture/texture_resource.h"

class engine_context;
class game_object;

//	エディター用アイコン
class editor_icons
{
public:
	//	アイコン描画
	void draw(engine_context& context, const ImVec2& image_position, const ImVec2& image_size);

	//	アイコン選択
	std::shared_ptr<game_object> pick(engine_context& context, const ImVec2& position, const ImVec2& image_size);

	//	選んでいるオブジェクトの形を線で出す
	void queue_selected_shape(engine_context& context);

private:
	//	配置アイコン
	struct placed_icon
	{
		std::shared_ptr<game_object> object;
		ImVec2 center;			//	位置
		float depth = 0.0f;		//	カメラからの奥行き
		size_t icon_index = 0;	//	アイコン番号
	};

	//	表示されるアイコンの一覧
	std::vector<placed_icon> collect(engine_context& context, const ImVec2& image_size);

	//	アイコン画像読込
	void load_textures();

private:
	//	オブジェクトに出すアイコンの番号
	static size_t find_icon(const game_object& object);

private:
	std::vector<texture_resource_ptr> textures;
	bool textures_loaded = false;
};
#endif	//	defined(USE_IMGUI)
