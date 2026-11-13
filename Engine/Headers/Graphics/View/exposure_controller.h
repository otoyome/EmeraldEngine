#pragma once

#include <cstdint>

//	露出の設定
struct exposure_settings
{
	//	自動露出の方式
	enum class auto_exposure_modes : std::uint8_t
	{
		none,			//	手動(F値・シャッター速度・ISO感度をそのまま使う)
		f_number,		//	F値を調整(足りない分は ISO感度で補う)
		shutter_speed,	//	シャッター速度を調整(足りない分は ISO感度で補う)
		iso_speed,		//	ISO感度を調整
	};

	//	測光モード
	enum class metering_modes : std::uint8_t
	{
		average,			//	平均測光
		center_weighted,	//	中央重点平均測光
	};

	auto_exposure_modes auto_exposure_mode = auto_exposure_modes::iso_speed;
	metering_modes metering_mode = metering_modes::center_weighted;
	float exposure_compensation = 0.0f;		//	露出補正(EV)
	float adaptation_time = 0.5f;			//	自動露出の応答時定数(秒)
	float f_number = 8.0f;					//	F値
	float shutter_speed = 1.0f / 125.0f;	//	秒
	float iso_speed = 100.0f;				//	ISO感度
};

//	APEX露出制御
class exposure_controller
{
public:
	//	ISO 感度の定数 N(センサーに届く光を物理的に計算する場合に ISO100 で Sv = 5 になる値)
	static constexpr float sensitivity_value_n = 0.32f;
	//	反射光式露出計の校正定数 K(ISO 2720)
	static constexpr float sensitivity_value_k = 12.5f;
	//	飽和基準感度定数(ISO 12232)
	static constexpr float saturation_value = 78.0f;
	//	レンズの像面照度の係数 q(ISO 12232 の標準値 T = 0.9、v = 0.98、θ = 10°、無限遠で約 0.65)
	static constexpr float lens_imaging_factor = 0.6516f;

	//	値の範囲
	static constexpr float f_number_minimum = 2.8f;
	static constexpr float f_number_maximum = 22.0f;
	static constexpr float shutter_speed_minimum = 1.0f / 8000.0f;
	static constexpr float shutter_speed_maximum = 1.0f;
	static constexpr float iso_speed_minimum = 50.0f;
	static constexpr float iso_speed_maximum = 25600.0f;
	static constexpr float base_iso_speed = 100.0f;

public:
	float get_f_number() const { return f_number; }
	float get_shutter_speed() const { return shutter_speed; }
	float get_iso_speed() const { return iso_speed; }

public:
	//	リセット
	void reset(const exposure_settings& settings);

	//	更新
	void update(const exposure_settings& settings, float average_luminance, float elapsed_time);

	//	露光倍率
	float calculate_exposure_scale() const;

public:
	static float calculate_Av(float f_number);
	static float calculate_f_number(float Av);
	static float calculate_Tv(float shutter_speed);
	static float calculate_shutter_speed(float Tv);
	static float calculate_Sv(float iso_speed);
	static float calculate_iso_speed(float Sv);
	static float calculate_Bv(float luminance);

private:
	float f_number = 8.0f;					//	F値
	float shutter_speed = 1.0f / 125.0f;	//	秒
	float iso_speed = 100.0f;				//	ISO感度
};
