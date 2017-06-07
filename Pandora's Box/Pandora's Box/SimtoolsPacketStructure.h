#pragma once
#include <cstdint>
namespace Simtools
{
	struct DOFPacket
	{
		static constexpr size_t GetStructSizeinByte()
		{
			return sizeof(DOFPacket);
		}

		int16_t		m_roll;
		int16_t		m_pitch;
		int16_t		m_heave;
		int16_t		m_yaw;
		int16_t		m_sway;
		int16_t		m_surge;
	};
}