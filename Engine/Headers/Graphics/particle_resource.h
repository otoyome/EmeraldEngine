#pragma once

#include "texture.h"
#include "material_resource.h"
#include "animation_resource.h"
#include "collision/capsule.h"
#include "collision/box.h"

#include "Graphics/compute_particle_system.h"

#include <variant>

namespace particle
{
	namespace item
	{
		//	レンダラー情報
		struct renderer
		{
			//	ステート
			enum class states
			{
				animation = 1 << 0,				//	アニメーション
				billboard_off = 1 << 1,			//	ビルボード無し
				reverse_depth_sort = 1 << 2,	//	逆深度ソート
			};

			//	形状指定
			enum class types
			{
				sprite,
				ribon,
				//object,	未対応(やるならインスタンシングの対応が必要)
			};

			//	軌跡
			struct trails
			{
				enum class states
				{
					use_fade = 1 << 0,	//	軌跡終端フェード
				};

				UINT state = 0;
				UINT max = 30; // 軌跡総数

				bool is_state(states flag) const;
				void set_state(states flag, bool value);
				template<class Archive>
				void serialize(Archive& archive, int version);
			};

			//	UV座標系
			struct texcoords
			{
				//	直接指定時のアニメーション方向を割り出すための基準値
				static constexpr math::uint2 animation_direction_basis_value = {5, 5};

				enum class states
				{
					direct = 1 << 0,	//	UV座標を直接指定
					animation = 1 << 1,	//	アニメーション使用フラグ
					random_index = 1 << 2,	//	スポーン時にランダムなインデックスを選ぶ
				};

				UINT state = 0; // ステート
				UINT index = 0;	// スプリット数に応じた番号
				float time = 0.0f;	//	アニメーションで使用する
				math::float4 rect = {0, 0, 1, 1};
				math::uint2 splits = {1, 1};	//	分割数

				bool is_state(states flag) const;
				void set_state(states flag, bool value);
				template<class Archive>
				void serialize(Archive& archive, int version);
			};

			//	色
			struct colors
			{
				enum class states
				{
					random = 1 << 0,		//	ランダム設定
					use_fade = 1 << 1,		//	フェード有効化
				};

				UINT state = 0; // ステート
				math::float4 color_min = {1, 1, 1, 1};		//	色の最小
				math::float4 color_max = {1, 1, 1, 1};		//	色の最大
				math::float2 fade_range = {0.1f, 0.9f};	//	フェード範囲(x:フェードイン終了 y:フェードアウト開始)

				bool is_state(states flag) const;
				void set_state(states flag, bool value);
				template<class Archive>
				void serialize(Archive& archive, int version);
			};

			UINT state = 0;
			types type = types::sprite;		//	レンダラータイプ
			UINT total_particle_count = 1;	//	最大パーティクル数
			std::shared_ptr<material_resource> material;	//	表示マテリアル

			trails trail;	//	軌跡
			texcoords uv;	//	UV
			colors color;	//	色情報

			bool is_state(states flag) const;
			void set_state(states flag, bool value);
			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		//	生成情報
		struct spawn
		{
			enum class states
			{
				burst = 1 << 0,		//	エミッター生成時のみ生成
				random = 1 << 1,	//	ランダム生成
			};

			UINT state = 0;
			math::float2 life_time_range = {0.5f, 1.0f}; // 生存時間範囲
			math::float2 scale_range = {1.0f, 1.0f}; // 拡縮範囲
			math::float3 area_min = {0, 0, 0}; // 生成範囲最小
			math::float3 area_max = {0, 0, 0}; // 生成範囲最大
			math::uint2 range = {10, 30}; // 生成数

			bool is_state(states flag) const;
			void set_state(states flag, bool value);
			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		//	速度情報
		struct velocity
		{
			enum class states
			{
				random_direction = 1 << 0,		//	directionを元に、angle_rangeの範囲でランダムに変更
				random_power = 1 << 1,			//	power_rangeの範囲でランダムに変更
				random_acceleration = 1 << 2,	//	加速をランダムに変更
				random_damping = 1 << 3,	//	減衰係数をランダムに変更
			};

			UINT state = 0;

			math::float3 direction = {0, 0, 1}; // 初期方向
			math::float2 angle_range = {0, (math::constants<float>::pi * 0.25f)}; // 初期角度範囲
			math::float2 power_range = {0, 10.0f}; // 初速威力範囲

			math::float3 acceleration_min = {0, 0, 0}; // 最低加速
			math::float3 acceleration_max = {0, 0, 0}; // 最大加速

			float damping_min = 0.0f; // 最低減衰係数(1/s)
			float damping_max = 0.0f; // 最大減衰係数(1/s)

			math::float3 gravity_scale = {1, 1, 1}; //	重力係数

			bool is_state(states flag) const;
			void set_state(states flag, bool value);
			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		//	引き込み情報
		struct attract
		{
			float strength = 1.0f; // 吸引力
			float falloff = 1.0f; // 範囲内の影響力を調整
			float damping = 1.0f; // 吸引による速度の減衰
			float radius = system_units::convert_m_to_unit(1.0f); // 影響範囲
			float kill_radius = system_units::convert_m_to_unit(0.5f); // 消滅範囲

			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		//	イベント情報
		struct event
		{
			enum class states
			{
				contact_collision = 1 << 0,	//	登録されているsdfとの当たり判定に切り替える
				kill_on_hit = 1 << 1,	//	ヒット時にパーティクルを死亡させる
				sweep_collision = 1 << 2,	//	前フレームとの間をスイープ判定する
			};

			UINT state = 0;
			std::string node_name;	// イベント通知ノード
			float radius = system_units::convert_m_to_unit(1.0f);	// 反応範囲
			bool is_state(states flag) const;
			void set_state(states flag, bool value);
			template<class Archive>
			void serialize(Archive& archive, int version);
		};

		//	保存型
		using variant = std::variant<renderer, spawn, velocity, attract, event>; 

		//	判断用
		template<class Type, class Variant>
		struct is_variant_alternative;
 
		template<class Type, class... Types>
		struct is_variant_alternative<Type, std::variant<Types...>>
			: std::disjunction<std::is_same<Type, Types>...> {};
 
		template<class Type, class Variant>
		inline constexpr bool is_variant_alternative_v = is_variant_alternative<Type, Variant>::value;
 
	}	//	namespace item

	//	パーティクルデータ
	struct data
	{
		//	アイテム登録
		template<class Type, class... Args>
		Type* register_item(Args&&... args)
		{
			static_assert(
				item::is_variant_alternative_v<Type, item::variant>,
				"Type must be one of item::variant's alternatives"
			);
			item::variant& v = items.emplace_back(std::in_place_type<Type>, std::forward<Args>(args)...);
			return std::get_if<Type>(&v);
		}

		//	アイテム取得
		template<class Type>
		Type* get_item()
		{
			for (auto& item : items)
			{
				if (auto* p = std::get_if<Type>(&item))
					return p;
			}
			return nullptr;
		}
		template<class Type>
		const Type* get_item() const
		{
			for (const auto& item : items)
			{
				if (auto* p = std::get_if<Type>(&item))
					return p;
			}
			return nullptr;
		}

		//	アイテム登録解除
		template<class Type>
		bool unregister_item()
		{
			auto it = std::find_if(items.begin(), items.end(),
				[](const item::variant& v) { return std::holds_alternative<Type>(v); });
			if (it == items.end())
				return false;
			items.erase(it);
			return true;
		}

		bool is_valid() const;

		particle_system_item::emitter_resource constract_system_resource() const;

		template<class Archive>
		void serialize(Archive& archive, int version);

		std::string name;
		std::vector<item::variant> items;
	};

}	//	namespace particle
