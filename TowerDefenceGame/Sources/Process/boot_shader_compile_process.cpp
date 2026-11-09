#include "Process/boot_shader_compile_process.h"
#include "Process/shader_compile_process.h"
#include "framework.h"

#include "Graphics/sprite.h"
#include "Object/world.h"
#include "Renderer/ui_world_renderer.h"
#include "Renderer/render_system.h"

bool boot_shader_compile_process::enter()
{
	framework& self = get_owner();

	//	読み込み画面のスプライトが使うマテリアルのシェーダー
	{
		sprite loading_sprite;
		loading_sprite.get_material().sync_from_resource();
	}

	//	読み込み画面のワールド
	self.boot_world = std::make_unique<world>();

	//	読み込み画面のワールドの描画
	self.boot_renderer = std::make_unique<ui_world_renderer>(self.renderer->get_executor(), render_output::create_back_buffer());
	if (!self.boot_renderer->initialize())
		return false;

	return true;
}

std::unique_ptr<framework_process> boot_shader_compile_process::next_process()
{
	return std::make_unique<shader_compile_process>(get_owner());
}
