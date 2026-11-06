#include "Editor/editor_items.h"
#include "texture_importer.h"

#if defined(USE_IMGUI)
#include <commdlg.h>
#include <random>
#include <set>
#include <algorithm>
#include <numeric>
#include "material_shader_map.h"

namespace
{
 
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

	auto effect_resource = effect_instance->get_resource_for_editor();
	auto& nodes = effect_resource->get_nodes_for_editor();
	auto& models = effect_resource->get_models_for_editor();
	auto& particles = effect_resource->get_particles_for_editor();
	auto& decals = effect_resource->get_decals_for_editor();
	auto& animations = effect_resource->get_animations_for_editor();

	//	ノード編集
	{
		size_t add_node_index = effect_resource::invalid_id;
		size_t delete_node_index = effect_resource::invalid_id;
		std::vector<std::pair<std::string, std::string>> renamed_nodes;

		//	高速化のために番号対応
		constexpr size_t none = static_cast<size_t>(-1);
		std::vector<size_t> model_index_by_node(nodes.size(), none);
		std::vector<size_t> particle_index_by_node(nodes.size(), none);
		std::vector<size_t> decal_index_by_node(nodes.size(), none);
		for (UINT index = 0; index < models.size(); ++index)
		{
			auto n = models[index].node_index;
			if (n < nodes.size() && model_index_by_node[n] == none)
				model_index_by_node[n] = index;
		}
		for (UINT index = 0; index < particles.size(); ++index)
		{
			auto n = particles[index].node_index;
			if (n < nodes.size() && particle_index_by_node[n] == none)
				particle_index_by_node[n] = index;
		}
		for (UINT index = 0; index < decals.size(); ++index)
		{
			auto n = decals[index].node_index;
			if (n < nodes.size() && decal_index_by_node[n] == none)
				decal_index_by_node[n] = index;
		}

		std::function<bool(effect_resource::node&, size_t)> node_func = [&](effect_resource::node& node, size_t node_index)
		{
			ImGui::PushID(context->get_item_id());
			bool edit_flag = false;

			//	ノード
			auto& flag_data = context->animation_ui_flag_maps[node.name];

			if (ImGui::Button((flag_data.node ? context->down_allow : context->right_allow).get<utf8_cache>().c_str()))
				flag_data.node = !flag_data.node;
			ImGui::SameLine();
			std::string old_node_name = node.name.get<utf8_cache>();
			std::string node_name = old_node_name;
			ImGui::InputText("name", node_name);
			if (ImGui::IsItemDeactivatedAfterEdit() && node_name != old_node_name)
			{
				node.name = system_utility::convert_shift_jis(node_name);
				renamed_nodes.emplace_back(old_node_name, node.name);
				edit_flag = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("Delete"))
			{
				delete_node_index = node_index;
				edit_flag = true;
			}

			//	ノード処理
			if (flag_data.node)
			{
				ImGui::Indent();
				if (ImGui::DragFloat3("scale", &node.scale.x, 0.01f, 0.0f, 100.0f))
					edit_flag = true;
				if (ImGui::DragFloat3("rotation", &node.rotation.x, 0.01f))
					edit_flag = true;
				if (ImGui::DragFloat3("position", &node.position.x, 0.01f))
					edit_flag = true;

				//	図形表示
				{
					math::float4 draw_color = {1, 1, 1, 1};
					math::vector3f s, t;
					math::quaternionf r;
					math::matrixf(effect_instance->get_nodes()[node_index].global_transform).decompose(s, r, t);
					QUEUE_DEBUG_PRIMITIVE_SPHERE(t.to_float3(), 1.0f, draw_color, true);
				}

				//	モデル
				{
					size_t data_index = model_index_by_node[node_index];

					ImGui::PushID(context->get_item_id());
					if (ImGui::Button((flag_data.model ? context->down_allow : context->right_allow).get<utf8_cache>().c_str()))
						flag_data.model = !flag_data.model;
					ImGui::SameLine();
					ImGui::Text("model");
					ImGui::SameLine();
					if (data_index != none)
					{
						if (ImGui::Button("Delete"))
						{
							models.erase(models.begin() + data_index);
							edit_flag = true;
						}
						else if (flag_data.model)
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
					size_t data_index = particle_index_by_node[node_index];

					ImGui::PushID(context->get_item_id());
					if (ImGui::Button((flag_data.particle ? context->down_allow : context->right_allow).get<utf8_cache>().c_str()))
						flag_data.particle = !flag_data.particle;
					ImGui::SameLine();
					ImGui::Text("particle");
					ImGui::SameLine();
					if (data_index != none)
					{
						if (ImGui::Button("Delete"))
						{
							particles.erase(particles.begin() + data_index);
							edit_flag = true;
						}
						else if (flag_data.particle)
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
					size_t data_index = decal_index_by_node[node_index];

					ImGui::PushID(context->get_item_id());
					if (ImGui::Button((flag_data.decal ? context->down_allow : context->right_allow).get<utf8_cache>().c_str()))
						flag_data.decal = !flag_data.decal;
					ImGui::SameLine();
					ImGui::Text("decal");
					ImGui::SameLine();
					if (data_index != none)
					{
						if (ImGui::Button("Delete"))
						{
							decals.erase(decals.begin() + data_index);
							edit_flag = true;
						}
						else if (flag_data.decal)
						{
							ImGui::Indent();
							if (edit_effect_decal(context, effect_resource, decals[data_index]))
								edit_flag = true;

							//	図形表示
							{
								math::float4 draw_color = {1, 1, 1, 1};
								math::vector3f s, t;
								math::quaternionf r;
								math::matrixf(effect_instance->get_nodes()[node_index].global_transform).decompose(s, r, t);
								QUEUE_DEBUG_PRIMITIVE_BOX(t.to_float3(), r.to_float4(), s.to_float3(), draw_color, true);
							}
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
			auto it_model = models.begin();
			while (it_model != models.end())
			{
				if (it_model->node_index == delete_node_index)
				{
					models.erase(it_model);
					break;
				}
				++it_model;
			}
			auto it_particle = particles.begin();
			while (it_particle != particles.end())
			{
				if (it_particle->node_index == delete_node_index)
				{
					particles.erase(it_particle);
					break;
				}
				++it_particle;
			}
			auto it_decal = decals.begin();
			while (it_decal != decals.end())
			{
				if (it_decal->node_index == delete_node_index)
				{
					decals.erase(it_decal);
					break;
				}
				++it_decal;
			}
			sort_nodes(effect_resource);
		}

		if (edit_flag)
		{
			//	アニメーションノードの編集
			auto& animation_nodes = animations.back().nodes;

			for (auto& renamed_node : renamed_nodes)
			{
				auto it = std::find_if(animation_nodes.begin(),
						 animation_nodes.end(),
						 [&](animation::node_data& n) {return n.node_name == renamed_node.first;});
				if (it != animation_nodes.end())
				{
					//	古いキーを破棄
					auto it_flags = context->animation_ui_flag_maps.find(it->node_name);
					if (it_flags != context->animation_ui_flag_maps.end())
					{
						auto flag_data = it_flags->second;
						context->animation_ui_flag_maps.erase(it_flags);
						context->animation_ui_flag_maps[renamed_node.second] = flag_data;
					}

					it->node_name = renamed_node.second;
				}
			}

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
		context->flag_maps[context::EditAnimationHideCurve] = true;
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
	std::string file_path = model.resource.get_reference().path;
	if (edit_model_resource_item(context, resource, &file_path))
	{
		model.resource = resource;
		edit_flag = true;
	}
	return edit_flag;
}

bool editor_items::edit_effect_particle(std::shared_ptr<context>& context, effect_resource_ptr& effect_resource, effect_resource::particle& particle)
{
	auto generate_material = [&]()
	{
		auto material_ptr = std::make_shared<material_resource>("default");
		auto material = material_ptr;
		material->set_domain(material_resource::domains::particle);
		material->set_alpha_clip_threshold(0.01f);
		material->set_shading_model(material_resource::shading_models::unlit);
		material->set_blend_mode(material_resource::blend_modes::translucent);

		//	フラグ類設定
		material->set_flag(material_resource::flags::two_sided, true);
	//	material->set_flag(material_resource::flags::shadow_caster, true);
	//	material->set_flag(material_resource::flags::shadow_receiver, true);
		material->set_flag(material_resource::flags::indirect_lighting_off, true);

		//	デフォルトシェーダー設定
		material->set_shader_path(material_shader_map::get_default_shader_path(material_resource::domains::particle));

		//	パラメーター設定
		material->set_float4("base_color_factor", math::float4(1, 1, 1, 1));
		if (auto texture = texture_importer::instance()->import("Assets/Resources/Textures/white_16x16.png", texture_import_settings::make_preset(texture_import_settings::presets::color)))
			material->set_texture("BaseColorMap", texture);
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
							material_resource_ptr slot_material = renderer->material;
							if (editor_items::edit_material_slot(context, slot_material, material_resource::domains::particle))
							{
								renderer->material = slot_material;
								edit_flag = true;
							}
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
							material_resource_ptr slot_material = renderer->material;
							if (editor_items::edit_material_slot(context, slot_material, material_resource::domains::particle))
							{
								renderer->material = slot_material;
								edit_flag = true;
							}
							if (editor_items::edit_material(context, *renderer->material))
							{
								edit_flag = true;
							}
							ImGui::TreePop();
						}

						//	軌跡
						if (ImGui::DragUInt("max", &renderer->trail.max, 0.1f, 1, effect_manager::trail_body_max))
							edit_flag = true;
						if (ImGui::EditFlags<particle::item::renderer::trails::states>(renderer->trail.state))
							edit_flag = true;
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
							math::int2 direction = math::int2(renderer->uv.splits.x, renderer->uv.splits.y);
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
						if (ImGui::DragUInt("index", &renderer->uv.index, 1.0f, 0, renderer->uv.splits.x * renderer->uv.splits.y - 1))
						{
							edit_flag = true;
						}

						bool random_index = renderer->uv.is_state(particle::item::renderer::texcoords::states::random_index);
						if (ImGui::Checkbox("random index", &random_index))
						{
							renderer->uv.set_state(particle::item::renderer::texcoords::states::random_index, random_index);
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

					//	フェード設定
					checked = renderer->color.is_state(particle::item::renderer::colors::states::use_fade);
					if (ImGui::Checkbox("use_fade", &checked))
					{
						renderer->color.set_state(particle::item::renderer::colors::states::use_fade, checked);
						edit_flag = true;
					}
					if (checked)
					{
						if (ImGui::SliderFloat2("fade_range", &renderer->color.fade_range.x, 0.0f, 1.0f))
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

				if (ImGui::SliderFloat3("base direction", &velocity->direction.x, -1.0f, 1.0f))
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
				if (ImGui::DragFloat("damping_min", &velocity->damping_min, 0.01f, 0.0f, 100000.0f))
					edit_flag = true;
				if (ImGui::DragFloat("damping_max", &velocity->damping_max, 0.01f, 0.0f, 100000.0f))
					edit_flag = true;
				if (ImGui::DragFloat3("gravity_scale", &velocity->gravity_scale.x, 0.01f, 0.0f, 1000.0f))
					edit_flag = true;
			}
			ImGui::TreePop();
		}
	}

	{
		auto attract = particle.particle.get_item<particle::item::attract>();
		if (ImGui::TreeNode("attract"))
		{
			if (ImGui::Button(!attract ? "Add attract" : "Remove attract"))
			{
				if (attract)
				{
					particle.particle.unregister_item<particle::item::attract>();
					attract = nullptr;
				}
				else
				{
					attract = particle.particle.register_item<particle::item::attract>();
				}
				edit_flag = true;
			}
			if (attract)
			{
				if (ImGui::DragFloat("radius", &attract->radius, 0.01f, 0.0f, 100000.0f))
					edit_flag = true;
				if (ImGui::DragFloat("strength", &attract->strength, 0.01f, 0.0f, 100000.0f))
					edit_flag = true;
				if (ImGui::DragFloat("damping", &attract->damping, 0.01f, 0.0f, 100000.0f))
					edit_flag = true;
				if (ImGui::DragFloat("falloff", &attract->falloff, 0.01f, 0.0f, 100000.0f))
					edit_flag = true;
				if (ImGui::DragFloat("kill_radius", &attract->kill_radius, 0.01f, 0.0f, 100000.0f))
					edit_flag = true;
			}
			ImGui::TreePop();
		}
	}

	{
		auto event = particle.particle.get_item<particle::item::event>();
		if (ImGui::TreeNode("event"))
		{
			if (ImGui::Button(!event ? "Add event" : "Remove event"))
			{
				if (event)
				{
					particle.particle.unregister_item<particle::item::event>();
					event = nullptr;
				}
				else
				{
					event = particle.particle.register_item<particle::item::event>();
				}
				edit_flag = true;
			}
			if (event)
			{
				//	対象パーティクルが付いているノード名のリスト
				auto& effect_nodes = effect_resource->get_nodes_for_editor();
				auto& effect_particles = effect_resource->get_particles_for_editor();
				std::vector<std::string> target_node_names;
				for (auto& target_particle : effect_particles)
					if (target_particle.node_index < effect_nodes.size())
						target_node_names.push_back(effect_nodes[target_particle.node_index].name);

				std::vector<const char*> target_node_name_ptrs;
				for (auto& target_node_name : target_node_names)
					target_node_name_ptrs.push_back(target_node_name.c_str());

				int target_node_current = -1;
				for (int i = 0; i < static_cast<int>(target_node_names.size()); ++i)
					if (target_node_names[i] == event->node_name)
					{
						target_node_current = i;
						break;
					}

				if (ImGui::Combo("target node", &target_node_current, target_node_name_ptrs.data(), static_cast<int>(target_node_name_ptrs.size())))
				{
					if (target_node_current >= 0 && target_node_current < static_cast<int>(target_node_names.size()))
					{
						event->node_name = target_node_names[target_node_current];
						edit_flag = true;
					}
				}
				if (ImGui::DragFloat("radius", &event->radius, 0.01f, 0.0f, 100000.0f))
					edit_flag = true;

				bool use_sdf_collision = event->is_state(particle::item::event::states::contact_collision);
				if (ImGui::Checkbox("use sdf collision", &use_sdf_collision))
				{
					event->set_state(particle::item::event::states::contact_collision, use_sdf_collision);
					edit_flag = true;
				}

				bool kill_on_hit = event->is_state(particle::item::event::states::kill_on_hit);
				if (ImGui::Checkbox("kill on hit", &kill_on_hit))
				{
					event->set_state(particle::item::event::states::kill_on_hit, kill_on_hit);
					edit_flag = true;
				}

				bool sweep_collision = event->is_state(particle::item::event::states::sweep_collision);
				if (ImGui::Checkbox("sweep collision", &sweep_collision))
				{
					event->set_state(particle::item::event::states::sweep_collision, sweep_collision);
					edit_flag = true;
				}
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
		auto material_ptr = std::make_shared<material_resource>("default");
		auto material = material_ptr;
		material->set_domain(material_resource::domains::deferred_decal);
		material->set_alpha_clip_threshold(0.01f);
		material->set_shading_model(material_resource::shading_models::default_lit);
		material->set_blend_mode(material_resource::blend_modes::mask);

		//	フラグ類設定
		//material->set_flag(material_resource::flags::two_sided, true);
		material->set_flag(material_resource::flags::indirect_lighting_off, true);

		//	デフォルトシェーダー設定
		material->set_shader_path(material_shader_map::get_default_shader_path(material_resource::domains::deferred_decal));

		//	パラメーター設定
		material->set_float4("base_color_factor", math::float4(1, 1, 1, 1));
		material->set_float("normal_map_scale", 1.0f);
		{
			if (auto texture = texture_importer::instance()->import("Assets/Resources/Textures/ground_crack.png", texture_import_settings::make_preset(texture_import_settings::presets::color)))
				material->set_texture("BaseColorMap", texture);

			//	法線マップがあれば
		}

		return material_ptr;
	};

	bool edit_flag = false;
	if (!decal.material)
	{
		decal.material = generate_material();
	}
	{
		material_resource_ptr slot_material = decal.material;
		if (editor_items::edit_material_slot(context, slot_material, material_resource::domains::deferred_decal))
		{
			decal.material = slot_material;
			edit_flag = true;
		}
	}
	if (editor_items::edit_material(context, *decal.material))
	{
		edit_flag = true;
	}
	return edit_flag;
}

#endif	//	defined(USE_IMGUI)