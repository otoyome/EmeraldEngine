#include "Editor/editor_items.h"

#if defined(USE_IMGUI)
#include <commdlg.h>
#include <random>
#include <set>
#include <algorithm>
#include <numeric>

namespace
{
	static constexpr material::set_surface_pipeline_states_desc pso_descs[] =
	{
		{material::surface_paths::depth_write_pass, "compute_patricle_sprite_depth"},
		{material::surface_paths::g_buffer_pass, "compute_patricle_sprite_gbuffer"},
		{material::surface_paths::shadow_write_pass, "compute_patricle_sprite_shadowmap"},
		{material::surface_paths::forward_pass, "compute_patricle_sprite_forward"},
	};
 
    //  nodes を親のインデックスが若い順に並べ替え
    void sort_nodes(effect_resource_ptr& effect_resource)
    {
		auto node_sort_key = [](size_t parent_index)
		{
			return (parent_index == effect_resource::invalid_id) ? 0 : parent_index + 1;
		};

        auto& nodes = effect_resource->get_nodes_for_editor();
        auto& models = effect_resource->get_models_for_editor();
        auto& particles = effect_resource->get_particles_for_editor();
        auto& decals = effect_resource->get_decals_for_editor();
 
        std::vector<size_t> order(nodes.size());
        std::iota(order.begin(), order.end(), 0);
        std::stable_sort(order.begin(), order.end(),
            [&](size_t lhs, size_t rhs)
            {
                return node_sort_key(nodes[lhs].parent_index) < node_sort_key(nodes[rhs].parent_index);
            });

        std::vector<size_t> old_to_new(nodes.size());
        for (size_t new_index = 0; new_index < order.size(); ++new_index)
            old_to_new[order[new_index]] = new_index;

        std::vector<effect_resource::node> sorted_nodes;
        sorted_nodes.reserve(nodes.size());
        for (size_t old_index : order)
            sorted_nodes.push_back(std::move(nodes[old_index]));
        nodes = std::move(sorted_nodes);

        for (auto& node : nodes)
        {
            if (node.parent_index != effect_resource::invalid_id)
                node.parent_index = old_to_new[node.parent_index];
        }
        for (auto& model : models)
        {
            if (model.node_index != effect_resource::invalid_id)
                model.node_index = old_to_new[model.node_index];
        }
        for (auto& particle : particles)
        {
            if (particle.node_index != effect_resource::invalid_id)
                particle.node_index = old_to_new[particle.node_index];
        }
        for (auto& decal : decals)
        {
            if (decal.node_index != effect_resource::invalid_id)
                decal.node_index = old_to_new[decal.node_index];
        }
    }
}

bool editor_items::edit_effect_noeds(std::shared_ptr<context>& context, effect_instance_ptr& effect_instance)
{
	bool edit_flag = false;

	auto& effect_resource = effect_instance->get_resource();
	auto& nodes = effect_resource->get_nodes_for_editor();
	auto& models = effect_resource->get_models_for_editor();
	auto& particles = effect_resource->get_particles_for_editor();
	auto& decals = effect_resource->get_decals_for_editor();
	auto& animations = effect_resource->get_animations_for_editor();

	//	ノード編集
	{
		size_t add_node_index = effect_resource::invalid_id;
		size_t delete_node_index = effect_resource::invalid_id;
		std::function<bool(effect_resource::node&, size_t)> node_func = [&](effect_resource::node& node, size_t node_index)
		{
			ImGui::PushID(context->get_item_id());
			bool edit_flag = false;

			//	ノード
			auto& flag = context->flag_maps[node.name];

			if (ImGui::Button(system_utility::convert_shift_jis_to_utf8(flag ? "↓" : "→").c_str()))
				flag = !flag;
			ImGui::SameLine();
			if (ImGui::InputTextUtf8("name", node.name))
				edit_flag = true;
			ImGui::SameLine();
			if (ImGui::Button("Delete"))
			{
				delete_node_index = node_index;
				edit_flag = true;
			}

			//	ノード処理
			if (flag)
			{
				ImGui::Indent();
				if (ImGui::DragFloat3("scale", &node.scale.x, 0.01f, 0.0f, 100.0f))
					edit_flag = true;
				if (ImGui::DragFloat3("rotation", &node.rotation.x, 0.01f))
					edit_flag = true;
				if (ImGui::DragFloat3("position", &node.position.x, 0.01f))
					edit_flag = true;
				//	図形表示
				DirectX::XMFLOAT4 draw_color = {1, 1, 1, 1};
				PUSH_DEBUG_PRIMITIVE_SPHERE(node.position, 1.0f, draw_color, true);

				//	モデル
				{
					size_t data_index = -1;
					for (UINT index = 0; index < models.size(); ++index)
					{
						auto& model = models[index];
						if (model.node_index == node_index)
						{
							data_index = index;
							break;
						}
					}

					ImGui::PushID(context->get_item_id());
					auto& flag = context->flag_maps[node.name + std::string("model")];
					if (ImGui::Button(system_utility::convert_shift_jis_to_utf8(flag ? "↓" : "→").c_str()))
						flag = !flag;
					ImGui::SameLine();
					ImGui::Text("model");
					ImGui::SameLine();
					if (data_index != -1)
					{
						if (ImGui::Button("Delete"))
						{
							models.erase(models.begin() + data_index);
							edit_flag = true;
						}
						else if (flag)
						{
							ImGui::Indent();
							if (edit_effect_model(context, effect_resource, models[data_index]))
								edit_flag = true;
							ImGui::Unindent();
						}
					}
					else
					{
						if (ImGui::Button("Add"))
						{
							auto& model = models.emplace_back();
							model.node_index = node_index;
							edit_flag = true;
						}
					}
					ImGui::PopID();
				}

				//	パーティクル
				{
					size_t data_index = -1;
					for (UINT index = 0; index < particles.size(); ++index)
					{
						auto& particle = particles[index];
						if (particle.node_index == node_index)
						{
							data_index = index;
							break;
						}
					}

					ImGui::PushID(context->get_item_id());
					auto& flag = context->flag_maps[node.name + std::string("particle")];
					if (ImGui::Button(system_utility::convert_shift_jis_to_utf8(flag ? "↓" : "→").c_str()))
						flag = !flag;
					ImGui::SameLine();
					ImGui::Text("particle");
					ImGui::SameLine();
					if (data_index != -1)
					{
						if (ImGui::Button("Delete"))
						{
							particles.erase(particles.begin() + data_index);
							edit_flag = true;
						}
						else if (flag)
						{
							ImGui::Indent();
							if (edit_effect_particle(context, effect_resource, particles[data_index]))
								edit_flag = true;
							ImGui::Unindent();
						}
					}
					else
					{
						if (ImGui::Button("Add"))
						{
							auto& particle = particles.emplace_back();
							particle.node_index = node_index;
							edit_flag = true;
						}
					}
					ImGui::PopID();
				}

				//	デカール
				{
					size_t data_index = -1;
					for (UINT index = 0; index < decals.size(); ++index)
					{
						auto& decal = decals[index];
						if (decal.node_index == node_index)
						{
							data_index = index;
							break;
						}
					}

					ImGui::PushID(context->get_item_id());
					auto& flag = context->flag_maps[node.name + std::string("decal")];
					if (ImGui::Button(system_utility::convert_shift_jis_to_utf8(flag ? "↓" : "→").c_str()))
						flag = !flag;
					ImGui::SameLine();
					ImGui::Text("decal");
					ImGui::SameLine();
					if (data_index != -1)
					{
						if (ImGui::Button("Delete"))
						{
							decals.erase(decals.begin() + data_index);
							edit_flag = true;
						}
						else if (flag)
						{
							ImGui::Indent();
							if (edit_effect_decal(context, effect_resource, decals[data_index]))
								edit_flag = true;
							ImGui::Unindent();
						}
					}
					else
					{
						if (ImGui::Button("Add"))
						{
							auto& decal = decals.emplace_back();
							decal.node_index = node_index;
							edit_flag = true;
						}
					}
					ImGui::PopID();
				}

				//	ノード追加
				if (ImGui::Button("Add Child"))
				{
					add_node_index = node_index;
					edit_flag = true;
				}

				//	子ノードを処理していく
				for (size_t index = 0; index < nodes.size(); ++index)
				{
					auto& child_node = nodes[index];
					if (child_node.parent_index != node_index)
						continue;

					if (node_func(child_node, index))
						edit_flag = true;
				}
				ImGui::Unindent();
			}

			ImGui::PopID();
			return edit_flag;
		};

		//	親から順に処理していく
		ImGui::PushID(context->get_item_id());
		for (size_t index = 0; index < nodes.size(); ++index)
		{
			auto& child_node = nodes[index];
			if (child_node.parent_index != effect_resource::invalid_id)
				continue;

			if (node_func(child_node, index))
				edit_flag = true;
		}
		ImGui::PopID();

		if (add_node_index != effect_resource::invalid_id)
		{
			auto& node = nodes.emplace_back();
			node.name = std::string("default node") + std::to_string(add_node_index + rand());
			node.parent_index = add_node_index;
			sort_nodes(effect_resource);
		}

		if (delete_node_index != effect_resource::invalid_id)
		{
			nodes.erase(nodes.begin() + delete_node_index);
			sort_nodes(effect_resource);
		}

		if (edit_flag)
		{
			//	アニメーションノードの編集
			auto& animation_nodes = animations.back().nodes;
			for (auto& node : nodes)
			{
				auto it = std::find_if(animation_nodes.begin(),
							 animation_nodes.end(),
							 [&](animation::node_data& n) {return n.node_name == node.name;});
				if (it == animation_nodes.end())
				{
					auto& animation_node = animation_nodes.emplace_back();
					animation_node.node_name = node.name;
				}
			}
		}
	}

	return edit_flag;
}

bool editor_items::edit_effect_animation(std::shared_ptr<context>& context, effect_instance_ptr& effect_instance)
{
	bool edit_flag = false;

	auto& effect_resource = effect_instance->get_resource();
	auto& animations = effect_resource->get_animations_for_editor();

	//	アニメーション編集
	if (ImGui::Begin("Effect Animation Window"))
	{
		//	アニメーションは基本的に一つしかない物とする
		auto& animation_data = animations.back();

		//	アニメーションプレイヤー
		{
			float animation_end_time = animation_data.end_time;
			float animation_time = effect_instance->get_animation_time();

			//	開始時の状態へ戻す
			if (animation_time <= 1e-6f)
			{
				effect_instance->reset_to_initial_state();
			}

			//	操作系
			auto& loop_flag = context->flag_maps["__Effect_Animation_Player_Loop_Flag__"];
			auto& pause_flag = context->flag_maps["__Effect_Animation_Player_Pause_Flag__"];
			ImGui::Checkbox("loop", &loop_flag);
			ImGui::SameLine();
			if (ImGui::Button(pause_flag ? "play" : "stop"))
			{
				pause_flag = !pause_flag;
			}

			ImGui::SliderFloat("animation time", &animation_time, 0.0f, animation_end_time + 0.001f, "%.6f");
			if (animation_time >= animation_end_time)
				animation_time = loop_flag ? 0.0f : animation_end_time;

			effect_instance->set_animation_time(animation_time);
			effect_instance->set_animation_speed(pause_flag ? 0.0f : 1.0f);
			ImGui::Separator();
		}

		//	アニメーション情報編集
		context->flag_maps[context::EditAnimationHideName] = true;
		context->flag_maps[context::EditAnimationHideRootMotion] = true;
		context->flag_maps[context::EditAnimationHideIK] = true;
		editor_items::edit_animation_data(context, animation_data);

		//	アニメーションタイムライン情報編集
		editor_items::edit_animation_data_timeline(context, animation_data);
	}
	ImGui::End();
	return edit_flag;
}

bool editor_items::edit_effect_model(std::shared_ptr<context>& context, effect_resource_ptr& effect_resource, effect_resource::model& model)
{
	bool edit_flag = false;
	model_resource_ptr resource = model.resource;
	std::string file_path = model.file_path;
	if (edit_model_resource_item(context, resource, &file_path))
	{
		model.resource = resource;
		model.file_path = file_path;
		edit_flag = true;
	}
	return edit_flag;
}

bool editor_items::edit_effect_particle(std::shared_ptr<context>& context, effect_resource_ptr& effect_resource, effect_resource::particle& particle)
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

	{
		auto renderer = particle.particle.get_item<particle::item::renderer>();
		if (ImGui::TreeNode("renderer"))
		{
			if (ImGui::Button(!renderer ? "Add Renderer" : "Remove Renderer"))
			{
				if (renderer)
				{
					particle.particle.unregister_item<particle::item::renderer>();
					renderer = nullptr;
				}
				else
				{
					renderer = particle.particle.register_item<particle::item::renderer>();
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

				//	フラグ類設定
				if (ImGui::EditFlags<particle::item::renderer::states>(renderer->state))
				{
					edit_flag = true;
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
		auto spawn = particle.particle.get_item<particle::item::spawn>();
		if (ImGui::TreeNode("spawn"))
		{
			if (ImGui::Button(!spawn ? "Add spawn" : "Remove spawn"))
			{
				if (spawn)
				{
					particle.particle.unregister_item<particle::item::spawn>();
					spawn = nullptr;
				}
				else
				{
					spawn = particle.particle.register_item<particle::item::spawn>();
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
		auto velocity = particle.particle.get_item<particle::item::velocity>();
		if (ImGui::TreeNode("velocity"))
		{
			if (ImGui::Button(!velocity ? "Add velocity" : "Remove velocity"))
			{
				if (velocity)
				{
					particle.particle.unregister_item<particle::item::velocity>();
					velocity = nullptr;
				}
				else
				{
					velocity = particle.particle.register_item<particle::item::velocity>();
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

	return edit_flag;
}

bool editor_items::edit_effect_decal(std::shared_ptr<context>& context, effect_resource_ptr& effect_resource, effect_resource::decal& decal)
{
	auto generate_material = [&]()
	{
		auto material_ptr = std::make_shared<material>("default");
		auto material = material_ptr;
		material->set_domain(material::domains::deferred_decal);
		material->set_alpha_clip_threshold(0.01f);
		material->set_shading_model(material::shading_models::default_lit);
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
	if (!decal.material)
	{
		decal.material = generate_material();
	}
	if (editor_items::edit_material(context, *decal.material))
	{
		edit_flag = true;
	}

	//	図形表示
	auto& node = effect_resource->get_nodes_for_editor()[decal.node_index];
	DirectX::XMFLOAT4 draw_color = {1, 1, 1, 1};
	PUSH_DEBUG_PRIMITIVE_BOX(node.position, math::rotator(node.rotation), node.scale, draw_color, true);
	return edit_flag;
}

#endif	//	defined(USE_IMGUI)


