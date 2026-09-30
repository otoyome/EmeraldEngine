#pragma once

#include "Math/math_common.h"

//	‹Èü“I‚È•âŠÔŠÖ”ŒQ
//	time ‚É 0`1‚ğ—^‚¦‚½‚ç•âŠ®ˆ—‚ğŒo‚Ä 0`1 ‚ğ•Ô‚·B
//	Interpolation ‚Æ‚ ‚¦‚Ä•ª‚¯‚Ä‚¢‚é‚Ì‚Í easing cheat seat ‚ÌŠÖŒWB
//	‚Ç‚¤‚¢‚¤‹Èü‚ª‚Å‚«‚é‚©‚Í https://easings.net/ QÆ‚µ‚Ä‚­‚¾‚³‚¢
//	@tool Šù‘¶‚Ì math::easeiSystem/math_utility.hj‚Æ–¼‘O‚ªÕ“Ë‚·‚é‚½‚ß math::easing ‚Æ‚µ‚Ä‚¢‚é
namespace math::easing
{
	template<typename T>
	T in_sine(T time) { return std::sin(constants<T>::pi_half * time); }
	template<typename T>
	T out_sine(T time) { return static_cast<T>(1) + std::sin(constants<T>::pi_half * (time - static_cast<T>(1))); }
	template<typename T>
	T in_out_sine(T time) { return static_cast<T>(0.5) * (static_cast<T>(1) + std::sin(constants<T>::pi * (time - static_cast<T>(0.5)))); }

	template<typename T>
	T in_quad(T time) { return time * time; }
	template<typename T>
	T out_quad(T time) { return time * (static_cast<T>(2) - time); }
	template<typename T>
	T in_out_quad(T time)
	{
		return time < static_cast<T>(0.5) ? static_cast<T>(2) * time * time : time * (static_cast<T>(4) - static_cast<T>(2) * time) - static_cast<T>(1);
	}

	template<typename T>
	T in_cubic(T time) { return time * time * time; }
	template<typename T>
	T out_cubic(T time)
	{
		time -= static_cast<T>(1);
		return static_cast<T>(1) + time * time * time;
	}
	template<typename T>
	T in_out_cubic(T time)
	{
		if (time < static_cast<T>(0.5))
		{
			return static_cast<T>(4) * time * time * time;
		}
		//	@tool Framework ‚Ì® 1 + (--t) * (2 * (--t)) * (2 * t) ‚Í•]‰¿‡‚ª–¢‹K’è‚Ì‚½‚ßAˆÓ}‚Ç‚¨‚è‚ÌŒ`‚É“WŠJ‚µ‚Ä‚¢‚é
		time -= static_cast<T>(1);
		return static_cast<T>(1) + static_cast<T>(4) * time * time * time;
	}

	template<typename T>
	T in_quart(T time)
	{
		time *= time;
		return time * time;
	}
	template<typename T>
	T out_quart(T time)
	{
		time -= static_cast<T>(1);
		time *= time;
		return static_cast<T>(1) - time * time;
	}
	template<typename T>
	T in_out_quart(T time)
	{
		if (time < static_cast<T>(0.5))
		{
			time *= time;
			return static_cast<T>(8) * time * time;
		}
		time -= static_cast<T>(1);
		time *= time;
		return static_cast<T>(1) - static_cast<T>(8) * time * time;
	}

	template<typename T>
	T in_quint(T time)
	{
		T time_pow = time * time;
		return time * time_pow * time_pow;
	}
	template<typename T>
	T out_quint(T time)
	{
		time -= static_cast<T>(1);
		T time_pow = time * time;
		return static_cast<T>(1) + time * time_pow * time_pow;
	}
	template<typename T>
	T in_out_quint(T time)
	{
		T time_pow;
		if (time < static_cast<T>(0.5))
		{
			time_pow = time * time;
			return static_cast<T>(16) * time * time_pow * time_pow;
		}
		time -= static_cast<T>(1);
		time_pow = time * time;
		return static_cast<T>(1) + static_cast<T>(16) * time * time_pow * time_pow;
	}

	template<typename T>
	T in_expo(T time) { return (std::pow(static_cast<T>(2), static_cast<T>(8) * time) - static_cast<T>(1)) / static_cast<T>(255); }
	template<typename T>
	T out_expo(T time) { return static_cast<T>(1) - std::pow(static_cast<T>(2), static_cast<T>(-8) * time); }
	template<typename T>
	T in_out_expo(T time)
	{
		if (time < static_cast<T>(0.5))
		{
			return (std::pow(static_cast<T>(2), static_cast<T>(16) * time) - static_cast<T>(1)) / static_cast<T>(510);
		}
		return static_cast<T>(1) - static_cast<T>(0.5) * std::pow(static_cast<T>(2), static_cast<T>(-16) * (time - static_cast<T>(0.5)));
	}

	template<typename T>
	T in_circ(T time) { return static_cast<T>(1) - std::sqrt(static_cast<T>(1) - time); }
	template<typename T>
	T out_circ(T time) { return std::sqrt(time); }
	template<typename T>
	T in_out_circ(T time)
	{
		if (time < static_cast<T>(0.5))
		{
			return (static_cast<T>(1) - std::sqrt(static_cast<T>(1) - static_cast<T>(2) * time)) * static_cast<T>(0.5);
		}
		return (static_cast<T>(1) + std::sqrt(static_cast<T>(2) * time - static_cast<T>(1))) * static_cast<T>(0.5);
	}

	template<typename T>
	T in_back(T time) { return time * time * (static_cast<T>(2.70158) * time - static_cast<T>(1.70158)); }
	template<typename T>
	T out_back(T time)
	{
		time -= static_cast<T>(1);
		return static_cast<T>(1) + time * time * (static_cast<T>(2.70158) * time + static_cast<T>(1.70158));
	}
	template<typename T>
	T in_out_back(T time)
	{
		if (time < static_cast<T>(0.5))
		{
			return time * time * (static_cast<T>(7) * time - static_cast<T>(2.5)) * static_cast<T>(2);
		}
		time -= static_cast<T>(1);
		return static_cast<T>(1) + time * time * static_cast<T>(2) * (static_cast<T>(7) * time + static_cast<T>(2.5));
	}

	template<typename T>
	T in_elastic(T time)
	{
		T time_pow = time * time;
		return time_pow * time_pow * std::sin(time * constants<T>::pi * static_cast<T>(4.5));
	}
	template<typename T>
	T out_elastic(T time)
	{
		T time_pow = (time - static_cast<T>(1)) * (time - static_cast<T>(1));
		return static_cast<T>(1) - time_pow * time_pow * std::cos(time * constants<T>::pi * static_cast<T>(4.5));
	}
	template<typename T>
	T in_out_elastic(T time)
	{
		T time_pow;
		if (time < static_cast<T>(0.45))
		{
			time_pow = time * time;
			return static_cast<T>(8) * time_pow * time_pow * std::sin(time * constants<T>::pi * static_cast<T>(9));
		}
		else if (time < static_cast<T>(0.55))
		{
			return static_cast<T>(0.5) + static_cast<T>(0.75) * std::sin(time * constants<T>::pi * static_cast<T>(4));
		}
		time_pow = (time - static_cast<T>(1)) * (time - static_cast<T>(1));
		return static_cast<T>(1) - static_cast<T>(8) * time_pow * time_pow * std::sin(time * constants<T>::pi * static_cast<T>(9));
	}

	template<typename T>
	T in_bounce(T time) { return std::pow(static_cast<T>(2), static_cast<T>(6) * (time - static_cast<T>(1))) * std::abs(std::sin(time * constants<T>::pi * static_cast<T>(3.5))); }
	template<typename T>
	T out_bounce(T time) { return static_cast<T>(1) - std::pow(static_cast<T>(2), static_cast<T>(-6) * time) * std::abs(std::cos(time * constants<T>::pi * static_cast<T>(3.5))); }
	template<typename T>
	T in_out_bounce(T time)
	{
		if (time < static_cast<T>(0.5))
		{
			return static_cast<T>(8) * std::pow(static_cast<T>(2), static_cast<T>(8) * (time - static_cast<T>(1))) * std::abs(std::sin(time * constants<T>::pi * static_cast<T>(7)));
		}
		return static_cast<T>(1) - static_cast<T>(8) * std::pow(static_cast<T>(2), static_cast<T>(-8) * time) * std::abs(std::sin(time * constants<T>::pi * static_cast<T>(7)));
	}

	//	•âŠÔˆ—’è”
	enum class type
	{
		in_sine = 0,
		out_sine,
		in_out_sine,
		in_quad,
		out_quad,
		in_out_quad,
		in_cubic,
		out_cubic,
		in_out_cubic,
		in_quart,
		out_quart,
		in_out_quart,
		in_quint,
		out_quint,
		in_out_quint,
		in_expo,
		out_expo,
		in_out_expo,
		in_circ,
		out_circ,
		in_out_circ,
		in_back,
		out_back,
		in_out_back,
		in_elastic,
		out_elastic,
		in_out_elastic,
		in_bounce,
		out_bounce,
		in_out_bounce,

		lerp,
		max,
	};

	//	@tool time ‚Í 0`1 ‚ÉŠÛ‚ß‚Ä‚©‚çŒvZ‚·‚éiFramework ‚ÍŠÛ‚ß‚½’l‚ğg‚Á‚Ä‚¢‚È‚©‚Á‚½j
	template<typename T>
	T calculate(T time, type easing_type)
	{
		T value = time < static_cast<T>(0) ? static_cast<T>(0) : (time > static_cast<T>(1) ? static_cast<T>(1) : time);
		switch (easing_type)
		{
			case type::in_sine:			return in_sine(value);
			case type::out_sine:		return out_sine(value);
			case type::in_out_sine:		return in_out_sine(value);

			case type::in_quad:			return in_quad(value);
			case type::out_quad:		return out_quad(value);
			case type::in_out_quad:		return in_out_quad(value);

			case type::in_cubic:		return in_cubic(value);
			case type::out_cubic:		return out_cubic(value);
			case type::in_out_cubic:	return in_out_cubic(value);

			case type::in_quart:		return in_quart(value);
			case type::out_quart:		return out_quart(value);
			case type::in_out_quart:	return in_out_quart(value);

			case type::in_quint:		return in_quint(value);
			case type::out_quint:		return out_quint(value);
			case type::in_out_quint:	return in_out_quint(value);

			case type::in_expo:			return in_expo(value);
			case type::out_expo:		return out_expo(value);
			case type::in_out_expo:		return in_out_expo(value);

			case type::in_circ:			return in_circ(value);
			case type::out_circ:		return out_circ(value);
			case type::in_out_circ:		return in_out_circ(value);

			case type::in_back:			return in_back(value);
			case type::out_back:		return out_back(value);
			case type::in_out_back:		return in_out_back(value);

			case type::in_elastic:		return in_elastic(value);
			case type::out_elastic:		return out_elastic(value);
			case type::in_out_elastic:	return in_out_elastic(value);

			case type::in_bounce:		return in_bounce(value);
			case type::out_bounce:		return out_bounce(value);
			case type::in_out_bounce:	return in_out_bounce(value);
		}
		return value;
	}

}	//	namespace math::easing
