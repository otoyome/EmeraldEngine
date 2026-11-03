#include "model_resource_utility.h"
#include "model_buffer_pool.h"

#include <cmath>
#include <cstring>
#include <map>
#include <tuple>
#include <functional>
#include <algorithm>

#include "graphics_utility.h"
#include "render_data.h"
#include "common_pipeline_states.h"

#include "meshoptimizer/meshoptimizer.h"

#if defined(USE_IMGUI)

namespace
{
	//	インデックス数を3の倍数に切り詰め
	UINT sanitize_triangle_list_count(UINT index_count)
	{
		return index_count - (index_count % 3);
	}

	//	メッシュからメッシュレット生成
	void generate_meshlets_for_geometry(
		const std::unordered_map<semantic, buffer_view>& vertex_buffer_views,
		const buffer_view& index_buffer_view,
		std::vector<model_resource::mesh::meshlet>& out_meshlets,
		std::vector<UINT>& out_meshlet_vertices,
		std::vector<uint8_t>& out_meshlet_triangles)
	{
		out_meshlets.clear();
		out_meshlet_vertices.clear();
		out_meshlet_triangles.clear();

		static const semantic position_semantic("POSITION", 0);

		const size_t max_vertices = 64;
		const size_t max_triangles = 124;
		const float cone_weight = 0.5f;

		auto position_it = vertex_buffer_views.find(position_semantic);
		if (position_it == vertex_buffer_views.end() ||
			index_buffer_view.row_data.empty())
			return;

		UINT index_count = static_cast<UINT>(index_buffer_view.count());
		index_count = sanitize_triangle_list_count(index_count);
		if (index_count == 0)
			return;

		//	インデックスバッファを32bitへ展開する(R16_UINT等、R32_UINT以外のフォーマットにも対応する)
		std::vector<UINT> widened_indices;
		const UINT* indices = nullptr;
		switch (graphics_utility::get_byte_offset_from_format(index_buffer_view.format))
		{
			case sizeof(UINT):
			{
				indices = reinterpret_cast<const UINT*>(index_buffer_view.row_data.data());
				break;
			}

			case sizeof(USHORT):
			{
				const USHORT* source = reinterpret_cast<const USHORT*>(index_buffer_view.row_data.data());
				widened_indices.assign(source, source + index_count);
				indices = widened_indices.data();
				break;
			}

			case sizeof(UCHAR):
			{
				const UCHAR* source = index_buffer_view.row_data.data();
				widened_indices.assign(source, source + index_count);
				indices = widened_indices.data();
				break;
			}

			default:
				//	未対応のインデックスフォーマット
				return;
		}

		auto& position_view = position_it->second;
		const math::float3* positions = reinterpret_cast<const math::float3*>(position_view.row_data.data());
		UINT vertex_count = static_cast<UINT>(position_view.count());

		if (vertex_count == 0)
			return;

		size_t max_meshlets = meshopt_buildMeshletsBound(index_count, max_vertices, max_triangles);
		if (max_meshlets == 0)
			return;

		std::vector<meshopt_Meshlet> meshlets(max_meshlets);
		std::vector<unsigned int> meshlet_vertices(max_meshlets * max_vertices);
		std::vector<unsigned char> meshlet_triangles(max_meshlets * max_triangles * 3);

		size_t meshlet_count = meshopt_buildMeshlets(
			meshlets.data(), meshlet_vertices.data(), meshlet_triangles.data(),
			indices, index_count,
			reinterpret_cast<const float*>(positions), vertex_count, sizeof(math::float3),
			max_vertices, max_triangles, cone_weight);

		if (meshlet_count == 0)
			return;

		meshlets.resize(meshlet_count);
		const meshopt_Meshlet& last = meshlets.back();
		meshlet_vertices.resize(static_cast<size_t>(last.vertex_offset) + last.vertex_count);
		meshlet_triangles.resize(static_cast<size_t>(last.triangle_offset) + ((static_cast<size_t>(last.triangle_count) * 3 + 3) & ~size_t(3)));

		//	バウンディングスフィア/法線コーン算出
		out_meshlets.reserve(meshlet_count);
		for (auto& m : meshlets)
		{
			meshopt_Bounds bounds = meshopt_computeMeshletBounds(
				&meshlet_vertices[m.vertex_offset], &meshlet_triangles[m.triangle_offset],
				m.triangle_count,
				reinterpret_cast<const float*>(positions), vertex_count, sizeof(math::float3));

			model_resource::mesh::meshlet dst{};
			dst.vertex_offset = m.vertex_offset;
			dst.triangle_offset = m.triangle_offset;
			dst.vertex_count = m.vertex_count;
			dst.triangle_count = m.triangle_count;
			dst.bounds_center = { bounds.center[0], bounds.center[1], bounds.center[2] };
			dst.bounds_radius = bounds.radius;
			dst.cone_apex = { bounds.cone_apex[0], bounds.cone_apex[1], bounds.cone_apex[2] };
			dst.cone_axis = { bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2] };
			dst.cone_cutoff = bounds.cone_cutoff;
			out_meshlets.emplace_back(dst);
		}

		out_meshlet_vertices = std::move(meshlet_vertices);
		out_meshlet_triangles = std::move(meshlet_triangles);
	}
}

void model_resource_utility::combine_unskinned_meshes(model_resource& resource, math::int3 grid_divisions)
{
	auto& meshes = resource.get_meshes_for_editor();
	auto& nodes = resource.get_nodes_for_editor();

	const int div_x = std::max(1, grid_divisions.x);
	const int div_y = std::max(1, grid_divisions.y);
	const int div_z = std::max(1, grid_divisions.z);

	const semantic position_semantic("POSITION", 0);
	const semantic joints_semantic("JOINTS", 0);
	const semantic weights_semantic("WEIGHTS", 0);

	//	ノードのローカル/グローバル変換
	auto local_matrix = [](const model_resource::node& n)
	{
		return math::matrixf::affine_transformation(n.scale, math::quaternionf(n.rotation), n.translation);
	};
	std::function<math::matrixf(size_t)> global_matrix = [&](size_t index) -> math::matrixf
	{
		if (index == model_resource::invalid_id || index >= nodes.size())
			return math::matrixf();
		return local_matrix(nodes[index]) * global_matrix(nodes[index].parent_index);
	};

	//	結合対象判定
	auto is_combinable = [&](const model_resource::mesh& m)
	{
		if (!m.bones.empty()) return false;
		if (m.index_buffer_view.count() == 0) return false;
		if (!m.morph_meshes.empty()) return false;
		if (m.vertex_buffer_views.count(joints_semantic) != 0) return false;
		if (m.vertex_buffer_views.count(weights_semantic) != 0) return false;
		if (m.vertex_buffer_views.count(position_semantic) == 0) return false;
		return true;
	};

	//	対象メッシュのワールド中心と全体 AABB を求める
	struct combinable_info { size_t mesh_index; math::float3 center; };
	std::vector<combinable_info> combinables;
	std::vector<size_t> passthrough;
	math::float3 world_min = { +FLT_MAX, +FLT_MAX, +FLT_MAX };
	math::float3 world_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
	for (size_t mesh_index = 0; mesh_index < meshes.size(); ++mesh_index)
	{
		if (!is_combinable(meshes[mesh_index]))
		{
			passthrough.push_back(mesh_index);
			continue;
		}

		const math::matrixf gm = global_matrix(meshes[mesh_index].node_index);
		const math::float3 lmin = meshes[mesh_index].bounds_min;
		const math::float3 lmax = meshes[mesh_index].bounds_max;
		math::float3 wmin = { +FLT_MAX, +FLT_MAX, +FLT_MAX };
		math::float3 wmax = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (int corner = 0; corner < 8; ++corner)
		{
			const math::vector3f p((corner & 1) ? lmax.x : lmin.x,
								   (corner & 2) ? lmax.y : lmin.y,
								   (corner & 4) ? lmax.z : lmin.z);
			const math::vector3f wp = math::vector3f::transform_coord(p, gm);
			wmin.x = std::min(wmin.x, wp.x); wmin.y = std::min(wmin.y, wp.y); wmin.z = std::min(wmin.z, wp.z);
			wmax.x = std::max(wmax.x, wp.x); wmax.y = std::max(wmax.y, wp.y); wmax.z = std::max(wmax.z, wp.z);
		}

		combinable_info info;
		info.mesh_index = mesh_index;
		info.center = { (wmin.x + wmax.x) * 0.5f, (wmin.y + wmax.y) * 0.5f, (wmin.z + wmax.z) * 0.5f };
		combinables.push_back(info);

		world_min.x = std::min(world_min.x, wmin.x); world_min.y = std::min(world_min.y, wmin.y); world_min.z = std::min(world_min.z, wmin.z);
		world_max.x = std::max(world_max.x, wmax.x); world_max.y = std::max(world_max.y, wmax.y); world_max.z = std::max(world_max.z, wmax.z);
	}

	//	結合の余地なし
	if (combinables.size() < 2)
		return;

	const math::float3 world_size =
	{
		std::max(world_max.x - world_min.x, 1e-4f),
		std::max(world_max.y - world_min.y, 1e-4f),
		std::max(world_max.z - world_min.z, 1e-4f),
	};

	//	頂点レイアウトの集合をハッシュ化
	auto layout_signature = [](const model_resource::mesh& m)
	{
		std::vector<std::pair<system_utility::hash_key, int>> parts;
		for (auto& vbv : m.vertex_buffer_views)
			parts.emplace_back(vbv.first.get_hash_key(), static_cast<int>(vbv.second.format));
		std::sort(parts.begin(), parts.end());
		std::size_t hash = 1469598103934665603ull;
		for (auto& part : parts)
		{
			hash = (hash ^ static_cast<std::size_t>(part.first)) * 1099511628211ull;
			hash = (hash ^ static_cast<std::size_t>(part.second)) * 1099511628211ull;
		}
		return hash;
	};

	//	グループ化
	std::map<std::tuple<size_t, std::size_t, int>, std::vector<size_t>> groups;
	for (auto& info : combinables)
	{
		const auto& m = meshes[info.mesh_index];
		int gx = std::clamp(static_cast<int>((info.center.x - world_min.x) / world_size.x * div_x), 0, div_x - 1);
		int gy = std::clamp(static_cast<int>((info.center.y - world_min.y) / world_size.y * div_y), 0, div_y - 1);
		int gz = std::clamp(static_cast<int>((info.center.z - world_min.z) / world_size.z * div_z), 0, div_z - 1);
		int cell = gx + gy * div_x + gz * div_x * div_y;
		groups[std::make_tuple(m.material_index, layout_signature(m), cell)].push_back(info.mesh_index);
	}

	std::vector<model_resource::mesh> out_meshes;
	const size_t bone_cap = common_pipeline_states::mesh_bone_max;
	UINT combine_counter = 0;

	for (auto& group_pair : groups)
	{
		auto& group = group_pair.second;
		if (group.size() == 1)
		{
			out_meshes.push_back(meshes[group[0]]);
			continue;
		}

		//	mesh_bone_max ごとに分割して結合メッシュを作る
		for (size_t chunk_begin = 0; chunk_begin < group.size(); chunk_begin += bone_cap)
		{
			const size_t chunk_end = std::min(group.size(), chunk_begin + bone_cap);
			const size_t chunk_count = chunk_end - chunk_begin;
			if (chunk_count == 1)
			{
				out_meshes.push_back(meshes[group[chunk_begin]]);
				continue;
			}

			//	合成ノードを追加
			nodes.emplace_back();
			nodes.back().name = "combined_mesh_" + std::to_string(combine_counter++);
			nodes.back().parent_index = model_resource::invalid_id;
			const size_t new_node_index = nodes.size() - 1;

			const model_resource::mesh& base_src = meshes[group[chunk_begin]];

			out_meshes.emplace_back();
			model_resource::mesh& dst = out_meshes.back();
			dst.material_index = base_src.material_index;
			dst.node_index = new_node_index;

			//	ボーン1本
			dst.bones.reserve(chunk_count);
			for (size_t group_index = chunk_begin; group_index < chunk_end; ++group_index)
			{
				model_resource::bone bone;
				bone.node_index = meshes[group[group_index]].node_index;
				dst.bones.push_back(bone);
			}

			//	インデックスバッファ（R32_UINT）
			dst.index_buffer_view.format = DXGI_FORMAT_R32_UINT;
			dst.index_buffer_view.stride_in_bytes = sizeof(UINT);
			dst.index_buffer_view.size_in_bytes = 0;
			dst.index_buffer_view.offset_in_bytes = 0;

			//	JOINTS / WEIGHTS + ソース側の全 semantic を空で用意
			{
				buffer_view joints_view;
				joints_view.format = DXGI_FORMAT_R32G32B32A32_UINT;
				joints_view.stride_in_bytes = sizeof(math::uint4);
				dst.vertex_buffer_views.insert(std::make_pair(joints_semantic, joints_view));

				buffer_view weights_view;
				weights_view.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				weights_view.stride_in_bytes = sizeof(math::float4);
				dst.vertex_buffer_views.insert(std::make_pair(weights_semantic, weights_view));
			}
			for (auto& src_vbv : base_src.vertex_buffer_views)
			{
				buffer_view view;
				view.format = src_vbv.second.format;
				view.stride_in_bytes = src_vbv.second.stride_in_bytes;
				dst.vertex_buffer_views.insert(std::make_pair(src_vbv.first, view));
			}

			//	各メッシュを連結
			UINT base_vertex = 0;
			for (size_t bone_index = 0; bone_index < chunk_count; ++bone_index)
			{
				const model_resource::mesh& src = meshes[group[chunk_begin + bone_index]];
				const UINT src_index_count = static_cast<UINT>(src.index_buffer_view.count());
				const UINT src_vertex_count = static_cast<UINT>(src.vertex_buffer_views.begin()->second.count());

				//	インデックス
				{
					const size_t offset = dst.index_buffer_view.row_data.size();
					dst.index_buffer_view.row_data.resize(offset + static_cast<size_t>(src_index_count) * sizeof(UINT));
					UINT* out_indices = reinterpret_cast<UINT*>(dst.index_buffer_view.row_data.data() + offset);
					if (src.index_buffer_view.stride_in_bytes == 2)
					{
						const USHORT* s = reinterpret_cast<const USHORT*>(src.index_buffer_view.row_data.data());
						for (UINT i = 0; i < src_index_count; ++i) out_indices[i] = base_vertex + s[i];
					}
					else if (src.index_buffer_view.stride_in_bytes == 1)
					{
						const UCHAR* s = reinterpret_cast<const UCHAR*>(src.index_buffer_view.row_data.data());
						for (UINT i = 0; i < src_index_count; ++i) out_indices[i] = base_vertex + s[i];
					}
					else
					{
						const UINT* s = reinterpret_cast<const UINT*>(src.index_buffer_view.row_data.data());
						for (UINT i = 0; i < src_index_count; ++i) out_indices[i] = base_vertex + s[i];
					}
					dst.index_buffer_view.size_in_bytes = dst.index_buffer_view.row_data.size();
				}

				//	JOINTS = [boneIndex, 0, 0, 0]
				{
					buffer_view& joints_view = dst.vertex_buffer_views.at(joints_semantic);
					const size_t offset = joints_view.row_data.size();
					joints_view.row_data.resize(offset + static_cast<size_t>(src_vertex_count) * sizeof(math::uint4));
					math::uint4* out_joints = reinterpret_cast<math::uint4*>(joints_view.row_data.data() + offset);
					for (UINT i = 0; i < src_vertex_count; ++i)
						out_joints[i] = math::uint4(static_cast<UINT>(bone_index), 0, 0, 0);
					joints_view.size_in_bytes = joints_view.row_data.size();
				}
				//	WEIGHTS = [1, 0, 0, 0]
				{
					buffer_view& weights_view = dst.vertex_buffer_views.at(weights_semantic);
					const size_t offset = weights_view.row_data.size();
					weights_view.row_data.resize(offset + static_cast<size_t>(src_vertex_count) * sizeof(math::float4));
					math::float4* out_weights = reinterpret_cast<math::float4*>(weights_view.row_data.data() + offset);
					for (UINT i = 0; i < src_vertex_count; ++i)
						out_weights[i] = math::float4(1.0f, 0.0f, 0.0f, 0.0f);
					weights_view.size_in_bytes = weights_view.row_data.size();
				}
				//	その他の頂点バッファをそのまま連結
				for (auto& src_vbv : src.vertex_buffer_views)
				{
					auto it = dst.vertex_buffer_views.find(src_vbv.first);
					if (it == dst.vertex_buffer_views.end())
						continue;
					const size_t offset = it->second.row_data.size();
					it->second.row_data.resize(offset + src_vbv.second.row_data.size());
					std::memcpy(it->second.row_data.data() + offset, src_vbv.second.row_data.data(), src_vbv.second.row_data.size());
					it->second.size_in_bytes = it->second.row_data.size();
				}

				base_vertex += src_vertex_count;
			}

			//	AABB 再計算
			{
				std::vector<math::matrixf> bone_transforms(dst.bones.size());
				for (size_t bone_index = 0; bone_index < dst.bones.size(); ++bone_index)
				{
					bone_transforms[bone_index] =
						math::matrixf(dst.bones[bone_index].offset_matrix) * global_matrix(dst.bones[bone_index].node_index);
				}

				auto pos_it = dst.vertex_buffer_views.find(position_semantic);
				auto joints_it = dst.vertex_buffer_views.find(joints_semantic);
				math::float3 bounds_min = { FLT_MAX, FLT_MAX, FLT_MAX };
				math::float3 bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
				if (pos_it != dst.vertex_buffer_views.end() && joints_it != dst.vertex_buffer_views.end())
				{
					const math::float3* positions = reinterpret_cast<const math::float3*>(pos_it->second.row_data.data());
					const math::uint4* joints = reinterpret_cast<const math::uint4*>(joints_it->second.row_data.data());
					const size_t vertex_count = pos_it->second.count();
					for (size_t i = 0; i < vertex_count; ++i)
					{
						const UINT bone = (joints[i].x < bone_transforms.size()) ? joints[i].x : 0;
						const math::vector3f wp = math::vector3f::transform_coord(positions[i], bone_transforms[bone]);
						bounds_min.x = std::min(bounds_min.x, wp.x); bounds_min.y = std::min(bounds_min.y, wp.y); bounds_min.z = std::min(bounds_min.z, wp.z);
						bounds_max.x = std::max(bounds_max.x, wp.x); bounds_max.y = std::max(bounds_max.y, wp.y); bounds_max.z = std::max(bounds_max.z, wp.z);
					}
				}
				dst.bounds_min = bounds_min;
				dst.bounds_max = bounds_max;
			}

			//	GPU バッファ生成
			model_buffer_pool_manager::instance()->build_vertex_buffers(dst.vertex_buffer_views);
		}
	}

	//	非結合メッシュを後ろに積む
	for (size_t mesh_index : passthrough)
		out_meshes.push_back(meshes[mesh_index]);

	meshes = std::move(out_meshes);
}

void model_resource_utility::optimized_meshs(model_resource& resource)
{
	auto& meshes = resource.get_meshes_for_editor();
	int mesh_index = 0;
	for (auto& mesh : meshes)
	{
		if (mesh.index_buffer_view.count() == 0 ||
			mesh.vertex_buffer_views.empty() ||
			mesh.vertex_buffer_views.begin()->second.size_in_bytes == 0)
			continue;

		//	モーフがある場合はしない
		//	する場合はstreamsにモーフのやつを追加して対応する必要がある
		if (!mesh.morph_meshes.empty())
			continue;

		//	meshoptimizerのMulti系関数はstream_countが16以下でなければならない
		if (mesh.vertex_buffer_views.size() > 16)
			continue;

		//	インデックスリストを生成
		UINT sanitized_index_count = sanitize_triangle_list_count(static_cast<UINT>(mesh.index_buffer_view.count()));
		std::vector<UINT> indices;
		indices.reserve(sanitized_index_count);
		for (UINT index = 0; index < sanitized_index_count; ++index)
		{
			if (mesh.index_buffer_view.stride_in_bytes == 2)
			{
				indices.push_back(reinterpret_cast<USHORT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else if (mesh.index_buffer_view.stride_in_bytes == 1)
			{
				indices.push_back(reinterpret_cast<UCHAR*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else
			{
				indices.push_back(reinterpret_cast<UINT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
		}

		{
			std::size_t vertex_count = mesh.vertex_buffer_views.begin()->second.count();

			//	stream生成
			std::vector<meshopt_Stream> streams;
			std::size_t position_stream_index = 0;
			{
				bool position_find = false;
				for (auto& vbv : mesh.vertex_buffer_views)
				{
					auto& stream = streams.emplace_back();
					stream.data = vbv.second.row_data.data();
					stream.size = vbv.second.stride_in_bytes;
					stream.stride = vbv.second.stride_in_bytes;
					if (!position_find)
					{
						if (vbv.first == semantic("POSITION", 0))
						{
							position_find = true;
						}
						else
						{
							++position_stream_index;
						}
					}
				}
			}

			//	重複頂点最適化
			std::vector<UINT> remap(vertex_count);
			size_t new_vertex_count = meshopt_generateVertexRemapMulti(
				remap.data(), indices.data(), indices.size(),
				vertex_count, streams.data(), streams.size());

			//	頂点バッファ最適化
			std::vector<std::vector<UCHAR>> vertex_buffer;
			vertex_buffer.reserve(streams.size());
			for (auto& stream : streams)
			{
				auto& new_data = vertex_buffer.emplace_back(new_vertex_count * stream.stride);
				meshopt_remapVertexBuffer(new_data.data(),
						stream.data, vertex_count, stream.stride, remap.data());
			}

			//	インデックスバッファ最適化
			meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), remap.data());

			//	頂点キャッシュ最適化
			meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), new_vertex_count);

			//	オーバードロー最適化
			auto pos = mesh.vertex_buffer_views.find(semantic("POSITION", 0));
			if (pos != mesh.vertex_buffer_views.end())
			{
				meshopt_optimizeOverdraw(
					indices.data(),
					indices.data(),
					indices.size(),
					reinterpret_cast<const float*>(vertex_buffer[position_stream_index].data()),
					new_vertex_count,
					pos->second.stride_in_bytes,
					1.05f);
			}

			//	
			std::vector<UINT> fetch_remap(new_vertex_count);
			meshopt_optimizeVertexFetchRemap(fetch_remap.data(), indices.data(), indices.size(), new_vertex_count);

			//	ベース頂点バッファ
			for (UINT index = 0; index < streams.size(); ++index)
			{
				auto& stream = streams[index];
				auto& vertices = vertex_buffer[index];
				std::vector<UCHAR> temp(vertices.size());
				meshopt_remapVertexBuffer(temp.data(), vertices.data(), new_vertex_count,
										  stream.stride, fetch_remap.data());
				vertices.swap(temp);
			}

			//	インデックスバッファ最適化
			meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), fetch_remap.data());

			//	インデックスバッファ書き戻し
			{
				mesh.index_buffer_view.row_data.resize(indices.size() * sizeof(UINT));
				memcpy_s(mesh.index_buffer_view.row_data.data(), mesh.index_buffer_view.row_data.size(),
						 indices.data(), indices.size() * sizeof(UINT));
				mesh.index_buffer_view.format = DXGI_FORMAT_R32_UINT;
				mesh.index_buffer_view.size_in_bytes = indices.size() * sizeof(UINT);
				mesh.index_buffer_view.stride_in_bytes = sizeof(UINT);
			}

			//	頂点バッファ書き戻し
			{
				UINT index = 0;
				for (auto& vbv : mesh.vertex_buffer_views)
				{
					auto& stream = streams[index];
					auto& vertices = vertex_buffer[index];
					vbv.second.row_data.resize(vertices.size());
					memcpy_s(vbv.second.row_data.data(), vbv.second.row_data.size(),
							 vertices.data(), vertices.size());
					vbv.second.size_in_bytes = vertices.size();
					++index;
				}
			}
		}
	}
}

void model_resource_utility::generate_shadow_meshes(model_resource& resource, bool regenerate_buffer)
{
	auto& meshes = resource.get_meshes_for_editor();
	for (auto& mesh : meshes)
	{
		if (mesh.index_buffer_view.count() == 0)
			continue;

		//	meshoptimizerのMulti系関数はstream_countが16以下でなければならない
		if (mesh.vertex_buffer_views.size() > 16)
			continue;

		//	インデックスリストを生成(3の倍数でない末尾は切り詰める)
		UINT sanitized_index_count = sanitize_triangle_list_count(static_cast<UINT>(mesh.index_buffer_view.count()));
		std::vector<UINT> indices;
		indices.reserve(sanitized_index_count);
		for (UINT index = 0; index < sanitized_index_count; ++index)
		{
			if (mesh.index_buffer_view.stride_in_bytes == 2)
			{
				indices.push_back(reinterpret_cast<USHORT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else if (mesh.index_buffer_view.stride_in_bytes == 1)
			{
				indices.push_back(reinterpret_cast<UCHAR*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else
			{
				indices.push_back(reinterpret_cast<UINT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
		}

		{
			//	stream生成
			std::vector<meshopt_Stream> streams;
			std::size_t vertex_count = 0, position_index = 0;
			{
				bool position_find = false;
				for (auto& vbv : mesh.vertex_buffer_views)
				{
					auto& stream = streams.emplace_back();
					stream.data = vbv.second.row_data.data();
					stream.size = vbv.second.stride_in_bytes;
					stream.stride = vbv.second.stride_in_bytes;
					if (vbv.second.count() > vertex_count)
					{
						vertex_count = vbv.second.count();
					}
					if (!position_find)
					{
						if (vbv.first == semantic("POSITION", 0))
						{
							position_find = true;
						}
						else
						{
							++position_index;
						}
					}
				}
			}

			if (vertex_count == 0)
				continue;

			//	重複頂点最適化
			meshopt_generateShadowIndexBufferMulti(indices.data(), indices.data(), indices.size(), vertex_count, streams.data(), streams.size());

			//	影のインデックスバッファ書き戻し
			{
				mesh.shadow_mesh.index_buffer_view.row_data.resize(indices.size() * sizeof(UINT));
				memcpy_s(mesh.shadow_mesh.index_buffer_view.row_data.data(),
						 mesh.shadow_mesh.index_buffer_view.row_data.size(),
						 indices.data(), indices.size() * sizeof(UINT));
				mesh.shadow_mesh.index_buffer_view.format = DXGI_FORMAT_R32_UINT;
				mesh.shadow_mesh.index_buffer_view.size_in_bytes = indices.size() * sizeof(UINT);
				mesh.shadow_mesh.index_buffer_view.stride_in_bytes = sizeof(UINT);
				//	メッシュレット生成
				generate_meshlets_for_geometry(mesh.vertex_buffer_views, mesh.shadow_mesh.index_buffer_view,
					mesh.shadow_mesh.meshlets, mesh.shadow_mesh.meshlet_vertices, mesh.shadow_mesh.meshlet_triangles);
			}
		}
	}

	if (regenerate_buffer)
	{
		resource.rebuild_meshlet_data();
	}
}

void model_resource_utility::generate_lod_meshs(model_resource& resource, int max_lod_level, bool regeneratge_buffer)
{
	auto& meshes = resource.get_meshes_for_editor();
	for (auto& mesh : meshes)
	{
		if (mesh.index_buffer_view.count() == 0)
			continue;

		//	インデックスリストを生成(3の倍数でない末尾は切り詰める)
		UINT sanitized_index_count = sanitize_triangle_list_count(static_cast<UINT>(mesh.index_buffer_view.count()));
		std::vector<UINT> indices;
		indices.reserve(sanitized_index_count);
		for (UINT index = 0; index < sanitized_index_count; ++index)
		{
			if (mesh.index_buffer_view.stride_in_bytes == 2)
			{
				indices.push_back(reinterpret_cast<USHORT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else if (mesh.index_buffer_view.stride_in_bytes == 1)
			{
				indices.push_back(reinterpret_cast<UCHAR*>(mesh.index_buffer_view.row_data.data())[index]);
			}
			else
			{
				indices.push_back(reinterpret_cast<UINT*>(mesh.index_buffer_view.row_data.data())[index]);
			}
		}

		{
			//	stream生成
			std::vector<meshopt_Stream> streams;
			std::size_t vertex_count = 0, position_index = 0;
			{
				bool position_find = false;
				for (auto& vbv : mesh.vertex_buffer_views)
				{
					auto& stream = streams.emplace_back();
					stream.data = vbv.second.row_data.data();
					stream.size = vbv.second.stride_in_bytes;
					stream.stride = vbv.second.stride_in_bytes;
					if (vbv.second.count() > vertex_count)
					{
						vertex_count = vbv.second.count();
					}
					if (!position_find)
					{
						if (vbv.first == semantic("POSITION", 0))
						{
							position_find = true;
						}
						else
						{
							++position_index;
						}
					}
				}
			}

			if (vertex_count == 0)
				continue;

			//	重複頂点最適化
			for (auto& old_lod_mesh : mesh.lod_meshs)
			{
				if (!old_lod_mesh.meshlet_draw_infos.empty())
				{
					model_buffer_pool_manager::instance()->free_meshlet_data(
						old_lod_mesh.meshlet_vertex_base, static_cast<UINT>(old_lod_mesh.meshlet_vertices.size()),
						old_lod_mesh.meshlet_triangle_base, static_cast<UINT>(old_lod_mesh.meshlet_triangles.size()));
				}
			}
			mesh.lod_meshs.clear();

			for (INT lod = 0; lod < max_lod_level; ++lod)
			{
				std::vector<UINT> lod_indicesLod(indices.size());
				UINT target_indices_count = static_cast<UINT>(indices.size() / pow(2, lod + 1));
				float target_error = static_cast<float>(lod) / max_lod_level;
				lod_indicesLod.resize(meshopt_simplify(lod_indicesLod.data(),
									 indices.data(),
									 indices.size(),
									 reinterpret_cast<const float*>(streams[position_index].data),
									 vertex_count,
									 streams[position_index].stride,
									 target_indices_count,
									 target_error));

				model_resource::mesh::sub_mesh sub_mesh;
				sub_mesh.index_buffer_view.row_data.resize(lod_indicesLod.size() * sizeof(UINT));
				memcpy_s(sub_mesh.index_buffer_view.row_data.data(),
						 sub_mesh.index_buffer_view.row_data.size(),
						 lod_indicesLod.data(), lod_indicesLod.size() * sizeof(UINT));
				sub_mesh.index_buffer_view.format = DXGI_FORMAT_R32_UINT;
				sub_mesh.index_buffer_view.size_in_bytes = lod_indicesLod.size() * sizeof(UINT);
				sub_mesh.index_buffer_view.stride_in_bytes = sizeof(UINT);
				//	メッシュレット生成
				generate_meshlets_for_geometry(mesh.vertex_buffer_views, sub_mesh.index_buffer_view,
					sub_mesh.meshlets, sub_mesh.meshlet_vertices, sub_mesh.meshlet_triangles);

				mesh.lod_meshs.emplace_back(sub_mesh);
			}
		}
	}

	if (regeneratge_buffer)
	{
		resource.rebuild_meshlet_data();
	}
}

void model_resource_utility::clear_lod_meshs(model_resource& resource)
{
	for (auto& mesh : resource.get_meshes_for_editor())
	{
		for (auto& lod_mesh : mesh.lod_meshs)
		{
			if (!lod_mesh.meshlet_draw_infos.empty())
			{
				model_buffer_pool_manager::instance()->free_meshlet_data(
					lod_mesh.meshlet_vertex_base, static_cast<UINT>(lod_mesh.meshlet_vertices.size()),
					lod_mesh.meshlet_triangle_base, static_cast<UINT>(lod_mesh.meshlet_triangles.size()));
			}
		}
		mesh.lod_meshs.clear();
	}
}

void model_resource_utility::clear_shadow_meshes(model_resource& resource)
{
	for (auto& mesh : resource.get_meshes_for_editor())
	{
		if (!mesh.shadow_mesh.meshlet_draw_infos.empty())
		{
			model_buffer_pool_manager::instance()->free_meshlet_data(
				mesh.shadow_mesh.meshlet_vertex_base, static_cast<UINT>(mesh.shadow_mesh.meshlet_vertices.size()),
				mesh.shadow_mesh.meshlet_triangle_base, static_cast<UINT>(mesh.shadow_mesh.meshlet_triangles.size()));
		}
		mesh.shadow_mesh = {};
	}
}

void model_resource_utility::optimize_meshs(model_resource& resource)
{
	for (auto& mesh : resource.get_meshes_for_editor())
	{
		model_buffer_pool_manager::instance()->free_vertex_buffers(mesh.vertex_buffer_views);

		if (!mesh.meshlet_draw_infos.empty())
		{
			model_buffer_pool_manager::instance()->free_meshlet_data(
				mesh.meshlet_vertex_base, static_cast<UINT>(mesh.meshlet_vertices.size()),
				mesh.meshlet_triangle_base, static_cast<UINT>(mesh.meshlet_triangles.size()));
		}
		mesh.meshlets.clear();
		mesh.meshlet_vertices.clear();
		mesh.meshlet_triangles.clear();
		mesh.meshlet_draw_infos.clear();
	}

	optimized_meshs(resource);

	for (auto& mesh : resource.get_meshes_for_editor())
	{
		model_buffer_pool_manager::instance()->build_vertex_buffers(mesh.vertex_buffer_views);
	}

	clear_lod_meshs(resource);
	clear_shadow_meshes(resource);
}



void model_resource_utility::generate_meshlets(model_resource& resource)
{
	for (auto& mesh : resource.get_meshes_for_editor())
	{
		generate_meshlets_for_geometry(mesh.vertex_buffer_views, mesh.index_buffer_view,
			mesh.meshlets, mesh.meshlet_vertices, mesh.meshlet_triangles);

		generate_meshlets_for_geometry(mesh.vertex_buffer_views, mesh.shadow_mesh.index_buffer_view,
			mesh.shadow_mesh.meshlets, mesh.shadow_mesh.meshlet_vertices, mesh.shadow_mesh.meshlet_triangles);

		for (auto& lod_mesh : mesh.lod_meshs)
		{
			generate_meshlets_for_geometry(mesh.vertex_buffer_views, lod_mesh.index_buffer_view,
				lod_mesh.meshlets, lod_mesh.meshlet_vertices, lod_mesh.meshlet_triangles);
		}
	}

	//	GPU側へ反映
	resource.rebuild_meshlet_data();
}

namespace
{
	//	プリミティブ生成用の頂点
	struct primitive_vertex
	{
		math::float3 position;
		math::float3 normal;
		math::float2 texcoord;
	};

	//	プリミティブ生成用のメッシュ
	struct primitive_geometry
	{
		std::vector<primitive_vertex> vertices;
		std::vector<UINT> indices;

		UINT add_vertex(const math::float3& position, const math::float3& normal, const math::float2& texcoord)
		{
			vertices.push_back({ position, normal, texcoord });
			return static_cast<UINT>(vertices.size() - 1);
		}

		//	三角形を追加する(向きは finalize でそろえるので、ここでは気にしない)
		void add_triangle(UINT a, UINT b, UINT c)
		{
			indices.push_back(a);
			indices.push_back(b);
			indices.push_back(c);
		}

		//	a-b-c-d の順に並んだ四角形を追加する
		void add_quad(UINT a, UINT b, UINT c, UINT d)
		{
			add_triangle(a, b, c);
			add_triangle(a, c, d);
		}
	};

	//	左手座標系で FrontCounterClockwise = TRUE なので、表の三角形は外積 (b-a)x(c-a) が法線と逆向きになる
	constexpr bool front_face_cross_against_normal = true;

	math::float3 sub3(const math::float3& l, const math::float3& r) { return { l.x - r.x, l.y - r.y, l.z - r.z }; }
	math::float3 cross3(const math::float3& l, const math::float3& r) { return { l.y * r.z - l.z * r.y, l.z * r.x - l.x * r.z, l.x * r.y - l.y * r.x }; }
	float dot3(const math::float3& l, const math::float3& r) { return l.x * r.x + l.y * r.y + l.z * r.z; }
	math::float3 normalize3(const math::float3& value)
	{
		const float length = std::sqrt(dot3(value, value));
		if (length <= 1e-12f)
			return { 0, 0, 0 };
		return { value.x / length, value.y / length, value.z / length };
	}

	//	面積の無い三角形を除き、表の向きをそろえる
	void orient_triangles(primitive_geometry& geometry)
	{
		std::vector<UINT> indices;
		indices.reserve(geometry.indices.size());
		for (size_t index = 0; index + 2 < geometry.indices.size(); index += 3)
		{
			UINT a = geometry.indices[index + 0];
			UINT b = geometry.indices[index + 1];
			UINT c = geometry.indices[index + 2];
			const auto& va = geometry.vertices[a];
			const auto& vb = geometry.vertices[b];
			const auto& vc = geometry.vertices[c];

			const math::float3 face = cross3(sub3(vb.position, va.position), sub3(vc.position, va.position));
			if (dot3(face, face) <= 1e-12f)
				continue;

			const math::float3 normal = { va.normal.x + vb.normal.x + vc.normal.x, va.normal.y + vb.normal.y + vc.normal.y, va.normal.z + vb.normal.z + vc.normal.z };
			const bool against = dot3(face, normal) < 0.0f;
			if (against != front_face_cross_against_normal)
				std::swap(b, c);

			indices.push_back(a);
			indices.push_back(b);
			indices.push_back(c);
		}
		geometry.indices = std::move(indices);
	}

	//	UV から接線を求める(w は従法線 = cross(法線, 接線) * w の向き)
	std::vector<math::float4> compute_primitive_tangents(const primitive_geometry& geometry)
	{
		std::vector<math::float3> tangents(geometry.vertices.size(), math::float3(0, 0, 0));
		std::vector<math::float3> bitangents(geometry.vertices.size(), math::float3(0, 0, 0));
		for (size_t index = 0; index + 2 < geometry.indices.size(); index += 3)
		{
			const UINT ids[3] = { geometry.indices[index + 0], geometry.indices[index + 1], geometry.indices[index + 2] };
			const auto& v0 = geometry.vertices[ids[0]];
			const auto& v1 = geometry.vertices[ids[1]];
			const auto& v2 = geometry.vertices[ids[2]];

			const math::float3 e1 = sub3(v1.position, v0.position);
			const math::float3 e2 = sub3(v2.position, v0.position);
			const float du1 = v1.texcoord.x - v0.texcoord.x;
			const float dv1 = v1.texcoord.y - v0.texcoord.y;
			const float du2 = v2.texcoord.x - v0.texcoord.x;
			const float dv2 = v2.texcoord.y - v0.texcoord.y;
			const float determinant = du1 * dv2 - du2 * dv1;
			if (std::abs(determinant) <= 1e-12f)
				continue;

			const float r = 1.0f / determinant;
			const math::float3 tangent = { (e1.x * dv2 - e2.x * dv1) * r, (e1.y * dv2 - e2.y * dv1) * r, (e1.z * dv2 - e2.z * dv1) * r };
			const math::float3 bitangent = { (e2.x * du1 - e1.x * du2) * r, (e2.y * du1 - e1.y * du2) * r, (e2.z * du1 - e1.z * du2) * r };
			for (UINT id : ids)
			{
				tangents[id] = { tangents[id].x + tangent.x, tangents[id].y + tangent.y, tangents[id].z + tangent.z };
				bitangents[id] = { bitangents[id].x + bitangent.x, bitangents[id].y + bitangent.y, bitangents[id].z + bitangent.z };
			}
		}

		std::vector<math::float4> result(geometry.vertices.size());
		for (size_t index = 0; index < geometry.vertices.size(); ++index)
		{
			const math::float3& normal = geometry.vertices[index].normal;
			//	法線に直交させる
			const float d = dot3(normal, tangents[index]);
			math::float3 tangent = normalize3({ tangents[index].x - normal.x * d, tangents[index].y - normal.y * d, tangents[index].z - normal.z * d });
			if (dot3(tangent, tangent) <= 0.0f)
				tangent = normalize3(cross3(std::abs(normal.y) < 0.99f ? math::float3(0, 1, 0) : math::float3(1, 0, 0), normal));
			const float w = dot3(cross3(normal, tangent), bitangents[index]) < 0.0f ? -1.0f : 1.0f;
			result[index] = { tangent.x, tangent.y, tangent.z, w };
		}
		return result;
	}

	//	球(中心は高さ radius)
	void build_sphere(primitive_geometry& geometry, float radius, UINT segments)
	{
		const UINT rings = (std::max)(segments / 2, 2u);
		std::vector<UINT> grid((rings + 1) * (segments + 1));
		for (UINT ring = 0; ring <= rings; ++ring)
		{
			const float theta = math::constants<float>::pi * ring / rings;
			for (UINT segment = 0; segment <= segments; ++segment)
			{
				const float phi = math::constants<float>::pi * 2.0f * segment / segments;
				const math::float3 normal = { std::sin(theta) * std::cos(phi), std::cos(theta), std::sin(theta) * std::sin(phi) };
				const math::float3 position = { normal.x * radius, normal.y * radius + radius, normal.z * radius };
				grid[ring * (segments + 1) + segment] = geometry.add_vertex(position, normal, { static_cast<float>(segment) / segments, static_cast<float>(ring) / rings });
			}
		}
		for (UINT ring = 0; ring < rings; ++ring)
		{
			for (UINT segment = 0; segment < segments; ++segment)
			{
				const UINT a = grid[ring * (segments + 1) + segment];
				const UINT b = grid[ring * (segments + 1) + segment + 1];
				const UINT c = grid[(ring + 1) * (segments + 1) + segment + 1];
				const UINT d = grid[(ring + 1) * (segments + 1) + segment];
				geometry.add_quad(a, b, c, d);
			}
		}
	}

	//	四角い面を1枚追加する(center を中心に、u_axis・v_axis 方向へ半分ずつ広げる)
	void add_face(primitive_geometry& geometry, const math::float3& center, const math::float3& normal, const math::float3& u_axis, const math::float3& v_axis)
	{
		auto corner = [&](float u, float v)
		{
			return math::float3(center.x + u_axis.x * u + v_axis.x * v, center.y + u_axis.y * u + v_axis.y * v, center.z + u_axis.z * u + v_axis.z * v);
		};
		const UINT a = geometry.add_vertex(corner(-1, -1), normal, { 0, 1 });
		const UINT b = geometry.add_vertex(corner(+1, -1), normal, { 1, 1 });
		const UINT c = geometry.add_vertex(corner(+1, +1), normal, { 1, 0 });
		const UINT d = geometry.add_vertex(corner(-1, +1), normal, { 0, 0 });
		geometry.add_quad(a, b, c, d);
	}

	//	立方体(底面は高さ0)
	void build_cube(primitive_geometry& geometry, float size)
	{
		const float h = size * 0.5f;
		add_face(geometry, { 0, h, -h }, { 0, 0, -1 }, { h, 0, 0 }, { 0, h, 0 });
		add_face(geometry, { 0, h, +h }, { 0, 0, +1 }, { -h, 0, 0 }, { 0, h, 0 });
		add_face(geometry, { -h, h, 0 }, { -1, 0, 0 }, { 0, 0, -h }, { 0, h, 0 });
		add_face(geometry, { +h, h, 0 }, { +1, 0, 0 }, { 0, 0, h }, { 0, h, 0 });
		add_face(geometry, { 0, size, 0 }, { 0, 1, 0 }, { h, 0, 0 }, { 0, 0, h });
		add_face(geometry, { 0, 0, 0 }, { 0, -1, 0 }, { h, 0, 0 }, { 0, 0, -h });
	}

	//	平面(高さ0で上向き)
	void build_plane(primitive_geometry& geometry, float size)
	{
		const float h = size * 0.5f;
		add_face(geometry, { 0, 0, 0 }, { 0, 1, 0 }, { h, 0, 0 }, { 0, 0, h });
	}

	//	円柱(直径・高さとも size、底面は高さ0)
	void build_cylinder(primitive_geometry& geometry, float size, UINT segments)
	{
		const float radius = size * 0.5f;
		std::vector<UINT> bottom(segments + 1), top(segments + 1);
		for (UINT segment = 0; segment <= segments; ++segment)
		{
			const float phi = math::constants<float>::pi * 2.0f * segment / segments;
			const math::float3 normal = { std::cos(phi), 0, std::sin(phi) };
			const float u = static_cast<float>(segment) / segments;
			bottom[segment] = geometry.add_vertex({ normal.x * radius, 0, normal.z * radius }, normal, { u, 1 });
			top[segment] = geometry.add_vertex({ normal.x * radius, size, normal.z * radius }, normal, { u, 0 });
		}
		for (UINT segment = 0; segment < segments; ++segment)
		{
			geometry.add_quad(bottom[segment], bottom[segment + 1], top[segment + 1], top[segment]);
		}

		//	上下のふた
		for (int side = 0; side < 2; ++side)
		{
			const float y = side == 0 ? size : 0.0f;
			const math::float3 normal = { 0, side == 0 ? 1.0f : -1.0f, 0 };
			const UINT center = geometry.add_vertex({ 0, y, 0 }, normal, { 0.5f, 0.5f });
			std::vector<UINT> ring(segments + 1);
			for (UINT segment = 0; segment <= segments; ++segment)
			{
				const float phi = math::constants<float>::pi * 2.0f * segment / segments;
				const float x = std::cos(phi), z = std::sin(phi);
				ring[segment] = geometry.add_vertex({ x * radius, y, z * radius }, normal, { 0.5f + x * 0.5f, 0.5f - z * 0.5f });
			}
			for (UINT segment = 0; segment < segments; ++segment)
			{
				geometry.add_triangle(center, ring[segment], ring[segment + 1]);
			}
		}
	}

	//	頂点バッファビューを1つ作る
	template<typename Type>
	buffer_view make_vertex_view(DXGI_FORMAT format, const std::vector<Type>& values)
	{
		buffer_view view;
		view.format = format;
		view.stride_in_bytes = graphics_utility::get_byte_offset_from_format(format);
		view.size_in_bytes = view.stride_in_bytes * values.size();
		view.row_data.resize(view.size_in_bytes);
		memcpy_s(view.row_data.data(), view.row_data.size(), values.data(), values.size() * sizeof(Type));
		return view;
	}
}

void model_resource_utility::generate_primitive_model(model_resource& resource, primitive_shapes shape, float size, UINT segments)
{
	segments = (std::max)(segments, 3u);

	primitive_geometry geometry;
	switch (shape)
	{
		case primitive_shapes::sphere:		build_sphere(geometry, size * 0.5f, segments);	break;
		case primitive_shapes::cube:		build_cube(geometry, size);						break;
		case primitive_shapes::plane:		build_plane(geometry, size);					break;
		case primitive_shapes::cylinder:	build_cylinder(geometry, size, segments);		break;
	}
	orient_triangles(geometry);
	const std::vector<math::float4> tangents = compute_primitive_tangents(geometry);

	auto& nodes = resource.get_nodes_for_editor();
	auto& meshes = resource.get_meshes_for_editor();
	auto& materials = resource.get_materials_for_editor();
	nodes.clear();
	meshes.clear();
	materials.clear();

	//	ルートノード
	{
		auto& node = nodes.emplace_back();
		node.name = std::string(magic_enum::enum_name(shape));
		node.parent_index = model_resource::invalid_id;
	}

	//	既定のマテリアル(gltf 変換と同じシェーダー)
	{
		auto& material = *materials.emplace_back(std::make_shared<::material_resource>());
		material.set_name("default");
		material.set_domain(material_resource::domains::surface);
		material.set_shading_model(material_resource::shading_models::default_lit);
		material.set_blend_mode(material_resource::blend_modes::opaque);
		material.set_flag(material_resource::flags::shadow_caster, true);
		material.set_flag(material_resource::flags::shadow_receiver, true);
		material.set_shader_path("Shaders/Materials/gltf.hlsl");
		material.set_float4("base_color_factor", { 0.8f, 0.8f, 0.8f, 1.0f });
		material.set_float("roughness_factor", 0.5f);
		material.set_float("metallic_factor", 0.0f);
	}

	//	メッシュ
	{
		auto& mesh = meshes.emplace_back();
		mesh.material_index = 0;
		mesh.node_index = 0;

		//	ボーンが無いので、メッシュのノードを0番ボーンとして指す
		mesh.bones.emplace_back().node_index = mesh.node_index;

		const size_t vertex_count = geometry.vertices.size();
		std::vector<math::float3> positions(vertex_count);
		std::vector<math::float3> normals(vertex_count);
		std::vector<math::float2> texcoords(vertex_count);
		mesh.bounds_min = { +FLT_MAX, +FLT_MAX, +FLT_MAX };
		mesh.bounds_max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (size_t index = 0; index < vertex_count; ++index)
		{
			const auto& vertex = geometry.vertices[index];
			positions[index] = vertex.position;
			normals[index] = vertex.normal;
			texcoords[index] = vertex.texcoord;
			mesh.bounds_min = { (std::min)(mesh.bounds_min.x, vertex.position.x), (std::min)(mesh.bounds_min.y, vertex.position.y), (std::min)(mesh.bounds_min.z, vertex.position.z) };
			mesh.bounds_max = { (std::max)(mesh.bounds_max.x, vertex.position.x), (std::max)(mesh.bounds_max.y, vertex.position.y), (std::max)(mesh.bounds_max.z, vertex.position.z) };
		}

		//	gltf 変換と同じく、シェーダーが参照する分は全て埋めておく
		mesh.vertex_buffer_views.insert(std::make_pair(semantic("POSITION", 0), make_vertex_view(DXGI_FORMAT_R32G32B32_FLOAT, positions)));
		mesh.vertex_buffer_views.insert(std::make_pair(semantic("NORMAL", 0), make_vertex_view(DXGI_FORMAT_R32G32B32_FLOAT, normals)));
		mesh.vertex_buffer_views.insert(std::make_pair(semantic("TANGENT", 0), make_vertex_view(DXGI_FORMAT_R32G32B32A32_FLOAT, tangents)));
		mesh.vertex_buffer_views.insert(std::make_pair(semantic("TEXCOORD", 0), make_vertex_view(DXGI_FORMAT_R32G32_FLOAT, texcoords)));
		mesh.vertex_buffer_views.insert(std::make_pair(semantic("COLOR", 0), make_vertex_view(DXGI_FORMAT_R32G32B32A32_FLOAT, std::vector<math::float4>(vertex_count, math::float4(1, 1, 1, 1)))));
		mesh.vertex_buffer_views.insert(std::make_pair(semantic("JOINTS", 0), make_vertex_view(DXGI_FORMAT_R32G32B32A32_UINT, std::vector<math::uint4>(vertex_count, math::uint4(0, 0, 0, 0)))));
		mesh.vertex_buffer_views.insert(std::make_pair(semantic("WEIGHTS", 0), make_vertex_view(DXGI_FORMAT_R32G32B32A32_FLOAT, std::vector<math::float4>(vertex_count, math::float4(1, 0, 0, 0)))));

		mesh.index_buffer_view = make_vertex_view(DXGI_FORMAT_R32_UINT, geometry.indices);

		model_buffer_pool_manager::instance()->build_vertex_buffers(mesh.vertex_buffer_views);
	}

	//	モデル編集シーンの変換後と同じく、最適化してメッシュレットを作る
	optimize_meshs(resource);
	generate_meshlets(resource);
}
#endif	//	defined(USE_IMGUI)
