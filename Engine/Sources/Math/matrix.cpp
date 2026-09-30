#include <cassert>

#include "Math/math_all.h"

namespace math
{
	namespace
	{
		//	@tool 3要素を大きい順に並べた添字を返す（Framework の RANKDECOMPOSE マクロ相当）
		template<typename T>
		void rank_decompose(std::uint32_t& a, std::uint32_t& b, std::uint32_t& c, T x, T y, T z)
		{
			if (x < y)
			{
				if (y < z)
				{
					a = 2; b = 1; c = 0;
				}
				else
				{
					a = 1;
					if (x < z)
					{
						b = 2; c = 0;
					}
					else
					{
						b = 0; c = 2;
					}
				}
			}
			else
			{
				if (x < z)
				{
					a = 2; b = 0; c = 1;
				}
				else
				{
					a = 0;
					if (y < z)
					{
						b = 2; c = 1;
					}
					else
					{
						b = 1; c = 2;
					}
				}
			}
		}

	}	//	namespace

	template<typename T>
	matrix<T>::matrix(const vector4<T>& v1, const vector4<T>& v2, const vector4<T>& v3, const vector4<T>& v4)
	{
		set(v1, v2, v3, v4);
	}

	template<typename T>
	matrix<T>::matrix(const DirectX::XMFLOAT4X4& source)
	{
		set(static_cast<T>(source._11), static_cast<T>(source._12), static_cast<T>(source._13), static_cast<T>(source._14),
			static_cast<T>(source._21), static_cast<T>(source._22), static_cast<T>(source._23), static_cast<T>(source._24),
			static_cast<T>(source._31), static_cast<T>(source._32), static_cast<T>(source._33), static_cast<T>(source._34),
			static_cast<T>(source._41), static_cast<T>(source._42), static_cast<T>(source._43), static_cast<T>(source._44));
	}

	template<typename T>
	matrix<T>::matrix(const quaternion<T>& rotation)
	{
		*this = rotation_quaternion(rotation);
	}

	template<typename T>
	matrix<T>::matrix(const quaternion<T>& rotation, const vector3<T>& position)
	{
		*this = rotation_quaternion(rotation);
		_41 = position.x;
		_42 = position.y;
		_43 = position.z;
		_44 = static_cast<T>(1);
	}

	template<typename T>
	void matrix<T>::set(T value)
	{
		set(value, value, value, value,
			value, value, value, value,
			value, value, value, value,
			value, value, value, value);
	}

	template<typename T>
	void matrix<T>::set(const vector4<T>& v1, const vector4<T>& v2, const vector4<T>& v3, const vector4<T>& v4)
	{
		if constexpr (use_simd_v<T>)
		{
			reg[0] = v1.reg;
			reg[1] = v2.reg;
			reg[2] = v3.reg;
			reg[3] = v4.reg;
		}
		else
		{
			//	@tool Framework のスカラー版は 2～4 行目の w を _14 に書き込んでいたため修正している
			set(v1.x, v1.y, v1.z, v1.w,
				v2.x, v2.y, v2.z, v2.w,
				v3.x, v3.y, v3.z, v3.w,
				v4.x, v4.y, v4.z, v4.w);
		}
	}

	template<typename T>
	bool matrix<T>::decompose(vector3<T>& out_scale, quaternion<T>& out_rotation, vector3<T>& out_position) const
	{
		out_position.set(_41, _42, _43);

		out_scale.set(std::sqrt(_11 * _11 + _12 * _12 + _13 * _13),
					  std::sqrt(_21 * _21 + _22 * _22 + _23 * _23),
					  std::sqrt(_31 * _31 + _32 * _32 + _33 * _33));

		std::uint32_t a, b, c;
		rank_decompose(a, b, c, out_scale.x, out_scale.y, out_scale.z);

		vector4<T> basis[3] = { to_vector4(0), to_vector4(1), to_vector4(2) };
		const vector4<T> identity_basis[3] =
		{
			vector4<T>(1, 0, 0, 0),
			vector4<T>(0, 1, 0, 0),
			vector4<T>(0, 0, 1, 0),
		};

		if (out_scale[a] < constants<T>::epsilon)
		{
			basis[a] = identity_basis[a];
		}
		basis[a].normalize();

		if (out_scale[b] < constants<T>::epsilon)
		{
			std::uint32_t aa, bb, cc;
			T abs_x = std::abs(basis[a].x);
			T abs_y = std::abs(basis[a].y);
			T abs_z = std::abs(basis[a].z);

			rank_decompose(aa, bb, cc, abs_x, abs_y, abs_z);

			basis[b] = vector4<T>(vector3<T>::cross(basis[a].to_vector3(), identity_basis[cc].to_vector3()));
		}
		basis[b].normalize();

		if (out_scale[c] < constants<T>::epsilon)
		{
			basis[c] = vector4<T>(vector3<T>::cross(basis[a].to_vector3(), basis[b].to_vector3()));
		}
		basis[c].normalize();

		matrix work(basis[0].x, basis[0].y, basis[0].z, 0,
					basis[1].x, basis[1].y, basis[1].z, 0,
					basis[2].x, basis[2].y, basis[2].z, 0,
					0, 0, 0, 1);
		T det = work.determinant();
		if (det < static_cast<T>(0))	//	クラメルの公式で座標系の手のひらを確認する
		{
			// スケールを反転させて、X軸の基底ベクトルを反転させることにより座標系を切り替える。
			out_scale[a] = -out_scale[a];
			basis[a].negate();

			det = -det;
		}

		det -= static_cast<T>(1);
		det *= det;
		if (constants<T>::epsilon < det)
		{
			//	SRT行列ではないと判断
			return false;
		}

		work.set(basis[0].x, basis[0].y, basis[0].z, 0,
				 basis[1].x, basis[1].y, basis[1].z, 0,
				 basis[2].x, basis[2].y, basis[2].z, 0,
				 0, 0, 0, 1);
		out_rotation = quaternion<T>::rotation_matrix(work);
		out_rotation.normalize();
		return true;
	}

	template<typename T>
	void matrix<T>::mirroring(mirroring_order order)
	{
		//	http://momose-d.cocolog-nifty.com/blog/2014/08/post-ac70.html
		switch (order)
		{
			case mirroring_order::x:
			{
				_12 = -_12;
				_13 = -_13;
				_21 = -_21;
				_31 = -_31;
				break;
			}
			case mirroring_order::y:
			{
				_12 = -_12;
				_21 = -_21;
				_23 = -_23;
				_32 = -_32;
				break;
			}
			case mirroring_order::z:
			{
				_13 = -_13;
				_23 = -_23;
				_31 = -_31;
				_32 = -_32;
				break;
			}
		}
	}

	template<typename T>
	vector4<T> matrix<T>::to_vector4(std::int32_t index) const
	{
		if constexpr (use_simd_v<T>)
		{
			return vector4<T>(reg[index]);
		}
		else
		{
			return vector4<T>(m[index][0], m[index][1], m[index][2], m[index][3]);
		}
	}

	template<typename T>
	vector3<T> matrix<T>::to_vector3(std::int32_t index) const
	{
		if constexpr (use_simd_v<T>)
		{
			return vector3<T>(reg[index]);
		}
		else
		{
			return vector3<T>(m[index][0], m[index][1], m[index][2]);
		}
	}

	template<typename T>
	vector3<T> matrix<T>::get_translation() const
	{
		return vector3<T>(_41, _42, _43);
	}

	template<typename T>
	void matrix<T>::set_translation(const vector3<T>& position)
	{
		_41 = position.x;
		_42 = position.y;
		_43 = position.z;
	}

	template<typename T>
	DirectX::XMFLOAT4X4 matrix<T>::to_xmfloat4x4() const
	{
		DirectX::XMFLOAT4X4 ret;
		for (std::int32_t row = 0; row < 4; ++row)
		{
			for (std::int32_t column = 0; column < 4; ++column)
			{
				ret.m[row][column] = static_cast<float>(m[row][column]);
			}
		}
		return ret;
	}

	template<typename T>
	DirectX::XMMATRIX matrix<T>::to_xmmatrix() const
	{
		if constexpr (use_simd_v<T>)
		{
			return DirectX::XMMATRIX(ops::to_float4(reg[0]), ops::to_float4(reg[1]), ops::to_float4(reg[2]), ops::to_float4(reg[3]));
		}
		else
		{
			DirectX::XMFLOAT4X4 f = to_xmfloat4x4();
			return DirectX::XMLoadFloat4x4(&f);
		}
	}

	template<typename T>
	matrix<T> matrix<T>::load(DirectX::FXMMATRIX source)
	{
		if constexpr (use_simd_v<T>)
		{
			return matrix(ops::from_float4(source.r[0]), ops::from_float4(source.r[1]), ops::from_float4(source.r[2]), ops::from_float4(source.r[3]));
		}
		else
		{
			DirectX::XMFLOAT4X4 f;
			DirectX::XMStoreFloat4x4(&f, source);
			return matrix(f);
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////////
	template<typename T>
	T matrix<T>::determinant(const matrix& mat)
	{
		if constexpr (use_simd_v<T> && std::is_same_v<T, float>)
		{
			return DirectX::XMVectorGetX(DirectX::XMMatrixDeterminant(mat.to_xmmatrix()));
		}
		else if constexpr (use_simd_v<T>)
		{
			return simd::determinant<ops>(mat.reg);
		}
		else
		{
			vector4<T> v1(mat.m[0][0], mat.m[1][0], mat.m[2][0], mat.m[3][0]);
			vector4<T> v2(mat.m[0][1], mat.m[1][1], mat.m[2][1], mat.m[3][1]);
			vector4<T> v3(mat.m[0][2], mat.m[1][2], mat.m[2][2], mat.m[3][2]);
			vector4<T> minor = vector4<T>::cross(v1, v2, v3);
			return -(mat.m[0][3] * minor.x + mat.m[1][3] * minor.y + mat.m[2][3] * minor.z + mat.m[3][3] * minor.w);
		}
	}

	template<typename T>
	matrix<T> matrix<T>::inverse(const matrix& mat)
	{
		if constexpr (use_simd_v<T> && std::is_same_v<T, float>)
		{
			//	@tool float 版は Framework の SIMD 版と同じく DirectXMath の実装を使う
			DirectX::XMVECTOR det;
			DirectX::XMMATRIX ret = DirectX::XMMatrixInverse(&det, mat.to_xmmatrix());
			if (DirectX::XMVectorGetX(det) == 0.0f)
			{
				return mat;
			}
			return load(ret);
		}
		else if constexpr (use_simd_v<T>)
		{
			matrix ret;
			if (!simd::inverse<ops>(mat.reg, ret.reg))
			{
				return mat;
			}
			return ret;
		}
		else
		{
			static const T sign[4] = { 1, -1, 1, -1 };

			T det = determinant(mat);
			if (det == static_cast<T>(0))
			{
				return mat;
			}

			matrix ret;
			for (std::int32_t i = 0; i < 4; ++i)
			{
				vector4<T> rows[3];
				for (std::int32_t j = 0; j < 4; ++j)
				{
					if (j != i)
					{
						std::int32_t a = j > i ? j - 1 : j;
						rows[a].set(mat.m[j][0], mat.m[j][1], mat.m[j][2], mat.m[j][3]);
					}
				}
				vector4<T> work = vector4<T>::cross(rows[0], rows[1], rows[2]);
				ret.m[0][i] = sign[i] * work.x / det;
				ret.m[1][i] = sign[i] * work.y / det;
				ret.m[2][i] = sign[i] * work.z / det;
				ret.m[3][i] = sign[i] * work.w / det;
			}
			return ret;
		}
	}

	template<typename T>
	matrix<T> matrix<T>::inverse_for_scaling(const matrix& mat)
	{
		matrix ret;
		ret._11 = static_cast<T>(1) / mat._11;
		ret._22 = static_cast<T>(1) / mat._22;
		ret._33 = static_cast<T>(1) / mat._33;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::inverse_for_rotation(const matrix& mat)
	{
		return transpose(mat);
	}

	template<typename T>
	matrix<T> matrix<T>::inverse_for_translation(const matrix& mat)
	{
		matrix ret;
		ret._41 = -mat._41;
		ret._42 = -mat._42;
		ret._43 = -mat._43;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::inverse_for_rt(const matrix& mat)
	{
		matrix rotation = mat;
		rotation._41 = rotation._42 = rotation._43 = static_cast<T>(0);
		rotation._44 = static_cast<T>(1);
		rotation = inverse_for_rotation(rotation);

		matrix ret = rotation;
		ret._41 = -mat._41 * rotation._11 - mat._42 * rotation._21 - mat._43 * rotation._31;
		ret._42 = -mat._41 * rotation._12 - mat._42 * rotation._22 - mat._43 * rotation._32;
		ret._43 = -mat._41 * rotation._13 - mat._42 * rotation._23 - mat._43 * rotation._33;
		ret._44 = static_cast<T>(1);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::transpose(const matrix& mat)
	{
		matrix ret;
		if constexpr (use_simd_v<T>)
		{
			ops::transpose(mat.reg, ret.reg);
		}
		else
		{
			for (std::int32_t row = 0; row < 4; ++row)
			{
				for (std::int32_t column = 0; column < 4; ++column)
				{
					ret.m[row][column] = mat.m[column][row];
				}
			}
		}
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::scaling(const vector3<T>& scale)
	{
		return scaling(scale.x, scale.y, scale.z);
	}

	template<typename T>
	matrix<T> matrix<T>::scaling(T scale_x, T scale_y, T scale_z)
	{
		matrix ret;
		ret._11 = scale_x;
		ret._22 = scale_y;
		ret._33 = scale_z;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::rotation_x(T pitch)
	{
		T sx, cx;
		math::angle::sin_cos(pitch, sx, cx);

		matrix ret;
		ret._22 =  cx;
		ret._23 =  sx;
		ret._32 = -sx;
		ret._33 =  cx;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::rotation_y(T yaw)
	{
		T sy, cy;
		math::angle::sin_cos(yaw, sy, cy);

		matrix ret;
		ret._11 =  cy;
		ret._13 = -sy;
		ret._31 =  sy;
		ret._33 =  cy;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::rotation_z(T roll)
	{
		T sz, cz;
		math::angle::sin_cos(roll, sz, cz);

		matrix ret;
		ret._11 =  cz;
		ret._12 =  sz;
		ret._21 = -sz;
		ret._22 =  cz;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::rotation_xyz(const vector3<T>& rotation, rotation_order order)
	{
		return rotation_xyz(rotation.x, rotation.y, rotation.z, order);
	}

	template<typename T>
	matrix<T> matrix<T>::rotation_xyz(T pitch, T yaw, T roll, rotation_order order)
	{
		T sx, cx, sy, cy, sz, cz;
		math::angle::sin_cos(pitch, sx, cx);
		math::angle::sin_cos(yaw, sy, cy);
		math::angle::sin_cos(roll, sz, cz);

		matrix ret;
		switch (order)
		{
			case rotation_order::xyz:
			{
				ret._11 = cy * cz;
				ret._12 = cy * sz;
				ret._13 = -sy;

				ret._21 = sx * sy * cz + cx * -sz;
				ret._22 = sx * sy * sz + cx * cz;
				ret._23 = sx * cy;

				ret._31 = cx * sy * cz + -sx * -sz;
				ret._32 = cx * sy * sz + -sx * cz;
				ret._33 = cx * cy;
				break;
			}
			case rotation_order::xzy:
			{
				ret._11 = cz * cy;
				ret._12 = sz;
				ret._13 = cz * -sy;

				ret._21 = cx * -sz * cy + sx * sy;
				ret._22 = cx * cz;
				ret._23 = cx * sz * sy + sx * cy;

				ret._31 = sx * sz * cy + cx * sy;
				ret._32 = -sx * cz;
				ret._33 = -sx * sz * sy + cx * cy;
				break;
			}
			case rotation_order::yxz:
			{
				ret._11 = cy * cz + -sy * -sx * -sz;
				ret._12 = cy * sz + -sy * -sx * cz;
				ret._13 = -sy * cx;

				ret._21 = cx * -sz;
				ret._22 = cx * cz;
				ret._23 = sx;

				ret._31 = sy * cz + cy * -sx * -sz;
				ret._32 = sy * sz + cy * -sx * cz;
				ret._33 = cy * cx;
				break;
			}
			case rotation_order::yzx:
			{
				ret._11 = cy * cz;
				ret._12 = cy * sz * cx + sy * sx;
				ret._13 = cy * sz * sx + -sy * cx;

				ret._21 = -sz;
				ret._22 = cz * cx;
				ret._23 = cz * sx;

				ret._31 = sy * cz;
				ret._32 = sy * sz * cx + cy * -sx;
				ret._33 = sy * sz * sx + cy * cx;
				break;
			}
			case rotation_order::zxy:
			{
				ret._11 = cz * cy + sz * sx * sy;
				ret._12 = sz * cx;
				ret._13 = cz * -sy + sz * sx * cy;

				ret._21 = -sz * cy + cz * sx * sy;
				ret._22 = cz * cx;
				ret._23 = sz * sy + cz * sx * cy;

				ret._31 = cx * sy;
				ret._32 = -sx;
				ret._33 = cx * cy;
				break;
			}
			case rotation_order::zyx:
			{
				ret._11 = cz * cy;
				ret._12 = sz * cx + cz * sy * sx;
				ret._13 = sz * sx + cz * -sy * cx;

				ret._21 = -sz * cy;
				ret._22 = cz * cx + sz * sy * -sx;
				ret._23 = cz * sx + sz * sy * cx;

				ret._31 = sy;
				ret._32 = cy * -sx;
				ret._33 = cy * cx;
				break;
			}
			default:
			{
				break;
			}
		}
		return ret;
	}

	template<typename T>
	bool matrix<T>::to_euler_angle(T& out_x, T& out_y, T& out_z, const matrix& rotation, rotation_order order)
	{
		//	@tool 中間の軸の角度は asin ではなく atan2(sin, cos) で求める（cos は同じ行・列の2要素の長さ）
		//	@tool Framework の実装はジンバルロック時の符号が xyz / xzy / yzx / zyx などで逆になっていた。
		//	@tool また ±90 度付近で asin の引数が閾値をわずかに下回ると残り2軸が誤差だけで決まってしまい、
		//	@tool float のクォータニオン経由では大きく崩れていたため、cos の閾値でジンバルロックを判定している
		const T threshold = constants<T>::gimbal_lock_threshold;
		const auto& r = rotation.m;
		switch (order)
		{
			case rotation_order::xyz:
			{
				T c = std::hypot(r[0][0], r[0][1]);
				out_y = std::atan2(-r[0][2], c);
				//	ジンバルロック判定
				if (c < threshold)
				{
					//	x == 0 と仮定
					out_x = 0;
					out_z = std::atan2(-r[1][0], r[1][1]);
					return true;
				}
				out_x = std::atan2(r[1][2], r[2][2]);
				out_z = std::atan2(r[0][1], r[0][0]);
				return false;
			}
			case rotation_order::xzy:
			{
				T c = std::hypot(r[0][0], r[0][2]);
				out_z = std::atan2(r[0][1], c);
				//	ジンバルロック判定
				if (c < threshold)
				{
					//	x == 0 と仮定
					out_x = 0;
					out_y = std::atan2(r[2][0], r[2][2]);
					return true;
				}
				out_x = std::atan2(-r[2][1], r[1][1]);
				out_y = std::atan2(-r[0][2], r[0][0]);
				return false;
			}
			case rotation_order::yxz:
			{
				T c = std::hypot(r[1][0], r[1][1]);
				out_x = std::atan2(r[1][2], c);
				//	ジンバルロック判定
				if (c < threshold)
				{
					//	y == 0 と仮定
					out_y = 0;
					out_z = std::atan2(r[0][1], r[0][0]);
					return true;
				}
				out_y = std::atan2(-r[0][2], r[2][2]);
				out_z = std::atan2(-r[1][0], r[1][1]);
				return false;
			}
			case rotation_order::yzx:
			{
				T c = std::hypot(r[1][1], r[1][2]);
				out_z = std::atan2(-r[1][0], c);
				//	ジンバルロック判定
				if (c < threshold)
				{
					//	y == 0 と仮定
					out_y = 0;
					out_x = std::atan2(-r[2][1], r[2][2]);
					return true;
				}
				out_x = std::atan2(r[1][2], r[1][1]);
				out_y = std::atan2(r[2][0], r[0][0]);
				return false;
			}
			case rotation_order::zxy:
			{
				T c = std::hypot(r[2][0], r[2][2]);
				out_x = std::atan2(-r[2][1], c);
				//	ジンバルロック判定
				if (c < threshold)
				{
					//	z == 0 と仮定
					out_z = 0;
					out_y = std::atan2(-r[0][2], r[0][0]);
					return true;
				}
				out_y = std::atan2(r[2][0], r[2][2]);
				out_z = std::atan2(r[0][1], r[1][1]);
				return false;
			}
			case rotation_order::zyx:
			{
				T c = std::hypot(r[2][1], r[2][2]);
				out_y = std::atan2(r[2][0], c);
				//	ジンバルロック判定
				if (c < threshold)
				{
					//	z == 0 と仮定
					out_z = 0;
					out_x = std::atan2(r[1][2], r[1][1]);
					return true;
				}
				out_x = std::atan2(-r[2][1], r[2][2]);
				out_z = std::atan2(-r[1][0], r[0][0]);
				return false;
			}
			default:
			{
				break;
			}
		}

		assert(!"NotComaptible RotationOrder.");
		return false;
	}

	template<typename T>
	bool matrix<T>::to_euler_angle(vector3<T>& out, const matrix& rotation, rotation_order order)
	{
		//	@tool Framework は x の出力先に z を渡していたため修正している
		return to_euler_angle(out.x, out.y, out.z, rotation, order);
	}

	template<typename T>
	matrix<T> matrix<T>::rotation_axis(const vector3<T>& axis, T angle)
	{
		return rotation_quaternion(quaternion<T>::rotation_axis(axis, angle));
	}

	template<typename T>
	matrix<T> matrix<T>::rotation_quaternion(const quaternion<T>& rotation)
	{
		if constexpr (use_simd_v<T> && std::is_same_v<T, float>)
		{
			//	@tool float 版は Framework の SIMD 版と同じ計算を DirectXMath で行う（double 版はスカラー計算）
			return load(DirectX::XMMatrixRotationQuaternion(rotation.reg));
		}
		else
		{
			const T xx = rotation.x * rotation.x;
			const T xy = rotation.x * rotation.y;
			const T xz = rotation.x * rotation.z;
			const T yy = rotation.y * rotation.y;
			const T yz = rotation.y * rotation.z;
			const T zz = rotation.z * rotation.z;
			const T wx = rotation.w * rotation.x;
			const T wy = rotation.w * rotation.y;
			const T wz = rotation.w * rotation.z;
			constexpr T one = static_cast<T>(1);
			constexpr T two = static_cast<T>(2);

			matrix ret;
			ret._11 = one - two * yy - two * zz;
			ret._12 =       two * xy + two * wz;
			ret._13 =       two * xz - two * wy;

			ret._21 =       two * xy - two * wz;
			ret._22 = one - two * xx - two * zz;
			ret._23 =       two * yz + two * wx;

			ret._31 =       two * xz + two * wy;
			ret._32 =       two * yz - two * wx;
			ret._33 = one - two * xx - two * yy;
			return ret;
		}
	}

	template<typename T>
	matrix<T> matrix<T>::translation(T position_x, T position_y)
	{
		matrix ret;
		ret._41 = position_x;
		ret._42 = position_y;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::translation(T position_x, T position_y, T position_z)
	{
		matrix ret;
		ret._41 = position_x;
		ret._42 = position_y;
		ret._43 = position_z;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::translation(T position_x, T position_y, T position_z, T position_w)
	{
		matrix ret;
		ret._41 = position_x;
		ret._42 = position_y;
		ret._43 = position_z;
		ret._44 = position_w;
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::translation(const vector2<T>& position)
	{
		return translation(position.x, position.y);
	}

	template<typename T>
	matrix<T> matrix<T>::translation(const vector3<T>& position)
	{
		return translation(position.x, position.y, position.z);
	}

	template<typename T>
	matrix<T> matrix<T>::translation(const vector4<T>& position)
	{
		return translation(position.x, position.y, position.z, position.w);
	}

	template<typename T>
	matrix<T> matrix<T>::transform(const vector3<T>& scale, const quaternion<T>& rotation, const vector3<T>& position)
	{
		matrix ret = scaling(scale) * rotation_quaternion(rotation);
		ret._41 = position.x;
		ret._42 = position.y;
		ret._43 = position.z;
		ret._44 = static_cast<T>(1);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::transform(const vector3<T>& scale, const vector3<T>& euler, const vector3<T>& position)
	{
		matrix ret = scaling(scale) * rotation_xyz(euler);
		ret._41 = position.x;
		ret._42 = position.y;
		ret._43 = position.z;
		ret._44 = static_cast<T>(1);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::look_at(const vector3<T>& position, const vector3<T>& target, const vector3<T>& up_vector, coordinate_order coordinate)
	{
		vector3<T> axis_z;
		switch (coordinate)
		{
			case coordinate_order::rh:
			{
				axis_z = (position - target).create_normalize();
				break;
			}
			case coordinate_order::lh:
			default:
			{
				axis_z = (target - position).create_normalize();
				break;
			}
		}
		vector3<T> axis_x = vector3<T>::cross(up_vector, axis_z).create_normalize();
		vector3<T> axis_y = vector3<T>::cross(axis_z, axis_x).create_normalize();

		return matrix(axis_x.x, axis_y.x, axis_z.x, 0,
					  axis_x.y, axis_y.y, axis_z.y, 0,
					  axis_x.z, axis_y.z, axis_z.z, 0,
					  -vector3<T>::dot(axis_x, position), -vector3<T>::dot(axis_y, position), -vector3<T>::dot(axis_z, position), 1);
	}

	namespace
	{
		//	@tool 透視投影の深度成分（_33 / _34 / _43）の設定
		//	@tool Framework の右手系は z_order を無視して既定値で分岐し、normal の式も誤っていたため修正している
		//	@tool また右手系の無限遠は未対応（DebugAssert）だったため実装している
		template<typename T>
		void set_perspective_depth(matrix<T>& ret, T near_z, T far_z, bool infinite_far_plane, coordinate_order coordinate, projection_z_order z_order)
		{
			switch (coordinate)
			{
				case coordinate_order::rh:
				{
					if (infinite_far_plane)
					{
						if (z_order == projection_z_order::normal)
						{
							ret._33 = static_cast<T>(-1);
							ret._43 = -near_z;
						}
						else
						{
							ret._33 = static_cast<T>(0);
							ret._43 = near_z;
						}
					}
					else
					{
						if (z_order == projection_z_order::normal)
						{
							ret._33 = far_z / (near_z - far_z);
							ret._43 = near_z * ret._33;
						}
						else
						{
							ret._33 = near_z / (far_z - near_z);
							ret._43 = far_z * ret._33;
						}
					}
					ret._34 = static_cast<T>(-1);
					break;
				}
				case coordinate_order::lh:
				default:
				{
					if (infinite_far_plane)
					{
						if (z_order == projection_z_order::normal)
						{
							ret._33 = static_cast<T>(1);
							ret._43 = -near_z;
						}
						else
						{
							ret._33 = static_cast<T>(0);
							ret._43 = near_z;
						}
					}
					else
					{
						if (z_order == projection_z_order::normal)
						{
							ret._33 = far_z / (far_z - near_z);
							ret._43 = -ret._33 * near_z;
						}
						else
						{
							ret._33 = -near_z / (far_z - near_z);
							ret._43 = -far_z * ret._33;
						}
					}
					ret._34 = static_cast<T>(1);
					break;
				}
			}
			ret._44 = static_cast<T>(0);
		}

	}	//	namespace

	template<typename T>
	matrix<T> matrix<T>::perspective(T width, T height, T near_z, T far_z, coordinate_order coordinate, projection_z_order z_order)
	{
		matrix ret;
		ret._11 = static_cast<T>(2) * near_z / width;
		ret._22 = static_cast<T>(2) * near_z / height;
		set_perspective_depth(ret, near_z, far_z, !(far_z > near_z), coordinate, z_order);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::perspective_fov(T fov_y, T aspect, T near_z, T far_z, coordinate_order coordinate, projection_z_order z_order)
	{
		T sin_fov, cos_fov;
		math::angle::sin_cos(static_cast<T>(0.5) * fov_y, sin_fov, cos_fov);
		T y_scale = cos_fov / sin_fov;
		T x_scale = y_scale / aspect;

		matrix ret;
		//	係数算出
		ret._11 = x_scale;
		ret._22 = y_scale;
		set_perspective_depth(ret, near_z, far_z, far_z < near_z, coordinate, z_order);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::perspective_off_center(T left, T right, T top, T bottom, T near_z, T far_z, coordinate_order coordinate, projection_z_order z_order)
	{
		//	XNDC	= a * XView / ZView + b;
		//	YNDC	= c * YView / ZView + d;
		//	係数
		//	a		= ( 2.0f * near ) / ( right -   left )
		//	c		= ( 2.0f * near ) / (   top - bottom )
		//	中心オフセット
		//	b		= ( right +   left ) / ( right -   left )
		//	d		= (   top + bottom ) / (   top - bottom )
		//	オフセットがない場合の各座標との関係( 逆算時に利用 )
		//	left	= +near * tan( 水平視野角 / 2 )
		//	right	= -near * tan( 水平視野角 / 2 )
		//	top		= +near * tan( 垂直視野角 / 2 )
		//	bottom	= -near * tan( 垂直視野角 / 2 )
		//	ZNDC	= e / ZView + f
		//	+0～+1
		//	e		= -far * near / ( far - near )
		//	f		= +far / ( far - near )
		//	+1～+0
		//	e		= far * near / ( far - near )
		//	f		= -near / ( far - near )
		//	-1～+1
		//	e		= -2.0f * ( far * near ) / ( far - near )
		//	f		= ( far + near ) / ( far - near )
		//	-+～-1	意味がない
		//

		T square_near = near_z + near_z;
		T reciprocal_width = static_cast<T>(1) / (right - left);
		T reciprocal_height = static_cast<T>(1) / (top - bottom);

		matrix ret;
		//	係数算出
		ret._11 = square_near * reciprocal_width;
		ret._22 = square_near * reciprocal_height;
		//	オフセット算出
		if (coordinate == coordinate_order::rh)
		{
			ret._31 = (left + right) * reciprocal_width;
			ret._32 = (top + bottom) * reciprocal_height;
		}
		else
		{
			ret._31 = -(left + right) * reciprocal_width;
			ret._32 = -(top + bottom) * reciprocal_height;
		}
		set_perspective_depth(ret, near_z, far_z, far_z < near_z, coordinate, z_order);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::perspective_fov_offset_center(T fov_y, T aspect, T near_z, T far_z, T offset_x, T offset_y, coordinate_order coordinate, projection_z_order z_order)
	{
		T height = std::tan(fov_y * static_cast<T>(0.5));
		T width = std::tan(math::angle::fov_horizontal_to_fov_vertical(fov_y, aspect) * static_cast<T>(0.5));
		return perspective_off_center(-near_z * width + offset_x,
									  +near_z * width + offset_x,
									  +near_z * height + offset_y,
									  -near_z * height + offset_y,
									  near_z,
									  far_z,
									  coordinate,
									  z_order);
	}

	template<typename T>
	matrix<T> matrix<T>::orthographic(T width, T height, T near_z, T far_z, coordinate_order coordinate, projection_z_order z_order)
	{
		matrix ret;
		ret._11 = static_cast<T>(2) / width;
		ret._22 = static_cast<T>(2) / height;

		//	Zオーダーが反転指定ならNear/Farを入れ替える
		if (z_order == projection_z_order::reverse)
		{
			T temp = near_z;
			near_z = far_z;
			far_z = temp;
		}

		switch (coordinate)
		{
			case coordinate_order::rh:
			{
				ret._33 = static_cast<T>(1) / (near_z - far_z);
				ret._43 = near_z / (near_z - far_z);
				break;
			}
			case coordinate_order::lh:
			default:
			{
				ret._33 = static_cast<T>(1) / (far_z - near_z);
				ret._43 = near_z / (near_z - far_z);
				break;
			}
		}

		ret._44 = static_cast<T>(1);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::orthographic_offset_center(T left, T right, T top, T bottom, T near_z, T far_z, coordinate_order coordinate, projection_z_order z_order)
	{
		//	Zオーダーが反転指定ならNear/Farを入れ替える
		if (z_order == projection_z_order::reverse)
		{
			T temp = near_z;
			near_z = far_z;
			far_z = temp;
		}

		T reciprocal_width = static_cast<T>(1) / (right - left);
		T reciprocal_height = static_cast<T>(1) / (bottom - top);

		matrix ret;
		ret._11 = reciprocal_width + reciprocal_width;
		ret._22 = reciprocal_height + reciprocal_height;
		ret._41 = -(left + right) * reciprocal_width;
		ret._42 = -(top + bottom) * reciprocal_height;
		switch (coordinate)
		{
			case coordinate_order::rh:
			{
				ret._33 = static_cast<T>(1) / (near_z - far_z);
				ret._43 = ret._33 * near_z;
				break;
			}
			case coordinate_order::lh:
			default:
			{
				ret._33 = static_cast<T>(1) / (far_z - near_z);
				ret._43 = -ret._33 * near_z;
				break;
			}
		}

		ret._44 = static_cast<T>(1);
		return ret;
	}

	template<typename T>
	matrix<T> matrix<T>::viewport(T left, T right, T top, T bottom, T min_z, T max_z)
	{
		matrix ret;
		ret._11 = (right - left) / static_cast<T>(2);
		ret._12 = 0;
		ret._13 = 0;
		ret._14 = 0;

		ret._21 = 0;
		ret._22 = -(bottom - top) / static_cast<T>(2);
		ret._23 = 0;
		ret._24 = 0;

		ret._31 = 0;
		ret._32 = 0;
		ret._33 = max_z - min_z;
		ret._34 = 0;

		ret._41 = left + ret._11;
		ret._42 = top - ret._22;
		ret._43 = min_z;
		ret._44 = static_cast<T>(1);

		return ret;
	}

	template class matrix<float>;
	template class matrix<double>;

}	//	namespace math
