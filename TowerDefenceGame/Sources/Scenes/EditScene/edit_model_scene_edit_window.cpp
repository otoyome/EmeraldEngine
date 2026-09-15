#include "scenes/editscene/edit_model_scene.h"

#include "model_instance.h"
#include "camera.h"

#include "object/light_object.h"

#include "object/model_object.h"

#include "custom_pipeline_state.h"

#if defined(USE_IMGUI)
#include "Editor/editor_camera.h"

namespace
{
	//	モデル
	constexpr TCHAR* ModelFileDialogFilter =
		L"モデルファイル (*.emo, *.pmx, *.gltf, *.glb)\0*.emo;*.pmx;*.gltf;*.glb;\0\
		すべてのファイル (*.*)\0*.*\0";
}

bool edit_model_scene::initialize_edit_core()
{
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

	if (!initialize_vertex_color_edit())
		return false;

	if (!initialize_animation())
		return false;

	return true;
}

bool edit_model_scene::uninitialize_edit_core()
{
	if (!uninitialize_vertex_color_edit())
		return false;

	if (!uninitialize_animation())
		return false;

	return true;
}

void edit_model_scene::update_edit_core(float elapsed_time)
{
	update_vertex_color_edit();

	update_animation();
}

void edit_model_scene::imgui_render_edit_core()
{
	if (!edit_model)
		return;

	//	メッシュごとに表示を切り替えたいとき用ウィンドウ
	imgui_render_mesh_list();

	//	マテリアル編集系ウィンドウ
	imgui_render_material_window();

	//	頂点カラー編集系ウィンドウ
	imgui_render_vertex_color_edit();

	//	アニメーション編集系ウィンドウ
	imgui_render_animation();

	//	モーフィング編集系ウィンドウ
	imgui_render_morph();

	//	リターゲッティング編集系ウィンドウ
	imgui_retarget_window();

	//	IK編集系ウィンドウ
	imgui_render_ik();
}

void edit_model_scene::imgui_render_mesh_list()
{
	if (ImGui::Begin("Mesh List Window"))
	{
		int push_id = 0xF0D37cfc;
		for (auto& mesh : edit_model_meshes)
		{
			auto& node = edit_model->get_model_instance()->get_nodes()[mesh.node_index];
			std::string id = node.name + std::to_string(push_id++);
			ImGui::PushID(id.data());
			if (ImGui::Checkbox("", &mesh.visible))
			{
				for (auto& md : mesh.meshes)
				{
					md->visible = mesh.visible;
				}
			}
			ImGui::SameLine();
			ImGui::Text(node.name.data());
			ImGui::PopID();
		}
	}
	ImGui::End();
}

void edit_model_scene::imgui_render_morph()
{
	if (!edit_model)
		return;

	if (ImGui::Begin("Morph Window"))
	{
		int edeit_id = 0x0FF1F38F;
		auto& morphs = edit_model->get_model_instance()->get_resource()->get_morphes_for_editor();

		//	リスト調整
		bool edit = false;
		int morph_list_size = morphs.size();
		ImGui::DragInt("List Size", &morph_list_size, 1);
		if (morph_list_size != morphs.size())
		{
			morphs.resize(morph_list_size);
			morphs.shrink_to_fit();
			edit = true;
		}
		ImGui::Separator();

		//	モーフ名リスト生成
		std::vector<std::string> mesh_names;
		for (auto& mesh : edit_model_meshes)
		{
			if (mesh.morphs.empty())
				continue;

			auto& node = edit_model->get_model_instance()->get_nodes()[mesh.node_index];
			for (auto& m : mesh.meshes)
			{
				for (auto& morph : m->mesh->morph_indices)
				{
					auto it = std::find_if(mesh_names.begin(),
										   mesh_names.end(),
										   [&](std::string& a){ return a == morph.first; });
					if (it != mesh_names.end())
						continue;

					mesh_names.emplace_back(morph.first);
				}
			}
		}

		for (auto& morph : morphs)
		{
			ImGui::PushID(edeit_id++);
			//if (ImGui::TreeNode(morph.name.c_str()))
			{
				ImGui::InputText("Name", morph.name);
				{
					auto& morphes = edit_model->get_model_instance()->get_morphes();
					auto it = std::find_if(morphes.begin(), morphes.end(),
						[&](model_instance::morph& m){ return m.morph->name == morph.name; });
					if (it != morphes.end())
					{
						ImGui::SliderFloat("weight", &it->weight, 0, 1);
					}
					else
					{
						ImGui::BeginDisabled();
						static float a = 0;
						ImGui::SliderFloat("weight", &a, 0, 1);
						ImGui::EndDisabled();
					}
				}

				//	リスト調整
				int list_size = morph.parts.size();
				ImGui::DragInt("List Size", &list_size, 1);
				if (list_size != morph.parts.size())
				{
					morph.parts.resize(list_size);
					morph.parts.shrink_to_fit();
					edit = true;
				}

				for (auto& morph_mesh : morph.parts)
				{
					ImGui::PushID(edeit_id++);
					ImGui::Separator();
					if (ImGui::Combo("Select Morph Mesh", morph_mesh.mesh_name, mesh_names))
						edit = true;
					ImGui::PopID();
				}
				ImGui::Separator();
			}
			ImGui::PopID();
		}

		if (edit)
		{
			auto& morphes = edit_model->get_model_instance()->get_morphes();
			morphes.clear();
			for (UINT index = 0; index < static_cast<UINT>(edit_model->get_model_instance()->get_resource()->get_morphes().size()); ++index)
			{
				auto& morph = morphes.emplace_back();
				morph.morph = &edit_model->get_model_instance()->get_resource()->get_morphes()[index];
				morph.weight = 0;
			}
		}
	}
	ImGui::End();
}

void edit_model_scene::imgui_render_material_window()
{
	if (ImGui::Begin("Material Window"))
	{
		for (auto& material : edit_model->get_model_instance()->get_materials())
		{
			if (ImGui::TreeNode(system_utility::convert_utf8(material.get_name()).data()))
			{
				editor_items::edit_material(editor_context, material);
				ImGui::TreePop();
			}
		}
	}
	ImGui::End();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void edit_model_scene::imgui_model_loader_window()
{
#pragma region 読込みウィンドウ
	if (!load_model_path.empty())
	{
		std::filesystem::path path(load_model_path);
		std::filesystem::path extension = path.extension();

		if (extension == std::filesystem::path(".pmx"))
		{
			convert_option_data.kind = convert_option_data::kinds::mmd;
		}
		else if (extension == std::filesystem::path(".gltf") || extension == std::filesystem::path(".glb"))
		{
			bool show_flag = !load_model_path.empty();
			if (ImGui::Begin("Convert Gltf Data", &show_flag))
			{
				ImGui::Checkbox("compute_tangent", &convert_option_data.gltf.is_compute_tangent);
				ImGui::Checkbox("populate_vertex_color", &convert_option_data.gltf.is_populate_vertex_color);
				ImGui::Checkbox("normalize_weight", &convert_option_data.gltf.is_normalize_weight);

				ImGui::Checkbox("change_coordinate_system", &convert_option_data.gltf.is_change_coordinate_system);
				ImGui::Checkbox("change_coordinate_system_indices", &convert_option_data.gltf.is_change_coordinate_system_indices);
				ImGui::Separator();
				ImGui::Checkbox("convert_texture_to_dds", &convert_option_data.gltf.is_convert_texture_to_dds);

				//	[エンジン改造:接線に再計算法線を含める対応]
				ImGui::Separator();
				ImGui::Checkbox("recomputed_normal", &convert_option_data.gltf.is_recomputed_normal);
				ImGui::Checkbox("recomputed_normal_to_copy_tangent", &convert_option_data.gltf.is_recomputed_normal_to_copy_tangent);
				//	[エンジン改造:接線に再計算法線を含める対応]

				if (ImGui::Button("Convert"))
				{
					convert_option_data.kind = convert_option_data::kinds::gltf;
				}
			}
			ImGui::End();
			//	閉じられたらクリアしておく
			if (!show_flag)
			{
				load_model_path.clear();
			}
		}
		else if (extension == std::filesystem::path("." + std::string(model_resource::extension)))
		{
			convert_option_data.kind = convert_option_data::kinds::emo;
		}
		else
		{
			assert(!L"Compatible Format Model.");
			load_model_path.clear();
		}
	}
#pragma endregion // 読込みウィンドウ

	//	モデル読込み関係
	{
		ImGui::BeginDisabled(!load_model_path.empty());

		ImGui::Text("Model ");
		//	モデル読み込み
		ImGui::SameLine();
		if (ImGui::Button("Load"))
		{
			std::string file_path;
			if (editor_items::open_file_dialog(ModelFileDialogFilter, file_path))
			{
				load_model_path = file_path;
				is_edit_vertex_color = false;
			}
		}

		//	モデル保存
		if (edit_model)
		{
			ImGui::SameLine();
			if (ImGui::Button("Save"))
			{
				save_model();
			}
		}

		ImGui::EndDisabled();

		//	LOD 生成
		if (edit_model)
		{
			ImGui::Separator();
			ImGui::Text("LOD");
			ImGui::SliderInt("lod_level", &generate_lod_level, 1, 8);
			if (ImGui::Button("Generate LOD"))
			{
				generate_lod();
			}
			ImGui::SameLine();
			if (ImGui::Button("Discard LOD"))
			{
				discard_lod();
			}

			int lod_max = edit_model->get_model_instance()->get_lod_max();
			if (lod_max > 0)
			{
				int lod_index = edit_model->get_model_instance()->get_lod_index();
				if (ImGui::SliderInt("preview_lod", &lod_index, 0, lod_max))
				{
					edit_model->get_model_instance()->set_lod_index(lod_index);
				}
			}
		}

		if (edit_model)
		{
			ImGui::Separator();
			ImGui::Text("Shadow Mesh");
			if (ImGui::Button("Generate Shadow Mesh"))
			{
				generate_shadow_mesh_asset();
			}
			ImGui::SameLine();
			if (ImGui::Button("Discard Shadow Mesh"))
			{
				discard_shadow_mesh();
			}
		}

		if (edit_model)
		{
			ImGui::Separator();
			ImGui::Text("Mesh");
			if (ImGui::Button("Optimize Mesh"))
			{
				optimize_mesh_asset();
			}
		}

		//	スキンなしメッシュの空間グリッド結合
		if (edit_model)
		{
			ImGui::Separator();
			ImGui::Text("Combine Meshes");
			ImGui::DragInt3("grid_divisions", &combine_grid_divisions.x, 0.1f, 1, 256);
			combine_grid_divisions.x = std::max(1, combine_grid_divisions.x);
			combine_grid_divisions.y = std::max(1, combine_grid_divisions.y);
			combine_grid_divisions.z = std::max(1, combine_grid_divisions.z);
			if (ImGui::Button("Combine Meshes"))
			{
				combine_meshes();
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(!has_combine_backup);
			if (ImGui::Button("Clear Combine"))
			{
				clear_combined_meshes();
			}
			ImGui::EndDisabled();
		}

		//	当たり判定メッシュ生成
		if (edit_model)
		{
			ImGui::Separator();
			ImGui::Text("Bounding Mesh");
			ImGui::SliderInt("separate_level", &bounding_mesh_option.separate_level, 1, 8);
			ImGui::Checkbox("cull_reverse_flag", &bounding_mesh_option.reverse_flag);
			ImGui::Checkbox("use_lod", &bounding_mesh_option.use_lod);
			if (bounding_mesh_option.use_lod)
			{
				int lod_max = edit_model->get_model_instance()->get_lod_max();
				ImGui::BeginDisabled(lod_max <= 0);
				ImGui::SliderInt("bm_lod_level", &bounding_mesh_option.lod_level, 0, lod_max > 0 ? lod_max - 1 : 0);
				ImGui::EndDisabled();
			}
			if (ImGui::Button("Generate Bounding Mesh"))
			{
				generate_bounding_mesh_asset();
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//	頂点カラー編集系
bool edit_model_scene::initialize_vertex_color_edit()
{
	//	頂点表示マテリアル
	if (pipeline_state_manager* instance = pipeline_state_manager::instance())
	{
		auto mat = std::make_shared<material>();
		mat->set_domain(material::domains::surface);
		mat->set_blend_mode(material::blend_modes::translucent);
		mat->set_alpha_clip_threshold(-1.0f);
		mat->set_shading_model(material::shading_models::unlit);
		mat->set_flag(material::flags::wire, true);

		mat->set_pipeline_state(material::surface_paths::depth_write_pass, "edit_model_color_depth_write");
		mat->set_pipeline_state(material::surface_paths::g_buffer_pass, "edit_model_color_gbuffer");
		mat->set_pipeline_state(material::surface_paths::forward_pass, "edit_model_color_forward");
		mat->set_pipeline_state(material::surface_paths::shadow_write_pass, "edit_model_color_csm");
		mat->set_float("line_factor", 0.001f);
		mat->set_float3("line_color", { 0, 1, 0 });
		vertex_color_edit_material = mat;
	}

	//	マウス購読
	{
		static constexpr std::string_view keys[] =
		{
			editor_camera::left_click_down_identifier,
			editor_camera::left_click_hold_identifier,
		};
		for (int index = 0; index < ARRAYSIZE(keys); ++index)
		{
			left_click_data_identifier[index] = object_manager::instance()->get_messenger().add_receiver(
				keys[index],
				[this](void* data_ptr) { on_mouse_left_down_and_hold(data_ptr); });
		}

		left_click_data_identifier[2] = object_manager::instance()->get_messenger().add_receiver(
			editor_camera::left_click_release_identifier,
			[this](void* data_ptr) { on_mouse_left_release(data_ptr); });
	}
	return true;
}

bool edit_model_scene::uninitialize_vertex_color_edit()
{
	//	左クリック登録解除
	{
		for (auto& id : left_click_data_identifier)
		{
			object_manager::instance()->get_messenger().remove_receiver(id);
		}
	}
	return true;
}

void edit_model_scene::update_vertex_color_edit()
{
	//	頂点編集時の座標表示
	if (brush_data.on)
	{
		QUEUE_DEBUG_PRIMITIVE_SPHERE(brush_data.position, brush_data.radius, DirectX::XMFLOAT4(1, 1, 1, 1), true);
	}
}

void edit_model_scene::imgui_render_vertex_color_edit()
{
	if (ImGui::Begin("Edit Vertex Color Window"))
	{
		if (ImGui::Button("Edit Vertex Color Mode"))
		{
			is_edit_vertex_color = !is_edit_vertex_color;
			if (vertex_color_edit_model)
				vertex_color_edit_model->set_active(is_edit_vertex_color);
		}
		ImGui::SliderFloat("radius", &brush_data.radius, 0.01f, 10.0f);
		ImGui::PushID("## vertex edit colors");
		ImGui::Checkbox("shading threshold offset", reinterpret_cast<bool*>(&brush_data.color_flags.x));
		ImGui::Checkbox("outline depth expansion factor", reinterpret_cast<bool*>(&brush_data.color_flags.y));
		ImGui::Checkbox("outline depth offet", reinterpret_cast<bool*>(&brush_data.color_flags.z));
		ImGui::Checkbox("outline scale factor", reinterpret_cast<bool*>(&brush_data.color_flags.w));
		ImGui::PopID();

		ImGui::SliderFloat("shading threshold offset", &brush_data.color.x, 0.0f, 1.0f);
		ImGui::SliderFloat("outline depth expansion factor", &brush_data.color.y, 0.0f, 1.0f);
		ImGui::SliderFloat("outline depth offet", &brush_data.color.z, 0.0f, 1.0f);
		ImGui::SliderFloat("outline scale factor", &brush_data.color.w, 0.0f, 1.0f);
	}
	ImGui::End();
}

void edit_model_scene::on_mouse_left_down_and_hold(void* data_ptr)
{
	if (!edit_model || !is_edit_vertex_color)
		return;

	auto data = static_cast<editor_camera::mouse_position_data*>(data_ptr);

	auto View = DirectX::XMLoadFloat4x4(&main_camera->get_camera()->get_view_matrix());
	auto Projection = DirectX::XMLoadFloat4x4(&main_camera->get_camera()->get_projection_matrix());

	DirectX::XMFLOAT3 start_position, end_position;
	DirectX::XMStoreFloat3(&start_position,
		DirectX::XMVector3Unproject(DirectX::XMVectorSet(data->position.x, data->position.y, 0, 1),
			0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1,
			Projection, View, DirectX::XMMatrixIdentity()));
	DirectX::XMStoreFloat3(&end_position,
		DirectX::XMVector3Unproject(DirectX::XMVectorSet(data->position.x, data->position.y, 1, 1),
			0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1,
			Projection, View, DirectX::XMMatrixIdentity()));

	//	ヒット位置算出
	float vertex_hit_length = FLT_MAX;
	for (auto& instance_mesh : edit_model->get_model_instance()->get_meshes())
	{
		if (!instance_mesh.mesh || !instance_mesh.visible)
			continue;

		auto& mesh = *instance_mesh.mesh;
		auto& positions = mesh.vertex_buffer_views.find(semantic("POSITION", 0));
		auto& indiceds = mesh.index_buffer_view;
		if (positions == mesh.vertex_buffer_views.end() || indiceds.count() == 0)
			continue;

		collision::triangles triangles = {};
		triangles.positions = reinterpret_cast<const DirectX::XMFLOAT3*>(positions->second.row_data.data());
		triangles.indices = reinterpret_cast<const UINT*>(indiceds.row_data.data());
		triangles.index_start = indiceds.offset();
		triangles.index_count = indiceds.count();
		triangles.reverse_index = false;
		triangles.indices_stride_16 = indiceds.stride_in_bytes == 2;
		collision::intersect_data result;
		if (collision::ray_cast(start_position, end_position, triangles, &result))
		{
			if (result[0].length < vertex_hit_length)
			{
				brush_data.position = result[0].point;
				vertex_hit_length = result[0].length;
			}
		}
	}

	//	ヒット位置周辺の頂点色を修正
	if (vertex_hit_length < FLT_MAX)
	{
		brush_data.on = true;
		for (auto& instance_mesh : edit_model->get_model_instance()->get_meshes())
		{
			if (!instance_mesh.mesh || !instance_mesh.visible)
				continue;

			auto& mesh = *const_cast<model_resource::mesh*>(instance_mesh.mesh);
			auto& position_view = mesh.vertex_buffer_views.find(semantic("POSITION", 0));
			auto& color_view = mesh.vertex_buffer_views.find(semantic("COLOR", 0));
			if (position_view == mesh.vertex_buffer_views.end() || color_view == mesh.vertex_buffer_views.end())
				continue;

			bool is_change = false;
			auto positions = reinterpret_cast<DirectX::XMFLOAT3*>(position_view->second.row_data.data());
			auto colors = reinterpret_cast<DirectX::XMFLOAT4*>(color_view->second.row_data.data());
			for (size_t index = position_view->second.offset(); index < position_view->second.count(); ++index)
			{
				auto p = positions + index;
				DirectX::XMFLOAT3 d;
				d.x = p->x - brush_data.position.x;
				d.y = p->y - brush_data.position.y;
				d.z = p->z - brush_data.position.z;
				if (d.x * d.x + d.y * d.y + d.z * d.z < brush_data.radius * brush_data.radius)
				{
					colors[index].x = math::lerp(colors[index].x, brush_data.color.x, static_cast<float>(brush_data.color_flags.x));
					colors[index].y = math::lerp(colors[index].y, brush_data.color.y, static_cast<float>(brush_data.color_flags.y));
					colors[index].z = math::lerp(colors[index].z, brush_data.color.z, static_cast<float>(brush_data.color_flags.z));
					colors[index].w = math::lerp(colors[index].w, brush_data.color.w, static_cast<float>(brush_data.color_flags.w));
					is_change = true;
				}
			}
			if (is_change)
			{
				auto device_context = render_data::instance()->get_immediate_context();
				device_context->UpdateSubresource(color_view->second.buffer.Get(), 0, nullptr, colors, 0, 0);
			}
		}
	}
}

void edit_model_scene::on_mouse_left_release(void* data_ptr)
{
	brush_data.on = false;
}

void edit_model_scene::on_model_release()
{
	selected_bones.clear();
	model_bone_names.clear();
	edit_model_meshes.clear();
	if (vertex_color_edit_model)
	{
		vertex_color_edit_model->set_active(false);
		object_manager::instance()->unregister_object(vertex_color_edit_model);
		vertex_color_edit_model = nullptr;
	}

	if (edit_model)
	{
		edit_model->set_active(false);
		object_manager::instance()->unregister_object(edit_model);
		edit_model = nullptr;
	}
}

void edit_model_scene::on_model_setup()
{
	if (!edit_model)
		return;

	edit_model->set_active(true);
	object_manager::instance()->register_object(edit_model);

	//	モデルのファイルパスを取得
	std::string file_path = std::string(edit_model->get_model_instance()->get_resource()->get_file_path());

	//	ボーン名前リストを生成
	{
		model_bone_names.emplace_back("[BLANK]");
		auto& node_list = edit_model->get_model_instance()->get_nodes();
		auto& mesh_list = edit_model->get_model_instance()->get_meshes();
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
	}

	//	ポイントモデルを生成表示
	{
		vertex_color_edit_model = std::make_shared<model_object>("point_model", edit_model->get_model_instance()->get_resource());
		for (UINT index = 0; index < vertex_color_edit_model->get_model_instance()->get_materials().size(); ++index)
		{
			vertex_color_edit_model->get_model_instance()->get_materials()[index] = *vertex_color_edit_material;
		}
		vertex_color_edit_model->set_display_inspector(false);
		vertex_color_edit_model->set_active(is_edit_vertex_color);
		object_manager::instance()->register_object(vertex_color_edit_model);
	}

	//	編集モデルのメッシュリストを収集
	{
		auto func = [&](std::shared_ptr<model_object>& model)
		{
			if (!model)
				return;

			for (auto& instance_mesh : model->get_model_instance()->get_meshes())
			{
				bool find = false;
				for (auto& mesh : edit_model_meshes)
				{
					if (mesh.node_index == instance_mesh.mesh->node_index)
					{
						mesh.meshes.push_back(&instance_mesh);
						for (auto& morph_index : instance_mesh.mesh->morph_indices)
						{
							auto it = std::find(mesh.morphs.begin(), mesh.morphs.end(), morph_index.first);
							if (it != mesh.morphs.end())
								continue;

							mesh.morphs.emplace_back(morph_index.first);
						}
						find = true;
						break;
					}
				}
				if (!find)
				{
					auto& mesh = edit_model_meshes.emplace_back();
					mesh.meshes.push_back(&instance_mesh);
					mesh.node_index = instance_mesh.mesh->node_index;
					mesh.visible = instance_mesh.visible;
					for (auto& morph_index : instance_mesh.mesh->morph_indices)
					{
						auto it = std::find(mesh.morphs.begin(), mesh.morphs.end(), morph_index.first);
						if (it != mesh.morphs.end())
							continue;

						mesh.morphs.emplace_back(morph_index.first);
					}
				}
			}
		};
		edit_model_meshes.reserve(edit_model->get_model_instance()->get_meshes().size());
		func(edit_model);
		func(vertex_color_edit_model);
	}

	//	リターゲット系の情報を生成
	if (!retarget_data.has_profile(file_path))
	{
		retarget_profile profile;
		profile.bone_map.clear();
		auto& node_list = edit_model->get_model_instance()->get_nodes();
		auto& mesh_list = edit_model->get_model_instance()->get_meshes();
		for (auto& mesh : mesh_list)
		{
			for (auto& bone : mesh.mesh->bones)
			{
				auto& bone_list = profile.bone_map;

				auto node_name = node_list[bone.node_index].name;
				if (bone_list.find(node_name) == bone_list.end())
				{
					retarget_profile::bone_data data;
					data.target_name = "";
					profile.bone_map.insert(std::make_pair(node_name, data));
				}
			}
		}
		std::vector<retarget_profile> vector;
		vector.push_back(profile);
		retarget_data.profiles.insert(std::make_pair(file_path, vector));
	}
}

#endif	//	defined(USE_IMGUI)
