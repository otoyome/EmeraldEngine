#pragma once

#if defined(USE_IMGUI)
#include "Editor/editor_items.h"

//	ログの表示
class log_window : public editor_items::ibase_editor
{
public:
	std::string get_label() const override { return "log"; }
	void render_window(std::shared_ptr<editor_items::context>& context, bool& open) override;

private:
	//	ログ系
	logger::categorys display_log_category = logger::categorys::max;
	UINT display_log_flags = (1 << static_cast<UINT>(logger::levels::error)) | (1 << static_cast<UINT>(logger::levels::warning));
	bool is_log_descending_order = false;
};

//	エンジンの設定
class engine_config_window : public editor_items::ibase_editor
{
public:
	std::string get_label() const override { return "engine_config"; }
	void render_window(std::shared_ptr<editor_items::context>& context, bool& open) override;

private:
	//	@tool 解像度と表示の方法の選択欄
	void render_display();

private:
	int display_resolution_index = -1;	//	@tool 選んでいる解像度（-1 は今の解像度）
	int display_window_mode = -1;		//	@tool 選んでいる表示の方法（-1 は今の方法）
};

//	モデル頂点/インデックスバッファプールの使用量
class model_buffer_pool_window : public editor_items::ibase_editor
{
public:
	std::string get_label() const override { return "model_buffer_pool"; }
	void render_window(std::shared_ptr<editor_items::context>& context, bool& open) override;
};

//	メモリアロケータの使用量
class memory_allocator_window : public editor_items::ibase_editor
{
public:
	std::string get_label() const override { return "memory_allocator"; }
	void render_window(std::shared_ptr<editor_items::context>& context, bool& open) override;
};
#endif	//	defined(USE_IMGUI)
