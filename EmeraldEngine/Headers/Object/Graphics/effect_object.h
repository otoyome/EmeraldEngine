#pragma once

#include "object/spatial_object.h"
#include "Effect/effect_instance.h"
#include "Resource/asset_reference.h"
#include "Resource/resource_operation.h"

//	エフェクトオブジェクト
class effect_object : public spatial_object
{
	GAME_OBJECT_DECRLEAR(effect_object, spatial_object)

public:
	static constexpr std::uint32_t serialize_version = 1;

	//	座標タイプ
	enum class position_types
	{
		self,		//	自身からの相対座標
		target,		//	追従先からの相対座標
		world,		//	ワールド座標
	};

public:
	//	エフェクトリソース
	const resource::asset_reference<effect_resource>& get_effect() const { return effect; }
	void set_effect(const resource::asset_reference<effect_resource>& value);

	//	読込後に自動で再生するか
	bool get_auto_play() const { return auto_play; }
	void set_auto_play(bool value) { auto_play = value; }

	//	ループ再生するか
	bool get_loop() const { return loop; }
	void set_loop(bool value);

	//	再生中か
	bool is_playing() const { return instance != nullptr || play_requested; }

	//	読込が終わっているか
	bool is_loaded() const { return loaded; }

	//	読み込んだエフェクト
	const effect_resource_ptr& get_loaded_resource() const { return loaded_resource; }

	//	パーティクル生成停止
	bool get_spawn_stop() const { return spawn_stop; }
	void set_spawn_stop(bool value);

	//	再生中のインスタンス
	const effect_instance_ptr& get_instance() const { return instance; }

	//	届いたパーティクルイベントリスト
	const std::vector<effect_instance::particle_event_info>& get_particle_events() const;

	//	イベントを出したパーティクルのノード名
	std::string_view get_particle_node_name(size_t particle_index) const;

	//	吸引座標関連
	position_types get_attract_type() const { return attract_type; }
	std::shared_ptr<spatial_object> get_attract_target() const { return attract_target.lock(); }
	const math::vector3d& get_attract_position() const { return attract_position; }
	void set_attract_target(const std::shared_ptr<spatial_object>& target, const math::vector3d& offset = math::vector3d());
	void set_attract_self(const math::vector3d& offset = math::vector3d());
	void set_attract_world(const math::vector3d& position);
	math::vector3d get_attract_world_position() const;

	//	イベント座標関連
	position_types get_event_type() const { return event_type; }
	std::shared_ptr<spatial_object> get_event_target() const { return event_target.lock(); }
	const math::vector3d& get_event_position() const { return event_position; }
	void set_event_target(const std::shared_ptr<spatial_object>& target, const math::vector3d& offset = math::vector3d());
	void set_event_self(const math::vector3d& offset = math::vector3d());
	void set_event_world(const math::vector3d& position);
	math::vector3d get_event_world_position() const;

public:
	effect_object(std::string_view name);
	~effect_object() override;

	void on_initialize() override;
	void on_finalize() override;

	void post_update(float elapsed_time) override;

	void serialize(cereal::JSONOutputArchive& archive) const override;
	void deserialize(cereal::JSONInputArchive& archive) override;

#if defined(USE_IMGUI)
	void imgui_render_core() override;
#endif	//	defined(USE_IMGUI)

public:
	//	再生
	void play();

	//	停止
	void stop();

protected:
	void on_active() override;
	void on_deactive() override;

private:	//	シリアライズ
	friend class game_object_serializer;
	template<class Archive>
	void serialize_fields(Archive& archive, std::uint32_t version);

private:
	//	読込開始/完了確認
	void request_load();
	void update_loading();

	//	インスタンス生成/解除
	void create_instance();
	void release_instance();

	//	姿勢と吸引/イベント座標をインスタンスへ反映
	void apply_transform();
	void apply_positions();
	math::vector3d resolve_position(position_types type, const std::weak_ptr<spatial_object>& target, const math::vector3d& position) const;

private:	//	設定
	resource::asset_reference<effect_resource> effect;	//	出すエフェクト
	bool auto_play;										//	読込後に自動で再生するか
	bool loop;											//	ループ再生
	position_types attract_type;						//	吸引座標の決め方
	std::weak_ptr<spatial_object> attract_target;		//	吸引座標の追従先
	math::vector3d attract_position;					//	吸引座標(self/target は相対、world はワールド)
	position_types event_type;							//	イベント座標の決め方
	std::weak_ptr<spatial_object> event_target;			//	イベント座標の追従先
	math::vector3d event_position;						//	イベント座標(self/target は相対、world はワールド)

private:	//	実行時
	bool spawn_stop;												//	パーティクル生成停止
	bool play_requested;											//	読み終わったら再生するか
	bool loaded;													//	読込が終わっているか
	resource::load_operation_ptr<effect_resource> load_operation;	//	読込操作
	effect_resource_ptr loaded_resource;							//	読み込んだエフェクト
	effect_instance_ptr instance;									//	再生中のインスタンス
};
