#pragma once

//	高速フーリエ変換クラス
class fast_fourier_transform
{
	//	方向
	enum class directions
	{
		row,
		col,
	};

	//	複素数テクスチャ番号
	enum class textures
	{
		real0,		// 実数部0
		imaginary0,	// 虚数部0
		real1,		// 実数部1
		imaginary1,	// 虚数部1
	};

public:
	//	テクスチャサイズ
	enum class sizes
	{
		_512,
		_1024,
	};

	//	変換モード
	enum class modes
	{
		forward,
		inverse,
	};

public:
	fast_fourier_transform(sizes size);
	~fast_fourier_transform();

	//	UAVのクリア
	void clear_uavs(ID3D11DeviceContext* immediate_context,
					math::float4 real_clear_value = {0.0f, 0.0f, 0.0f, 0.0f},
					math::float4 imaginary_clear_value = {0.0f, 0.0f, 0.0f, 1.0f});

	//	フーリエ変換
	void solve(ID3D11DeviceContext* immediate_context, modes mode);

	//	複素数テクスチャの取得
	const texture_ptr& get_complex_real() const;
	const texture_ptr& get_complex_imaginary() const;

private:
	//	各方向に対してのフーリエ変換
	void solve_core(ID3D11DeviceContext* context, modes mode, directions direction);

private:
	std::shared_ptr<compute_shader> cs[magic_enum::enum_count<modes>()][magic_enum::enum_count<directions>()];
	texture_ptr complex_number_textures[magic_enum::enum_count<textures>()];
	sizes texture_size;
	int	current_texture_index = 0;
	int	next_texture_index = 1;
};
