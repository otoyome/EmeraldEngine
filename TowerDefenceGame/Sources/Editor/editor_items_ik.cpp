#include "Editor/editor_items.h"
#include "Object/engine_context.h"

#if defined(USE_IMGUI)
#include <commdlg.h>
#include <random>
#include <set>

#include "Animation/ik_resource.h"
#include "Animation/ik_solver.h"
#include "Model/model_instance.h"

namespace
{
	//	
	class ik_resource_editor : public editor_items::ibase_editor
	{
		std::string	blank_name	= "[BLANK]";

	public:
		ik_resource_editor()
		: editor_items::ibase_editor()
		{
		}
		~ik_resource_editor() override
		{
		}

		void set_data(engine_context& engine, model_instance_ptr model_instance, ik_resource_ptr ik_resource)
		{
			this->engine = &engine;

			if (this->model_instance == model_instance && this->ik_resource == ik_resource)
				return;

			this->model_instance = model_instance;
			this->ik_resource = ik_resource;

			model_bone_names.clear();
			if (model_instance == nullptr)
				return;

			//	IK計算
			ik_solver = std::make_shared<::ik_solver>(model_instance, ik_resource);

			//	ボーン名前リストを生成
			model_bone_names.emplace_back(blank_name);
			auto& node_list = model_instance->get_nodes();
			auto& mesh_list = model_instance->get_meshes();
			for (auto& mesh : mesh_list)
			{
				for (auto& bone : mesh.mesh->bones)
				{
					auto& node_name = node_list[bone.node_index].name;
					auto it = std::find_if(model_bone_names.begin(), model_bone_names.end(),
						[node_name](const std::string& r) {return node_name == r; });
					if (it == model_bone_names.end())
					{
						model_bone_names.emplace_back(node_name.data());
					}
				}
			}

			//	IKソルバー名構築
			{
				solver_names.clear();
				for( auto& solve_data : ik_resource->get_solve_data_list_for_editor())
				{
					solver_names.emplace_back(solve_data.name);
				}
			}
		}

		std::size_t search_ik_node(std::string_view node_name)
		{
			if (node_name.empty())
				return std::size_t(-1);

			auto& nodes = ik_resource->get_node_list_for_editor();

			//	登録済みか判定
			{
				auto it = std::find_if(nodes.begin(), nodes.end(),
					[&](const ik_resource::node& r) { return r.source_node_name == node_name; });
				if (it != nodes.end())
					return std::size_t(std::distance(nodes.begin(), it));
			}

			std::size_t return_index = nodes.size();
			auto& node = nodes.emplace_back();
			node.source_node_name = std::string(node_name);
			return return_index;
		}

		std::string search_ik_node_name(std::size_t node_id)
		{
			if (node_id == size_t(-1))
				return	blank_name;

			auto& nodes = ik_resource->get_node_list_for_editor();
			return nodes[node_id].source_node_name;
		}

		std::size_t search_ik_solve_data(std::string_view name)
		{
			if (name.empty())
				return std::size_t(-1);

			auto& datas = ik_resource->get_solve_data_list_for_editor();
			for (UINT index = 0; index < datas.size(); ++index)
			{
				if (datas[index].name == name)
					return index;
			}

			return std::size_t(-1);
		}

		std::string search_ik_solver_name(std::size_t solve_id)
		{
			if (solve_id == size_t(-1))
				return	blank_name;

			auto& nodes = ik_resource->get_solve_data_list_for_editor();
			return nodes[solve_id].name;
		}

		void render(std::shared_ptr<editor_items::context>& context) override
		{
			ImGui::PushID(context->get_item_id());
			{
				ImGui::Checkbox("display_bone", &display_bone);

				//	IK情報
				bool edit_flag = render_ik_solve_datas(context);

				ImGui::Separator();
				render_solve_tests(context, edit_flag);

				//	実行情報
				if (render_ik_behavior_datas(context))
					edit_flag = true;

				ImGui::Separator();
				render_behavior_tests(context, edit_flag);
			}
			ImGui::PopID();

			render_solver_bones();
			render_behavior_bones();

			//	編集モデルのボーン表示
			if (display_bone)
			{
				selected_bone_names.clear();
				editor_items::render_model_bones(*engine, *model_instance, selected_bone_names);
			}
		}

	private:
		bool render_ik_solve_datas(std::shared_ptr<editor_items::context>& context)
		{
			bool edited = false;

			//	IK Data
			auto& ik_data_list = ik_resource->get_solve_data_list_for_editor();
			UINT ik_data_list_size = static_cast<UINT>(ik_data_list.size());
			if (ImGui::DragUInt("ik_data_count", &ik_data_list_size, 0.1f, 0, 1000))
			{
				ik_data_list.resize(ik_data_list_size);
			}
			edited |= ImGui::IsItemEdited();

			for (auto& ik_data : ik_data_list)
			{
				ImGui::PushID(context->get_item_id());
				bool tree_open = ImGui::TreeNode(" ");
				ImGui::SameLine();
				ImGui::PushID(context->get_item_id());
				ImGui::InputTextUtf8("", ik_data.name);
				edited |= ImGui::IsItemEdited();
				ImGui::PopID();
				if (tree_open)
				{
					ImGui::Combo<ik_resource::solve_mode>("mode", ik_data.mode);
					edited |= ImGui::IsItemEdited();
					switch (ik_data.mode)
					{
						case ik_resource::solve_mode::aim:
						{
							edited |= edit_aim(ik_data, context);
							break;
						}
						case ik_resource::solve_mode::limb:
						{
							edited |= edit_limb(ik_data, context);
							break;
						}
						case ik_resource::solve_mode::ccd:
						case ik_resource::solve_mode::ccd_pmx:
						{
							edited |= edit_ccd(ik_data, context);
							break;
						}
					}

					ImGui::TreePop();
				}

				ImGui::PopID();
			}

			//	IKソルバー名構築
			if (edited)
			{
				solver_names.clear();
				for( auto& solve_data : ik_resource->get_solve_data_list_for_editor())
				{
					solver_names.emplace_back(solve_data.name);
				}
			}

			return edited;
		}

		bool render_ik_behavior_datas(std::shared_ptr<editor_items::context>& context)
		{
			bool edited = false;

			//	IK Data
			auto& ik_behavior_data_list = ik_resource->get_behavior_data_list_for_editor();
			UINT ik_behavior_data_size = static_cast<UINT>(ik_behavior_data_list.size());
			if (ImGui::DragUInt("ik_behavior_data_count", &ik_behavior_data_size, 0.1f, 0, 1000))
			{
				ik_behavior_data_list.resize(ik_behavior_data_size);
			}
			edited |= ImGui::IsItemEdited();

			for (auto& ik_behavior_data : ik_behavior_data_list)
			{
				ImGui::PushID(context->get_item_id());
				bool tree_open = ImGui::TreeNode(" ");
				ImGui::SameLine();

				ImGui::PushID(context->get_item_id());
				ImGui::InputTextUtf8("", ik_behavior_data.name);
				edited |= ImGui::IsItemEdited();
				ImGui::PopID();

				if (tree_open)
				{
					ImGui::PushID(context->get_item_id());

					if (ImGui::Combo<ik_resource::behavior_mode>("mode", ik_behavior_data.mode))
					{
						switch (ik_behavior_data.mode)
						{
							case ik_resource::behavior_mode::foot:
							{
								ik_behavior_data.data.emplace<ik_resource::foot_data>();
								break;
							}
							case ik_resource::behavior_mode::hand:
							{
								ik_behavior_data.data.emplace<ik_resource::hand_data>();
								break;
							}
							case ik_resource::behavior_mode::lookat:
							{
								ik_behavior_data.data.emplace<ik_resource::look_at_data>();
								break;
							}
						}
					}
					edited |= ImGui::IsItemEdited();

					switch (ik_behavior_data.mode)
					{
						case ik_resource::behavior_mode::foot:
						{
							auto& foot_data = std::get<ik_resource::foot_data>(ik_behavior_data.data);

							//	ソルバー指定
							{
								if (ik_behavior_data.solve_ids.size() < 2)
									ik_behavior_data.solve_ids.resize(2);

								std::string solver_name = search_ik_solver_name(ik_behavior_data.solve_ids[0]);
								if (ImGui::Combo("0", solver_name, solver_names))
									ik_behavior_data.solve_ids[0] = search_ik_solve_data(solver_name);
								ImGui::DragFloat("foot_offset0", &foot_data.ray_origin_offsets[0], 0.001f, -system_units::convert_cm_to_unit(100.0f), system_units::convert_cm_to_unit(100.0f));
								edited |= ImGui::IsItemEdited();

								solver_name = search_ik_solver_name(ik_behavior_data.solve_ids[1]);
								if (ImGui::Combo("1", solver_name, solver_names))
									ik_behavior_data.solve_ids[1] = search_ik_solve_data(solver_name);
								ImGui::DragFloat("foot_offset1", &foot_data.ray_origin_offsets[1], 0.001f, -system_units::convert_cm_to_unit(100.0f), system_units::convert_cm_to_unit(100.0f));

								edited |= ImGui::IsItemEdited();
							}
							{
								//	腰設定
								{
									//	IK ソルバーサイズ指定
									UINT adjust_node_size = static_cast<UINT>(foot_data.adjust_node_indices.size());
									if (ImGui::DragUInt("adjust_node_size", &adjust_node_size, 0.1f, 0, 1000))
									{
										foot_data.adjust_node_indices.resize(adjust_node_size);
									}

									//	ソルバー指定
									UINT index = 0;
									for (auto& adjust_node_index : foot_data.adjust_node_indices)
									{
										ImGui::PushID(context->get_item_id());

										std::string node_name = search_ik_node_name(adjust_node_index);
										if (ImGui::Combo("node", node_name, model_bone_names))
											adjust_node_index = search_ik_node(node_name);
										edited |= ImGui::IsItemEdited();

										ImGui::PopID();
									}
								}

								ImGui::DragFloat("ray_length", &foot_data.ray_length, 0.001f, 0.0f, system_units::convert_cm_to_unit(100.0f));
								ImGui::DragFloat("foot_offset", &foot_data.foot_offset, 0.001f, -system_units::convert_cm_to_unit(100.0f), system_units::convert_cm_to_unit(100.0f));
								edited |= ImGui::IsItemEdited();
								//ImGui::Checkbox("align_to_normal", &foot_data.align_to_normal
								//edited |= ImGui::IsItemEdited();
							}
							break;
						}
						case ik_resource::behavior_mode::lookat:
						{
							//	IK ソルバーサイズ指定
							UINT solve_data_list_size = static_cast<UINT>(ik_behavior_data.solve_ids.size());
							if (ImGui::DragUInt("ik_solver_count", &solve_data_list_size, 0.1f, 0, 1000))
							{
								ik_behavior_data.solve_ids.resize(solve_data_list_size);
							}

							//	ソルバー指定
							UINT index = 0;
							for (auto& solve_id : ik_behavior_data.solve_ids)
							{
								ImGui::PushID(context->get_item_id());

								std::string solver_name = search_ik_solver_name(solve_id);
								if (ImGui::Combo("solver", solver_name, solver_names))
									solve_id = search_ik_solve_data(solver_name);
								edited |= ImGui::IsItemEdited();

								ImGui::PopID();
							}

							break;
						}
					}
					ImGui::PopID();
					ImGui::TreePop();
				}

				ImGui::PopID();
			}

			return edited;
		}

		void render_solve_tests(std::shared_ptr<editor_items::context>& context, bool item_edited)
		{
			ImGui::PushID(context->get_item_id());
			if (item_edited || test_solve_iks.size() != ik_resource->get_solve_data_list().size())
			{
				std::vector<test_ik> temp_iks;
				for (auto& ik_data : ik_resource->get_solve_data_list())
				{
					test_ik& data = temp_iks.emplace_back();

					//	同名登録判定
					auto it = std::find_if(test_solve_iks.begin(),
										test_solve_iks.end(),
										[&](const test_ik& r)
										{
											return ik_data.name == r.data_nama;
										});
					if (it != test_solve_iks.end())
					{
						data = *it;
					}
					else
					{
						//	新しく追加
						data.data_nama = ik_data.name;
						data.test_flag = false;
					}
				}
				test_solve_iks = temp_iks;
				ik_solver = std::make_shared<::ik_solver>(model_instance, ik_resource);
			}
			if (ImGui::TreeNode("solve test"))
			{
				for (auto& test_ik : test_solve_iks)
				{
					ImGui::PushID(context->get_item_id());
					ImGui::BeginDisabled();
					ImGui::InputTextUtf8("", test_ik.data_nama);
					ImGui::EndDisabled();
					ImGui::Checkbox("test_flag", &test_ik.test_flag);
					ImGui::DragFloat3("position", &test_ik.position.x, 0.01f);
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		void render_behavior_tests(std::shared_ptr<editor_items::context>& context, bool item_edited)
		{
			ImGui::PushID(context->get_item_id());
			if (item_edited || test_behavior_iks.size() != ik_resource->get_behavior_data_list().size())
			{
				std::vector<test_ik> temp_iks;
				for (auto& ik_data : ik_resource->get_behavior_data_list())
				{
					test_ik& data = temp_iks.emplace_back();

					//	同名登録判定
					auto it = std::find_if(test_behavior_iks.begin(),
										test_behavior_iks.end(),
										[&](const test_ik& r)
										{
											return ik_data.name == r.data_nama;
										});
					if (it != test_behavior_iks.end())
					{
						data = *it;
					}
					else
					{
						//	新しく追加
						data.data_nama = ik_data.name;
						data.test_flag = false;
					}
				}
				test_behavior_iks = temp_iks;
				ik_solver = std::make_shared<::ik_solver>(model_instance, ik_resource);
			}
			if (ImGui::TreeNode("behavior test"))
			{
				for (auto& test_ik : test_behavior_iks)
				{
					ImGui::PushID(context->get_item_id());
					ImGui::BeginDisabled();
					ImGui::InputTextUtf8("", test_ik.data_nama);
					ImGui::EndDisabled();
					ImGui::Checkbox("test_flag", &test_ik.test_flag);
					ImGui::DragFloat3("position", &test_ik.position.x, 0.01f);
					ImGui::SliderAngle3("rotation", &test_ik.rotation.x);
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		bool edit_limb(ik_resource::solve_data& ik_data, std::shared_ptr<editor_items::context>& context)
		{
			bool edited = false;
			ImGui::PushID(context->get_item_id());
			ImGui::Separator();

			auto& instance_node_list = model_instance->get_nodes();
			bool use_pole = ik_data.node_list.size() == 3;
			if (use_pole)
			{
				if (ImGui::Button("disable pole node"))
					ik_data.node_list.resize(2);
			}
			else
			{
				if (ImGui::Button("use pole node"))
					ik_data.node_list.resize(3);
			}
			edited |= ImGui::IsItemEdited();
			auto root_name = search_ik_node_name(ik_data.node_list[0]);
			auto middle_name = search_ik_node_name(ik_data.node_list[1]);
			auto end_name = search_ik_node_name(ik_data.target_node_index);
			if (ImGui::Combo("root node", root_name, model_bone_names))
			{
				ik_data.node_list[0] = search_ik_node(root_name);
			}
			edited |= ImGui::IsItemEdited();
			if (ImGui::Combo("middle node", middle_name, model_bone_names))
			{
				ik_data.node_list[1] = search_ik_node(middle_name);
			}
			edited |= ImGui::IsItemEdited();
			if (ImGui::Combo("end node", end_name, model_bone_names))
			{
				ik_data.target_node_index = search_ik_node(end_name);
			}
			edited |= ImGui::IsItemEdited();
			if (use_pole)
			{
				if (ImGui::Button(ik_data.use_pole_node_offset ? "use node" : "use offset"))
				{
					ik_data.use_pole_node_offset = !ik_data.use_pole_node_offset;
				}

				if (ik_data.use_pole_node_offset)
				{
					ImGui::DragFloat3("pole offset", &ik_data.pole_offset.x, 0.1f);
				}
				else
				{
					auto pole_name = search_ik_node_name(ik_data.node_list[2]);
					if (ImGui::Combo("pole node", pole_name, model_bone_names))
					{
						ik_data.node_list[2] = search_ik_node(pole_name);
					}
				}
				edited |= ImGui::IsItemEdited();
			}

			ImGui::PopID();

			return edited;
		}
		void edit_link_node(ik_resource::node& node, bool& edited)
		{
			ImGui::Combo<ik_resource::constraint_mode>("mode", node.mode);
			edited |= ImGui::IsItemEdited();
			switch (node.mode)
			{
				case ik_resource::constraint_mode::hinge:
				{
					ImGui::DragAngle2("hinge_limit", node.limit_angles);
					edited |= ImGui::IsItemEdited();
					ImGui::Combo<ik_resource::axis>("local axis", node.use_axis);
					edited |= ImGui::IsItemEdited();
					break;
				}
				case ik_resource::constraint_mode::ball_joint:
				{
					ImGui::DragAngle2("twist_limit", &node.limit_angles[0]);
					edited |= ImGui::IsItemEdited();
					ImGui::DragAngle("swing_max", &node.limit_angles[2]);
					edited |= ImGui::IsItemEdited();
					ImGui::Combo<ik_resource::axis>("local axis", node.use_axis);
					edited |= ImGui::IsItemEdited();
					break;
				}
				case ik_resource::constraint_mode::euler:
				{
					ImGui::DragAngle3("angle_min", &node.limit_angles[0]);
					edited |= ImGui::IsItemEdited();
					ImGui::DragAngle3("angle_max", &node.limit_angles[3]);
					edited |= ImGui::IsItemEdited();
					break;
				}
			}
			ImGui::Separator();
		}
		bool edit_ccd(ik_resource::solve_data& ik_data, std::shared_ptr<editor_items::context>& context)
		{
			bool edited = false;
			ImGui::DragUInt("iteration", &ik_data.iteration, 1, 0, 1000);
			edited |= ImGui::IsItemEdited();
			ImGui::DragAngle("loop_angle_limit", &ik_data.constraint_angle);
			edited |= ImGui::IsItemEdited();

			//	終端ノード名
			{
				auto target_node_name = search_ik_node_name(ik_data.target_node_index);
				if (ImGui::Combo("target_node", target_node_name, model_bone_names))
				{
					ik_data.target_node_index = search_ik_node(target_node_name);
				}
				edited |= ImGui::IsItemEdited();
			}

			//	IKリンク情報
			auto& ik_node_list = ik_resource->get_node_list_for_editor();
			if (ImGui::TreeNode("ik link"))
			{
				UINT count = static_cast<UINT>(ik_data.node_list.size());
				if (ImGui::DragUInt("link size", &count, 0.1f, 0, 1000))
				{
					ik_data.node_list.resize(count);
				}
				edited |= ImGui::IsItemEdited();
				ImGui::Separator();
				int element_count = 0;
				for (auto& ik_link : ik_data.node_list)
				{
					ImGui::PushID(context->get_item_id());

					//	ノード名
					{
						std::string label = std::to_string(element_count++);
						auto link_node_name = search_ik_node_name(ik_link);
						if (ImGui::Combo(label.data(), link_node_name, model_bone_names))
						{
							ik_link = search_ik_node(link_node_name);
						}
						edited |= ImGui::IsItemEdited();
					}

					//	ノードの制限情報
					edit_link_node(ik_node_list[ik_link], edited);
					ImGui::PopID();
				}
				ImGui::TreePop();
			}

			return edited;
		}

		bool edit_aim(ik_resource::solve_data& ik_data, std::shared_ptr<editor_items::context>& context)
		{
			bool edited = false;
			if (ik_data.node_list.empty())
			{
				ik_data.node_list.resize(1);
			}

			//	IKリンク情報
			ImGui::PushID(context->get_item_id());

			//	ローカル方向
			{
				if (ImGui::SliderFloat3("local_direction", &ik_data.aim_axis.x, -1.0f, 1.0f))
				{
					float q = ik_data.aim_axis.x * ik_data.aim_axis.x + ik_data.aim_axis.y * ik_data.aim_axis.y + ik_data.aim_axis.z * ik_data.aim_axis.z;
					if (q > 1e-6f)
					{
						q = sqrtf(q);
						ik_data.aim_axis.x /= q;
						ik_data.aim_axis.y /= q;
						ik_data.aim_axis.z /= q;
					}
				}
				edited |= ImGui::IsItemEdited();
				ImGui::DragFloat3("local_offset", &ik_data.aim_offset.x, 0.01f);
				edited |= ImGui::IsItemEdited();
			}

			auto& ik_link = ik_data.node_list[0];
			auto& ik_node_list = ik_resource->get_node_list_for_editor();
			//	ノード名
			{
				auto link_node_name = search_ik_node_name(ik_link);
				if (ImGui::Combo("", link_node_name, model_bone_names))
				{
					ik_link = search_ik_node(link_node_name);
				}
				edited |= ImGui::IsItemEdited();
			}

			//	ノードの制限情報
			edit_link_node(ik_node_list[ik_link], edited);
			ImGui::PopID();

			return edited;
		}

		void render_solver_bones()
		{
			for (auto& test_ik : test_solve_iks)
			{
				if (!test_ik.test_flag)
					continue;

				auto index = ik_solver->search_index_by_name(test_ik.data_nama);
				if (index == ik_resource::invalid_id)
					continue;

				//	オフセット座標表示
				auto& ik_nodes = ik_resource->get_node_list();
				auto& ik_data = ik_solver->get_solve_data()[index];
				if (ik_data.mode == ik_resource::solve_mode::limb &&
					ik_data.use_pole_node_offset &&
					ik_data.node_list.size() == 3 &&
					ik_data.node_list[2] != ik_resource::invalid_id)
				{
					size_t index = model_instance->get_node_index_by_name(ik_nodes[ik_data.node_list[1]].source_node_name);
					if (index != model_resource::invalid_id)
					{
						auto& middle_node = model_instance->get_nodes()[index];
						const math::vector3f global_position = math::vector3f::transform_coord(ik_data.pole_offset, middle_node.global_transform);
						QUEUE_DEBUG_PRIMITIVE_SPHERE(*engine, global_position.to_float3(), 0.1f, math::float4(1, 1, 1, 1), true);
					}
				}

				ik_solver->solve(index, test_ik.position);

				//	ポイント座標表示
				QUEUE_DEBUG_PRIMITIVE_SPHERE(*engine, test_ik.position, 0.1f, math::float4(1, 1, 1, 1), true);
			}
		}

		void render_behavior_bones()
		{
			for (UINT index = 0; index < test_behavior_iks.size(); ++index)
			{
				auto& test_ik = test_behavior_iks[index];
				if (!test_ik.test_flag)
					continue;

				auto& behavior_data = ik_resource->get_behavior_data_list()[index];

				switch (behavior_data.mode)
				{
					case ::ik_resource::behavior_mode::foot:
					{
						if (behavior_data.solve_ids.empty() ||
							behavior_data.solve_ids.size() < 2 ||
							behavior_data.solve_ids[0] == ik_resource::invalid_id ||
							behavior_data.solve_ids[1] == ik_resource::invalid_id)
							continue;

						math::float3 s = {10, 10, 10};
						const math::float4 q = math::quaternionf::rotation_xyz(test_ik.rotation.x, test_ik.rotation.y, test_ik.rotation.z).to_float4();
						math::float3 t = test_ik.position;

						collision::bounding_box box{math::double3(s), math::double4(q), math::double3(t)};
						auto& model_instance_nodes = model_instance->get_nodes();
						{
							auto& left_solve_data = ik_solver->get_solve_data()[behavior_data.solve_ids[0]];
							auto& right_solve_data = ik_solver->get_solve_data()[behavior_data.solve_ids[1]];
							auto& left_node_data = ik_resource->get_node_list()[left_solve_data.target_node_index];
							auto& right_node_data = ik_resource->get_node_list()[right_solve_data.target_node_index];
							auto& left_node = model_instance_nodes[model_instance->get_node_index_by_name(left_node_data.source_node_name)];
							auto& right_node = model_instance_nodes[model_instance->get_node_index_by_name(right_node_data.source_node_name)];

							//	
							auto& foot_data = std::get<ik_resource::foot_data>(behavior_data.data);
							const math::matrixf world_matrix(model_instance->get_world_transform());
							const math::float4x4 left_world_matrix = (math::matrixf(left_node.global_transform) * world_matrix).to_float4x4();
							const math::float4x4 right_world_matrix = (math::matrixf(right_node.global_transform) * world_matrix).to_float4x4();

							//	各足が埋まるかを確認
							collision::intersect_data left_hit_data, right_hit_data;
							bool left_hit, right_hit;
							{
								math::float3 left_start, right_start, left_end, right_end;

								left_start = math::float3(left_world_matrix._41,
															   left_world_matrix._42 + foot_data.ray_origin_offsets[0],
															   left_world_matrix._43);
								right_start = math::float3(right_world_matrix._41,
															    right_world_matrix._42 + foot_data.ray_origin_offsets[1],
															    right_world_matrix._43);

								left_end = math::float3(left_world_matrix._41,
															 left_world_matrix._42 - (foot_data.ray_origin_offsets[0] + foot_data.ray_length),
														     left_world_matrix._43);
								right_end = math::float3(right_world_matrix._41,
															  right_world_matrix._42 - (foot_data.ray_origin_offsets[1] + foot_data.ray_length),
															  right_world_matrix._43);

								left_hit = box.intersect_segment(math::double3(left_start), math::double3(left_end), &left_hit_data);
								right_hit = box.intersect_segment(math::double3(right_start), math::double3(right_end), &right_hit_data);
							}

							//	腰位置を下げる
							{
								float length = FLT_MIN;
								if (left_hit)
									length = foot_data.ray_length - static_cast<float>(left_hit_data[0].length);
								if (right_hit)
									length = std::max(length, foot_data.ray_length - static_cast<float>(right_hit_data[0].length));

								const math::vector3f pelvis_offset(0, -length, 0);
								for (auto& node_index : foot_data.adjust_node_indices)
								{
									auto& pelvis_node_data = ik_resource->get_node_list()[node_index];
									auto& pelvis_node = model_instance_nodes[model_instance->get_node_index_by_name(pelvis_node_data.source_node_name)];

									const math::matrixf inverse_pelvis_world_matrix = (math::matrixf(pelvis_node.global_transform) * world_matrix).inverted();
									const math::vector3f local_direction = math::vector3f::transform_normal(pelvis_offset, inverse_pelvis_world_matrix);
									pelvis_node.local_transform._41 += local_direction.x;
									pelvis_node.local_transform._42 += local_direction.y;
									pelvis_node.local_transform._43 += local_direction.z;
								}
								model_instance->update_node_transforms();
							}

							//	ヒット位置に足を補正
							if (left_hit)
							{
								left_hit_data[0].point.y -= foot_data.foot_offset;
								ik_solver->solve(behavior_data.solve_ids[0], math::float3(left_hit_data[0].point));
							}
							if (right_hit)
							{
								right_hit_data[0].point.y -= foot_data.foot_offset;
								ik_solver->solve(behavior_data.solve_ids[1], math::float3(right_hit_data[0].point));
							}
						}

						QUEUE_DEBUG_PRIMITIVE_BOX(*engine, t, q, s, math::float4(1, 1, 1, 1), true);
						break;
					}
					case ::ik_resource::behavior_mode::lookat:
					{
						for (auto& solve_id : behavior_data.solve_ids)
						{
							ik_solver->solve(solve_id, test_ik.position);
						}
						QUEUE_DEBUG_PRIMITIVE_SPHERE(*engine, test_ik.position, 0.1f, math::float4(1, 1, 1, 1), true);
						break;
					}
				}
			}
		}

	private:
		model_instance_ptr model_instance;
		engine_context* engine = nullptr;
		bool display_bone = true;
		ik_resource_ptr ik_resource;
		ik_solver_ptr ik_solver;
		std::vector<std::string> model_bone_names;
		std::vector<std::string> solver_names;
		std::vector<std::string_view> selected_bone_names;

		struct test_ik
		{
			std::string data_nama;
			bool test_flag = false;
			math::float3 rotation = {0, 0, 0};
			math::float3 position = {0, 0, 0};
		};
		std::vector<test_ik> test_solve_iks;
		std::vector<test_ik> test_behavior_iks;
	};
}

bool editor_items::edit_ik(std::shared_ptr<context>& context, engine_context& engine, model_instance_ptr& model_instance, ik_resource_ptr& ik_resource)
{
	{
		if (!context->ik_editor)
			context->ik_editor = std::make_unique<ik_resource_editor>();

		ik_resource_editor* editor = static_cast<ik_resource_editor*>(context->ik_editor.get());
		editor->set_data(engine, model_instance, ik_resource);
		editor->render(context);
	}
	return true;
}

#endif	//	defined(USE_IMGUI)


