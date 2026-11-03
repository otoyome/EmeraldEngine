#include "data_converter.h"
#include <unordered_set>
#include "model_buffer_pool.h"

#include "graphics_utility.h"
#include "render_data.h"
#include "texture.h"

#include "animation_data_version.h"

#include "model_resource_utility.h"

#if defined(USE_IMGUI)

#include "ModelFormat/gltf_loader.h"

namespace
{
	//	[エンジン改造:接線に再計算法線を含める対応]
	//	再計算用クラス
	constexpr float epsilon = 0.001f;
	struct Vec3Key
	{
		float x = 0, y = 0, z = 0;
		Vec3Key() {}
		Vec3Key(const Vec3Key& d)
			: x(d.x), y(d.y), z(d.z) {}
		Vec3Key(Vec3Key&& d)
			: x(d.x), y(d.y), z(d.z) {}
		Vec3Key(const math::float3* d)
			: x(d->x), y(d->y), z(d->z) {}
		bool operator==(const Vec3Key& o) const
		{
			return fabsf(x - o.x) < epsilon && fabsf(y - o.y) < epsilon && fabsf(z - o.z) < epsilon;
		}
		bool operator==(const math::float3& o) const
		{
			return fabsf(x - o.x) < epsilon && fabsf(y - o.y) < epsilon && fabsf(z - o.z) < epsilon;
		}
	};
	struct Vec3KeyHash
	{
		size_t operator()(const Vec3Key& v) const noexcept
		{
			auto q = [&](float f) {
				return static_cast<int>(f / epsilon);
				};

			size_t h1 = std::hash<int>()(q(v.x));
			size_t h2 = std::hash<int>()(q(v.y));
			size_t h3 = std::hash<int>()(q(v.z));

			return h1 ^ (h2 << 1) ^ (h3 << 2);
		}
	};
	//	[エンジン改造:接線に再計算法線を含める対応]

	//	ノードの並びをソート
	struct sorted_node
	{
		UINT src_index;
		UINT dest_index;
		UINT hierarchy_level;
	};
	std::vector<sorted_node> compute_sort_node_map(const std::vector<gltf_loader::node_data>& gltf_nodes)
	{
		std::vector<sorted_node> sorted_nodes(gltf_nodes.size());
		for (UINT index = 0; index < static_cast<UINT>(sorted_nodes.size()); ++index)
		{
			auto& node = gltf_nodes[index];
			sorted_nodes[index].src_index = index;
			sorted_nodes[index].hierarchy_level = 0;

			size_t parent_index = node.parent_node_index;
			while (parent_index != model_resource::invalid_id)
			{
				sorted_nodes[index].hierarchy_level++;
				parent_index = gltf_nodes[parent_index].parent_node_index;
			}
		}
		std::sort(sorted_nodes.begin(), sorted_nodes.end(),
			[](const sorted_node& l, const sorted_node& r)
			{
				return l.hierarchy_level < r.hierarchy_level;
			});
		for (UINT index = 0; index < static_cast<UINT>(sorted_nodes.size()); ++index)
		{
			auto& node = gltf_nodes[index];
			sorted_nodes[index].dest_index = index;
		}
		std::sort(sorted_nodes.begin(), sorted_nodes.end(),
			[](const sorted_node& l, const sorted_node& r)
			{
				return l.src_index < r.src_index;
			});

		return sorted_nodes;
	}

	//	終端時間の算出
	float compute_end_time(const gltf_loader::animation_data& gltf_animation)
	{
		if (gltf_animation.end_time > 0.0f)
			return gltf_animation.end_time;

		//	end_timeが記録されていない、
		//	ノードにのみフレーム情報があるデータが存在する模様
		float end_time = 0.0f;
		for (UINT node_index = 0; node_index < gltf_animation.nodes.size(); ++node_index)
		{
			for (auto& data : gltf_animation.nodes[node_index].scales)
			{
				if (end_time < data.second)
					end_time = data.second;
			}
			for (auto& data : gltf_animation.nodes[node_index].rotations)
			{
				if (end_time < data.second)
					end_time = data.second;
			}
			for (auto& data : gltf_animation.nodes[node_index].translations)
			{
				if (end_time < data.second)
					end_time = data.second;
			}
		}
		return end_time;
	}

	//	リターゲッティング用にノードリストを生成
	std::vector<std::pair<size_t, size_t>> compute_retarget_node_list(model_instance& source_model_instance, model_instance& dest_model_instance, const retarget_profile& retarget_profile)
	{
		std::vector<std::pair<size_t, size_t>> node_lists;

		//	リターゲッティング用に存在するノードリスト算出
		auto& destination_nodes = dest_model_instance.get_nodes();
		for (UINT destination_node_index = 0; destination_node_index < destination_nodes.size(); ++destination_node_index)
		{
			auto& destination_node = destination_nodes[destination_node_index];

			//	ソース側がノードを持っているか判定
			size_t source_node_index = source_model_instance.get_node_index_by_name(destination_node.name);
			if (source_node_index == model_resource::invalid_id)
			{
				//	無ければリターゲットボーンマップから検索
				auto it = retarget_profile.bone_map.find(destination_node.name);
				if (it != retarget_profile.bone_map.end())
				{
					source_node_index = source_model_instance.get_node_index_by_name(it->second.target_name);
				}
			}
			if (source_node_index == model_resource::invalid_id)
				continue;

			node_lists.emplace_back(std::make_pair(destination_node_index, source_node_index));
		}

		return node_lists;
	}

	//	GLTF > EMOに変換
	bool convert(model_resource& resource, gltf_loader& loader, const data_converter::gltf::convert_model_data& convert_data)
	{
		std::filesystem::path path(convert_data.file_path);
		std::vector<gltf_loader::node_data> gltf_nodes;
		std::vector<gltf_loader::mesh_data> gltf_meshes;
		std::vector<gltf_loader::material_data> gltf_materials;
		std::vector<gltf_loader::animation_data> gltf_animations;
		loader.load_nodes(gltf_nodes);
		{
			gltf_loader::convert_mesh_data convert_mesh_data;
			convert_mesh_data.is_compute_tangent = convert_data.is_compute_tangent;
			convert_mesh_data.is_normalize_weight = convert_data.is_normalize_weight;
			convert_mesh_data.is_change_coordinate_system = convert_data.is_change_coordinate_system;
			convert_mesh_data.is_change_coordinate_system_indices = convert_data.is_change_coordinate_system_indices;
			loader.load_meshes(gltf_meshes, gltf_nodes, convert_mesh_data);
		}
		loader.load_materials(gltf_materials);

		//	頂点カラーのｎを埋める
		if (convert_data.is_populate_vertex_color)
		{
			gltf_loader::register_vertex_data data;
			data.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			data.set_data.resize(graphics_utility::get_byte_offset_from_format(data.format));
			math::float4* p = reinterpret_cast<math::float4*>(data.set_data.data());
			*p = { 1.0f, 1.0f, 1.0f, 1.0f };
			loader.populate_vertex_data(gltf_meshes, semantic("COLOR", 0), data);
		}

		//	[エンジン改造:接線に再計算法線を含める対応]
		if (convert_data.is_recomputed_normal)
		{
			//	法線再算出
			for (auto& gltf_mesh : gltf_meshes)
			{
				math::float3* positions = nullptr;
				math::float3* normals = nullptr;
				math::float4* tangents = nullptr;

				auto it_position = gltf_mesh.vertex_buffers.find({ "POSITION", 0 });
				auto it_normal = gltf_mesh.vertex_buffers.find({ "NORMAL", 0 });
				auto it_tangent = gltf_mesh.vertex_buffers.find({ "TANGENT", 0 });
				if (it_position != gltf_mesh.vertex_buffers.end())
					positions = it_position->second.data<math::float3>();
				if (it_normal != gltf_mesh.vertex_buffers.end())
					normals = it_normal->second.data<math::float3>();
				if (it_tangent != gltf_mesh.vertex_buffers.end())
					tangents = it_tangent->second.data<math::float4>();

				if (!positions)
					continue;

				//	頂点情報から面法線を計算して再算出して行く
				std::unordered_map<Vec3Key, Vec3Key, Vec3KeyHash> recomputed_normals;
				for (UINT index = 0; index < gltf_mesh.index_buffer.count(); index += 3)
				{
					const UINT* indices = reinterpret_cast<const UINT*>(gltf_mesh.index_buffer.row_data.data());
					//	面法線計算
					math::float3* a = positions + indices[index + 0];
					math::float3* b = positions + indices[index + 1];
					math::float3* c = positions + indices[index + 2];
					math::vector3f A(*a);
					math::vector3f B(*b);
					math::vector3f C(*c);
					math::vector3f AB = B - A;
					math::vector3f AC = C - A;
					math::vector3f Normal = (math::vector3f::cross(AC, AB)).normalized();
					math::float3 normal;
					normal = Normal.to_float3();

					//	同じ位置の法線に加算していく
					auto& it_a = recomputed_normals[Vec3Key(a)];
					auto& it_b = recomputed_normals[Vec3Key(b)];
					auto& it_c = recomputed_normals[Vec3Key(c)];
					it_a.x += normal.x;	it_a.y += normal.y;	it_a.z += normal.z;
					it_b.x += normal.x;	it_b.y += normal.y;	it_b.z += normal.z;
					it_c.x += normal.x;	it_c.y += normal.y;	it_c.z += normal.z;
				}

				//	平均化して各頂点に再納入
				for (auto& r_n : recomputed_normals)
				{
					float x = r_n.second.x;
					float y = r_n.second.y;
					float z = r_n.second.z;
					float l = sqrtf(x * x + y * y + z * z);
					x /= l;	y /= l;	z /= l;
					for (std::size_t index = 0; index < it_position->second.count(); ++index)
					{
						if (!(r_n.first == positions[index]))
							continue;

						if (convert_data.is_recomputed_normal_to_copy_tangent)
						{
							tangents[index].x = x;
							tangents[index].y = y;
							tangents[index].z = z;
							tangents[index].w = 1;
						}
						else
						{
							normals[index].x = x;
							normals[index].y = y;
							normals[index].z = z;
						}
					}
				}
			}
		}
		//	[エンジン改造:接線に再計算法線を含める対応]

		//	アニメーション読込
		loader.load_animations(gltf_animations, gltf_nodes);

		auto& nodes = resource.get_nodes_for_editor();
		auto& meshes = resource.get_meshes_for_editor();
		auto& materials = resource.get_materials_for_editor();
		auto& animations = resource.get_animations_for_editor();

		//	ノード構築
		for (auto& gltf_node : gltf_nodes)
		{
			nodes.emplace_back();
			auto& node = nodes.back();
			node.name = gltf_node.name;
			node.parent_index = gltf_node.parent_node_index;

			node.scale = gltf_node.scale;
			node.rotation = gltf_node.rotation;
			node.translation = gltf_node.translation;
		}

		//	ソートノードマップ生成
		std::vector<sorted_node> sorted_nodes = compute_sort_node_map(gltf_nodes);

		//	ノードの並びをソート
		{
			std::vector<model_resource::node> dst_nodes(sorted_nodes.size());
			for (size_t index = 0; index < sorted_nodes.size(); ++index)
			{
				size_t dest_index = sorted_nodes[index].dest_index;
				dst_nodes[dest_index] = nodes[sorted_nodes[index].src_index];
				size_t parent_index = dst_nodes[dest_index].parent_index;
				if (parent_index != model_resource::invalid_id)
				{
					dst_nodes[dest_index].parent_index = sorted_nodes[parent_index].dest_index;
				}
			}
			nodes = dst_nodes;
		}

		//	メッシュ構築
		for (auto& gltf_mesh : gltf_meshes)
		{
			meshes.emplace_back();
			auto& mesh = meshes.back();
			mesh.material_index = gltf_mesh.material_index;
			mesh.node_index = sorted_nodes[gltf_mesh.node_index].dest_index;

			mesh.bounds_min = gltf_mesh.bounds_min;
			mesh.bounds_max = gltf_mesh.bounds_max;

			//	ボーン
			for (auto& gltf_bone : gltf_mesh.bones)
			{
				mesh.bones.emplace_back();
				auto& bone = mesh.bones.back();
				bone.node_index = sorted_nodes[gltf_bone.node_index].dest_index;
				bone.offset_matrix = gltf_bone.offset_matrix;
			}

			//	インデックスバッファ
			{
				mesh.index_buffer_view.format = gltf_mesh.index_buffer.format;
				mesh.index_buffer_view.offset_in_bytes = gltf_mesh.index_buffer.offset_in_bytes;
				mesh.index_buffer_view.size_in_bytes = gltf_mesh.index_buffer.size_in_bytes;
				mesh.index_buffer_view.stride_in_bytes = gltf_mesh.index_buffer.stride_in_bytes;
				mesh.index_buffer_view.row_data = gltf_mesh.index_buffer.row_data;
			}

			//	頂点バッファ
			for (auto& it_vertex_buffer : gltf_mesh.vertex_buffers)
			{
				buffer_view vertex_buffer_view = {};
				vertex_buffer_view.format = it_vertex_buffer.second.format;
				vertex_buffer_view.offset_in_bytes = it_vertex_buffer.second.offset_in_bytes;
				vertex_buffer_view.size_in_bytes = it_vertex_buffer.second.size_in_bytes;
				vertex_buffer_view.stride_in_bytes = it_vertex_buffer.second.stride_in_bytes;
				vertex_buffer_view.row_data = it_vertex_buffer.second.row_data;
				mesh.vertex_buffer_views.insert(std::make_pair(it_vertex_buffer.first, vertex_buffer_view));
			}

			//	モーフバッファ
			for (auto& morph_data : gltf_mesh.morph_datas)
			{
				mesh.morph_indices.insert_or_assign(morph_data.first, mesh.morph_meshes.size());
				auto& morph_mesh = mesh.morph_meshes.emplace_back();
				for (auto& it_vertex_buffer : morph_data.second.vertex_buffers)
				{
					buffer_view vertex_buffer_view = {};
					vertex_buffer_view.format = it_vertex_buffer.second.format;
					vertex_buffer_view.offset_in_bytes = it_vertex_buffer.second.offset_in_bytes;
					vertex_buffer_view.size_in_bytes = it_vertex_buffer.second.size_in_bytes;
					vertex_buffer_view.stride_in_bytes = it_vertex_buffer.second.stride_in_bytes;
					vertex_buffer_view.row_data = it_vertex_buffer.second.row_data;
					morph_mesh.vertex_buffer_views.insert(std::make_pair(it_vertex_buffer.first, vertex_buffer_view));
				}
			}

			//	JOINT番号を変換
			for (UINT index = 0; index < 8; ++index)
			{
				auto it = gltf_mesh.vertex_buffers.find({ "JOINTS", index });
				if (it == gltf_mesh.vertex_buffers.end())
					continue;

				math::uint4* value = reinterpret_cast<math::uint4*>(it->second.row_data.data());
				for (UINT index = 0; index < it->second.count(); ++index)
				{
					value[index].x = sorted_nodes[value[index].x].dest_index;
					value[index].y = sorted_nodes[value[index].y].dest_index;
					value[index].z = sorted_nodes[value[index].z].dest_index;
					value[index].w = sorted_nodes[value[index].w].dest_index;
				}
			}

			//	システム的に情報が無いと不味いので埋めておく。
			//	じゃないとシェーダー増やす必要が出てくる。
			auto register_dummy_vertex_buffer = [&](const semantic sem, DXGI_FORMAT format, std::vector<unsigned char> buffer)
				{
					buffer_view vertex_buffer_view = {};
					vertex_buffer_view.format = format;
					vertex_buffer_view.offset_in_bytes = 0;
					vertex_buffer_view.size_in_bytes = buffer.size();
					vertex_buffer_view.stride_in_bytes = graphics_utility::get_byte_offset_from_format(format);
					vertex_buffer_view.row_data = buffer;
					mesh.vertex_buffer_views.insert(std::make_pair(sem, vertex_buffer_view));
				};
			if (gltf_mesh.vertex_buffers.find({ "COLOR", 0 }) == gltf_mesh.vertex_buffers.end())
			{
				if (gltf_mesh.vertex_buffers.begin() != gltf_mesh.vertex_buffers.end())
				{
					std::vector<unsigned char> data;
					data.resize(gltf_mesh.vertex_buffers.begin()->second.count() * 16u);
					math::float4* value = reinterpret_cast<math::float4*>(data.data());
					for (UINT index = 0; index < data.size() / 16u; ++index)
					{
						value[index].x = 1.0f;
						value[index].y = 1.0f;
						value[index].z = 1.0f;
						value[index].w = 1.0f;
					}
					register_dummy_vertex_buffer({ "COLOR", 0 }, DXGI_FORMAT_R32G32B32A32_FLOAT, data);
				}
			}
			if (gltf_mesh.bones.empty() &&
				gltf_mesh.vertex_buffers.find({ "JOINTS", 0 }) == gltf_mesh.vertex_buffers.end() &&
				gltf_mesh.vertex_buffers.find({ "WEIGHTS", 0 }) == gltf_mesh.vertex_buffers.end())
			{
				//	ボーンが存在していない場合はメッシュノードの物を適応する必要がある

				//	ノードを指定するボーンを設定
				{
					mesh.bones.emplace_back();
					auto& bone = mesh.bones.back();
					bone.node_index = mesh.node_index;
				}

				//	0番ボーンとして指定しておく
				std::vector<unsigned char> data;
				{
					data.resize(gltf_mesh.vertex_buffers.begin()->second.count() * 16u);
					math::uint4* value = reinterpret_cast<math::uint4*>(data.data());
					for (UINT index = 0; index < data.size() / 16u; ++index)
					{
						value[index].x = 0;
						value[index].y = 0;
						value[index].z = 0;
						value[index].w = 0;
					}
					register_dummy_vertex_buffer({ "JOINTS", 0 }, DXGI_FORMAT_R32G32B32A32_UINT, data);
				}
				{
					data.resize(gltf_mesh.vertex_buffers.begin()->second.count() * 16u);
					math::float4* value = reinterpret_cast<math::float4*>(data.data());
					for (UINT index = 0; index < data.size() / 16u; ++index)
					{
						value[index].x = 1.0f;
						value[index].y = 0.0f;
						value[index].z = 0.0f;
						value[index].w = 0.0f;
					}
					register_dummy_vertex_buffer({ "WEIGHTS", 0 }, DXGI_FORMAT_R32G32B32A32_FLOAT, data);
				}
			}
		}

		//	各種バッファを設定
		auto generate_buffer_func = [&](model_resource::mesh::sub_mesh& mesh)
		{
			if (mesh.index_buffer_view.count() == 0)
				return;

			//	頂点バッファ書き戻し
			model_buffer_pool_manager::instance()->build_vertex_buffers(mesh.vertex_buffer_views);
		};
		for (auto& mesh : meshes)
		{
			//	頂点バッファ書き戻し
			model_buffer_pool_manager::instance()->build_vertex_buffers(mesh.vertex_buffer_views);

			//	影メッシュ生成
			if (mesh.shadow_mesh.is_valid())
			{
				generate_buffer_func(mesh.shadow_mesh);
			}

			//	LODメッシュ生成
			for (UINT index = 0; index < mesh.lod_meshs.size(); ++index)
			{
				generate_buffer_func(mesh.lod_meshs[index]);
			}
		}

		//	マテリアル生成
		materials.reserve(gltf_materials.size());

		//	マテリアル毎に逐次ロードすると重いので一括並列ロードする
		struct material_texture_slots
		{
			texture_ptr base_texture;
			texture_ptr emissive_texture;
			texture_ptr roughness_metalness_texture;
			texture_ptr normal_texture;
			texture_ptr occlusion_texture;
		};
		std::vector<material_texture_slots> material_textures(gltf_materials.size());
		{
			std::vector<std::string> texture_path_storage;
			std::vector<texture_ptr*> texture_path_targets;
			texture_path_storage.reserve(gltf_materials.size() * 5);
			texture_path_targets.reserve(gltf_materials.size() * 5);

			std::unordered_set<std::string> converted_texture_paths;
			auto collect_path = [&](std::string_view relative_path, texture_ptr* target)
			{
				if (relative_path.empty())
					return;
				std::filesystem::path file_path = path.parent_path() / relative_path;

				//	DDS変更 & ミップマップ生成
				if (convert_data.is_convert_texture_to_dds)
				{
					std::string source_key = file_path.string();
					std::filesystem::path dds_path = file_path;
					dds_path.replace_extension(".dds");

					bool converted = converted_texture_paths.count(source_key) != 0;
					if (!converted)
					{
						converted = texture_manager::instance()->convert_texture_to_dds(source_key, dds_path.string(), true,
																						 static_cast<UINT>(std::max(convert_data.max_texture_size, 0)));
						if (converted)
							converted_texture_paths.insert(source_key);
					}
					if (converted)
						file_path = dds_path;
				}

				texture_path_storage.emplace_back(file_path.string());
				texture_path_targets.emplace_back(target);
			};

			for (UINT material_index = 0; material_index < gltf_materials.size(); ++material_index)
			{
				auto& gltf_material = gltf_materials[material_index];
				auto& slots = material_textures[material_index];
				collect_path(gltf_material.base_texture_path, &slots.base_texture);
				collect_path(gltf_material.emissive_texture_path, &slots.emissive_texture);
				collect_path(gltf_material.roughness_metalness_texture_path, &slots.roughness_metalness_texture);
				collect_path(gltf_material.normal_texture_path, &slots.normal_texture);
				collect_path(gltf_material.occlusion_texture_path, &slots.occlusion_texture);
			}

			std::vector<std::string_view> texture_path_views(texture_path_storage.begin(), texture_path_storage.end());
			std::vector<texture_ptr> loaded_textures;
			texture_manager::instance()->load_textures_from_files(loaded_textures, texture_path_views);

			for (std::size_t index = 0; index < loaded_textures.size(); ++index)
			{
				if (loaded_textures[index])
					*texture_path_targets[index] = loaded_textures[index];
			}
		}

		for (UINT material_index = 0; material_index < gltf_materials.size(); ++material_index)
		{
			auto& gltf_material = gltf_materials[material_index];
			auto& slots = material_textures[material_index];
			auto& material = *materials.emplace_back(std::make_shared<::material>());
			material.set_name(gltf_material.name);
			material.set_domain(material::domains::surface);
			material.set_alpha_clip_threshold(gltf_material.alpha_cutoff);
			material.set_shading_model(material::shading_models::default_lit);
			if (gltf_material.alpha_mode == gltf_loader::alpha_modes::opaque)
				material.set_blend_mode(material::blend_modes::opaque);
			if (gltf_material.alpha_mode == gltf_loader::alpha_modes::mask)
				material.set_blend_mode(material::blend_modes::mask);
			if (gltf_material.alpha_mode == gltf_loader::alpha_modes::translucent)
				material.set_blend_mode(material::blend_modes::translucent);
			material.set_flag(material::flags::two_sided, gltf_material.double_sided);

			//	デフォルトシェーダー設定
			material.set_shader_path("Shaders/Materials/gltf.hlsl");

			material.set_float4("base_color_factor", gltf_material.base_color);
			if (slots.base_texture)
			{
				slots.base_texture->set_flag(texture::flags::srgb, true);
				material.set_texture("BaseColorMap", slots.base_texture);
				texture_manager::instance()->save_texture(slots.base_texture);
			}
			material.set_float3("emissive_color_factor", gltf_material.emissive_color);
			if (slots.emissive_texture)
			{
				slots.emissive_texture->set_flag(texture::flags::srgb, true);
				material.set_texture("EmissiveMap", slots.emissive_texture);
				texture_manager::instance()->save_texture(slots.emissive_texture);
			}
			material.set_float("roughness_factor", gltf_material.roughness);
			material.set_float("metallic_factor", gltf_material.metalness);
			if (slots.roughness_metalness_texture)
			{
				slots.roughness_metalness_texture->set_flag(texture::flags::srgb, true);
				material.set_texture("RoughnessMetallicMap", slots.roughness_metalness_texture);
				texture_manager::instance()->save_texture(slots.roughness_metalness_texture);
			}
			material.set_float("normal_map_scale", gltf_material.normal_scale);
			if (slots.normal_texture)
			{
				slots.normal_texture->set_flag(texture::flags::srgb, true);
				material.set_texture("NormalMap", slots.normal_texture);
				texture_manager::instance()->save_texture(slots.normal_texture);
			}
			material.set_float("occlusion_strength", gltf_material.occlusion_strength);
			if (slots.occlusion_texture)
			{
				slots.occlusion_texture->set_flag(texture::flags::srgb, true);
				material.set_texture("OcclusionMap", slots.occlusion_texture);
				texture_manager::instance()->save_texture(slots.occlusion_texture);
			}
		}

		//	アニメーション生成
		{
			//	初期姿勢用アニメーション
			{
				animation::data animation;
				animation.name = std::string(data_converter::DefaultAnimationName);
				animation.end_time = 1.0f;

				//	ノード分登録
				for (auto& node : nodes)
				{
					animation.nodes.emplace_back();
					auto& na = animation.nodes.back();
					na.node_name = node.name;
					na.nodes.emplace_back();
					na.nodes.back().time = 0.0f;
					na.nodes.back().scale = node.scale;
					na.nodes.back().rotation = node.rotation;
					na.nodes.back().translation = node.translation;
				}
				animations.emplace_back(animation);
			}

			//	登録されているアニメーション情報を適応
			for (auto& gltf_animation : gltf_animations)
			{
				animations.emplace_back();
				auto& animation = animations.back();
				animation.name = gltf_animation.name;
				animation.end_time = compute_end_time(gltf_animation);

				//	ノード情報のみ登録
				animation.nodes.resize(nodes.size());
				for (float sampling_time = 0; sampling_time <= animation.end_time; sampling_time += data_converter::animation_sampling_rate)
				{
					for (UINT node_index = 0; node_index < gltf_animation.nodes.size(); ++node_index)
					{
						UINT animation_node_index = sorted_nodes[node_index].dest_index;

						auto pose = gltf_animation.nodes[node_index].calculate(sampling_time);
						auto& n = animation.nodes[animation_node_index];
						n.node_name = gltf_animation.nodes[node_index].node_name;
						auto& na = n.nodes.emplace_back();
						na.time = sampling_time;
						na.scale = pose.scale;
						na.rotation = pose.rotation;
						na.translation = pose.translation;
					}
				}
			}
		}

		//	EMOに変換したのでファイルパスを改めて設定
		path.replace_extension(model_resource::extension);
		resource.set_file_path(path.string());
		return true;
	}

	//	通常リターゲッティング
	bool normal_retarget(animation_resource& animation_resource,
						 model_instance& source_model_instance,
						 model_instance& dest_model_instance,
						 size_t source_animation_index,
						 std::vector<std::pair<size_t, size_t>> retarget_node_indices,
						 const data_converter::gltf::convert_animation_data& convert_data)
	{
		//	アニメーションの基準をスケーリング
		auto animation_node_adjust = [&](const model_instance& model_instance,
										 std::vector<model_instance::node_animation>& nodes)
		{
			auto& bone_map = convert_data.retarget_profile.bone_map;
			for (UINT index = 0; index < nodes.size(); ++index)
			{
				if (model_instance.get_nodes()[index].parent_index == model_resource::invalid_id)
				{
					nodes[index].scale.x *= convert_data.retarget_profile.base_scaling.x;
					nodes[index].scale.y *= convert_data.retarget_profile.base_scaling.y;
					nodes[index].scale.z *= convert_data.retarget_profile.base_scaling.z;
				}
				else
				{
					//	ボーン補助
					for (auto& retarget_node_index : retarget_node_indices)
					{
						if (retarget_node_index.second != index)
							continue;

						auto& name = dest_model_instance.get_nodes()[retarget_node_index.first].name;
						auto it = bone_map.find(name);
						if (it == bone_map.end())
							continue;

						auto& data = bone_map.find(name)->second;

						math::matrixf LocalMatrix;
						LocalMatrix = math::matrixf::scaling(nodes[index].scale.x, nodes[index].scale.y, nodes[index].scale.z);
						LocalMatrix *= math::matrixf::rotation_quaternion(math::quaternionf(nodes[index].rotation));
						LocalMatrix *= math::matrixf::translation(nodes[index].translation.x, nodes[index].translation.y, nodes[index].translation.z);
						
						math::matrixf AdjustMatrix;
						AdjustMatrix = math::matrixf::scaling(data.scale.x, data.scale.y, data.scale.z);
						AdjustMatrix *= math::matrixf::rotation_quaternion(math::quaternionf(data.rotation));
						AdjustMatrix *= math::matrixf::translation(data.translation.x, data.translation.y, data.translation.z);

						math::vector3f Scale, Translation;
						math::quaternionf Rotation;
						(AdjustMatrix * LocalMatrix).decompose(Scale, Rotation, Translation);

						nodes[index].scale = Scale.to_float3();
						nodes[index].rotation = Rotation.to_float4();
						nodes[index].translation = Translation.to_float3();
					}
				}
			}
		};

		auto& source_animations = source_model_instance.get_resource()->get_animations();
		auto& source_animation = source_animations[source_animation_index];

		animation::data dest_animation;
		dest_animation.name = source_animation.name;
		dest_animation.end_time = source_animation.end_time;
		dest_animation.nodes.resize(dest_model_instance.get_nodes().size());
		for (auto& retarget_node_index : retarget_node_indices)
		{
			auto& dest_node_index = retarget_node_index.first;
			auto& source_node_index = retarget_node_index.second;
			auto& dest_instance_node = dest_model_instance.get_nodes()[dest_node_index];
			auto& source_instance_node = source_model_instance.get_nodes()[source_node_index];

			//	初期姿勢に変換
			std::vector<model_instance::node_animation> dest_animation_nodes;
			{
				std::vector<model_instance::node_animation> source_nodes;
				dest_model_instance.compute_animation_nodes(dest_animation_nodes, 0, 0.0f);
				source_model_instance.compute_animation_nodes(source_nodes, 0, 0.0f);
				animation_node_adjust(source_model_instance, source_nodes);

				dest_model_instance.set_node_poses(dest_animation_nodes);
				source_model_instance.set_node_poses(source_nodes);

				dest_model_instance.update_transform(math::transformd::identity());
				source_model_instance.update_transform(math::transformd::identity());
			}

			//	バインド行列を生成
			math::matrixf SourceParentMatrix;
			if (source_instance_node.parent_index != model_resource::invalid_id)
			{
				auto& parent_instance_node = source_model_instance.get_nodes()[source_instance_node.parent_index];
				SourceParentMatrix = math::matrixf(parent_instance_node.global_transform);
			}
			math::matrixf SourceMatrix(source_instance_node.global_transform);
			math::matrixf InverseSourceMatrix = SourceMatrix.inverted();

			math::matrixf DestParentMatrix;
			if (dest_instance_node.parent_index != model_resource::invalid_id)
			{
				auto& parent_instance_node = dest_model_instance.get_nodes()[dest_instance_node.parent_index];
				DestParentMatrix = math::matrixf(parent_instance_node.global_transform);
			}
			math::matrixf DestMatrix(dest_instance_node.global_transform);
			math::matrixf InverseDestParentMatrix = DestParentMatrix.inverted();

			math::matrixf DestRelativeMatrix = DestMatrix * InverseSourceMatrix;
			math::matrixf SourceRelativeParentMatrix = SourceParentMatrix * InverseDestParentMatrix;

			//	適応ロックフラグ
			bool is_lock_scale = false, is_lock_rotation = false, is_lock_translation = false;
			{
				auto& bone_data = convert_data.retarget_profile.bone_map.find(dest_instance_node.name)->second;
				is_lock_scale = bone_data.is_lock_scale;
				is_lock_rotation = bone_data.is_lock_rotation;
				is_lock_translation = bone_data.is_lock_translation;
			}

			//	フレーム数分処理
			std::vector<model_instance::node_animation> nodes;
			auto& dest_animation_node = dest_animation.nodes[dest_node_index];
			for (float sampling_time = 0; sampling_time <= source_animation.end_time; sampling_time += data_converter::animation_sampling_rate)
			{
				//	全体のアニメーション更新
				source_model_instance.compute_animation_nodes(nodes, source_animation_index, sampling_time);
				animation_node_adjust(source_model_instance, nodes);
				source_model_instance.set_node_poses(nodes);
				source_model_instance.update_transform(math::transformd::identity());

				//	親の初期行列からの差分を求めて納入
				math::matrixf LocalMatrix(source_instance_node.local_transform);
				LocalMatrix = DestRelativeMatrix * LocalMatrix * SourceRelativeParentMatrix;

				math::vector3f Scale, Translation;
				math::quaternionf Rotation;
				LocalMatrix.decompose(Scale, Rotation, Translation);

				auto& na = dest_animation_node.nodes.emplace_back();
				na.time = sampling_time;
				if (!is_lock_scale)
					na.scale = Scale.to_float3();
				else
					na.scale = dest_animation_nodes[dest_node_index].scale;
				if (!is_lock_rotation)
					na.rotation = Rotation.to_float4();
				else
					na.rotation = dest_animation_nodes[dest_node_index].rotation;
				if (!is_lock_translation)
					na.translation = Translation.to_float3();
				else
					na.translation = dest_animation_nodes[dest_node_index].translation;
			}
		}

		//	空いている所を埋める
		for (UINT index = 0; index < dest_animation.nodes.size(); ++index)
		{
			auto& animation_node = dest_animation.nodes[index];
			auto& da = dest_model_instance.get_resource()->get_nodes()[index];
			animation_node.node_name = da.name;
			if (!animation_node.nodes.empty())
				continue;

			auto& na = animation_node.nodes.emplace_back();
			na.time = 0.0f;
			na.scale = da.scale;
			na.rotation = da.rotation;
			na.translation = da.translation;
		}

		//	リソースに追加
		animation_resource.add_animation_data(dest_animation);
		return true;
	}

	//	リターゲッティング
	bool retargeting(animation_resource& animation_resource, const data_converter::gltf::convert_animation_data& convert_data)
	{
		gltf_loader loader;
		if (!loader.Load(convert_data.file_path))
			return false;

		//	アニメーション側のモデルリソース生成
		std::shared_ptr<model_resource> source_model_resource;
		{
			data_converter::gltf::convert_model_data convert_model_data;
			memset(&convert_model_data, 0, sizeof(convert_model_data));
			convert_model_data.file_path = convert_data.file_path;
			convert_model_data.is_change_coordinate_system = true;

			source_model_resource = std::make_shared<model_resource>();
			if (!convert(*source_model_resource, loader, convert_model_data))
				return false;	//	変換失敗時は何もしない
		}

		//	モデルインスタンス生成
		model_instance source_model_instance, dest_model_instance;
		source_model_instance = model_instance(source_model_resource);
		dest_model_instance = model_instance(convert_data.retarget_resource);

		//	リターゲッティング用に存在するノードリスト算出
		std::vector<std::pair<size_t, size_t>> retarget_node_indices;
		retarget_node_indices = compute_retarget_node_list(source_model_instance, dest_model_instance, convert_data.retarget_profile);

		//	アニメーションリターゲッティング
		auto& source_animations = source_model_instance.get_resource()->get_animations();
		for (size_t source_animation_index = 0; source_animation_index < source_animations.size(); ++source_animation_index)
		{
			//	デフォルト追加のアニメーションは除外
			if (source_animation_index == 0)
				continue;

			normal_retarget(animation_resource,
							source_model_instance,
							dest_model_instance,
							source_animation_index,
							retarget_node_indices,
							convert_data);
		}
		return true;
	}

}

bool data_converter::gltf::convert(model_resource& resource, const convert_model_data& convert_data)
{
	gltf_loader loader;
	if (!loader.Load(convert_data.file_path))
		return false;
	if (!::convert(resource, loader, convert_data))
		return false;

	return true;
}

bool data_converter::gltf::convert(animation_resource& resource, const convert_animation_data& convert_data)
{
	//	リターゲッティング処理が必要な場合
	if (convert_data.retarget_resource)
	{
		return ::retargeting(resource, convert_data);
	}

	gltf_loader loader;
	if (!loader.Load(convert_data.file_path))
		return false;

	//	ノードとアニメーション読込
	std::vector<gltf_loader::node_data> gltf_nodes;
	std::vector<gltf_loader::animation_data> gltf_animations;
	loader.load_nodes(gltf_nodes);
	loader.load_animations(gltf_animations, gltf_nodes);
	std::vector<sorted_node> sorted_nodes = compute_sort_node_map(gltf_nodes);

	//	登録されているアニメーション情報を適応
	for (auto& gltf_animation : gltf_animations)
	{
		animation::data animation;
		animation.name = gltf_animation.name;
		animation.end_time = compute_end_time(gltf_animation);

		//	ノード情報のみ登録
		animation.nodes.resize(sorted_nodes.size());
		for (float sampling_time = 0; sampling_time <= animation.end_time; sampling_time += data_converter::animation_sampling_rate)
		{
			for (UINT node_index = 0; node_index < gltf_animation.nodes.size(); ++node_index)
			{
				UINT animation_node_index = sorted_nodes[node_index].dest_index;

				auto pose = gltf_animation.nodes[node_index].calculate(sampling_time);
				auto& na = animation.nodes[animation_node_index].nodes.emplace_back();
				na.time = sampling_time;
				na.scale = pose.scale;
				na.rotation = pose.rotation;
				na.translation = pose.translation;
			}
		}

		//	リソースに納入
		resource.add_animation_data(animation);
	}
	return true;
}

#endif	//	defined(USE_IMGUI)
