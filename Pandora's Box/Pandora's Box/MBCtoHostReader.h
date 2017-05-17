#pragma once
#include <stdint.h>

namespace DataManager
{
	enum Optional_Status_Data
	{
		OSDW_None,		// needs to be == 0
		OSDW_DOF,
		OSDW_Length,
		OSDW_Data,
		OSDW_Accelerometer,
		OSDW_NumOptions = OSDW_Accelerometer
	};

	struct MBC_Header
	{
		uint32_t PacketLength = 0;		// Not including this header
		uint32_t PacketSeqCount = 0;	
		uint32_t Reserved = 0;			// Unused
		uint32_t MessageID = 0;			// Should be 200
	};

	enum MachineState
	{
		MS_Initializing = 0,	// Motion control software is initializing is operational parameters and internal states.
		MS_Ready = 1,			// System is ready to accept an Engage command.
		MS_StandBy = 2,			// System is Engaging.
		MS_Engaged = 3,			// System is Engaged and can accept commanded set-points.
		MS_Parking = 4,			// System is Disengaging.
		MS_Fault_1 = 8,			// Class 1 fault has occurred and the system is waiting for it to clear.
		MS_Fault_2 = 9,			// Class 2 fault has occurred and the system requires a Reset to clear its fault condition.
		MS_Fault_2_Hold = 10,	// Class 2 fault has occurred and the system is waiting for it to clear.
		MS_Disabled = 11,		// Unused.
		MS_Inhibited = 12,		// Not used
		MS_Frozen = 13			// System is Engaged, but motion is currently frozen.
	};

	enum MotionCommandMode
	{
		MCM_Local,			
		MCM_DOF,			
		MCM_Length,			
		MCM_MotionCue		
	};


	struct StatusResponse : public MBC_Header
	{
		uint32_t MachineStatus = 0;					// Use struct memeber functions to pull useful data from MachineStatus
		uint32_t Discrete_IO = 0;					// TODO: Set up functions to read bit flags.
		uint32_t Latched_Fault_Data_1 = 0;			// TODO: Set up functions to read bit flags.
		uint32_t Latched_Fault_Data_2 = 0;			// TODO: Set up functions to read bit flags.
		uint32_t Latched_Fault_Data_3 = 0;			// TODO: Set up functions to read bit flags.
		uint32_t OptionalStatusData = OSDW_None;	// Optional_Status_Data

		MachineState GetMachineState() const;
		MotionCommandMode GetMotionCommandMode();
		uint8_t GetMDATuningFileNum();
		uint16_t GetUpdateRate();
	};

	// OSD_# : Optional Status Data Number
	struct OSD_1_DOF
	{
		uint32_t Roll = 0;							// Radians
		uint32_t Pitch = 0;							// Radians
		uint32_t Yaw = 0;							// Radians
		uint32_t Longitudinal_X = 0;				// Meters
		uint32_t Lateral_Y = 0;						// Meters
		uint32_t Heave_Z = 0;						// Meters
		uint32_t OptionalStatusData = OSDW_None;	// Optional_Status_Data
	};

	// OSD_# : Optional Status Data Number
	struct OSD_2_Length
	{
		uint32_t Actuator_A = 0;					// Meters
		uint32_t Actuator_B = 0;					// Meters
		uint32_t Actuator_C = 0;					// Meters
		uint32_t Actuator_D = 0;					// Meters
		uint32_t Actuator_E = 0;					// Meters
		uint32_t Actuator_F = 0;					// Meters
		uint32_t OptionalStatusData = OSDW_None;	// Optional_Status_Data
	};

	struct FeedBackData
	{
		uint32_t Actuator_A = 0;
		uint32_t Actuator_B = 0;
		uint32_t Actuator_C = 0;
		uint32_t Actuator_D = 0;
		uint32_t Actuator_E = 0;
		uint32_t Actuator_F = 0;
	};

	enum DataInfo
	{
		Data_Current,			// (amps)
		Data_Velocity,			// (m/s)
		Data_Following_Error,	// (meters)
		Data_NOT_USED_3,
		Data_NOT_USED_4,
		Data_NOT_USED_5,
		Data_NOT_USED_6,
		Data_NOT_USED_7,
		Data_NOT_USED_8,
		Data_NOT_USED_9
	};

	// OSD_# : Optional Status Data Number
	struct OSD_3_Actuator
	{
		uint32_t NumMessages = 10;		// Value Range = 10... thats correct, just 10, currently no variance
		FeedBackData DataArray[10];		// Index using enum DataManager::DataInfo or Member Function: GetFeedBackDataInfo(DataInfo _dataInfoType)
		uint32_t OptionalStatusData = OSDW_None;		// Optional_Status_Data

		FeedBackData* GetFeedBackDataInfo(DataInfo _dataInfoType) { return &DataArray[_dataInfoType]; };
	};

	struct OSD_4_Accelerometer_Data
	{
		uint32_t X = 0;		// Meters
		uint32_t Y = 0;		// Meters
		uint32_t Z = 0;		// Meters
		uint32_t OptionalStatusData = OSDW_None;		// Optional_Status_Data
	};

	class MBCtoHostReader
	{
		uint8_t* Buffer;
		void* BufferLocations[OSDW_NumOptions];

		void Healper_GetNextWord(const uint32_t* _word);
		const uint32_t* Healper_CheckForStruct(Optional_Status_Data _OSDW);

	public:
		MBCtoHostReader();
		~MBCtoHostReader();

		static uint32_t ReaderDataBufferSize;

		uint8_t* GetBufferAdder() { return Buffer; };
		// Returns nullptr if header MessageID != 200
		const MBC_Header* GetHeaderInfo();
		// Returns nullptr if header MessageID != 200
		const StatusResponse* GetStatusResponse();

		// Returns nullptr if no OSD_1_DOF was found in the buffer
		const OSD_1_DOF* GetOSD_1();
		// Returns nullptr if no OSD_2_Length was found in the buffer
		const OSD_2_Length* GetOSD_2();
		// Returns nullptr if no OSD_3_Actuator was found in the buffer
		const OSD_3_Actuator* GetOSD_3();
		// Returns nullptr if no OSD_4_Accelerometer_Data was found in the buffer
		const OSD_4_Accelerometer_Data* GetOSD_4();
	};

}