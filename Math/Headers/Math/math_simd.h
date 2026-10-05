#pragma once

#include <cstdint>

//	x86 / x64 以外は未対応
#if !defined(_M_X64) && !defined(_M_IX86)
#error "Math ライブラリは x86 / x64 のみ対応"
#endif
#include <emmintrin.h>
#if defined(__AVX2__)
#include <immintrin.h>
#endif

//	SIMD 処理
namespace math::simd
{
	//	float 4要素
	struct f4
	{
		using value_type = float;
		using reg = __m128;

		static reg set(float x, float y, float z, float w) { return _mm_setr_ps(x, y, z, w); }
		static reg set1(float value) { return _mm_set1_ps(value); }
		static reg zero() { return _mm_setzero_ps(); }

		static reg add(const reg& a, const reg& b) { return _mm_add_ps(a, b); }
		static reg sub(const reg& a, const reg& b) { return _mm_sub_ps(a, b); }
		static reg mul(const reg& a, const reg& b) { return _mm_mul_ps(a, b); }
		static reg div(const reg& a, const reg& b) { return _mm_div_ps(a, b); }
		static reg min(const reg& a, const reg& b) { return _mm_min_ps(a, b); }
		static reg max(const reg& a, const reg& b) { return _mm_max_ps(a, b); }
		static reg sqrt(const reg& a) { return _mm_sqrt_ps(a); }
		static reg neg(const reg& a) { return _mm_sub_ps(_mm_setzero_ps(), a); }
		static reg abs(const reg& a) { return _mm_andnot_ps(_mm_set1_ps(-0.0f), a); }

		static reg cmp_ge(const reg& a, const reg& b) { return _mm_cmpge_ps(a, b); }
		static reg cmp_lt(const reg& a, const reg& b) { return _mm_cmplt_ps(a, b); }
		static reg cmp_neq(const reg& a, const reg& b) { return _mm_cmpneq_ps(a, b); }
		//	mask が立っている要素は if_true、それ以外は if_false
		static reg select(const reg& mask, const reg& if_true, const reg& if_false) { return _mm_or_ps(_mm_and_ps(mask, if_true), _mm_andnot_ps(mask, if_false)); }

		template<int I0, int I1, int I2, int I3>
		static reg permute(const reg& v) { return _mm_shuffle_ps(v, v, _MM_SHUFFLE(I3, I2, I1, I0)); }
		//	要素 0, 1 を a から、要素 2, 3 を b から取り出す
		template<int I0, int I1, int I2, int I3>
		static reg shuffle(const reg& a, const reg& b) { return _mm_shuffle_ps(a, b, _MM_SHUFFLE(I3, I2, I1, I0)); }
		template<int I>
		static reg splat(const reg& v) { return permute<I, I, I, I>(v); }

		//	w を 0 にする
		static reg mask_xyz(const reg& v) { return _mm_and_ps(v, _mm_castsi128_ps(_mm_set_epi32(0, -1, -1, -1))); }
		//	z, w を 0 にする
		static reg mask_xy(const reg& v) { return _mm_and_ps(v, _mm_castsi128_ps(_mm_set_epi32(0, 0, -1, -1))); }
		static float first(const reg& v) { return _mm_cvtss_f32(v); }

		//	内積
		static reg dot2(const reg& a, const reg& b)
		{
			reg d = _mm_mul_ps(a, b);
			reg t = _mm_shuffle_ps(d, d, _MM_SHUFFLE(1, 1, 1, 1));
			d = _mm_add_ss(d, t);
			return splat<0>(d);
		}
		static reg dot3(const reg& a, const reg& b)
		{
			reg d = _mm_mul_ps(a, b);
			reg t = _mm_shuffle_ps(d, d, _MM_SHUFFLE(2, 1, 2, 1));
			d = _mm_add_ss(d, t);
			t = _mm_shuffle_ps(t, t, _MM_SHUFFLE(1, 1, 1, 1));
			d = _mm_add_ss(d, t);
			return splat<0>(d);
		}
		static reg dot4(const reg& a, const reg& b)
		{
			reg d = _mm_mul_ps(a, b);
			reg t = _mm_shuffle_ps(d, d, _MM_SHUFFLE(2, 3, 0, 1));
			d = _mm_add_ps(d, t);
			t = _mm_shuffle_ps(d, d, _MM_SHUFFLE(1, 0, 3, 2));
			return _mm_add_ps(d, t);
		}

		//	4x4 の転置
		static void transpose(const reg* in, reg* out)
		{
			reg r0 = in[0];
			reg r1 = in[1];
			reg r2 = in[2];
			reg r3 = in[3];
			_MM_TRANSPOSE4_PS(r0, r1, r2, r3);
			out[0] = r0;
			out[1] = r1;
			out[2] = r2;
			out[3] = r3;
		}

		static __m128 to_float4(const reg& v) { return v; }
		static reg from_float4(__m128 v) { return v; }
	};

#if defined(__AVX2__)
	//	double 4要素
	struct d4
	{
		using value_type = double;
		using reg = __m256d;

		static reg set(double x, double y, double z, double w) { return _mm256_setr_pd(x, y, z, w); }
		static reg set1(double value) { return _mm256_set1_pd(value); }
		static reg zero() { return _mm256_setzero_pd(); }

		static reg add(const reg& a, const reg& b) { return _mm256_add_pd(a, b); }
		static reg sub(const reg& a, const reg& b) { return _mm256_sub_pd(a, b); }
		static reg mul(const reg& a, const reg& b) { return _mm256_mul_pd(a, b); }
		static reg div(const reg& a, const reg& b) { return _mm256_div_pd(a, b); }
		static reg min(const reg& a, const reg& b) { return _mm256_min_pd(a, b); }
		static reg max(const reg& a, const reg& b) { return _mm256_max_pd(a, b); }
		static reg sqrt(const reg& a) { return _mm256_sqrt_pd(a); }
		static reg neg(const reg& a) { return _mm256_sub_pd(_mm256_setzero_pd(), a); }
		static reg abs(const reg& a) { return _mm256_andnot_pd(_mm256_set1_pd(-0.0), a); }

		static reg cmp_ge(const reg& a, const reg& b) { return _mm256_cmp_pd(a, b, _CMP_GE_OQ); }
		static reg cmp_lt(const reg& a, const reg& b) { return _mm256_cmp_pd(a, b, _CMP_LT_OQ); }
		static reg cmp_neq(const reg& a, const reg& b) { return _mm256_cmp_pd(a, b, _CMP_NEQ_UQ); }
		static reg select(const reg& mask, const reg& if_true, const reg& if_false) { return _mm256_blendv_pd(if_false, if_true, mask); }

		template<int I0, int I1, int I2, int I3>
		static reg permute(const reg& v) { return _mm256_permute4x64_pd(v, _MM_SHUFFLE(I3, I2, I1, I0)); }
		template<int I0, int I1, int I2, int I3>
		static reg shuffle(const reg& a, const reg& b)
		{
			return _mm256_blend_pd(_mm256_permute4x64_pd(a, _MM_SHUFFLE(I1, I0, I1, I0)), _mm256_permute4x64_pd(b, _MM_SHUFFLE(I3, I2, I3, I2)), 0b1100);
		}
		template<int I>
		static reg splat(const reg& v) { return permute<I, I, I, I>(v); }

		static reg mask_xyz(const reg& v) { return _mm256_blend_pd(v, _mm256_setzero_pd(), 0b1000); }
		static reg mask_xy(const reg& v) { return _mm256_blend_pd(v, _mm256_setzero_pd(), 0b1100); }
		static double first(const reg& v) { return _mm256_cvtsd_f64(v); }

		static reg dot2(const reg& a, const reg& b)
		{
			reg d = _mm256_mul_pd(a, b);
			return splat<0>(_mm256_add_pd(d, permute<1, 0, 3, 2>(d)));
		}
		static reg dot3(const reg& a, const reg& b)
		{
			return dot4(mask_xyz(a), b);
		}
		static reg dot4(const reg& a, const reg& b)
		{
			reg d = _mm256_mul_pd(a, b);
			d = _mm256_add_pd(d, permute<1, 0, 3, 2>(d));
			return _mm256_add_pd(d, permute<2, 3, 0, 1>(d));
		}

		static void transpose(const reg* in, reg* out)
		{
			reg t0 = _mm256_unpacklo_pd(in[0], in[1]);
			reg t1 = _mm256_unpackhi_pd(in[0], in[1]);
			reg t2 = _mm256_unpacklo_pd(in[2], in[3]);
			reg t3 = _mm256_unpackhi_pd(in[2], in[3]);
			out[0] = _mm256_permute2f128_pd(t0, t2, 0x20);
			out[1] = _mm256_permute2f128_pd(t1, t3, 0x20);
			out[2] = _mm256_permute2f128_pd(t0, t2, 0x31);
			out[3] = _mm256_permute2f128_pd(t1, t3, 0x31);
		}

		static __m128 to_float4(const reg& v) { return _mm256_cvtpd_ps(v); }
		static reg from_float4(__m128 v) { return _mm256_cvtps_pd(v); }
	};
#else
	//	double 4要素
	struct d4
	{
		using value_type = double;
		struct alignas(32) reg
		{
			__m128d xy;
			__m128d zw;
		};

		static reg set(double x, double y, double z, double w) { return { _mm_setr_pd(x, y), _mm_setr_pd(z, w) }; }
		static reg set1(double value) { __m128d v = _mm_set1_pd(value); return { v, v }; }
		static reg zero() { return { _mm_setzero_pd(), _mm_setzero_pd() }; }

		static reg add(const reg& a, const reg& b) { return { _mm_add_pd(a.xy, b.xy), _mm_add_pd(a.zw, b.zw) }; }
		static reg sub(const reg& a, const reg& b) { return { _mm_sub_pd(a.xy, b.xy), _mm_sub_pd(a.zw, b.zw) }; }
		static reg mul(const reg& a, const reg& b) { return { _mm_mul_pd(a.xy, b.xy), _mm_mul_pd(a.zw, b.zw) }; }
		static reg div(const reg& a, const reg& b) { return { _mm_div_pd(a.xy, b.xy), _mm_div_pd(a.zw, b.zw) }; }
		static reg min(const reg& a, const reg& b) { return { _mm_min_pd(a.xy, b.xy), _mm_min_pd(a.zw, b.zw) }; }
		static reg max(const reg& a, const reg& b) { return { _mm_max_pd(a.xy, b.xy), _mm_max_pd(a.zw, b.zw) }; }
		static reg sqrt(const reg& a) { return { _mm_sqrt_pd(a.xy), _mm_sqrt_pd(a.zw) }; }
		static reg neg(const reg& a) { return { _mm_sub_pd(_mm_setzero_pd(), a.xy), _mm_sub_pd(_mm_setzero_pd(), a.zw) }; }
		static reg abs(const reg& a)
		{
			__m128d sign = _mm_set1_pd(-0.0);
			return { _mm_andnot_pd(sign, a.xy), _mm_andnot_pd(sign, a.zw) };
		}

		static reg cmp_ge(const reg& a, const reg& b) { return { _mm_cmpge_pd(a.xy, b.xy), _mm_cmpge_pd(a.zw, b.zw) }; }
		static reg cmp_lt(const reg& a, const reg& b) { return { _mm_cmplt_pd(a.xy, b.xy), _mm_cmplt_pd(a.zw, b.zw) }; }
		static reg cmp_neq(const reg& a, const reg& b) { return { _mm_cmpneq_pd(a.xy, b.xy), _mm_cmpneq_pd(a.zw, b.zw) }; }
		static reg select(const reg& mask, const reg& if_true, const reg& if_false)
		{
			return { _mm_or_pd(_mm_and_pd(mask.xy, if_true.xy), _mm_andnot_pd(mask.xy, if_false.xy)),
					 _mm_or_pd(_mm_and_pd(mask.zw, if_true.zw), _mm_andnot_pd(mask.zw, if_false.zw)) };
		}

		//	4要素の中から2要素を選んで __m128d を作る
		template<int A, int B>
		static __m128d pick(const reg& v)
		{
			const __m128d& source_a = A < 2 ? v.xy : v.zw;
			const __m128d& source_b = B < 2 ? v.xy : v.zw;
			return _mm_shuffle_pd(source_a, source_b, (A & 1) | ((B & 1) << 1));
		}
		template<int I0, int I1, int I2, int I3>
		static reg permute(const reg& v) { return { pick<I0, I1>(v), pick<I2, I3>(v) }; }
		template<int I0, int I1, int I2, int I3>
		static reg shuffle(const reg& a, const reg& b) { return { pick<I0, I1>(a), pick<I2, I3>(b) }; }
		template<int I>
		static reg splat(const reg& v) { return permute<I, I, I, I>(v); }

		static reg mask_xyz(const reg& v) { return { v.xy, _mm_move_sd(_mm_setzero_pd(), v.zw) }; }
		static reg mask_xy(const reg& v) { return { v.xy, _mm_setzero_pd() }; }
		static double first(const reg& v) { return _mm_cvtsd_f64(v.xy); }

		static reg dot2(const reg& a, const reg& b)
		{
			__m128d d = _mm_mul_pd(a.xy, b.xy);
			d = _mm_add_pd(d, _mm_shuffle_pd(d, d, 1));
			return { d, d };
		}
		static reg dot3(const reg& a, const reg& b)
		{
			__m128d xy = _mm_mul_pd(a.xy, b.xy);
			__m128d z = _mm_mul_sd(a.zw, b.zw);
			__m128d d = _mm_add_pd(xy, _mm_shuffle_pd(xy, xy, 1));
			d = _mm_add_pd(d, _mm_unpacklo_pd(z, z));
			return { d, d };
		}
		static reg dot4(const reg& a, const reg& b)
		{
			__m128d d = _mm_add_pd(_mm_mul_pd(a.xy, b.xy), _mm_mul_pd(a.zw, b.zw));
			d = _mm_add_pd(d, _mm_shuffle_pd(d, d, 1));
			return { d, d };
		}

		static void transpose(const reg* in, reg* out)
		{
			reg r0 = in[0];
			reg r1 = in[1];
			reg r2 = in[2];
			reg r3 = in[3];
			out[0] = { _mm_unpacklo_pd(r0.xy, r1.xy), _mm_unpacklo_pd(r2.xy, r3.xy) };
			out[1] = { _mm_unpackhi_pd(r0.xy, r1.xy), _mm_unpackhi_pd(r2.xy, r3.xy) };
			out[2] = { _mm_unpacklo_pd(r0.zw, r1.zw), _mm_unpacklo_pd(r2.zw, r3.zw) };
			out[3] = { _mm_unpackhi_pd(r0.zw, r1.zw), _mm_unpackhi_pd(r2.zw, r3.zw) };
		}

		static __m128 to_float4(const reg& v) { return _mm_movelh_ps(_mm_cvtpd_ps(v.xy), _mm_cvtpd_ps(v.zw)); }
		static reg from_float4(__m128 v) { return { _mm_cvtps_pd(v), _mm_cvtps_pd(_mm_movehl_ps(v, v)) }; }
	};
#endif	//	defined(__AVX2__)

	//	長さの2乗 length_sq を使って正規化する
	template<typename Ops>
	typename Ops::reg normalize(const typename Ops::reg& v, const typename Ops::reg& length_sq, typename Ops::value_type threshold)
	{
		typename Ops::reg length = Ops::sqrt(length_sq);
		typename Ops::reg mask = Ops::cmp_ge(length, Ops::set1(threshold));
		return Ops::select(mask, Ops::div(v, length), Ops::zero());
	}

	//	3次元の外積
	template<typename Ops>
	typename Ops::reg cross3(const typename Ops::reg& a, const typename Ops::reg& b)
	{
		return Ops::sub(Ops::mul(Ops::template permute<1, 2, 0, 3>(a), Ops::template permute<2, 0, 1, 3>(b)),
						Ops::mul(Ops::template permute<2, 0, 1, 3>(a), Ops::template permute<1, 2, 0, 3>(b)));
	}

	//	4次元の外積
	template<typename Ops>
	typename Ops::reg cross4(const typename Ops::reg& l, const typename Ops::reg& c, const typename Ops::reg& r)
	{
		typename Ops::reg term1 = Ops::mul(Ops::template permute<1, 0, 0, 0>(l),
			Ops::sub(Ops::mul(Ops::template permute<2, 2, 1, 1>(c), Ops::template permute<3, 3, 3, 2>(r)),
					 Ops::mul(Ops::template permute<2, 2, 1, 1>(r), Ops::template permute<3, 3, 3, 2>(c))));
		typename Ops::reg term2 = Ops::mul(Ops::template permute<2, 2, 1, 1>(l),
			Ops::sub(Ops::mul(Ops::template permute<1, 0, 0, 0>(c), Ops::template permute<3, 3, 3, 2>(r)),
					 Ops::mul(Ops::template permute<1, 0, 0, 0>(r), Ops::template permute<3, 3, 3, 2>(c))));
		typename Ops::reg term3 = Ops::mul(Ops::template permute<3, 3, 3, 2>(l),
			Ops::sub(Ops::mul(Ops::template permute<1, 0, 0, 0>(c), Ops::template permute<2, 2, 1, 1>(r)),
					 Ops::mul(Ops::template permute<1, 0, 0, 0>(r), Ops::template permute<2, 2, 1, 1>(c))));
		return Ops::mul(Ops::add(Ops::sub(term1, term2), term3), Ops::set(1, -1, 1, -1));
	}

	template<typename Ops>
	typename Ops::reg transform4(const typename Ops::reg& v, const typename Ops::reg* m)
	{
		typename Ops::reg result = Ops::mul(Ops::template splat<0>(v), m[0]);
		result = Ops::add(result, Ops::mul(Ops::template splat<1>(v), m[1]));
		result = Ops::add(result, Ops::mul(Ops::template splat<2>(v), m[2]));
		result = Ops::add(result, Ops::mul(Ops::template splat<3>(v), m[3]));
		return result;
	}

	template<typename Ops>
	typename Ops::reg transform3_point(const typename Ops::reg& v, const typename Ops::reg* m)
	{
		typename Ops::reg result = Ops::add(Ops::mul(Ops::template splat<2>(v), m[2]), m[3]);
		result = Ops::add(result, Ops::mul(Ops::template splat<1>(v), m[1]));
		result = Ops::add(result, Ops::mul(Ops::template splat<0>(v), m[0]));
		return result;
	}

	template<typename Ops>
	typename Ops::reg transform3_normal(const typename Ops::reg& v, const typename Ops::reg* m)
	{
		typename Ops::reg result = Ops::mul(Ops::template splat<2>(v), m[2]);
		result = Ops::add(result, Ops::mul(Ops::template splat<1>(v), m[1]));
		result = Ops::add(result, Ops::mul(Ops::template splat<0>(v), m[0]));
		return result;
	}

	//	4x4 行列の積 out = a * b
	template<typename Ops>
	void multiply(const typename Ops::reg* a, const typename Ops::reg* b, typename Ops::reg* out)
	{
		typename Ops::reg r0 = transform4<Ops>(a[0], b);
		typename Ops::reg r1 = transform4<Ops>(a[1], b);
		typename Ops::reg r2 = transform4<Ops>(a[2], b);
		typename Ops::reg r3 = transform4<Ops>(a[3], b);
		out[0] = r0;
		out[1] = r1;
		out[2] = r2;
		out[3] = r3;
	}

	//	クォータニオンの積 q * r
	template<typename Ops>
	typename Ops::reg quaternion_multiply(const typename Ops::reg& q, const typename Ops::reg& r)
	{
		typename Ops::reg result = Ops::mul(Ops::template splat<3>(r), q);
		result = Ops::add(result, Ops::mul(Ops::template splat<0>(r), Ops::mul(Ops::template permute<3, 2, 1, 0>(q), Ops::set(1, -1, 1, -1))));
		result = Ops::add(result, Ops::mul(Ops::template splat<1>(r), Ops::mul(Ops::template permute<2, 3, 0, 1>(q), Ops::set(1, 1, -1, -1))));
		result = Ops::add(result, Ops::mul(Ops::template splat<2>(r), Ops::mul(Ops::template permute<1, 0, 3, 2>(q), Ops::set(-1, 1, 1, -1))));
		return result;
	}

	//	4x4 行列の行列式
	template<typename Ops>
	typename Ops::value_type determinant(const typename Ops::reg* m)
	{
		typename Ops::reg columns[4];
		Ops::transpose(m, columns);
		typename Ops::reg minor = cross4<Ops>(columns[0], columns[1], columns[2]);
		return -Ops::first(Ops::dot4(columns[3], minor));
	}

	//	4x4 行列の逆行列
	template<typename Ops>
	bool inverse(const typename Ops::reg* m, typename Ops::reg* out)
	{
		typename Ops::value_type det = determinant<Ops>(m);
		if (det == 0)
		{
			return false;
		}

		typename Ops::reg cofactor[4];
		cofactor[0] = cross4<Ops>(m[1], m[2], m[3]);
		cofactor[1] = Ops::neg(cross4<Ops>(m[0], m[2], m[3]));
		cofactor[2] = cross4<Ops>(m[0], m[1], m[3]);
		cofactor[3] = Ops::neg(cross4<Ops>(m[0], m[1], m[2]));

		typename Ops::reg inv_det = Ops::set1(static_cast<typename Ops::value_type>(1) / det);
		Ops::transpose(cofactor, out);
		for (int i = 0; i < 4; ++i)
		{
			out[i] = Ops::mul(out[i], inv_det);
		}
		return true;
	}

}	//	namespace math::simd
