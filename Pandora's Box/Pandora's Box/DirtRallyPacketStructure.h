#pragma once
#include <cstdint>
namespace DirtRally
{
	struct UDPPacketNoExtra
	{
		static constexpr size_t GetStructSizeinByte()
		{
			return sizeof(UDPPacketNoExtra);
		}

		uint32_t	m_time;
		float		m_angvelx;
		float		m_angvely;
		float		m_angvelz;
		float		m_heading;
		float		m_pitch;
		float		m_roll;
		float		m_accelx;
		float		m_accely;
		float		m_accelz;
		float		m_velx;
		float		m_vely;
		float		m_velz;
		float		m_x;
		float		m_y;
		float		m_z;
		uint32_t	m_id; //unused

		
	};
	struct UDPPacket 
	{
		static constexpr size_t GetStructSizeinByte()
		{
			return sizeof(UDPPacket);
		}

		float m_time;
		float m_lapTime;
		float m_lapDistance;
		float m_totalDistance;
		float m_x; // World space position
		float m_y; // World space position
		float m_z;// World space position
		float m_speed;
		float m_xv;// Velocity in world space
		float m_yv;// Velocity in world space
		float m_zv;// Velocity in world space
		float m_xr;// World space right direction
		float m_yr;// World space right direction
		float m_zr;// World space right direction
		float m_xd;// World space forward direction 
		float m_yd;// World space forward direction 
		float m_zd;// World space forward direction 

		////Extra Data
		//float m_susp_pos_bl;
		//float m_susp_pos_br;
		//float m_susp_pos_fl;
		//float m_susp_pos_fr;
		//float m_susp_vel_bl;
		//float m_susp_vel_br;
		//float m_susp_vel_fl;
		//float m_susp_vel_fr;
		//float m_wheel_speed_bl;
		//float m_wheel_speed_br;
		//float m_wheel_speed_fl;
		//float m_wheel_speed_fr;
		//float m_throttle;
		//float m_steer;
		//float m_brake;
		//float m_clutch;
		//float m_gear;
		//float m_gforce_lat;
		//float m_gforce_lon;
		//float m_lap;
		//float m_engineRate;
		//float m_sli_pro_native_support;// SLI Pro support
		//float m_car_position;// car race position
		//float m_kers_level;// kers energy left
		//float m_kers_max_level;// kers maximum energy
		//float m_drs;// 0 = off, 1 = on
		//float m_traction_control;// 0 (off) - 2 (high)
		//float m_anti_lock_brakes;// 0 (off) - 1 (on)
		//float m_fuel_in_tank;// current fuel mass
		//float m_fuel_capacity;// fuel capacity
		//float m_in_pits;// 0 = none, 1 = pitting, 2 = in pit area
		//float m_sector;// 0 = sector1, 1 = sector2; 2 = sector3
		//float m_sector1_time;// time of sector1 (or 0)
		//float m_sector2_time;// time of sector2 (or 0)
		//float m_brakes_temp[4];// brakes temperature (centigrade)
		//float m_wheels_pressure[4];// wheels pressure PSI
		//float m_team_info;// team ID
		//float m_total_laps;// total number of laps in this race
		//float m_track_size;// track size meters
		//float m_last_lap_time;// last lap time
		//float m_max_rpm;// cars max RPM, at which point the rev limiter will kick in
		//float m_idle_rpm;// cars idle RPM
		//float m_max_gears;// maximum number of gears
		//float m_sessionType;// 0 = unknown, 1 = practice, 2 = qualifying, 3 = race 
		//float m_drsAllowed;// 0 = not allowed, 1 = allowed, -1 = invalid / unknown
		//float m_track_number;// -1 for unknown, 0-21 for tracks 
		//float m_vehicleFIAFlags;// -1 = invalid/unknown, 0 = none, 1 = green, 2 = blue, 3 = yellow, 4 = red
	};
}