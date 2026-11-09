#include "scenes/editscene/edit_model_scene.h"
#include "Object/engine_context.h"

#include "Model/model_instance.h"
#include "Graphics/camera.h"
#include "graphics/scene_view.h"

#include "object/light_object.h"

#include "object/model_object.h"

#include "Graphics/custom_pipeline_state.h"

#if defined(USE_IMGUI)
#include "Editor/editor_camera.h"

namespace
{
	//	モデル
	constexpr const TCHAR* ModelFileDialogFilter =
		L"モデルファイル (*.emo, *.pmx, *.gltf, *.glb)\0*.emo;*.pmx;*.gltf;*.glb;\0\
		すべてのファイル (*.*)\0*.*\0";
}

bool edit_model_scene::initialize_edit_core()
{
	//	エディターカメラ生成
	{
		editor_camera = std::make_shared<editor_camera_object>("エディターカメラ",
			system_units::convert_m_to_unit(math::vector3d(0.0, 1.0, 8.0)),
			math::rotatorf(0, math::constants<float>::pi, 0),
			0.25f);
		register_object(editor_camera);
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

	editor_camera.reset();

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
	if (ImGui::Begin(get_context().make_window_name("Mesh List Window").c_str()))
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

	if (ImGui::Begin(get_context().make_window_name("Morph Window").c_str()))
	{
		int edeit_id = 0x0FF1F38F;
		auto& morphs = edit_model->get_model_instance()->get_resource()->get_morphes_for_editor();

		//	リスト調整
		bool edit = false;
		int morph_list_size = static_cast<int>(morphs.size());
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
				int list_size = static_cast<int>(morph.parts.size());
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
	if (ImGui::Begin(get_context().make_window_name("Material Window").c_str()))
	{
		auto& model_instance = *edit_model->get_model_instance();
		auto& materials = model_instance.get_resource_for_editor()->get_materials_for_editor();
		for (std::size_t index = 0; index < materials.size(); ++index)
		{
			auto& material = materials[index];
			ImGui::PushID(static_cast<int>(index));
			if (ImGui::TreeNode(system_utility::convert_utf8(material->get_name()).data()))
			{
				//	別のマテリアルリソースに差し替えたら、表示中のインスタンスも差し替える
				if (editor_items::edit_material_slot(editor_context, material, material_resource::domains::surface) &&
					index < model_instance.get_materials().size())
				{
					model_instance.get_materials()[index].set_resource(material);
				}
				editor_items::edit_material(editor_context, *material);
				ImGui::TreePop();
			}
			ImGui::PopID();
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
			if (ImGui::Begin(get_context().make_window_name("Convert Gltf Data").c_str(), &show_flag))
			{
				ImGui::Checkbox("compute_tangent", &convert_option_data.gltf.is_compute_tangent);
				ImGui::Checkbox("populate_vertex_color", &convert_option_data.gltf.is_populate_vertex_color);
				ImGui::Checkbox("normalize_weight", &convert_option_data.gltf.is_normalize_weight);

				ImGui::Checkbox("change_coordinate_system", &convert_option_data.gltf.is_change_coordinate_system);
				ImGui::Checkbox("change_coordinate_system_indices", &convert_option_data.gltf.is_change_coordinate_system_indices);

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
			ImGui::SameLine();
			if (ImGui::Button("Generate Meshlets"))
			{
				generate_meshlet_asset();
			}
			//	全頂点が白の頂点カラーを削除
			if (ImGui::Button("Strip White Vertex Color"))
			{
				strip_white_vertex_color_asset();
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
			ImGui::Checkbox("cull_reverse_flag", &bounding_mesh_option.reverse_flag);
			ImGui::Checkbox("use_lod", &bounding_mesh_option.use_lod);
			if (bounding_mesh_option.use_lod)
			{
				int lod_max = edit_model->get_model_instance()->get_lod_max();
				ImGui::BeginDisabled(lod_max <= 0);
				ImGui::SliderInt("bm_lod_level", &bounding_mesh_option.lod_level, 0, lod_max > 0 ? lod_max - 1 : 0);
				ImGui::EndDisabled();
			}
			ImGui::DragInt3("bm_combine_grid_divisions", &bounding_mesh_option.combine_grid_divisions.x, 0.1f, 1, 64);
			if (ImGui::Button("Generate Bounding Mesh"))
			{
				generate_bounding_mesh_asset();
			}
		}

		//	テクスチャの最大解像度制限
		if (edit_model)
		{
			ImGui::Separator();
			ImGui::Text("Texture Resolution");
			ImGui::DragInt("texture_max_resolution", &texture_max_resolution, 16.0f, 4, 16384);
			if (ImGui::Button("Limit Texture Resolution"))
			{
				limit_texture_resolution_asset();
			}
		}

		//	cubic SDF生成
		if (edit_model)
		{
			ImGui::Separator();
			ImGui::Text("Cubic SDF");
			ImGui::SliderInt("sdf_resolution", &cubic_sdf_option.resolution, 4, 128);
			ImGui::SliderFloat("sdf_bounds_padding_rate", &cubic_sdf_option.bounds_padding_rate, 0.0f, 1.0f);
			if (ImGui::Button("Generate Cubic SDF"))
			{
				generate_cubic_sdf_asset();
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
		auto mat = std::make_shared<material_resource>();
		mat->set_domain(material_resource::domains::surface);
		mat->set_blend_mode(material_resource::blend_modes::translucent);
		mat->set_alpha_clip_threshold(-1.0f);
		mat->set_shading_model(material_resource::shading_models::unlit);
		mat->set_flag(material_resource::flags::wire, true);

		mat->set_shader_path("Assets/GameShader/edit_model_color.hlsl");
		mat->set_float("line_factor", 0.001f);
		mat->set_float3("line_color", { 0, 1, 0 });
		vertex_color_edit_material = mat;
	}

	//	マウス購読
	{
		static constexpr std::string_view keys[] =
		{
			editor_camera_object::left_click_down_identifier,
			editor_camera_object::left_click_hold_identifier,
		};
		for (int index = 0; index < ARRAYSIZE(keys); ++index)
		{
			left_click_data_identifier[index] = get_context().get_objects().get_messenger().add_receiver(
				keys[index],
				[this](void* data_ptr) { on_mouse_left_down_and_hold(data_ptr); });
		}

		left_click_data_identifier[2] = get_context().get_objects().get_messenger().add_receiver(
			editor_camera_object::left_click_release_identifier,
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
			get_context().get_objects().get_messenger().remove_receiver(id);
		}
	}
	return true;
}

void edit_model_scene::update_vertex_color_edit()
{
	//	頂点編集時の座標表示
	if (brush_data.on)
	{
		QUEUE_DEBUG_PRIMITIVE_SPHERE(get_context(), brush_data.position, brush_data.radius, math::float4(1, 1, 1, 1), true);
	}
}

void edit_model_scene::imgui_render_vertex_color_edit()
{
	if (ImGui::Begin(get_context().make_window_name("Edit Vertex Color Window").c_str()))
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

	auto data = static_cast<editor_camera_object::mouse_position_data*>(data_ptr);

	if (!editor_camera)
		return;

	//	表示しているエディターカメラの視点からレイを作る
	const view_parameters& parameters = editor_camera->get_camera()->get_parameters();
	const scene_view picking_view = scene_view::from_parameters(parameters, math::vector3d::zero());
	const math::matrixf view(picking_view.get_view_matrix());
	const math::matrixf projection(picking_view.get_projection_matrix());
	const math::matrixf viewport = math::matrixf::viewport(0, data->viewport_size.x, 0, data->viewport_size.y, 0, 1);

	const math::float3 start_position = math::vector3f::calc_screen_to_world(data->position.x, data->position.y, 0, view, projection, viewport).to_float3();
	const math::float3 end_position = math::vector3f::calc_screen_to_world(data->position.x, data->position.y, 1, view, projection, viewport).to_float3();

	//	ヒット位置算出
	float vertex_hit_length = FLT_MAX;
	for (auto& instance_mesh : edit_model->get_model_instance()->get_meshes())
	{
		if (!instance_mesh.mesh || !instance_mesh.visible)
			continue;

		auto& mesh = *instance_mesh.mesh;
		auto positions = mesh.vertex_buffer_views.find(semantic("POSITION", 0));
		auto& indiceds = mesh.index_buffer_view;
		if (positions == mesh.vertex_buffer_views.end() || indiceds.count() == 0)
			continue;

		collision::triangles triangles = {};
		triangles.positions = reinterpret_cast<const math::float3*>(positions->second.row_data.data());
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
				brush_data.position = math::float3(result[0].point);
				vertex_hit_length = static_cast<float>(result[0].length);
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
			auto position_view = mesh.vertex_buffer_views.find(semantic("POSITION", 0));
			auto color_view = mesh.vertex_buffer_views.find(semantic("COLOR", 0));
			if (position_view == mesh.vertex_buffer_views.end() || color_view == mesh.vertex_buffer_views.end())
				continue;

			bool is_change = false;
			auto positions = reinterpret_cast<math::float3*>(position_view->second.row_data.data());
			auto colors = reinterpret_cast<math::float4*>(color_view->second.row_data.data());
			for (size_t index = position_view->second.offset(); index < position_view->second.count(); ++index)
			{
				auto p = positions + index;
				math::float3 d;
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
				render_data::instance()->get_upload_list().update_whole(color_view->second.buffer.Get(), colors, static_cast<UINT>(color_view->second.row_data.size()));
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
		get_context().get_objects().unregister_object(vertex_color_edit_model);
		vertex_color_edit_model = nullptr;
	}

	if (edit_model)
	{
		edit_model->set_active(false);
		get_context().get_objects().unregister_object(edit_model);
		edit_model = nullptr;
	}
}

void edit_model_scene::on_model_setup()
{
	if (!edit_model)
		return;

	edit_model->set_active(true);
	get_context().get_objects().register_object(edit_model);

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
			vertex_color_edit_model->get_model_instance()->get_materials()[index] = material_instance(vertex_color_edit_material);
		}
		vertex_color_edit_model->set_display_inspector(false);
		vertex_color_edit_model->set_active(is_edit_vertex_color);
		get_context().get_objects().register_object(vertex_color_edit_model);
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
