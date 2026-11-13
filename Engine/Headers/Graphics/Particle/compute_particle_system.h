#pragma once

#include <d3d11.h>
#include <wrl.h>

#include <vector>
#include <stack>
#include <memory>
#include <mutex>

#include "Texture/texture.h"
#include "Graphics/Pipeline/shader.h"
#include "system/variable_position_allocator.h"

#include "Graphics/Geometry/cubic_sdf_buffer_pool.h"

namespace particle_system_item
{
	//	描画タイプ
	enum class renderer_types
	{
		sprite,
		ribon,
	};

	//	@remainder	これを一々CPP/HLSL両方で定義するのはめんどくさいよね～( ;∀;)
	namespace particle
	{
		//	パーティクル共通情報
		struct common
		{
			//  パーティクルステートビット
			enum class state
			{
				bit_spawn_frame = 1 << 0,	//	生成フレームフラグ
				bit_event_fired = 1 << 1,	//	イベント通知済みフラグ
			};

			UINT state;
			float life_time;
			float life_time_max;
			math::float3 scale; // 拡縮情報
			math::float4 rotation; // 回転情報
			math::float3 translation; // 移動情報
			math::float4 color; // 色情報
		};
		//  軌跡アイテム
		struct trail_header
		{
			UINT counter;
		};
		struct trail_body
		{
			math::float3 scale; // 拡縮情報
			math::float4 rotation; // 回転情報
			math::float3 translation; // 移動情報
		};
		//	UVアイテム
		struct uv
		{
			UINT index;
		};
		//  速度アイテム
		struct velocity
		{
			math::float3 velocity; // 移動速度
		};

		//  引き込みアイテム
		struct attract
		{
			math::float3 velocity;
		};

		//	イベントアイテム
		struct event
		{
			math::float3 previous_position;
		};
	}

	//	@remainder	これを一々CPP/HLSL両方で定義するのはめんどくさいよね～( ;∀;)
	namespace emitter
	{
		//	エミッター基礎情報
		struct common
		{
			//	エミッターステートフラグ類
			enum class state
			{
				active = 1 << 0,				//	アクティブフラグ
				billboard_off = 1 << 1,			//	ビルボード無し
				reverse_depth_sort = 1 << 2,	//	逆深度ソート(半透明描画用)
			};

			//	エミッター情報
			UINT emitter_state = 0; // エミッターステート
			UINT active_items = 0; // アイテムアクティブフラグ

			// パーティクル発生情報
			math::float2 spawn_life_time_range = {1.0f, 1.0f}; // 生存時間範囲
			math::float2 spawn_scale_range = {1.0f, 1.0f}; // 拡縮範囲
			math::float3 spawn_area_min = {0, 0, 0}; // 生成範囲最小
			math::float3 spawn_area_max = {0, 0, 0}; // 生成範囲最大
			UINT spawn_count = 0; // 生成数
			UINT spawn_offset = 0; // 発生オフセット

			//	エミッター姿勢情報
			math::float3 scale; // 拡縮情報
			math::float4 rotation; // 回転情報
			math::float3 translation; // 移動情報
			float elapsed_time = 0.0f; // 経過時間
			UINT random_seed = 0; // 乱数種
		};

		//	軌跡アイテム
		struct trail
		{
			static constexpr UINT data_id = 0;

			enum class states
			{
				use_fade = 1 << 0,	//	軌跡終端フェード
			};

			UINT state = 0;
			UINT max = 0; // 軌跡総数
		};

		//	UVアイテム
		struct uv
		{
			static constexpr UINT data_id = 1;

			enum class state
			{
				direct = 1 << 0,	//	UV座標を直接指定
				animation = 1 << 1,	//	アニメーション使用フラグ
				random_index = 1 << 2,	//	スポーン時にランダムなインデックスを選ぶ
			};

			UINT state = 0; // ステート
			UINT index = 0;	// スプリット数に応じた番号
			float time = 0.0f;	//	アニメーションで使用する
			math::uint2 split_count = {1, 1};	//	分割数
			math::float4 rect = {0, 0, 1, 1};	//	UV矩形
		};

		//	色アイテム
		struct color
		{
			static constexpr UINT data_id = 2;

			enum class states
			{
				random = 1 << 0,	//	ランダム設定
				use_fade = 1 << 1,	//	フェード有効化
			};

			UINT state = 0;
			math::float4 color_min; // 色の範囲
			math::float4 color_max; // 色の範囲
			math::float2 fade_range = {0.1f, 0.9f};	// フェード範囲(x:フェードイン終了 y:フェードアウト開始)
		};

		//	速度アイテム
		struct velocity
		{
			static constexpr UINT data_id = 3;

			enum class states
			{
				random_direction = 1 << 0,		//	directionを元に、angle_rangeの範囲でランダムに変更
				random_power = 1 << 1,			//	power_rangeの範囲でランダムに変更
				random_acceleration = 1 << 2,	//	加速をランダムに変更
				random_damping = 1 << 3,	//	減衰係数をランダムに変更
			};

			UINT state = 0;
			math::float3 direction; // 初期方向
			math::float2 angle_range; // 初期角度範囲
			math::float2 power_range; // 初速威力範囲

			math::float3 acceleration_min; // 最低加速
			math::float3 acceleration_max; // 最大加速

			float damping_min; // 最低減衰係数(1/s)
			float damping_max; // 最大減衰係数(1/s)

			math::float3 gravity; // 重力
		};

		//	引き込みアイテム
		struct attract
		{
			static constexpr UINT data_id = 4;

			math::float3 position; // 位置
			float strength; // 吸引力
			float radius; // 影響範囲
			float falloff; // 範囲内の影響力を調整
			float kill_radius; // 消滅範囲
			float damping; // 減衰
		};

		//	イベントアイテム
		struct event
		{
			static constexpr UINT data_id = 5;

			enum class state
			{
				contact_collision = 1 << 0,	//	登録されているsdfとの当たり判定に切り替える
				kill_on_hit = 1 << 1,	//	ヒット時にパーティクルを死亡させる
				sweep_collision = 1 << 2,	//	前フレームとの間をスイープ判定する
			};

			UINT state = 0; // ステート
			math::float3 center; // 位置
			float radius;	// 範囲
			UINT target_emitter_id;	// 呼び出しエミッターID
		};
	}

	//	エミッター基礎情報リソース
	struct emitter_system_resource
	{
		UINT particle_type_size = 0;	//	パーティクル型サイズ
		UINT total_particle_count = 0;	//	パーティクル生成最大数
		UINT max_update_item_size = 0;	//	更新用アイテム最大サイズ
	};

	//	エミッター描画情報リソース
	struct emitter_render_resource
	{
		renderer_types renderer_type = renderer_types::sprite;	//	描画タイプ
		UINT trail_count = 0;	//	軌跡数
	};

	//	エミッターリソース
	struct emitter_resource
	{
		emitter_system_resource system;
		emitter_render_resource render;
	};

}	//	namespace particle_system_item

static constexpr UINT invalid_emitter_id = 0xFFFFFFFF;

class compute_particle_system
{
public:
	//	実行スレッド数
	static constexpr UINT NumDispatchThread = 64;

	//	エミッターテーブル
	//	エミッターヘッダーのインデックスの指し先
	using emitter_table = UINT;

	//	エミッターバイナリ
	using emitter_binary = UINT;

	//  エミッターヘッダー
	struct emitter_header
	{
		UINT	emitter_binary_head;	//	エミッターバイナリ先頭アドレス
		UINT	particle_binary_head;	//	パーティクルバイナリ先頭アドレス
		UINT	particle_index_head;	//	パーティクル番号先頭アドレス
		UINT	particle_size;			//	パーティクル一つのサイズ
	};

	//  エミッター範囲
	struct emitter_range
	{
		UINT	alive_head;	//	生存範囲の先頭
		UINT	alive_end;	//	生存範囲の終端
		UINT	dead_head;	//	死亡範囲の先頭
		UINT	dead_end;	//	死亡範囲の終端
	};

	//  エミッター情報
	struct emitter_data
	{
		UINT	spawn_head;		//	生成情報範囲の先頭
		UINT	spawn_num;		//	生成情報範囲の終端
		UINT	particle_num;	//	現在パーティクル数
		UINT	particle_max;	//	最大パーティクル数
	};

	//  パーティクルヘッダー
	struct particle_header
	{
		UINT	tag;	//	1 : 生存フラグ, 10 : エミッター番号, 21 : パーティクル番号
		float	depth;	//	深度
	};
	static constexpr UINT invalid_tag = 0xFFFFFFFF;

	//	パーティクル番号リスト
	using particle_index_list = UINT;

	//	パーティクルバイナリ
	using particle_binary = UINT;

	//	エミッター更新情報
	struct update_emitter_data
	{
		particle_system_item::emitter::common common;
		std::vector<char> item_binary;
	};

	//	DispatchIndirect用構造体
	using	dispatch_indirect = math::uint3;	//	Dispatch.x / Dispatch.y / Dispatch.z
	//	DrawIndirect用構造体
	using	draw_indirect = math::uint4;	//	vertex_count_per_instance / instance_count / start_vertex_location / start_instance_location

	//	00バイト目：現在のパーティクル総数
	//	04バイト目：1F前のパーティクル総数
	//  08バイト目：パーティクル更新回数
	//  20バイト目以降エミッターIDごとの draw_indirect データ
	static	constexpr	UINT	NumCurrentParticleOffset = 0;
	static	constexpr	UINT	NumPreviousParticleOffset = NumCurrentParticleOffset + sizeof(UINT);
	static	constexpr	UINT	UpdateParticleDispatchIndirect = NumPreviousParticleOffset + sizeof(UINT);
	static	constexpr	UINT	FixedIndirectDataSize = UpdateParticleDispatchIndirect + sizeof(dispatch_indirect);
	//	以降はエミッター用

	//	頂点情報
	struct vertex_data
	{
		math::float3 position;
		math::float4 color;
		math::float2 texcoord;
	};

	//	内部管理用エミッター情報
	struct inner_emitter_resource
	{
		particle_system_item::emitter_resource resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> shader_resource_view;
	};

	//	パーティクルイベント情報
	struct particle_event
	{
		math::float3 position;
		UINT target_emitter_id;
	};

public:
	compute_particle_system(ID3D11Device* device,
		UINT num_emitter, UINT emitter_data_size,
		UINT num_particle, UINT particle_data_size);
	~compute_particle_system();

	//	更新
	void update(graphics_context& context, float elapsed_time);
	void build_primitives(graphics_context& context);

	//	描画情報を構築
	void generate_render_data(UINT emitter_id, effect_container::sprite_trail& data);

	//	リセット処理
	void reset();
	void reset_emitter(UINT emitter_id);

	//	パーティクルイベント取得
	void fetch_events(graphics_context& context, std::vector<particle_event>& out_events);

	//	エミッター登録/解除
	UINT register_emitter(ID3D11Device* device, const particle_system_item::emitter_resource& emitter_resource);
	bool unregister_emitter(UINT emitter_id);

	//	登録エミッター更新
	void update_emitter(UINT emitter_id, const update_emitter_data& data);

	void set_cubic_sdf_gpu_resource(std::shared_ptr<cubic_sdf_buffer_pool> resource);

	//	シミュレーション空間の原点
	const math::vector3d& get_simulation_origin() const { return simulation_origin; }

	//	描画原点から一定以上離れていたらシミュレーション原点を描画原点へ移す
	void update_simulation_origin(const math::vector3d& render_origin);

private:
	bool read_particle_event_staging(ID3D11DeviceContext* device_context, UINT slot, bool wait, std::vector<particle_event>& out_events);

private:
	UINT total_emitter_count;
	UINT emitter_binary_buffer_size;
	UINT total_particle_count;
	UINT particle_binary_buffer_size;
	bool one_shot_initialize;
	bool update_stop = false;
	bool pending_reset = false;
	math::vector3d simulation_origin;
	math::float3 pending_rebase_offset = { 0.0f, 0.0f, 0.0f };
	bool pending_rebase = false;
	UINT current_buffer_index = 0;
	variable_position_allocator emitter_binary_allocator;
	variable_position_allocator particle_binary_allocator;
	variable_position_allocator particle_index_allocator;

	//	エミッター管理関係
	std::stack<UINT> emitter_id_stack; // 空きエミッター番号リスト
	std::vector<UINT> emitter_id_table; // 使用エミッター番号リスト
	std::stack<UINT> additional_emitter_ids; // 現フレーム生成エミッターIDリスト
	std::vector<UINT> reset_emitter_ids; // 現フレームリセットエミッターIDリスト
	std::vector<UINT> remove_emitter_ids; // 現フレーム破棄エミッターIDリスト
	std::vector<UINT> garbage_correct_emitter_ids; // ガベージコレクション対応エミッターIDリスト
	std::vector<emitter_header> emitter_headers; // エミッターヘッダーリスト
	std::vector<update_emitter_data> update_emitter_datas; // エミッター更新アイテムリスト
	std::vector<inner_emitter_resource> emitter_resources; // エミッターリソース

	//	定数バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> common_constant_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> fill_unused_index_constant_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> bitonic_sort_constant_buffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> range_particles_constant_buffer;

	Microsoft::WRL::ComPtr<ID3D11Buffer> build_primitive_constant_buffer;

	//	各種情報格納
	Microsoft::WRL::ComPtr<ID3D11Buffer> emitter_table_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> emitter_table_shader_resource_view;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> emitter_table_unordered_access_view;
	Microsoft::WRL::ComPtr<ID3D11Buffer> emitter_header_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> emitter_header_shader_resource_view;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> emitter_header_unordered_access_view;
	Microsoft::WRL::ComPtr<ID3D11Buffer> emitter_binary_buffer[2];
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> emitter_binary_shader_resource_view[2];
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> emitter_binary_unordered_access_view[2];
	Microsoft::WRL::ComPtr<ID3D11Buffer> emitter_range_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> emitter_range_shader_resource_view;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> emitter_range_unordered_access_view;
	Microsoft::WRL::ComPtr<ID3D11Buffer> emitter_data_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> emitter_data_shader_resource_view;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> emitter_data_unordered_access_view;

	Microsoft::WRL::ComPtr<ID3D11Buffer> particle_index_buffer[2];
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particle_index_shader_resource_view[2];
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particle_index_unordered_access_view[2];
	Microsoft::WRL::ComPtr<ID3D11Buffer> particle_header_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particle_header_shader_resource_view;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particle_header_unordered_access_view;
	Microsoft::WRL::ComPtr<ID3D11Buffer> particle_binary_buffer[2];
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particle_binary_shader_resource_view[2];
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particle_binary_unordered_access_view[2];

	//	Indirect情報バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> indirect_data_buffer;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> indirect_data_unordered_access_view;

	//	イベント情報バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer> particle_event_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particle_event_shader_resource_view;
	Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> particle_event_append_uav;
	static constexpr UINT particle_event_staging_count = 8;	//	読み出し用リング数
	Microsoft::WRL::ComPtr<ID3D11Buffer> particle_event_staging[particle_event_staging_count];
	Microsoft::WRL::ComPtr<ID3D11Buffer> particle_event_count_staging[particle_event_staging_count];
	UINT particle_event_staging_head = 0;	//	読み出し待ち先頭
	UINT particle_event_staging_pending = 0;	//	読み出し待ち数
	std::vector<particle_event> particle_event_overflow;	//	同期読み出し済みイベント
	Microsoft::WRL::ComPtr<ID3D11Buffer> particle_event_count_buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particle_event_count_shader_resource_view;

	//	SDFリソース
	std::shared_ptr<cubic_sdf_buffer_pool> sdf_buffer_pool;

	//	各種シェーダー
	enum class dispatch_shaderes
	{
		clear_system_shader,
		clear_particles,

		begin_update,
		fill_unused_index,
		spawn_particles,
		initialize_particles,
		spawn_from_events,

		update_particles,	//	@remainder	複数カメラを考えると深度ソートを別に移動させるほうが良いか？

		sort_b2,
		sort_c2,

		range_particles,
		terminate_particles,
		build_emitter_draw_arguments,

		end_update,

		build_primitive,

		rebase_particles,
	};
	std::shared_ptr<compute_shader> compute_shaderes[magic_enum::enum_count<dispatch_shaderes>()];
};
