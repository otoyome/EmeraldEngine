#pragma once

//	ミニゲームのレコード
struct minigame_record_entry
{
	float clear_time = 0.0f;
	float max_speed_kmh = 0.0f;
	int trash_count = 0;
	int trash_max = 1;

	template<class Archive>
	void serialize(Archive& archive, int version);
};

//	ミニゲームのレコードデータ
struct minigame_record_data
{
	SINGLETON(minigame_record_data)

public:
	static constexpr size_t record_max = 5;

public:
	const std::vector<minigame_record_entry>& get_records() const;
	void add_record(const minigame_record_entry& entry);

private:
	std::vector<minigame_record_entry> records;

private:
	minigame_record_data();
	~minigame_record_data();
	minigame_record_data(const minigame_record_data&) = delete;
	minigame_record_data(minigame_record_data&&) = delete;

	void save();
};
