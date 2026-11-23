#pragma once

namespace core
{
	template<typename BitFlagType, typename FlagType>
	bool is_bitflag(const BitFlagType& bit_flags, FlagType flag)
	{
		return bit_flags & static_cast<BitFlagType>(flag);
	}

	template<typename BitFlagType, typename FlagType>
	void set_bitflag(BitFlagType& bit_flags, FlagType flag, bool value)
	{
		if (value)	bit_flags |= static_cast<BitFlagType>(flag);
		else		bit_flags &= ~static_cast<BitFlagType>(flag);
	}

}	//	namespace core
