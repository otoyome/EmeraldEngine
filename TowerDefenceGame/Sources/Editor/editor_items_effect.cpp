#include "Editor/editor_items.h"

#if defined(USE_IMGUI)
#include <commdlg.h>
#include <random>
#include <set>

namespace
{
	static constexpr material::set_surface_pipeline_states_desc pso_descs[] =
	{
		{material::surface_paths::depth_write_pass, "compute_patricle_sprite_depth"},
		{material::surface_paths::g_buffer_pass, "compute_patricle_sprite_gbuffer"},
		{material::surface_paths::shadow_write_pass, "compute_patricle_sprite_shadowmap"},
		{material::surface_paths::forward_pass, "compute_patricle_sprite_forward"},
	};
}

bool editor_items::edit_effect(std::shared_ptr<context>& context, effect_resource_ptr& effect_resource)
{
	bool edit_flag = false;
	ImGui::PushID(context->get_item_id());

	//	モデル情報
	if (ImGui::TreeNode("model"))
	{
		auto& models = effect_resource->get_models_for_editor();
		UINT model_size = static_cast<UINT>(models.size());
		if (ImGui::DragUInt("model size", &model_size, 0.1f))
		{
			models.resize(model_size);
		}
		for (auto& model : models)
		{
			ImGui::Separator();
			ImGui::PushID(context->get_item_id());
			ImGui::InputTextUtf8("name", model.name);

			if (ImGui::DragFloat3("scale", &model.scale.x, 0.01f, 0.0f, 100.0f))
				edit_flag = true;
			math::rotator rot = model.rotation;
			if (ImGui::DragFloat3("rotation", &rot.x, 0.01f, 0.0f, 100.0f))
			{
				model.rotation = rot.convert_quaternion();
				edit_flag = true;
			}
			if (ImGui::DragFloat3("position", &model.position.x, 0.01f))
				edit_flag = true;

			//	モデル関係
			{
				model_resource_ptr resource = model.resource;
				std::string file_path = model.file_path;
				if (edit_model_resource_item(context, resource, &file_path))
				{
					model.resource = resource;
					model.file_path = file_path;
					edit_flag = true;
				}
			}

			ImGui::Separator();
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	//	デカール情報
	edit_effect_decal(context, effect_resource);

	//	パーティクル情報
	edit_effect_particle(context, effect_resource);

	ImGui::PopID();
	return edit_flag;
}

bool editor_items::edit_effect_particle(std::shared_ptr<context>& context, effect_resource_ptr& effect_resource)
{
	auto generate_material = [&]()
	{
		auto material_ptr = std::make_shared<material>("default");
		auto material = material_ptr;
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
			std::filesystem::path file_path = "Assets/Resources/Textures/white_16x16.png";
			if (texture_manager::instance()->load_texture_from_file(texture, file_path.string()))
			{
				texture->set_flag(texture::flags::srgb, true);
				material->set_texture("BaseColorMap", texture);
				texture_manager::instance()->save_texture(texture);
			}
		}
		return material_ptr;
	};

	bool edit_flag = false;
	if (ImGui::TreeNode("particle"))
	{
		auto& particles = effect_resource->get_particles_for_editor();
		UINT particle_size = static_cast<UINT>(particles.size());
		if (ImGui::DragUInt("particle size", &particle_size, 0.1f))
		{
			particles.resize(particle_size);
			edit_flag = true;
		}
		for (auto& particle : particles)
		{
			ImGui::Separator();
			ImGui::PushID(context->get_item_id());
			ImGui::InputTextUtf8("name", particle.name);

			{
				auto renderer = particle.get_item<particle::item::renderer>();
				if (ImGui::TreeNode("renderer"))
				{
					if (ImGui::Button(!renderer ? "Add Renderer" : "Remove Renderer"))
					{
						if (renderer)
						{
							particle.unregister_item<particle::item::renderer>();
							renderer = nullptr;
						}
						else
						{
							renderer = particle.register_item<particle::item::renderer>();
						}
						edit_flag = true;
					}
					if (renderer)
					{
						if (ImGui::DragUInt("total particle count", &renderer->total_particle_count))
							edit_flag = true;

						//	描画タイプ指定
						{
							std::vector<std::string> row_texts;
							std::vector<char*> combo_texts;
							for (auto& name : magic_enum::enum_names<particle::item::renderer::types>())
							{
								row_texts.emplace_back(name);
							}
							for (auto& name : row_texts)
							{
								combo_texts.emplace_back(name.data());
							}
							int type = static_cast<int>(renderer->type);
							if (ImGui::Combo("type", &type, combo_texts.data(), static_cast<int>(combo_texts.size())))
							{
								renderer->type = static_cast<particle::item::renderer::types>(type);
								edit_flag = true;
							}
						}

						switch (renderer->type)
						{
							case particle::item::renderer::types::sprite:
							{
								//	マテリアル
								if (!renderer->material)
								{
									renderer->material = generate_material();
								}
								if (ImGui::TreeNode("material"))
								{
									if (editor_items::edit_material(context, *renderer->material))
									{
										edit_flag = true;
									}
									ImGui::TreePop();
								}
								break;
							}
							case particle::item::renderer::types::ribon:
							{
								//	マテリアル
								if (!renderer->material)
								{
									renderer->material = generate_material();
								}
								if (ImGui::TreeNode("material"))
								{
									if (editor_items::edit_material(context, *renderer->material))
									{
										edit_flag = true;
									}
									ImGui::TreePop();
								}

								//	軌跡
								if (ImGui::DragUInt("max", &renderer->trail.max, 0.1f))
									edit_flag = true;
								if (ImGui::EditFlags<particle::item::renderer::trails::states>(renderer->trail.state))
									edit_flag = true;
								if (renderer->trail.is_state(particle::item::renderer::trails::states::use_time_fade))
								{
									if (ImGui::DragFloat2("time_fade_rate", &renderer->trail.time_fade_rate.x, 0.001f))
										edit_flag = true;
								}
								break;
							}
							default:
							{
								renderer->material = nullptr;
								break;
							}
						}

						//	UV
						if (ImGui::TreeNode("uv"))
						{
							//	ベース矩形
							if (ImGui::SliderFloat4("uv rect", &renderer->uv.rect.x, 0.0f, 1.0f))
							{
								edit_flag = true;
							}

							//	UVの処理の仕方
							bool checked = renderer->uv.is_state(particle::item::renderer::texcoords::states::direct);
							if (ImGui::Button(checked ? "uv split" : "direct"))
							{
								checked = !checked;
								renderer->uv.set_state(particle::item::renderer::texcoords::states::direct, checked);

								//	方向を指定する
								if (checked)
								{
									renderer->uv.splits.x = particle::item::renderer::texcoords::animation_direction_basis_value.x + 1;
									renderer->uv.splits.y = particle::item::renderer::texcoords::animation_direction_basis_value.y;
								}
								edit_flag = true;
							}
							if (checked)
							{
								checked = renderer->uv.is_state(particle::item::renderer::texcoords::states::animation);
								if (ImGui::Checkbox("animation", &checked))
								{
									renderer->uv.set_state(particle::item::renderer::texcoords::states::animation, checked);
									edit_flag = true;
								}
								if (checked)
								{
									DirectX::XMINT2 direction = DirectX::XMINT2(renderer->uv.splits.x, renderer->uv.splits.y);
									direction.x -= 5;
									direction.y -= 5;
									if (ImGui::DragInt2("direction", &direction.x, 0.1f, -1, 1))
									{
										renderer->uv.splits.x = direction.x + particle::item::renderer::texcoords::animation_direction_basis_value.x;
										renderer->uv.splits.y = direction.y + particle::item::renderer::texcoords::animation_direction_basis_value.y;
										edit_flag = true;
									}

									if (ImGui::SliderFloat("speed", &renderer->uv.time, 0.0f, 10.0f))
									{
										edit_flag = true;
									}
								}
							}
							else
							{
								if (ImGui::DragUInt2("splits", &renderer->uv.splits.x, 0.1f))
								{
									edit_flag = true;
								}
								if (ImGui::DragUInt("index", &renderer->uv.index, renderer->uv.splits.x * renderer->uv.splits.y))
								{
									edit_flag = true;
								}
								checked = renderer->uv.is_state(particle::item::renderer::texcoords::states::animation);
								if (ImGui::Checkbox("animation", &checked))
								{
									renderer->uv.set_state(particle::item::renderer::texcoords::states::animation, checked);
									edit_flag = true;
								}
								if (checked)
								{
									if (ImGui::SliderFloat("increment", &renderer->uv.time, 0.0f, 10.0f))
									{
										edit_flag = true;
									}
								}
							}

							ImGui::TreePop();
						}

						//	色
						if (ImGui::TreeNode("color"))
						{
							bool checked = renderer->color.is_state(particle::item::renderer::colors::states::random);
							if (ImGui::Button(checked ? "direct" : "random"))
							{
								checked = !checked;
								renderer->color.set_state(particle::item::renderer::colors::states::random, checked);
								edit_flag = true;
							}

							if (checked)
							{
								if (ImGui::ColorEdit4("color_min", &renderer->color.color_min.x))
								{
									edit_flag = true;
								}
								if (ImGui::ColorEdit4("color_max", &renderer->color.color_max.x))
								{
									edit_flag = true;
								}
							}
							else
							{
								if (ImGui::ColorEdit4("color", &renderer->color.color_min.x))
								{
									edit_flag = true;
								}
							}

							ImGui::TreePop();
						}
					}

					ImGui::TreePop();
				}
			}

			{
				auto spawn = particle.get_item<particle::item::spawn>();
				if (ImGui::TreeNode("spawn"))
				{
					if (ImGui::Button(!spawn ? "Add spawn" : "Remove spawn"))
					{
						if (spawn)
						{
							particle.unregister_item<particle::item::spawn>();
							spawn = nullptr;
						}
						else
						{
							spawn = particle.register_item<particle::item::spawn>();
						}
					}
					if (spawn)
					{
						if (ImGui::EditFlags<particle::item::spawn::states>(spawn->state))
							edit_flag = true;

						if (ImGui::DragFloat2("life_time_range", &spawn->life_time_range.x, 0.01f, 0.01f, 1000.0f))
							edit_flag = true;
						if (ImGui::DragFloat2("scale_range", &spawn->scale_range.x, 0.01f, 0.01f, 1000.0f))
							edit_flag = true;
						if (ImGui::DragFloat3("area_min", &spawn->area_min.x, 0.01f, 0.01f, 1000.0f))
							edit_flag = true;
						if (ImGui::DragFloat3("area_max", &spawn->area_max.x, 0.01f, 0.01f, 1000.0f))
							edit_flag = true;
						if (ImGui::DragUInt2("spawn_count_range", &spawn->range.x, 0.01f))
							edit_flag = true;
					}
					ImGui::TreePop();
				}
			}

			{
				auto velocity = particle.get_item<particle::item::velocity>();
				if (ImGui::TreeNode("velocity"))
				{
					if (ImGui::Button(!velocity ? "Add velocity" : "Remove velocity"))
					{
						if (velocity)
						{
							particle.unregister_item<particle::item::velocity>();
							velocity = nullptr;
						}
						else
						{
							velocity = particle.register_item<particle::item::velocity>();
						}
						edit_flag = true;
					}
					if (velocity)
					{
						if (ImGui::EditFlags<particle::item::velocity::states>(velocity->state))
							edit_flag = true;

						if (ImGui::SliderFloat3("life_time_range", &velocity->direction.x, -1.0f, 1.0f))
						{
							float l = velocity->direction.x * velocity->direction.x + velocity->direction.y * velocity->direction.y + velocity->direction.z * velocity->direction.z;
							if (l > 0.0001f)
							{
								l = sqrtf(l);
								velocity->direction.x /= l;
								velocity->direction.y /= l;
								velocity->direction.z /= l;
							}
							edit_flag = true;
						}
						if (ImGui::DragAngle2("angle_range", &velocity->angle_range.x, 0.01f, 0.0f, 360.0f))
							edit_flag = true;
						if (ImGui::DragFloat2("power_range", &velocity->power_range.x, 0.01f, 0.01f, 1000.0f))
							edit_flag = true;
						if (ImGui::DragFloat3("acceleration_min", &velocity->acceleration_min.x, 0.01f, 0.01f, 1000.0f))
							edit_flag = true;
						if (ImGui::DragFloat3("acceleration_max", &velocity->acceleration_max.x, 0.01f, 0.01f, 1000.0f))
							edit_flag = true;
						if (ImGui::DragFloat3("deceleration_min", &velocity->deceleration_min.x, 0.01f, 0.01f, 1000.0f))
							edit_flag = true;
						if (ImGui::DragFloat3("deceleration_max", &velocity->deceleration_max.x, 0.01f, 0.01f, 1000.0f))
							edit_flag = true;
						if (ImGui::DragFloat3("gravity_scale", &velocity->gravity_scale.x, 0.01f, 0.0f, 1000.0f))
							edit_flag = true;
					}
					ImGui::TreePop();
				}
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
	return edit_flag;
}

bool editor_items::edit_effect_decal(std::shared_ptr<context>& context, effect_resource_ptr& effect_resource)
{
	auto generate_material = [&]()
	{
		auto material_ptr = std::make_shared<material>("default");
		auto material = material_ptr;
		material->set_domain(material::domains::deferred_decal);
		material->set_alpha_clip_threshold(0.01f);
		material->set_shading_model(material::shading_models::unlit);
		material->set_blend_mode(material::blend_modes::mask);

		//	フラグ類設定
		//material->set_flag(material::flags::two_sided, true);
		material->set_flag(material::flags::indirect_lighting_off, true);

		//	デフォルトシェーダー設定
		material->set_pipeline_state(material::decal_paths::primitive, "deferred_decal_default_primitive");
		material->set_pipeline_state(material::decal_paths::fullscreen, "deferred_decal_default_fullscreen");

		//	パラメーター設定
		material->set_float4("base_color_factor", DirectX::XMFLOAT4(1, 1, 1, 1));
		material->set_float("normal_map_scale", 1.0f);
		{
			texture_ptr texture;
			std::filesystem::path file_path = "Assets/Resources/Textures/ground_crack.png";
			if (texture_manager::instance()->load_texture_from_file(texture, file_path.string()))
			{
				texture->set_flag(texture::flags::srgb, true);
				material->set_texture("BaseColorMap", texture);
				texture_manager::instance()->save_texture(texture);
			}

			//	法線マップがあれば
		}

		return material_ptr;
	};

	bool edit_flag = false;
	if (ImGui::TreeNode("decal"))
	{
		auto& decals = effect_resource->get_decals_for_editor();
		UINT decal_size = static_cast<UINT>(decals.size());
		if (ImGui::DragUInt("decal size", &decal_size, 0.1f))
		{
			decals.resize(decal_size);
			edit_flag = true;
		}
		for (auto& decal : decals)
		{
			ImGui::Separator();
			ImGui::PushID(context->get_item_id());
			ImGui::InputTextUtf8("name", decal.name);

			if (ImGui::DragFloat3("scale", &decal.scale.x, 0.01f, 0.0f, 100.0f))
				edit_flag = true;
			math::rotator rot = decal.rotation;
			if (ImGui::DragFloat3("rotation", &rot.x, 0.01f, 0.0f, 100.0f))
			{
				decal.rotation = rot.convert_quaternion();
				edit_flag = true;
			}
			if (ImGui::DragFloat3("position", &decal.position.x, 0.01f))
				edit_flag = true;

			//	マテリアル
			if (!decal.material)
			{
				decal.material = generate_material();
			}
			if (ImGui::TreeNode("material"))
			{
				if (editor_items::edit_material(context, *decal.material))
				{
					edit_flag = true;
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}
	return edit_flag;
}

#endif	//	defined(USE_IMGUI)


