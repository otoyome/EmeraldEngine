#include "scenes/editscene/edit_particle_scene.h"

#include "model_instance.h"
#include "camera.h"

#include "object/light_object.h"

#include "object/model_object.h"

#include "custom_pipeline_state.h"

#include "effect_instance.h"

#if defined(USE_IMGUI)
#include "Editor/editor_camera.h"

namespace
{
	//	エフェクト登録
	static constexpr material::set_surface_pipeline_states_desc pso_descs[] =
	{
		{material::surface_paths::depth_write_pass, "compute_patricle_sprite_depth"},
		{material::surface_paths::g_buffer_pass, "compute_patricle_sprite_gbuffer"},
		{material::surface_paths::shadow_write_pass, "compute_patricle_sprite_shadowmap"},
		{material::surface_paths::forward_pass, "compute_patricle_sprite_forward"},
	};

	//	テストコード
	static constexpr std::string_view file_pathes[] =
	{
		"Assets/Resources/Effects/BasicBomb.eme",
		"Assets/Resources/Effects/BasicTrail.eme",
		"Assets/Resources/Effects/BlowHitEffect.eme",
	};

	//	エフェクト
	constexpr TCHAR* EffectFileDialogFilter =
		L"エフェクトファイル (*.eme)\0*.eme;\0\
		すべてのファイル (*.*)\0*.*\0";
}

edit_particle_scene::edit_particle_scene()
: scene_base()
{
	//	テスト用にスプライトパーティクルエミッター登録
	{
		static constexpr DirectX::XMUINT2 split_count = { 5, 5 };
		static constexpr float increment_time = system_units::convert_fps_to_sec(1.0f);

		effect_resource_ptr resource;
		if (!effect_resource_manager::instance()->load(resource, file_pathes[0]))
		{
			resource = std::make_shared<effect_resource>();

			auto& particle_datas = resource->get_particles_for_editor();
			auto& particle_data = particle_datas.emplace_back();
			//	レンダラー設定
			{
				auto renderer = particle_data.register_item<particle::item::renderer>();

				renderer->type = particle::item::renderer::types::sprite;
				renderer->total_particle_count = 0x8FFF;

				//	スプライトアニメーション
				{
					//renderer->uv.set_state(particle::item::renderer::texcoords::states::direct, true);
					renderer->uv.set_state(particle::item::renderer::texcoords::states::animation, true);
					renderer->uv.rect = {0, 0, 1, 1};
					renderer->uv.index = 0;
					renderer->uv.splits = split_count;
					renderer->uv.time = increment_time;
				}

				//	マテリアル設定
				{
					renderer->material = std::make_shared<material>("default");
					auto material = renderer->material;
					material->set_domain(material::domains::particle);
					material->set_alpha_clip_threshold(0.5f);
					material->set_shading_model(material::shading_models::unlit);
					material->set_blend_mode(material::blend_modes::mask);

					//	フラグ類設定
					material->set_flag(material::flags::two_sided, true);
				//	material->set_flag(material::flags::shadow_caster, true);
				//	material->set_flag(material::flags::shadow_receiver, true);
					material->set_flag(material::flags::indirect_lighting_off, true);

					//	デフォルトシェーダー設定
					material->set_pipeline_state(pso_descs, ARRAYSIZE(pso_descs));

					//	パラメーター設定
					material->set_float4("base_color_factor", DirectX::XMFLOAT4(1, 1, 1, 1));
					{
						texture_ptr texture;
						std::filesystem::path file_path = "Assets/Resources/Textures/Explosion01_5x5.png";
						if (texture_manager::instance()->load_texture_from_file(texture, file_path.string()))
						{
							texture->set_flag(texture::flags::srgb, true);
							material->set_texture("BaseColorMap", texture);
							texture_manager::instance()->save_texture(texture);
						}
					}
				}
			}
			//	発生設定
			{
				auto spawn = particle_data.register_item<particle::item::spawn>();
				spawn->set_state(particle::item::spawn::states::burst, true);
				spawn->set_state(particle::item::spawn::states::random, true);

				spawn->life_time_range =
				{
					split_count.x * split_count.y * increment_time,
					split_count.x * split_count.y * increment_time
				};
				spawn->scale_range =
				{
					3.0f,
					15.0f
				};
				spawn->area_min =
				{
					system_units::convert_m_to_unit(-4.0f),
					system_units::convert_m_to_unit(+0.0f),
					system_units::convert_m_to_unit(-4.0f)
				};
				spawn->area_max =
				{
					system_units::convert_m_to_unit(+4.0f),
					system_units::convert_m_to_unit(+0.0f),
					system_units::convert_m_to_unit(+4.0f)
				};
				spawn->range =
				{
					10,
					50,
				};
			}

			//	ファイル保存
			resource->serialize(file_pathes[0]);
		}
	}

	//	テスト用にリボンパーティクルエミッター登録
	{
		static constexpr UINT TrailBodySize = 60;
		effect_resource_ptr resource;
		if (!effect_resource_manager::instance()->load(resource, file_pathes[1]))
		{
			resource = std::make_shared<effect_resource>();

			auto& particle_datas = resource->get_particles_for_editor();

			//	ラインパーティクル
			auto& particle_data = particle_datas.emplace_back();

			//	レンダラー設定
			{
				auto renderer = particle_data.register_item<particle::item::renderer>();

				renderer->type = particle::item::renderer::types::ribon;
				renderer->total_particle_count = 0x1FFF;
				renderer->trail.max = TrailBodySize;

				//	UV設定
				{
					static constexpr DirectX::XMUINT2 split_count = { 5, 5 };
					static constexpr float increment_time = system_units::convert_fps_to_sec(1.0f);

					renderer->uv.set_state(particle::item::renderer::texcoords::states::animation, true);
					renderer->uv.rect = {0.001f, 0.001f, 0.999f, 0.999f};
					renderer->uv.index = 0;
					renderer->uv.splits = split_count;
					renderer->uv.time = increment_time;
				}

				//	マテリアル設定
				{
					renderer->material = std::make_shared<material>("default");
					auto material = renderer->material;
					material->set_domain(material::domains::particle);
					material->set_alpha_clip_threshold(0.01f);
					material->set_shading_model(material::shading_models::unlit);
					material->set_blend_mode(material::blend_modes::translucent);

					//	フラグ類設定
					material->set_flag(material::flags::two_sided, true);
				//	material->set_flag(material::flags::shadow_caster, true);
				//	material->set_flag(material::flags::shadow_receiver, true);
					material->set_flag(material::flags::indirect_lighting_off, true);

					//	デフォルトシェーダー設定
					material->set_pipeline_state(pso_descs, ARRAYSIZE(pso_descs));

					//	パラメーター設定
					material->set_float4("base_color_factor", DirectX::XMFLOAT4(1, 1, 1, 1));
					{
						texture_ptr texture;
						//std::filesystem::path file_path = "Assets/Resources/Textures/trail.png";
						std::filesystem::path file_path = "Assets/Resources/Textures/Explosion01_5x5.png";
						if (texture_manager::instance()->load_texture_from_file(texture, file_path.string()))
						{
							texture->set_flag(texture::flags::srgb, true);
							material->set_texture("BaseColorMap", texture);
							texture_manager::instance()->save_texture(texture);
						}
					}
				}
			}
			//	発生設定
			{
				auto spawn = particle_data.register_item<particle::item::spawn>();
				spawn->set_state(particle::item::spawn::states::random, true);

				spawn->life_time_range =
				{
					10.0f,
					20.0f
				};
				spawn->scale_range =
				{
					1.5f,
					2.5f
				};
				spawn->area_min =
				{
					system_units::convert_m_to_unit(-4.0f),
					system_units::convert_m_to_unit(+0.0f),
					system_units::convert_m_to_unit(-4.0f)
				};
				spawn->area_max =
				{
					system_units::convert_m_to_unit(+4.0f),
					system_units::convert_m_to_unit(+4.0f),
					system_units::convert_m_to_unit(+4.0f)
				};
				spawn->range =
				{
					1,
					5,
				};
			}

			//	速度設定
			{
				auto velocity = particle_data.register_item<particle::item::velocity>();
				velocity->set_state(particle::item::velocity::states::random_direction, true);
				velocity->set_state(particle::item::velocity::states::random_power, true);
				velocity->set_state(particle::item::velocity::states::random_acceleration, true);
				velocity->set_state(particle::item::velocity::states::random_deceleration, true);
				velocity->direction = { 0.0f, 1.0f, 0.0f };
				velocity->angle_range = { -DirectX::XM_PI / 4, +DirectX::XM_PI / 4 };
				velocity->power_range = { 100.0f, 200.0f };

				{
					velocity->acceleration_min.x = -10.0f;
					velocity->acceleration_min.y = -10.0f;
					velocity->acceleration_min.z = -10.0f;
					velocity->acceleration_max.x = +10.0f;
					velocity->acceleration_max.y = +10.0f;
					velocity->acceleration_max.z = +10.0f;
				}

				{
					velocity->deceleration_min.x = -10.0f;
					velocity->deceleration_min.y = -10.0f;
					velocity->deceleration_min.z = -10.0f;
					velocity->deceleration_max.x = +10.0f;
					velocity->deceleration_max.y = +10.0f;
					velocity->deceleration_max.z = +10.0f;
				}
				velocity->gravity_scale = { 1.0f, 1.0f, 1.0f };
			}

			//	ファイル保存
			resource->serialize(file_pathes[1]);
		}
	}

	//	打撃エフェクト
	{
		effect_resource_ptr resource;
		if (!effect_resource_manager::instance()->load(resource, file_pathes[2]))
		{
			resource = std::make_shared<effect_resource>();
			
			static constexpr DirectX::XMUINT2 split_count = { 5, 5 };
			static constexpr float increment_time = system_units::convert_fps_to_sec(1.0f);

			{
				auto& models = resource->get_models_for_editor();
				auto& model = models.emplace_back();
				model.file_path = "Assets/Resources/Models/Effect/Blow/Blow.emo";
			}
			{
				auto& particles = resource->get_particles_for_editor();
				//auto& particle = particles.emplace_back();
			}

			//	ファイル保存
			resource->serialize(file_pathes[2]);
		}
	}
}

edit_particle_scene::~edit_particle_scene()
{
}

bool edit_particle_scene::initialize_core()
{
	//	メインカメラ生成
	{
		main_camera = std::make_shared<camera_object>("メインカメラ");
		main_camera->get_camera()->set_fov_y(DirectX::XMConvertToRadians(15.0f));
	//	main_camera->get_camera()->set_flag(camera::flags::hybrid_projection, true);
	//	main_camera->get_camera()->set_hybrid_rate(0.3f);	//	透視投影
		register_object(main_camera);
		mouse::instance()->set_center_lock(false);
		mouse::instance()->set_cursol_hide(false);
	}

	//	ライト生成
	{
		//	平行光源生成
		{
			auto light = std::make_shared<directional_light_object>("平行光源");
			auto color = light->get_color();
			color.w = 1.0f;
			light->set_color(color);
			light->set_flag(light::flags::use_shadow, false);
			register_object(light);
		}

		//	間接光生成
		{
			auto sky_light = std::make_shared<sky_light_object>("スカイライト");
			auto color = sky_light->get_color();
			color.w = 1.0f;
			sky_light->set_color(color);

			texture_ptr ibl_texture;
			texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/hilly_terrain_01_puresky_4k/diffuse_iem.dds");
			sky_light->set_diffuse_ibl_texture(ibl_texture);
			texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/hilly_terrain_01_puresky_4k/specular_pmrem.dds");
			sky_light->set_specular_ibl_texture(ibl_texture);
			texture_manager::instance()->load_texture_from_file(ibl_texture, "Assets/Resources/Textures/lut_ggx.dds");
			sky_light->set_lut_ggx_texture(ibl_texture);

			register_object(sky_light);
		}
	}

	//	エディターカメラ生成
	{
		auto camera = std::make_shared<editor_camera>(main_camera);
		camera->set_position(system_units::convert_m_to_unit({ 0, 1, 8 }));
		camera->set_rotation({ 0, DirectX::XM_PI, 0 });
		camera->set_rotation_speed(camera->get_rotation_speed() * 0.25f);
		camera->set_move_speed(camera->get_move_speed() * 0.25f);
		register_object(camera);
	}

	//	エディターアイテム用コンテキスト生成
	{
		editor_context = editor_items::generate_context();
	}

	//	新規パーティクル生成
	{
		on_new();
	}
	return true;
}

bool edit_particle_scene::uninitialize_core()
{
	return true;
}

void edit_particle_scene::update_core(float elapsed_time)
{
}

void edit_particle_scene::imgui_render_core()
{
	//	パーティクル読込み関係
	{
		ImGui::Text("Effect ");
		ImGui::SameLine();
		if (ImGui::Button("Load"))
		{
			std::string file_path;
			if (editor_items::open_file_dialog(EffectFileDialogFilter, file_path))
			{
				effect_resource_ptr resource;
				if (effect_resource_manager::instance()->load(resource, file_path))
				{
					if (edit_instance)
						effect_manager::instance()->unregister_instance(edit_instance);
					edit_resource = resource;
					edit_instance = std::make_shared<effect_instance>(edit_resource);
					effect_manager::instance()->register_instance(edit_instance);
				}
			}
		}

		//	モデル保存
		if (edit_instance)
		{
			ImGui::SameLine();
			if (ImGui::Button("Save"))
			{
				std::string file_path;
				if (editor_items::save_file_dialog(EffectFileDialogFilter, file_path, L".eme"))
				{
					edit_resource->serialize(file_path);
				}
			}
		}

		//	モデル保存
		if (edit_instance)
		{
			ImGui::SameLine();
			if (ImGui::Button("New"))
			{
				on_new();
			}
		}
	}

	//	
	if (edit_resource)
	{
		if (ImGui::Begin("Edit Effect"))
		{
			if (editor_items::edit_effect(editor_context, edit_resource))
			{
				if (edit_instance)
					effect_manager::instance()->unregister_instance(edit_instance);
				edit_instance = std::make_shared<effect_instance>(edit_resource);
				effect_manager::instance()->register_instance(edit_instance);
			}
		}
		ImGui::End();
	}
}
void edit_particle_scene::on_new()
{
	if (edit_instance)
		effect_manager::instance()->unregister_instance(edit_instance);

	edit_resource = std::make_shared<effect_resource>();
	edit_instance = std::make_shared<effect_instance>(edit_resource);
	effect_manager::instance()->register_instance(edit_instance);
}
#endif	//	defined(USE_IMGUI)
