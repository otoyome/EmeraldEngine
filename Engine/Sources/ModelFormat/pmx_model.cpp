#include "ModelFormat/pmx_model.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <functional>

#include "Texture/texture.h"
#include "Graphics/shader.h"

#include <stdio.h>
#include <tchar.h>
#include <locale.h>
#include <iostream>
#include <string>
#include <windows.h>

namespace
{
	float difference_angle(float a, float b)
	{
		float diff = math::angle::normalize_positive(a) - math::angle::normalize_positive(b);
		if (diff > math::constants<float>::pi)
		{
			return diff - math::constants<float>::pi2;
		}
		else if (diff < -math::constants<float>::pi)
		{
			return diff + math::constants<float>::pi2;
		}
		return diff;
	}
	math::float3 decompose(const math::matrixf& m, const math::float3& before)
	{
		math::float3 r;
		float sy = -m.storage[0][2];
		const float e = 1.0e-6f;

		if ((1.0f - std::abs(sy)) < e)
		{
			r.y = std::asin(sy);
			float sx = std::sin(before.x);
			float sz = std::sin(before.z);
			if (std::abs(sx) < std::abs(sz))
			{
				float cx = std::cos(before.x);
				if (cx > 0)
				{
					r.x = 0;
					r.z = std::asin(-m.storage[1][0]);
				}
				else
				{
					r.x = math::constants<float>::pi;
					r.z = std::asin(m.storage[1][0]);
				}
			}
			else
			{
				float cz = std::cos(before.z);
				if (cz > 0)
				{
					r.z = 0;
					r.x = std::asin(-m.storage[2][1]);
				}
				else
				{
					r.z = math::constants<float>::pi;
					r.x = std::asin(m.storage[2][1]);
				}
			}
		}
		else
		{
			r.x = std::atan2(m.storage[1][2], m.storage[2][2]);
			r.y = std::asin(-m.storage[0][2]);
			r.z = std::atan2(m.storage[0][1], m.storage[0][0]);
		}

		const float pi = math::constants<float>::pi;
		math::float3 tests[] =
		{
			{ r.x + pi, pi - r.y, r.z + pi },
			{ r.x + pi, pi - r.y, r.z - pi },
			{ r.x + pi, -pi - r.y, r.z + pi },
			{ r.x + pi, -pi - r.y, r.z - pi },
			{ r.x - pi, pi - r.y, r.z + pi },
			{ r.x - pi, pi - r.y, r.z - pi },
			{ r.x - pi, -pi - r.y, r.z + pi },
			{ r.x - pi, -pi - r.y, r.z - pi },
		};

		float errX = std::abs(difference_angle(r.x, before.x));
		float errY = std::abs(difference_angle(r.y, before.y));
		float errZ = std::abs(difference_angle(r.z, before.z));
		float minErr = errX + errY + errZ;
		for (const auto test : tests)
		{
			float err = std::abs(difference_angle(test.x, before.x))
				+ std::abs(difference_angle(test.y, before.y))
				+ std::abs(difference_angle(test.z, before.z));
			if (err < minErr)
			{
				minErr = err;
				r = test;
			}
		}
		return r;
	}
	math::float3 decompose(const math::quaternionf& q, const math::float3& before)
	{
		return decompose(math::matrixf::rotation_quaternion(q), before);
	}

	math::float3 subtract_angle(const math::float3& current_angle, const math::float3& prev_angle)
	{
		math::float3 angle;
		angle.x = math::angle::normalize_positive(current_angle.x - prev_angle.x);
		angle.y = math::angle::normalize_positive(current_angle.y - prev_angle.y);
		angle.z = math::angle::normalize_positive(current_angle.z - prev_angle.z);
		return	angle;
	}
	math::float3 additional_angle(const math::float3& current_angle, const math::float3& prev_angle)
	{
		math::float3 angle;
		angle.x = math::angle::normalize_positive(current_angle.x + prev_angle.x);
		angle.y = math::angle::normalize_positive(current_angle.y + prev_angle.y);
		angle.z = math::angle::normalize_positive(current_angle.z + prev_angle.z);
		return	angle;
	}
	math::float3 clamp_angle(const math::float3& current_angle, const math::float3& min, const math::float3& max)
	{
		math::float3 angle;
		angle.x = std::clamp(current_angle.x, min.x, max.x);
		angle.y = std::clamp(current_angle.y, min.y, max.y);
		angle.z = std::clamp(current_angle.z, min.z, max.z);
		return	angle;
	}

	//	MMDは30FPS基準で作られている
	static constexpr float normalized_frame = 30.0f;
}

void pmx_model::calculate_node_data::begin()
{
	animation_scale = { 1, 1, 1 };
	animation_rotation = { 0, 0, 0, 1 };
	animation_translation = { 0, 0, 0 };

	append_rotation = { 0, 0, 0, 1 };
	append_translation = { 0, 0, 0 };

	ik_rotation = { 0, 0, 0, 1 };

	scale = { 1, 1, 1 };
	rotation = { 0, 0, 0, 1 };
	position = { 0, 0, 0 };
}

void pmx_model::calculate_node_data::update_local_matrix()
{
	math::vector3f local_scale, local_position;
	math::quaternionf local_rotation;
	local_scale = math::vector3f(animation_scale);

	local_rotation	= math::quaternionf(0, 0, 0, 1);
	local_rotation	= local_rotation * math::quaternionf(append_rotation);
	local_rotation	= local_rotation * math::quaternionf(animation_rotation);
	local_rotation	= local_rotation * math::quaternionf(ik_rotation);

	local_position	= math::vector3f(append_translation);
	local_position	= local_position + math::vector3f(animation_translation);

	math::matrixf matrix;
	matrix	= math::matrixf::scaling(local_scale)
			* math::matrixf::rotation_quaternion(local_rotation)
			* math::matrixf::translation(local_position)
			* math::matrixf(initialize_matrix);
	matrix.decompose(local_scale, local_rotation, local_position);
	scale = local_scale.to_float3();
	rotation = local_rotation.to_float4();
	position = local_position.to_float3();
	locll_matrix = matrix.to_float4x4();
	global_matrix = locll_matrix;
}

void pmx_model::calculate_node_data::update_global_matrix(const math::matrixf& parent_global_matrix)
{
	math::matrixf matrix(global_matrix);
	global_matrix = (matrix * parent_global_matrix).to_float4x4();
}

#define	SWAP( a, b )	if(a > b){ decltype( a )	t	= b; b	= a; a	= t; }

///////////////////////////////////////////////////////////////////////////////////////////////////////
pmx_model::pmx_model(ID3D11Device* device, pmx_loader& loader)
{
	//	テクスチャ読み込み
	texture_names = loader.get_textures();

	//	マテリアル/サブセット
	std::vector<subset> subsets;
	{
		materials.reserve(loader.get_materials().size());
		subsets.reserve(loader.get_materials().size());
		size_t index_count = 0;
		for (auto& m : loader.get_materials())
		{
			material wm;
			wm.name = m.name;
			wm.draw_flag = m.draw_flag;
			wm.def.diffuse_color = m.diffuse;
			wm.def.diffuse_texture_index = m.deffuse_texture_index;
			wm.def.sphere_texture_index = m.sphere_texture_index;
			wm.def.ramp_texture_index = m.toon_texture_index;
			if (m.toon_flag != 0)
			{
				wm.def.ramp_texture_index = m.toon_flag;
				wm.def.ramp_texture_index |= UseDefaultRamp;
			}
			wm.def.specular_color.x = m.specular.x;
			wm.def.specular_color.y = m.specular.y;
			wm.def.specular_color.z = m.specular.z;
			wm.def.specular_color.w = m.specularlity;
			wm.def.edge_color = m.edge_color;
			wm.def.edge_size = m.edge_size;
			{
				wm.def.alpha_clip_threshold = 0.01f;
				wm.def.alpha_dither_factor = 0.0f;
				wm.def.color_diff_line_factor = 1.0f;
				wm.def.color_diff_line_threshold = 0.005f;
				wm.def.color_line_adjust_saturate = 1.0f;
				wm.def.color_line_adjust_brightness = 1.0f;
				wm.def.color_posterize_factor = 24.0f;
				wm.def.depth_normal_outline_factor = 0.1f;
			}

			subset subset;
			subset.start = index_count;
			subset.count = m.index_count;
			subset.material_index = static_cast<size_t>(materials.size());

			subsets.emplace_back(subset);
			materials.emplace_back(wm);
			index_count += m.index_count;
		}
	}

	//	ノード
	{
		nodes.reserve(loader.get_bones().size());
		for (auto& b : loader.get_bones())
		{
			node wn;
			wn.name = b.name;
			wn.level = b.level;
			wn.parent_node_index = b.parent_index;
			wn.calculate_data.init_translation = b.position;

			//	階層
			{
				bool	is_find	= false;
				for(auto level : levels)
				{
					if( level == wn.level )
					{
						is_find	= true;
						break;
					}
				}
				if( !is_find )
				{
					levels.emplace_back(wn.level);
					std::sort(levels.begin(), levels.end());
				}
			}

			math::matrixf im;
			im = math::matrixf::translation(wn.calculate_data.init_translation.x, wn.calculate_data.init_translation.y, wn.calculate_data.init_translation.z);
			wn.calculate_data.initialize_matrix = im.to_float4x4();
			wn.offset_matrix = im.inverted().to_float4x4();

			//	Append
			if ((b.bone_flag & static_cast<unsigned short>(pmx_bone_bitflag::is_append_translation)) ||
				(b.bone_flag & static_cast<unsigned short>(pmx_bone_bitflag::is_append_rotation)))
			{
				wn.append_node_index = append_datas.size();
				append_data append;
				append.source_index = nodes.size();
				append.target_index = b.append_parent_index;
				append.weight = b.append_weight;
				append.is_rotation = (b.bone_flag & static_cast<unsigned short>(pmx_bone_bitflag::is_append_rotation));
				append.is_translation = (b.bone_flag & static_cast<unsigned short>(pmx_bone_bitflag::is_append_translation));
				append_datas.emplace_back(append);
			}

			//	IK
			if (b.bone_flag & static_cast<unsigned short>(pmx_bone_bitflag::is_ik))
			{
				ik_data ik;
				ik.source_index = nodes.size();
				ik.target_index = b.ik_target_bone_index;
				ik.loop = b.ik_loop;
				ik.angile_limit = b.ik_loop_angle_limit;
				ik.ik_link_start = ik_link_datas.size();
				bool is_insert = false;
				for (auto& il : b.ik_link_list)
				{
					if (b.ik_target_bone_index == ik.source_index || il.link_target == ik.source_index || b.ik_target_bone_index == il.link_target)
					{
						is_insert = false;
						break;
					}
					ik_link_data wil;
					wil.target_index = il.link_target;

					wil.angle_min = il.angle_min;
					wil.angle_max = il.angle_max;
					SWAP( wil.angle_min.x, wil.angle_max.x );
					SWAP( wil.angle_min.y, wil.angle_max.y );
					SWAP( wil.angle_min.z, wil.angle_max.z );

					if (il.angle_lock)
					{
						wil.lock = plane_lock::lock;
						if (wil.angle_min.x != wil.angle_max.x &&
							wil.angle_min.y == wil.angle_max.y &&
							wil.angle_min.z == wil.angle_max.z)
						{
							wil.lock = plane_lock::plane_x;
						}
						else if (wil.angle_min.x == wil.angle_max.x &&
								 wil.angle_min.y != wil.angle_max.y &&
								 wil.angle_min.z == wil.angle_max.z)
						{
							wil.lock = plane_lock::plane_y;
						}
						else if (wil.angle_min.x == wil.angle_max.x &&
								 wil.angle_min.y == wil.angle_max.y &&
								 wil.angle_min.z != wil.angle_max.z)
						{
							wil.lock = plane_lock::plane_z;
						}
					}

					is_insert = true;
					ik_link_datas.emplace_back(wil); 
				}
				if (is_insert)
				{
					wn.ik_node_index = static_cast<size_t>(ik_datas.size());
					ik.ik_link_end = static_cast<size_t>(ik_link_datas.size());
					ik_datas.emplace_back(ik);
				}
			}
			nodes.emplace_back(wn);
		}

		for (auto& n : nodes)
		{
			math::matrixf im(n.calculate_data.initialize_matrix);
			if (n.parent_node_index != pmx_invalid_target_id)
				im *= math::matrixf(nodes[n.parent_node_index].offset_matrix);

			math::vector3f scale, translation;
			math::quaternionf rotation;
			im.decompose(scale, rotation, translation);
			n.calculate_data.init_scale = scale.to_float3();
			n.calculate_data.init_rotation = rotation.normalized().to_float4();
			n.calculate_data.init_translation = translation.to_float3();
			im	= math::matrixf::scaling(math::vector3f(n.calculate_data.init_scale))
				* math::matrixf::rotation_quaternion(math::quaternionf(n.calculate_data.init_rotation))
				* math::matrixf::translation(math::vector3f(n.calculate_data.init_translation));
			n.calculate_data.initialize_matrix = im.to_float4x4();
		}
	}

	//	メッシュ
	{
		vertices.reserve(loader.get_vertices().size());
		for (auto& v : loader.get_vertices())
		{
			vertex wv;
			wv.position = v.position;
			wv.normal = v.normal;
			//wv.color = { 1.0f,1.0f ,1.0f ,1.0f };
			wv.texcoord = v.texcoord;
			for (int ii = 0; ii < loader.get_index_data().uv; ++ii)
			{
				wv.additional_uv[ii] = v.additional_uv[ii];
			}
			wv.bone_indices = v.bone_indices;
			wv.bone_weights = v.bone_weights;
			vertices.emplace_back(wv);
		}
		this->subsets = subsets;
	}

	//	ノードソート
	{
		struct	sort_node
		{
			std::string	name;		//	名前
			size_t		order;		//	オーダー
			size_t		level;		//	変形階層
			size_t		old_index;	//	元番号
			size_t		new_index;	//	新番号
			node		row_node;	//	ノード情報
		};
		std::vector<sort_node>	sorted_nodes( nodes.size() );

		//	ソートオーダーを算出
		for( size_t ii = 0; ii < nodes.size(); ++ii )
		{
			sorted_nodes[ii].row_node	= nodes[ii];
			sorted_nodes[ii].name		= nodes[ii].name;
			sorted_nodes[ii].old_index	= ii;
			sorted_nodes[ii].order		= 0;
			sorted_nodes[ii].level		= nodes[ii].level;
			//	階層算出
			auto	node_pointer	= nodes.data() + ii;
			while( node_pointer->parent_node_index != pmx_invalid_target_id )
			{
				sorted_nodes[ii].order++;
				node_pointer	= nodes.data() + node_pointer->parent_node_index;
			}
		}

		//	オーダーソート
		std::sort( sorted_nodes.begin(), sorted_nodes.end(),
				   [&]( sort_node& right, sort_node& left )
					{
						if( right.level == left.level )
						{
							return	right.old_index < left.old_index;
						}
						return	right.level < left.level;
					} );

		//	新番号設定
		for( size_t ii = 0; ii < nodes.size(); ++ii )
		{
			sorted_nodes[ii].new_index	= ii;
			nodes[ii]	= sorted_nodes[ii].row_node;
		}

		//	元に戻す
		std::sort( sorted_nodes.begin(),
				   sorted_nodes.end(),
				   []( sort_node& right, sort_node& left )
					{
						return	right.old_index < left.old_index;
					} );

		//	頂点ノード再設定
		for( auto& it : vertices )
		{
			if( it.bone_indices.x != pmx_invalid_target_id )
			{
				it.bone_indices.x	= static_cast<UINT>(sorted_nodes[it.bone_indices.x].new_index);
			}
			if( it.bone_indices.y != pmx_invalid_target_id )
			{
				it.bone_indices.y	= static_cast<UINT>(sorted_nodes[it.bone_indices.y].new_index);
			}
			if( it.bone_indices.z != pmx_invalid_target_id )
			{
				it.bone_indices.z	= static_cast<UINT>(sorted_nodes[it.bone_indices.z].new_index);
			}
			if( it.bone_indices.w != pmx_invalid_target_id )
			{
				it.bone_indices.w	= static_cast<UINT>(sorted_nodes[it.bone_indices.w].new_index);
			}
		}

		indices.reserve(loader.get_indices().size());
		for(UINT i = 0; i < loader.get_indices().size(); i += 3)
		{
			indices.push_back(loader.get_indices()[i + 2]);
			indices.push_back(loader.get_indices()[i + 1]);
			indices.push_back(loader.get_indices()[i + 0]);
		}

		for( auto& it : nodes )
		{
			if( it.parent_node_index != pmx_invalid_target_id )
			{
				it.parent_node_index	= sorted_nodes[it.parent_node_index].new_index;
			}
		}

		for( auto& it : append_datas )
		{
			it.source_index	= sorted_nodes[it.source_index].new_index;
			it.target_index	= sorted_nodes[it.target_index].new_index;
		}

		for( auto& it : ik_datas )
		{
			it.source_index	= sorted_nodes[it.source_index].new_index;
			it.target_index	= sorted_nodes[it.target_index].new_index;
		}

		for( auto& it : ik_link_datas )
		{
			it.target_index	= sorted_nodes[it.target_index].new_index;
		}
	}

	//	モーフ
	{
		morphs = loader.get_morphs();
	}
}

pmx_model::~pmx_model()
{
}

void pmx_model::compute_animation(std::string_view animation_name, float time, std::vector<node_animation_data>& node_datas)
{
	if(node_datas.size() != nodes.size())
	{
		node_datas.resize(nodes.size());
	}

	//	アニメーションが存在しない場合は初期値を入れる
	const auto& it_animation = animations.find(std::string(animation_name));
	if (it_animation == animations.end())
	{
		for (size_t node_index = 0; node_index < nodes.size(); ++node_index)
		{
			auto& node = nodes[node_index];
			auto& node_data = node_datas[node_index];
			node_data.scale = node.calculate_data.init_scale;
			node_data.rotation = node.calculate_data.init_rotation;
			node_data.translation = node.calculate_data.init_translation;
		}
		return;
	}

	//	計算するために初期化
	for (auto& node : nodes)
	{
		node.calculate_data.begin();
	}

	//	アニメーション計算
	for (size_t node_index = 0; node_index < it_animation->second.nodes.size(); ++node_index)
	{
		auto& animation_node = it_animation->second.nodes[node_index];
		if (animation_node.empty())
			continue;

		size_t keyframe_max	= animation_node.size() - 1;
		if( keyframe_max >= animation_node.size() )
			continue;

		auto& node = nodes[node_index];

		size_t keyframe_index;
		for (keyframe_index = 0; keyframe_index < keyframe_max; ++keyframe_index)
		{
			const auto& keyframe0 = animation_node.at(keyframe_index + 0);
			const auto& keyframe1 = animation_node.at(keyframe_index + 1);
			if (!(keyframe0.time <= time && time < keyframe1.time))
				continue;

			float rate = (time - keyframe0.time) / (keyframe1.time - keyframe0.time);
			math::quaternionf R0(keyframe0.rotation);
			math::quaternionf R1(keyframe1.rotation);
			math::vector3f T0(keyframe0.translation);
			math::vector3f T1(keyframe1.translation);
			node.calculate_data.animation_rotation = ((math::slerp(R0, R1, rate)).normalized()).to_float4();
			node.calculate_data.animation_translation = (math::lerp(T0, T1, rate)).to_float3();
			break;
		}

		if( keyframe_max == keyframe_index )
		{
			const auto& keyframe = animation_node.back();
			if (keyframe.time <= time)
			{
				node.calculate_data.animation_rotation = keyframe.rotation;
				node.calculate_data.animation_translation = keyframe.translation;
			}
		}
	}

	for(auto& ik_link_data : ik_link_datas)
	{
		ik_link_data.plane_mode_angle = 0;
		ik_link_data.prev_angle = {0, 0, 0};
	}

	//	ノード一旦更新
	for (size_t node_index = 0; node_index < nodes.size(); ++node_index)
	{
		auto& node = nodes[node_index];

		node.calculate_data.update_local_matrix();
		if (node.parent_node_index != pmx_invalid_target_id)
		{
			math::matrixf matrix;
			matrix = math::matrixf(nodes[node.parent_node_index].calculate_data.global_matrix);
			node.calculate_data.update_global_matrix(matrix);
		}
	}

	//	その他計算
	for (size_t node_index = 0; node_index < nodes.size(); ++node_index)
	{
		update_append(node_index);

		update_ik(node_index);
	}

	//	最終更新
	for (size_t node_index = 0; node_index < nodes.size(); ++node_index)
	{
		auto& node = nodes[node_index];
		auto& node_data = node_datas[node_index];

		node.calculate_data.update_local_matrix();
		node_data.scale = node.calculate_data.scale;
		node_data.rotation = node.calculate_data.rotation;
		node_data.translation = node.calculate_data.position;
	}
}

void pmx_model::update_ik(size_t node_index)
{
	auto& source_node = nodes[node_index];
	if( source_node.ik_node_index == pmx_invalid_target_id )
		return;

	auto& ikdata = ik_datas[source_node.ik_node_index];

	//	CCD-IK
	if (!ikdata.enable)
		return;

	math::vector3f ik_target_position(source_node.calculate_data.global_matrix._41, source_node.calculate_data.global_matrix._42, source_node.calculate_data.global_matrix._43);

	int loop_max = ikdata.loop;
	for (int loop_count = 0; loop_count < loop_max; ++loop_count)
	{
		auto& target_node = nodes[ikdata.target_index];
		math::vector3f target_position(target_node.calculate_data.global_matrix._41, target_node.calculate_data.global_matrix._42, target_node.calculate_data.global_matrix._43);
		math::vector3f vector = ik_target_position - target_position;
		float distance = vector.square_length();
		if (distance <= 0.001f)
			break;

		for (size_t link_index = ikdata.ik_link_start; link_index < ikdata.ik_link_end; ++link_index)
		{
			auto& link_data = ik_link_datas[link_index];
			auto& link_node = nodes[link_data.target_index];

			//	平面の処理に変更
			if (link_data.lock != plane_lock::free && link_data.lock != plane_lock::lock)
			{
				update_ik_plane(loop_count, ikdata, link_data, link_data.lock);
			}
			//	平面処理以外
			else
			{
				math::matrixf inverse_link_global_matrix = math::matrixf(link_node.calculate_data.global_matrix).inverted();

				math::vector3f effector_global_position(target_node.calculate_data.global_matrix._41, target_node.calculate_data.global_matrix._42, target_node.calculate_data.global_matrix._43);
				math::vector3f effector_local_position = math::vector3f::transform_coord(effector_global_position, inverse_link_global_matrix);
				math::vector3f target_local_position = math::vector3f::transform_coord(ik_target_position, inverse_link_global_matrix);

				math::vector3f effector_local_direction = effector_local_position.normalized();
				math::vector3f target_local_direction = target_local_position.normalized();
				float	dot = math::vector3f::dot(effector_local_direction, target_local_direction);
				float	angle = acosf(dot);
				math::vector3f axis = (math::vector3f::cross(effector_local_direction, target_local_direction)).normalized();
				if (isnan(angle) || math::vector3f::dot(axis, axis) <= 0.001f)
					continue;

				angle = std::min(angle, ikdata.angile_limit);

				math::quaternionf rotation = math::quaternionf::rotation_axis(axis, angle);

				//	適応
				math::quaternionf node_rotation(link_node.calculate_data.rotation);
				node_rotation = rotation * node_rotation;
				node_rotation = node_rotation.normalized();

				//	角度制限
				if (link_data.lock == plane_lock::lock)
				{
					math::float3 angle = decompose(node_rotation, link_data.prev_angle);

					math::float3 limit_angle = clamp_angle(angle, link_data.angle_min, link_data.angle_max);
					limit_angle = clamp_angle(subtract_angle(limit_angle, link_data.prev_angle),
											{-ikdata.angile_limit, -ikdata.angile_limit, -ikdata.angile_limit},
											{+ikdata.angile_limit, +ikdata.angile_limit, +ikdata.angile_limit});
					limit_angle = additional_angle(limit_angle, link_data.prev_angle);

					node_rotation = math::quaternionf::rotation_xyz(limit_angle.x, limit_angle.y, limit_angle.z);
					link_data.prev_angle = limit_angle;
				}

				//	IKノード情報に変換
				const math::quaternionf append_rotation(link_node.calculate_data.append_rotation);
				const math::quaternionf animation_rotation(link_node.calculate_data.animation_rotation);
				const math::quaternionf inverse_rotation = (append_rotation * animation_rotation).inverted();
				node_rotation = inverse_rotation * node_rotation;
				link_node.calculate_data.ik_rotation = node_rotation.normalized().to_float4();
			}

			//	グローバル行列再計算
			update_node_transforms(link_data.target_index);
		}
	}
}

void pmx_model::update_ik_plane(int iteration, ik_data& ik_data, ik_link_data& link_data, plane_lock axis)
{
	math::vector3f rotate_axis;
	float limit_min_angle = 0;
	float limit_max_angle = 0;

	switch (axis)
	{
		case plane_lock::plane_x:
		{
			limit_min_angle = link_data.angle_min.x;
			limit_max_angle = link_data.angle_max.x;
			rotate_axis = math::vector3f(1.0f, 0.0f, 0.0f);
			break;
		}
		case plane_lock::plane_y:
		{
			limit_min_angle = link_data.angle_min.y;
			limit_max_angle = link_data.angle_max.y;
			rotate_axis = math::vector3f(0.0f, 1.0f, 0.0f);
			break;
		}
		case plane_lock::plane_z:
		{
			limit_min_angle = link_data.angle_min.z;
			limit_max_angle = link_data.angle_max.z;
			rotate_axis = math::vector3f(0.0f, 0.0f, 1.0f);
			break;
		}
	}
	node& source_node	= nodes[ik_data.source_index];
	node& target_node	= nodes[ik_data.target_index];
	node& link_node		= nodes[link_data.target_index];

	math::vector3f ik_target_position(source_node.calculate_data.global_matrix._41, source_node.calculate_data.global_matrix._42, source_node.calculate_data.global_matrix._43);
	math::vector3f target_position(target_node.calculate_data.global_matrix._41, target_node.calculate_data.global_matrix._42, target_node.calculate_data.global_matrix._43);

	math::matrixf inverse_link_global_matrix = math::matrixf(link_node.calculate_data.global_matrix).inverted();

	math::vector3f effector_global_position(target_node.calculate_data.global_matrix._41, target_node.calculate_data.global_matrix._42, target_node.calculate_data.global_matrix._43);
	math::vector3f effector_local_position = math::vector3f::transform_coord(effector_global_position, inverse_link_global_matrix);
	math::vector3f target_local_position = math::vector3f::transform_coord(ik_target_position, inverse_link_global_matrix);

	math::vector3f effector_local_direction = effector_local_position.normalized();
	math::vector3f target_local_direction = target_local_position.normalized();

	float dot = math::vector3f::dot(effector_local_direction, target_local_direction);
	float angle = std::acos(dot);
	if(isnan(angle))
		return;

	angle = std::clamp(angle, -ik_data.angile_limit, ik_data.angile_limit);

	//	回転方向を判断するため、両方の方向に回転させてみて適応角度を変える
	math::quaternionf rotation1 = math::quaternionf::rotation_axis(rotate_axis, +angle);
	math::vector3f target_vector1 = rotation1.rotate(effector_local_direction);
	float dot1 = math::vector3f::dot(target_vector1, target_local_direction);

	math::quaternionf rotation2 = math::quaternionf::rotation_axis(rotate_axis, -angle);
	math::vector3f target_vector2 = rotation2.rotate(effector_local_direction);
	float dot2 = math::vector3f::dot(target_vector2, target_local_direction);

	float new_angle = link_data.plane_mode_angle + ((dot1 > dot2) ? +angle : -angle);
	if (iteration == 0)
	{
		if (new_angle < limit_min_angle || new_angle > limit_max_angle)
		{
			if (-new_angle > limit_min_angle && -new_angle < limit_max_angle)
			{
				new_angle *= -1;
			}
			else
			{
				float halfRadian = (limit_min_angle + limit_max_angle) * 0.5f;
				if (abs(halfRadian - new_angle) > abs(halfRadian + new_angle))
				{
					new_angle *= -1;
				}
			}
		}
	}
	new_angle = std::clamp(new_angle, limit_min_angle, limit_max_angle);
	link_data.plane_mode_angle = new_angle;
	math::quaternionf node_rotation = math::quaternionf::rotation_axis(rotate_axis, new_angle);

	//	IKノード情報に変換
	const math::quaternionf append_rotation(link_node.calculate_data.append_rotation);
	const math::quaternionf animation_rotation(link_node.calculate_data.animation_rotation);
	const math::quaternionf inverse_rotation = (append_rotation * animation_rotation).inverted();
	node_rotation	= inverse_rotation * node_rotation;
	link_node.calculate_data.ik_rotation = node_rotation.normalized().to_float4();
}

void pmx_model::update_append(size_t node_index)
{
	auto& node = nodes[node_index];
	if( node.append_node_index == pmx_invalid_target_id )
		return;

	auto& append = append_datas[node.append_node_index];
	auto& source_node = nodes[append.source_index];
	auto& target_node = nodes[append.target_index];

	bool is_update = false;
	if (append.is_translation)
	{
		math::float3 translation;
		if( target_node.append_node_index != pmx_invalid_target_id )
		{
			translation	= target_node.calculate_data.append_translation;
		}
		else
		{
			translation	= target_node.calculate_data.animation_translation;
		}

		if (!(math::vector3f(source_node.calculate_data.append_translation) == math::vector3f(translation)))
		{
			source_node.calculate_data.append_translation.x	= translation.x * append.weight;
			source_node.calculate_data.append_translation.y	= translation.y * append.weight;
			source_node.calculate_data.append_translation.z	= translation.z * append.weight;
			is_update = true;
		}
	}
	if (append.is_rotation)
	{
		math::quaternionf rotation(0, 0, 0, 1);
		if( target_node.append_node_index != pmx_invalid_target_id )
		{
			rotation	= math::quaternionf(target_node.calculate_data.append_rotation);
		}
		else
		{
			rotation	= rotation * math::quaternionf(target_node.calculate_data.append_rotation);
			rotation	= rotation * math::quaternionf(target_node.calculate_data.animation_rotation);
		}

		rotation	= rotation * math::quaternionf(target_node.calculate_data.ik_rotation);
		rotation	= rotation * math::quaternionf(target_node.calculate_data.init_rotation);
		rotation	= rotation.normalized();

		math::quaternionf append_rotation(source_node.calculate_data.append_rotation);
		if (!(append_rotation == rotation))
		{
			append_rotation = math::slerp(math::quaternionf(0, 0, 0, 1), rotation, append.weight);
			append_rotation = append_rotation.normalized();
			source_node.calculate_data.append_rotation = append_rotation.to_float4();
			is_update = true;
		}
	}

	if (!is_update)
		return;

	update_node_transforms(node_index);
}

void pmx_model::update_node_transforms(size_t node_index)
{
	auto& node = nodes[node_index];

	node.calculate_data.update_local_matrix();
	if (node.parent_node_index != pmx_invalid_target_id)
	{
		node.calculate_data.update_global_matrix(math::matrixf(nodes[node.parent_node_index].calculate_data.global_matrix));
	}

	//	子供を更新していく
	for (int index = 0; index < nodes.size(); ++index)
	{
		auto& node = nodes[index];
		if(node.parent_node_index != node_index)
			continue;

		update_node_transforms(index);
	}
}

void pmx_model::update_transforms(std::vector<node_animation_data>& node_datas)
{
	//	ノード一旦更新
	for (size_t node_index = 0; node_index < nodes.size(); ++node_index)
	{
		auto& node = nodes[node_index];
		auto& node_data = node_datas[node_index];

		math::matrixf local, global;
		local	= math::matrixf::scaling(math::vector3f(node_data.scale))
				* math::matrixf::rotation_quaternion(math::quaternionf(node_data.rotation))
				* math::matrixf::translation(math::vector3f(node_data.translation));
		global = local;
		if (node.parent_node_index != pmx_invalid_target_id)
		{
			global = math::matrixf(nodes[node.parent_node_index].global_matrix);
			global = local * global;
		}
		node.local_matrix = local.to_float4x4();
		node.global_matrix = global.to_float4x4();
	}
}

bool pmx_model::add_vmd_animation(const char* motion_name, const char* filepath)
{
	//	ファイル読み込み
	vmd_loader loader;
	{
		std::filesystem::path path = filepath;
		std::string directory = path.parent_path().string();

		std::ifstream	read_file;
		read_file.open(path, std::ios::in | std::ios::binary);
		_ASSERT_EXPR_A(read_file, L"Failed to load file");

		std::string	file_buffer;
		file_buffer = std::string(std::istreambuf_iterator<char>(read_file), std::istreambuf_iterator<char>());

		bool succeeded = loader.load(file_buffer.data(), static_cast<unsigned int>(file_buffer.size()));
		_ASSERT_EXPR_A(succeeded, L"Failed to load file");
		if (!succeeded)
			return false;
	}
	return add_vmd_animation(motion_name, loader);
}

bool pmx_model::add_vmd_animation(const char* motion_name, const vmd_loader& loader)
{
	animation animation;
	animation.end_time = loader.get_end_frame() / normalized_frame;
	animation.name = motion_name;
	animation.nodes.resize(nodes.size());
	for (auto& bone_animation : loader.get_bone())
	{
		int node_index = pmx_invalid_target_id;
		for (int index = 0; index < nodes.size(); ++index)
		{
			if (nodes[index].name == bone_animation.name)
			{
				node_index = index;
				break;
			}
		}

		if (node_index == pmx_invalid_target_id)
			continue;

		auto& node = nodes[node_index];
		node_animation anim;
		anim.time = bone_animation.frame / normalized_frame;
		math::matrixf animation_matrix;
		animation_matrix = math::matrixf::rotation_quaternion(math::quaternionf(bone_animation.rotation))
						* math::matrixf::translation(math::vector3f(bone_animation.position));
		math::vector3f scale, translation;
		math::quaternionf rotation;
		animation_matrix.decompose(scale, rotation, translation);
		anim.rotation = rotation.normalized().to_float4();
		anim.translation = translation.to_float3();
		animation.nodes[node_index].emplace_back(anim);
	}
	//	ソート
	for (auto& animes : animation.nodes)
	{
		std::sort(animes.begin(), animes.end(), [&](const node_animation& l, const node_animation& r) {return l.time < r.time; });
	}
	animations.insert_or_assign(animation.name, animation);
	return true;
}

std::vector<std::string> pmx_model::get_animation_names()
{
	std::vector<std::string> names;
	names.reserve(animations.size());
	for (auto& it : animations)
	{
		names.push_back(it.first);
	}
	return names;
}

pmx_model::animation* pmx_model::search_animation(std::string_view name)
{
	auto it	= animations.find(std::string(name));
	if(it == animations.end())
		return nullptr;

	return &it->second;
}

pmx_model::node* pmx_model::search_node(std::string_view name)
{
	auto it	= std::find_if(nodes.begin(), nodes.end(), [&](const node& l){ return l.name == std::string(name); });
	if(it == nodes.end())
		return nullptr;

	return &(*it);
}
