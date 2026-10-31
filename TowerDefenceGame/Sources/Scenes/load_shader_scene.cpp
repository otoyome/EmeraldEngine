#include "Scenes/load_shader_scene.h"

#include "custom_pipeline_state.h"
#include "material_shader_map.h"

#include "scenes/minigamescene/minigame_scene.h"
#include "scenes/minigamescene/minigame_title_scene.h"
#include "Scenes/setup_scene.h"

#include "scenes/editscene/edit_model_scene.h"
#include "scenes/editscene/edit_particle_scene.h"

load_shader_scene::load_shader_scene()
: scene_base()
{
}

load_shader_scene::~load_shader_scene()
{
}

bool load_shader_scene::initialize_core()
{
	pipeline_state_manager* instance = pipeline_state_manager::instance();

	//	@remainder	失敗時のデカールとUIパイプラインステートの実装
#if	defined(USE_IMGUI)

	//	3Dモデルエラー用デフォルトパイプラインステート
	{
		material_shader_map::material_shader_desc desc;
		desc.name = "3d_failed";
		desc.file_path = "Shaders/Materials/3d_failed.hlsl";
		desc.domain = pipeline_state_base::domains::surface;
		desc.hidden = true;
		material_shader_map::instance()->generate(desc);
	}

#pragma	region	ポストプロセスエラー用デフォルトパイプラインステート
	{
		std::shared_ptr<graphics_custom_pipeline_state> pso;
		instance->generate_pipeline_state<graphics_custom_pipeline_state>("postprocess_failed", &pso);
		pso->set_domain(pipeline_state_base::domains::post_process);
		pso->set_flag(pipeline_state_base::flags::hidden, true);

		//	頂点シェーダー読み込み
		{
			load_shader_file desc;
			desc.file_path = "Shaders/FullscreenQuad/fullscreen_quad_vs.hlsl";

			std::shared_ptr<vertex_shader> shader;
			shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
			pso->set_shader(shader);
		}

		//	ピクセルシェーダー読み込み
		{
			load_shader_file desc;
			desc.file_path = "Shaders/Editor/postprocess_failed_ps.hlsl";

			std::shared_ptr<pixel_shader> shader;
			shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
			pso->set_shader(shader);
		}
	}
#pragma endregion	//	ポストプロセスエラー用デフォルトパイプラインステート

#pragma	region	コンピュートスプライト/トレイルパーティクル用デフォルトパイプラインステート
	{
		//	深度プリパス
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("particle_failed_depth", &pso);
			pso->set_domain(pipeline_state_base::domains::particle);
			pso->set_flag(pipeline_state_base::flags::hidden, true);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_depth_write_vs.hlsl";

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_depth_write_ps.hlsl";

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	シャドウマップ生成
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("particle_failed_shadowmap", &pso);
			pso->set_domain(pipeline_state_base::domains::particle);
			pso->set_flag(pipeline_state_base::flags::hidden, true);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_csm_vs.hlsl";

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ジオメトリシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_csm_gs.hlsl";

				std::shared_ptr<geometry_shader> shader;
				shader_manager::instance()->load_shader<geometry_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_depth_write_ps.hlsl";

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	G-Buffer生成
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("particle_failed_gbuffer", &pso);
			pso->set_domain(pipeline_state_base::domains::particle);
			pso->set_flag(pipeline_state_base::flags::hidden, true);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_vs.hlsl";

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_g_buffer_ps.hlsl";

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	フォワードレンダリング
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("particle_failed_forward", &pso);
			pso->set_domain(pipeline_state_base::domains::particle);
			pso->set_flag(pipeline_state_base::flags::hidden, true);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_vs.hlsl";

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_ps.hlsl";

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
	}
#pragma	endregion	//	コンピュートスプライト/トレイルパーティクル用デフォルトパイプラインステート

	//	頂点カラー編集用デフォルトパイプラインステート
	{
		material_shader_map::material_shader_desc desc;
		desc.name = "edit_model_color";
		desc.file_path = "Shaders/Materials/edit_model_color.hlsl";
		desc.domain = pipeline_state_base::domains::surface;
		desc.geometry_passes = material_shader_map::to_bit(material_shader_map::surface_passes::depth_write)
							 | material_shader_map::to_bit(material_shader_map::surface_passes::g_buffer);
		desc.hidden = true;
		material_shader_map::instance()->generate(desc);
	}

#pragma	region	GBufferDecalエラー用デフォルトパイプラインステート
	{
		//	プリミティブ
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("deferred_decal_failed_primitive", &pso);
			pso->set_domain(pipeline_state_base::domains::deferred_decal);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/DeferredDecal/gbuffer_decal_primitive_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/DeferredDecal/Failed/failed_decal_primitive_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	フルスクリーン
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("deferred_decal_failed_fullscreen", &pso);
			pso->set_domain(pipeline_state_base::domains::particle);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/DeferredDecal/gbuffer_decal_fullscreen_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/DeferredDecal/Failed/failed_decal_fullscreen_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
	}
#pragma	endregion	//	GBufferDecal用デフォルトパイプラインステート
#endif	//	defined(USE_IMGUI)

#pragma	region	UI用デフォルトパイプラインステート
	{
		std::shared_ptr<graphics_custom_pipeline_state> pso;
		instance->generate_pipeline_state<graphics_custom_pipeline_state>("default_ui", &pso);
		pso->set_domain(pipeline_state_base::domains::post_process);

		//	頂点シェーダー読み込み
		{
			load_shader_file desc;
			desc.file_path = "Shaders/Sprite/sprite_vs.hlsl";

			std::shared_ptr<vertex_shader> shader;
			shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
			pso->set_shader(shader);
		}

		//	ピクセルシェーダー読み込み
		{
			load_shader_file desc;
			desc.file_path = "Shaders/Sprite/sprite_ps.hlsl";

			std::shared_ptr<pixel_shader> shader;
			shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
			pso->set_shader(shader);
		}
	}
#pragma endregion	//	UI用デフォルトパイプラインステート

	//	GLTF用デフォルトパイプラインステート
	{
		material_shader_map::material_shader_desc desc;
		desc.name = "gltf";
		desc.file_path = "Shaders/Materials/gltf.hlsl";
		desc.domain = pipeline_state_base::domains::surface;
		material_shader_map::instance()->generate(desc);
	}

#pragma	region	コンピュートスプライト/トレイルパーティクル用デフォルトパイプラインステート
	{
		//	深度プリパス
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("compute_patricle_sprite_depth", &pso);
			pso->set_domain(pipeline_state_base::domains::particle);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_depth_write_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_depth_write_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	シャドウマップ生成
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("compute_patricle_sprite_shadowmap", &pso);
			pso->set_domain(pipeline_state_base::domains::particle);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_csm_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ジオメトリシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_csm_gs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<geometry_shader> shader;
				shader_manager::instance()->load_shader<geometry_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_depth_write_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	G-Buffer生成
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("compute_patricle_sprite_gbuffer", &pso);
			pso->set_domain(pipeline_state_base::domains::particle);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_g_buffer_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	フォワードレンダリング
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("compute_patricle_sprite_forward", &pso);
			pso->set_domain(pipeline_state_base::domains::particle);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/ComputeParticle/Renderer/Default/compute_particle_default_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
	}
#pragma	endregion	//	コンピュートスプライト/トレイルパーティクル用デフォルトパイプラインステート

	//	PMX用デフォルトパイプラインステート
	{
		material_shader_map::material_shader_desc desc;
		desc.name = "pmx";
		desc.file_path = "Shaders/Materials/pmx.hlsl";
		desc.domain = pipeline_state_base::domains::surface;
		desc.geometry_passes = material_shader_map::to_bit(material_shader_map::surface_passes::depth_write)
							 | material_shader_map::to_bit(material_shader_map::surface_passes::g_buffer);
		material_shader_map::instance()->generate(desc);
	}

	//	GGSTシェーディング用デフォルトパイプラインステート
	{
		material_shader_map::material_shader_desc desc;
		desc.name = "ggst";
		desc.file_path = "Shaders/Materials/ggst.hlsl";
		desc.domain = pipeline_state_base::domains::surface;
		desc.geometry_passes = material_shader_map::to_bit(material_shader_map::surface_passes::depth_write)
							 | material_shader_map::to_bit(material_shader_map::surface_passes::g_buffer);
		material_shader_map::instance()->generate(desc);
	}

	//	トゥーンシェーディング用パイプラインステート
	{
		material_shader_map::material_shader_desc desc;
		desc.name = "toon";
		desc.file_path = "Shaders/Materials/toon.hlsl";
		desc.domain = pipeline_state_base::domains::surface;
		desc.geometry_passes = material_shader_map::to_bit(material_shader_map::surface_passes::depth_write)
							 | material_shader_map::to_bit(material_shader_map::surface_passes::g_buffer);
		material_shader_map::instance()->generate(desc);
	}

#pragma	region	GBufferDecal用デフォルトパイプラインステート
	{
		//	プリミティブ
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("deferred_decal_default_primitive", &pso);
			pso->set_domain(pipeline_state_base::domains::deferred_decal);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/DeferredDecal/gbuffer_decal_primitive_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ピクセルシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/DeferredDecal/Default/default_decal_primitive_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
		//	フルスクリーン
		{
			std::shared_ptr<graphics_custom_pipeline_state> pso;
			instance->generate_pipeline_state<graphics_custom_pipeline_state>("deferred_decal_default_fullscreen", &pso);
			pso->set_domain(pipeline_state_base::domains::particle);
			//	頂点シェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/DeferredDecal/gbuffer_decal_fullscreen_vs.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<vertex_shader> shader;
				shader_manager::instance()->load_shader<vertex_shader>(shader, desc);
				pso->set_shader(shader);
			}
			//	ジオメトリシェーダー読み込み
			{
				load_shader_file desc;
				desc.file_path = "Shaders/DeferredDecal/Default/default_decal_fullscreen_ps.hlsl";
#if defined(_DEBUG)
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::disable_optimize, true);
				system_utility::set_bitflag(desc.compile_flags, shader_compile_flags::debug, true);
#endif	//	defined(_DEBUG)

				std::shared_ptr<pixel_shader> shader;
				shader_manager::instance()->load_shader<pixel_shader>(shader, desc);
				pso->set_shader(shader);
			}
		}
	}
#pragma	endregion	//	GBufferDecal用デフォルトパイプラインステート

	return true;
}

bool load_shader_scene::uninitialize_core()
{
	return true;
}

void load_shader_scene::update_core(float elapsed_time)
{
	scene_manager::instance()->remove_scene<load_shader_scene>();
	scene_manager::instance()->add_scene<setup_scene>();
}

#if defined(USE_IMGUI)
void load_shader_scene::imgui_render_core()
{
	if (ImGui::TreeNode("load_shader_scene"))
	{
		ImGui::TreePop();
	}
}
#endif	//	defined(USE_IMGUI)
