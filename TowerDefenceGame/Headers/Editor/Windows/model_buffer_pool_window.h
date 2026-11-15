#pragma once

#if defined(USE_IMGUI)
#include "Editor/editor_items.h"

//	モデル頂点/インデックスバッファプールの使用量
class model_buffer_pool_window : public editor_items::ibase_editor
{
public:
	std::string get_label() const override { return "model_buffer_pool"; }
	void render_window(std::shared_ptr<editor_items::context>& context, bool& open) override;
};
#endif	//	defined(USE_IMGUI)
