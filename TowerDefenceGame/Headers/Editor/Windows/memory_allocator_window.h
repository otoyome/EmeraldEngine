#pragma once

#if defined(USE_IMGUI)
#include "Editor/editor_items.h"

//	メモリアロケータの使用量
class memory_allocator_window : public editor_items::ibase_editor
{
public:
	std::string get_label() const override { return "memory_allocator"; }
	void render_window(std::shared_ptr<editor_items::context>& context, bool& open) override;
};
#endif	//	defined(USE_IMGUI)
