#pragma once

enum class renderer_domain
{
	//	描画開始前パス
	begin_pass,

	//	シーン構築パス
	scene_constantract_pass,

	//	深度プリパス
	depth_prepass,

	//	オクルージョンカリングパス
	occlusion_culling_pass,

	//	G-Buffer生成
	g_buffer_pass,
	g_buffer_decal_pass,
	screen_space_ambient_occlusion_pass,
	deffered_lighting_pass,
	screen_space_reflection_pass,

	//	描画パス
	effect_render_pass,

	//	被写界深度
	before_dof_translucency_pass,
	before_dof_post_process_pass,
	depth_of_field_pass,
	after_dof_post_process_pass,
	after_dof_translucency_pass,
	after_translucency_post_process_pass,

	//	モーションブラー
	motion_blur_pass,
	after_motion_blur_forward_pass,

	//	トーンマッピング
	before_bloom_post_process_pass,
	exposure_pass,
	bloom_pass,
	tone_mapping_pass,
	after_tone_mapping_post_process_pass,

	//	最終パス
	anti_alias_post_process_pass,
	display_mapping_pass,

#if defined(USE_IMGUI)
	//	デバッグ描画
	debug_draw,
#endif // defined(USE_IMGUI)

	//	UIレンダリング
	ui_rendering,

#if defined(USE_IMGUI)
	//	ImGui描画
	imgui_draw_pass,
#endif // defined(USE_IMGUI)

	//	プレゼントパス
	present_pass,
};
