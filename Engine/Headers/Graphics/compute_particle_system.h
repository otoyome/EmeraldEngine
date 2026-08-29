#pragma once

#include <d3d11.h>
#include <wrl.h>
#include <directxmath.h>

#include <vector>
#include <stack>
#include <memory>
#include <mutex>

#include "texture.h"
#include "shader.h"
#include "system/variable_position_allocator.h"

namespace particle_system_item
{
	//	描画タイプ
	enum class renderer_types
	{
		sprite,
		ribon,
	};

	//	パーティクルの型は存在せず、
	//	エミッターの構築情報から構成される
	//	シェーダー側でもこれらを用いて情報設定して行く。
	//	@remainder	これを一々CPP/HLSL両方で定義するのはめんどくさいよね～( ;∀;)
	namespace particle
	{
		//	パーティクル共通情報
		struct common
		{
			UINT state;
			float life_time;
			float life_time_max;
			DirectX::XMFLOAT3 scale; // 拡縮情報
			DirectX::XMFLOAT4 rotation; // 回転情報
			DirectX::XMFLOAT3 translation; // 移動情報
			DirectX::XMFLOAT4 color; // 色情報
		};
		//  軌跡アイテム
		struct trail_header
		{
			UINT counter;
		};
		struct trail_body
		{
			DirectX::XMFLOAT3 scale; // 拡縮情報
			DirectX::XMFLOAT4 rotation; // 回転情報
			DirectX::XMFLOAT3 translation; // 移動情報
		};
		//  速度アイテム
		struct velocity
		{
			DirectX::XMFLOAT3 velocity; // 移動速度
		};

		//  引き込みアイテム
		struct attract
		{
			DirectX::XMFLOAT3 velocity;
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
				active = 1 << 0,		//	アクティブフラグ
				no_billboard = 1 << 1,	//	ビルボード無し
			};

			//	エミッター情報
			UINT emitter_state = 0; // エミッターステート
			UINT active_items = 0; // アイテムアクティブフラグ

			// パーティクル発生情報
			DirectX::XMFLOAT2 spawn_life_time_range; // 生存時間範囲
			DirectX::XMFLOAT2 spawn_scale_range; // 拡縮範囲
			DirectX::XMFLOAT3 spawn_area_min; // 生成範囲最小
			DirectX::XMFLOAT3 spawn_area_max; // 生成範囲最大
			UINT spawn_count = 0; // 生成数

			//	エミッター姿勢情報
			DirectX::XMFLOAT3 scale; // 拡縮情報
			DirectX::XMFLOAT4 rotation; // 回転情報
			DirectX::XMFLOAT3 translation; // 移動情報
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
				use_time_fade = 1 << 1,	//	時間フェード
			};

			UINT state = 0;
			UINT max = 0; // 軌跡総数
			DirectX::XMFLOAT2 fade_rate = {0.2f, 0.8f};	//	フェード
		};

		//	UVアイテム
		struct uv
		{
			static constexpr UINT data_id = 1;

			enum class state
			{
				direct = 1 << 0,	//	UV座標を直接指定
				animation = 1 << 1,	//	アニメーション使用フラグ
			};

			UINT state = 0; // ステート
			UINT index = 0;	// スプリット数に応じた番号
			float time = 0.0f;	//	アニメーションで使用する
			DirectX::XMUINT2 split_count = {1, 1};	//	分割数
			DirectX::XMFLOAT4 rect = {0, 0, 1, 1};	//	UV矩形
		};

		//	色アイテム
		struct color
		{
			static constexpr UINT data_id = 2;

			enum class states
			{
				random = 1 << 0,		//	ランダム設定
			};

			UINT state = 0;
			DirectX::XMFLOAT4 color_min; // 色の範囲
			DirectX::XMFLOAT4 color_max; // 色の範囲
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
				random_deceleration = 1 << 3,	//	減速をランダムに変更
			};

			UINT state = 0;
			DirectX::XMFLOAT3 direction; // 初期方向
			DirectX::XMFLOAT2 angle_range; // 初期角度範囲
			DirectX::XMFLOAT2 power_range; // 初速威力範囲

			DirectX::XMFLOAT3 acceleration_min; // 最低加速
			DirectX::XMFLOAT3 acceleration_max; // 最大加速

			DirectX::XMFLOAT3 deceleration_min; // 最低減速
			DirectX::XMFLOAT3 deceleration_max; // 最大減速

			DirectX::XMFLOAT3 gravity; // 重力
		};

		//	引き込み情報
		struct attract
		{
			static constexpr UINT data_id = 4;

			DirectX::XMFLOAT3 position; // 位置
			float strength; // 吸引力
			float radius; // 影響範囲
			float falloff; // 範囲内の影響力を調整
			float kill_radius; // 消滅範囲
			float damping; // 減衰
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

	//  パーティクルステートビット
	static const UINT particle_binary_state_bit_spawn_frame = 1 << 0;

	//	エミッター更新情報
	struct update_emitter_data
	{
		particle_system_item::emitter::common common;
		std::vector<char> item_binary;
	};

	//	DispatchIndirect用構造体
	using	dispatch_indirect = DirectX::XMUINT3;	//	Dispatch.x / Dispatch.y / Dispatch.z
	//	DrawIndirect用構造体
	using	draw_indirect = DirectX::XMUINT4;	//	vertex_count_per_instance / instance_count / start_vertex_location / start_instance_location

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
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
		DirectX::XMFLOAT2 texcoord;
	};

	//	内部管理用エミッター情報
	struct inner_emitter_resource
	{
		particle_system_item::emitter_resource resource;
		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> unordered_access_view;
	};

public:
	compute_particle_system(ID3D11Device* device,
		UINT num_emitter, UINT emitter_data_size,
		UINT num_particle, UINT particle_data_size);
	~compute_particle_system();

	void update(graphics_context& context, float elapsed_time, bool build_primitives = true);

	//	全パーティクルをクリアして再初期化する（次回 update から反映）
	void reset();
	void reset_emitter(UINT emitter_id);
	void generate_render_data(UINT emitter_id, effect_container::sprite_trail& data);

	UINT register_emitter(ID3D11Device* device, const particle_system_item::emitter_resource& emitter_resource);
	bool unregister_emitter(UINT emitter_id);
	void update_emitter(UINT emitter_id, const update_emitter_data& data);

private:
	UINT total_emitter_count;
	UINT emitter_binary_buffer_size;
	UINT total_particle_count;
	UINT particle_binary_buffer_size;
	bool one_shot_initialize;
	bool update_stop = false;
	bool pending_reset = false;
	UINT current_buffer_index = 0;
	variable_position_allocator emitter_binary_allocator;
	variable_position_allocator particle_binary_allocator;
	variable_position_allocator particle_index_allocator;

	//	エミッター管理関係
	std::stack<UINT> emitter_id_stack; // 空きエミッター番号リスト
	std::vector<UINT> emitter_id_table; // 使用エミッター番号リスト
	std::stack<UINT> additional_emitter_ids; // 現フレーム生成エミッターIDリスト
	std::vector<UINT> reset_emitter_ids; // 現フレーム リセット要求エミッターIDリスト
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

	//	各種シェーダー
	enum class dispatch_shaderes
	{
		clear_system_shader,
		clear_particles,

		begin_update,
		fill_unused_index,
		spawn_particles,
		initialize_particles,

		update_particles,	//	カメラ位置を考えると深度計算位置を変更したほうが良い

		sort_b2,
		sort_c2,

		range_particles,
		terminate_particles,

		end_update,

		build_emitter_draw_arguments,
		build_primitive,

		max,
	};
	std::shared_ptr<compute_shader> compute_shaderes[static_cast<int>(dispatch_shaderes::max)];
};
