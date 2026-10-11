#pragma	once

//	ƒQ[ƒ€—p‚Ìİ’èî•ñ
struct game_config_data
{
	SINGLETON(game_config_data)

private:
	game_config_data();
	~game_config_data();
	game_config_data(const game_config_data&) = delete;
	game_config_data(game_config_data&&) = delete;
};
