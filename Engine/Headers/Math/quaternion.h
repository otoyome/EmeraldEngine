#pragma once

#include "Math/math_common.h"

namespace math
{
	//	クォータニオン演算クラス
	//	@tool 【格納】(x, y, z, w)。規約に関係なく同じで、内部の計算も 1 通り
	//	@tool 【規約で変わるもの】q1 * q2 の順序（行ベクトル規約：q1 の後に q2、列ベクトル規約：q2 の後に q1。matrix の * と同じ）
	//	@tool         front() / look_rotation() の前方向（左手系：+Z、右手系：-Z）
	//	@tool 規約に関係なく使えるもの：concatenate(first, second)（first の後に second）、hamilton_product(a, b)（数学上の積）、rotate(v)
	template<typename T>
	class alignas(simd_alignment_v<T>) quaternion
	{
		static_assert(is_supported_scalar_v<T>, "quaternion は float / double のみ対応");

	public:
		using value_type = T;
		using ops = simd_ops_t<T>;
		using register_type = simd_register_t<T>;
		static constexpr std::uint32_t element_count = 4;	//	要素数

	public:
		static quaternion identity() { return quaternion(0, 0, 0, 1); }

	public:
		quaternion() { set(0, 0, 0, 1); }
		quaternion(const quaternion&) = default;
		quaternion& operator=(const quaternion&) = default;
		quaternion(T x, T y, T z, T w) { set(x, y, z, w); }
		explicit quaternion(const T values[element_count]) { set(values[0], values[1], values[2], values[3]); }
		quaternion(const basic_element4<T>& v) { set(v.x, v.y, v.z, v.w); }
		//	@tool 要素の型が違う保存用の型からの変換
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit quaternion(const basic_element4<U>& v) { set(static_cast<T>(v.x), static_cast<T>(v.y), static_cast<T>(v.z), static_cast<T>(v.w)); }
		explicit quaternion(const vector3<T>& euler, rotation_order order = default_rotation_order);
		quaternion(const vector3<T>& axis, T angle);
		quaternion(const vector3<T>& vector1, const vector3<T>& vector2);
		explicit quaternion(const matrix<T>& m);
		//	@tool float <> double の変換（精度が変わるため明示的に行う）
		template<typename U, std::enable_if_t<!std::is_same_v<T, U>, int> = 0>
		explicit quaternion(const quaternion<U>& q) { set(static_cast<T>(q.x), static_cast<T>(q.y), static_cast<T>(q.z), static_cast<T>(q.w)); }
		//	@tool SIMD レジスタから生成
		explicit quaternion(const register_type& v) { reg = v; }

		void set(T set_x, T set_y, T set_z, T set_w)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::set(set_x, set_y, set_z, set_w);
			}
			else
			{
				x = set_x;
				y = set_y;
				z = set_z;
				w = set_w;
			}
		}

		//	長さ取得
		T length() const { return std::sqrt(square_length()); }

		//	長さの2乗を取得
		T square_length() const { return dot(*this); }

		//	@tool ベクトルを回転させたベクトルを取得（q * v * q^-1。規約に関係なく同じ結果）
		vector3<T> rotate(const vector3<T>& v) const;

		//	回転軸の取得
		vector3<T> axis() const;

		//	回転軸に対する回転角度
		T angle() const { return std::acos(w) * static_cast<T>(2); }

		//	回転軸と回転角度の取得
		void axis_angle(vector3<T>& out_axis, T& out_angle) const;

		//	前方向ベクトル取得
		//	@tool 左手系は +Z、右手系は -Z を回転させた向き
		vector3<T> front(coordinate_order coordinate = default_coordinate_order) const;

		//	右方向ベクトル取得
		vector3<T> right() const;

		//	上方向ベクトル取得
		vector3<T> up() const;

		//	正規化
		//	@tool 長さが閾値未満のときは変更しない（Framework の実装どおり）
		void normalize()
		{
			if constexpr (use_simd_v<T>)
			{
				register_type length = ops::sqrt(ops::dot4(reg, reg));
				register_type mask = ops::cmp_ge(length, ops::set1(constants<T>::normalize_threshold));
				reg = ops::select(mask, ops::div(reg, length), reg);
			}
			else
			{
				T work = length();
				if (work >= constants<T>::normalize_threshold)
				{
					work = static_cast<T>(1) / work;
					x *= work;
					y *= work;
					z *= work;
					w *= work;
				}
			}
		}

		//	正規化済みクォータニオンの取得
		quaternion normalized() const { quaternion ret = *this; ret.normalize(); return ret; }

		//	共役化
		void conjugate()
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::mul(reg, ops::set(-1, -1, -1, 1));
			}
			else
			{
				x = -x;
				y = -y;
				z = -z;
			}
		}

		//	共役化済みクォータニオンの取得
		quaternion conjugated() const { quaternion ret = *this; ret.conjugate(); return ret; }

		//	逆クォータニオン化
		void invert()
		{
			T work = square_length();
			conjugate();
			multiply(static_cast<T>(1) / work);
		}

		//	逆クォータニオン化済みクォータニオンの取得
		quaternion inverted() const { quaternion ret = *this; ret.invert(); return ret; }

		//	オイラー角に変換して取得
		vector3<T> euler_angle(rotation_order order = default_rotation_order) const;

		//	オイラー角に変換して設定
		void euler_angle(const vector3<T>& euler, rotation_order order = default_rotation_order);

		//	ミラーリング
		void mirror(mirroring_order order);

		//	ミラーリング
		quaternion mirrored(mirroring_order order) const { quaternion ret = *this; ret.mirror(order); return ret; }

	public:
		//	内積
		T dot(const quaternion& r) const
		{
			if constexpr (use_simd_v<T>)
			{
				return ops::first(ops::dot4(reg, r.reg));
			}
			else
			{
				return x * r.x + y * r.y + z * r.z + w * r.w;
			}
		}

		//	クォータニオン同士の結合（*this = *this * r）
		//	@tool 行ベクトル規約は「this の回転の後に r の回転」、列ベクトル規約は「r の回転の後に this の回転」
		//	@tool どちらの規約でも matrix(q1 * q2) == matrix(q1) * matrix(q2) になる
		void multiply(const quaternion& r)
		{
			if constexpr (is_column_vector)
			{
				*this = concatenate(r, *this);
			}
			else
			{
				*this = concatenate(*this, r);
			}
		}

		//	@tool 「first の回転の後に second の回転」を表すクォータニオン（規約に関係なく同じ意味。matrix::concatenate と対応）
		static quaternion concatenate(const quaternion& first, const quaternion& second)
		{
			return hamilton_product(second, first);
		}
		//	@tool 数学上のハミルトン積 a * b（規約に関係なく「b の回転の後に a の回転」）
		static quaternion hamilton_product(const quaternion& a, const quaternion& b)
		{
			if constexpr (use_simd_v<T>)
			{
				return quaternion(simd::quaternion_multiply<ops>(b.reg, a.reg));
			}
			else
			{
				return quaternion(a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
								  a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
								  a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
								  a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z);
			}
		}
		//	掛け算
		void multiply(T value)
		{
			if constexpr (use_simd_v<T>)
			{
				reg = ops::mul(reg, ops::set1(value));
			}
			else
			{
				x *= value;
				y *= value;
				z *= value;
				w *= value;
			}
		}

		//	回転制限
		//	@return	制限がかかったかどうか
		bool restrict(const vector3<T>& min_angle, const vector3<T>& max_angle);

		//	@tool 保存用の型への変換（double -> float は丸められる）
		float4 to_float4() const
		{
			float4 ret;
			if constexpr (use_simd_v<T>)
			{
				_mm_storeu_ps(ret.elements, ops::to_float4(reg));
			}
			else
			{
				ret = float4(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(w));
			}
			return ret;
		}
		double4 to_double4() const { return double4(static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), static_cast<double>(w)); }

	public:
		T& operator[](std::int32_t index) { return elements[index]; }
		const T& operator[](std::int32_t index) const { return elements[index]; }
		quaternion operator-() const { return quaternion(-x, -y, -z, -w); }
		quaternion operator*(const quaternion& q) const { quaternion ret = *this; ret.multiply(q); return ret; }
		quaternion operator*(T value) const { quaternion ret = *this; ret.multiply(value); return ret; }
		quaternion& operator*=(const quaternion& q) { multiply(q); return *this; }
		quaternion& operator*=(T value) { multiply(value); return *this; }

		bool operator==(const quaternion& q) const { return x == q.x && y == q.y && z == q.z && w == q.w; }
		bool operator!=(const quaternion& q) const { return !(*this == q); }

	public:
		//	ピッチ角を指定したクォータニオンを生成
		static quaternion rotation_x(T pitch);

		//	ヨー角を指定したクォータニオンを生成
		static quaternion rotation_y(T yaw);

		//	ロール角を指定したクォータニオンを生成
		static quaternion rotation_z(T roll);

		//	オイラー角をクォータニオンに変換
		static quaternion rotation_xyz(const vector3<T>& rotation, rotation_order order = default_rotation_order);

		//	オイラー角をクォータニオンに変換
		static quaternion rotation_xyz(T pitch, T yaw, T roll, rotation_order order = default_rotation_order);

		//	オイラー角を取得する
		//	@return	ジンバルロック判定
		static bool to_euler_angle(vector3<T>& out, const quaternion& rotation, rotation_order order = default_rotation_order);

		//	オイラー角を取得する
		//	@return	ジンバルロック判定
		static bool to_euler_angle(T& out_x, T& out_y, T& out_z, const quaternion& rotation, rotation_order order = default_rotation_order);

		//	指定軸と軸周りの回転角度からクォータニオンを生成
		static quaternion rotation_axis(const vector3<T>& axis, T angle);

		//	２つの方向ベクトルからクォータニオンを生成
		//	@tool 引数は正規化済みの方向ベクトルであること
		static quaternion vector_to_vector(const vector3<T>& direction1, const vector3<T>& direction2);

		//	回転行列からクォータニオンを生成
		static quaternion rotation_matrix(const matrix<T>& m);

		//	指定の上方向を考慮し、指定の方向に向いたクォータニオンを算出
		//	@tool 回転後の front() が forward を向く（左手系は +Z、右手系は -Z を forward に合わせる）
		static quaternion look_rotation(const vector3<T>& forward, const vector3<T>& up, coordinate_order coordinate = default_coordinate_order);

	public:
		union
		{
			register_type reg;
			T elements[element_count];
			struct { T x, y, z, w; };
		};
	};

	using quaternionf = quaternion<float>;
	using quaterniond = quaternion<double>;

	extern template class quaternion<float>;
	extern template class quaternion<double>;

}	//	namespace math
