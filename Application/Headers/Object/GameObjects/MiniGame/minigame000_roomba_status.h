#pragma once

#include "object/gameobjects/MiniGame/minigame000_roomba_movement.h"

namespace minigame000
{
	//	状態情報
	//	通信ではこれをホストが配る
	struct roomba_status_snapshot
	{
		std::uint32_t tick = 0;		//	最後に処理したinput_command
		float wet_value = 0.0f;		//	濡れ量
		float heat_value = 0.0f;	//	熱量
		bool dead = false;			//	死亡しているか
	};

	//	セッティング情報
	struct roomba_status_settings
	{
		float max_wet_value = 10000.0f;		//	耐水性能
		float max_heat_value = 10000.0f;	//	耐熱性能
		float base_wet_value = 30.0f;		//	乾燥量の基準
		float base_heat_value = 30.0f;		//	加熱/放熱量の基準

		template<class Archive>
		void serialize(Archive& archive, std::uint32_t version);
	};

	//	状態計算
	class roomba_status
	{
	public:
		roomba_status_settings& get_settings() { return settings; }
		const roomba_status_settings& get_settings() const { return settings; }

		//	濡れ率/熱率(0～1)
		float get_wet_rate(const roomba_status_snapshot& snapshot) const;
		float get_heat_rate(const roomba_status_snapshot& snapshot) const;

	public:
		//	シミュレート
		void simulate(roomba_status_snapshot& snapshot, const roomba_input_command& command, float add_wet_value, const roomba_movement_snapshot& movement_snapshot, const roomba_movement& movement, float elapsed_time) const;

	private:
		//	熱量/濡れ量更新
		void update_values(roomba_status_snapshot& snapshot, const roomba_input_command& command, const roomba_movement_snapshot& movement_snapshot, const roomba_movement& movement, float elapsed_time) const;

		//	値の設定
		void set_wet_value(roomba_status_snapshot& snapshot, float value) const;
		void set_heat_value(roomba_status_snapshot& snapshot, float value) const;

	private:
		roomba_status_settings settings;
	};
}	//	namespace minigame000

//	CEREALバージョン定義
CEREAL_CLASS_VERSION(minigame000::roomba_status_settings, 1)
