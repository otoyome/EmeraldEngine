#pragma once

#if defined(USE_IMGUI)
#include "Editor/editor_items.h"

//	ÉçÉOÇÃï\é¶
class log_window : public editor_items::ibase_editor
{
public:
	std::string get_label() const override { return "log"; }
	void render_window(std::shared_ptr<editor_items::context>& context, bool& open) override;

private:
	//	ÉçÉOån
	logger::categorys display_log_category = logger::categorys::max;
	UINT display_log_flags = (1 << static_cast<UINT>(logger::levels::error)) | (1 << static_cast<UINT>(logger::levels::warning));
	bool is_log_descending_order = false;
};
#endif	//	defined(USE_IMGUI)
