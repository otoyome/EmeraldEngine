#include "Process/editor_process.h"
#include "framework.h"

#if defined(USE_IMGUI)
#include "Renderer/render_system.h"
#include "Editor/editor_host.h"
#include "Editor/editor_items.h"
#include "Editor/detail_profiler.h"
#include "Graphics/debug_primitive_collector.h"
#include "System/editor_config_data.h"

bool editor_process::enter()
{
	framework& self = get_owner();

	//	エディター/デバッグ関係
	{
		editor_config_data::create_instance();
		detail_profiler::create_instance();
		editor_items::initialize();
	}

	self.editor = std::make_unique<editor_host>(self.hwnd);
	self.editor->imgui_initialize();
	editor_host* editor = self.editor.get();
	self.renderer->set_overlay([editor] { editor->render_overlay(); });
	return true;
}

void editor_process::exit()
{
	framework& self = get_owner();

	if (self.renderer)
		self.renderer->set_overlay(nullptr);
	if (self.editor)
	{
		self.editor->imgui_terminate();
		self.editor.reset();
	}

	//	エディター/デバッグ関係
	{
		editor_items::terminate();
		editor_config_data::delete_instance();
		detail_profiler::delete_instance();
	}
}

void editor_process::update(float elapsed_time)
{
	framework& self = get_owner();
	if (IsWindowVisible(self.hwnd))
		self.editor->begin_frame();
}

void editor_process::imgui_render()
{
	if (get_owner().editor->is_frame_begun())
		get_owner().editor->render_windows();
}

void editor_process::release_display_resources()
{
	framework& self = get_owner();
	if (self.editor)
		self.editor->release_display_resources();
}

bool editor_process::create_display_resources()
{
	framework& self = get_owner();
	if (self.editor)
		self.editor->create_display_resources();
	return true;
}
#endif	//	defined(USE_IMGUI)
