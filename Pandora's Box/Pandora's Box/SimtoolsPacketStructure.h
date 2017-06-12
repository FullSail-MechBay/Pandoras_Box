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

		uint16_t		m_roll;
		uint16_t		m_pitch;
		uint16_t		m_heave;
		uint16_t		m_yaw;
		uint16_t		m_sway;
		uint16_t		m_surge;
	};
}