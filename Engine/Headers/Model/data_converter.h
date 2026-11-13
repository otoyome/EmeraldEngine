#pragma	once

#include "Model/model_resource.h"
#include "Model/model_instance.h"
#include "Animation/animation_resource.h"
#include "Animation/ik_resource.h"

#if defined(USE_IMGUI)
#include "Animation/ik_resource.h"

#include "Animation/retarget_profile.h"

namespace data_converter
{
	//	初期姿勢ポーズアニメーション名
	constexpr std::string_view DefaultAnimationName = "default";

	//	アニメーションサンプリングレート
	constexpr float animation_sampling_rate = system_units::convert_fps_to_sec(1.0f);

	//	マテリアルフォルダ名
	constexpr std::string_view material_directory_name = "Materials";

	//	マテリアル名割当
	void assign_material_paths(const std::vector<material_resource_ptr>& materials, std::string_view owner_file_path);
	void assign_material_paths(const std::vector<resource::asset_handle<material_resource>>& materials, std::string_view owner_file_path);

	//	変換先にモデルリソースが既にあれば、モデルのUUIDと同じ名前のマテリアルを引き継ぐ
	void inherit_existing_model(model_resource& resource);

	//	GLTF形式の変換関数
	namespace gltf
	{
		struct convert_model_data
		{
			std::string_view file_path;							//	ファイルパス
			bool is_compute_tangent = false;					//	接線の無いメッシュの場合に接戦を算出する
			bool is_populate_vertex_color = false;				//	頂点色の無いメッシュに頂点色を含める
			bool is_change_coordinate_system = true;			//	座標系変換
			bool is_change_coordinate_system_indices = false;	//	座標系変換時にインデックスを反転
			bool is_normalize_weight = true;					//	影響度の正規化

			bool is_recomputed_normal = false;					//	法線を再計算するか
			bool is_recomputed_normal_to_copy_tangent = true;	//	再計算した法線を接線情報としてコピーするか
		};

		struct convert_animation_data
		{
			std::string_view file_path;				//	ファイルパス
			bool is_root_ignore = true;				//	ルートノードは無視

			model_resource_ptr retarget_resource;	//	リターゲッティングリソース
			retarget_profile retarget_profile;		//	リターゲッティング情報
		};

		bool convert(model_resource& resource, const convert_model_data& convert_data);
		bool convert(animation_resource& resource, const convert_animation_data& convert_data);
	}	//	namespace gltf

	//	MMD形式の変換関数
	namespace mmd
	{
		struct convert_model_data
		{
			std::string_view file_path;							//	ファイルパス
			bool is_compute_tangent = true;						//	接線の無いメッシュの場合に接戦を算出する
			bool is_populate_vertex_color = true;				//	頂点色の無いメッシュに頂点色を含める
			bool is_change_coordinate_system = true;			//	座標系変換
			bool is_change_coordinate_system_indices = false;	//	座標系変換時にインデックスを反転
			bool is_normalize_weight = true;					//	影響度の正規化

			bool is_recomputed_normal = true;					//	法線を再計算するか
			bool is_recomputed_normal_to_copy_tangent = true;	//	再計算した法線を接線情報としてコピーするか
		};

		struct convert_animation_data
		{
			std::string_view file_path;				//	ファイルパス
			bool is_root_ignore = true;				//	ルートノードは無視

			model_resource_ptr retarget_resource;	//	リターゲッティングリソース
			retarget_profile retarget_profile;		//	リターゲッティング情報
		};

		bool convert(model_resource& resource, const convert_model_data& convert_data);
		bool convert(animation_resource& resource, const convert_animation_data& convert_data);
	}	//	namespace mmd

}	//	namespace	data_converter
#endif	//	defined(USE_IMGUI)
