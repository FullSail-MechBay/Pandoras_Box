#pragma once
#include <stdint.h>

namespace DataManager
{
#define DM_DataBufferSize 256

#pragma region Enums

	enum DataModes
	{
		DataMode_DOF, DataMode_Length, DataMode_Motion, DataMode_PlayBack, DataMode_NumModes
	};

	enum Command_State
	{
		CommandState_NO_CHANGE,		// No Command Update (Normal Operating Mode)
		CommandState_ENGAGE,		// Makes Platform ready to run. * Valid Only in READY state
		CommandState_DISENGAGE,		// Causes the Motion Base to return to the SETTLED position under power.  Power is then removed from the motors. * Valid only in STANDBY & ENGAGED
		CommandState_RESET,			// Used to recover from any FAULT state. This command also restores normal operation after the INHIBIT command is received. * Valid only in STANDBY, ENGAGED & READY
		CommandState_START,			// Used only in Playback Mode.  Initiates start of motion after reaching ENGAGED state. * Valid only in ENGAGED state
		CommandState_UP,			// Used only with the “Draw Bridge Two” option. When received causes the drawbridge (access way) to go up. * Valid only in READY state
		CommandState_DOWN,			// Used only with the “Draw Bridge Two” option. When received causes the drawbridge (access way) to go down. * Valid only in READY state
		CommandState_STOP			// Used only with the “Draw Bridge Two” option. When received causes the drawbridge (access way) to disable. * Valid only in READY state
	};

	// Requested Feedback Data: Bit mask that specifies the data that is requested to be paced by the MBC into the status response message.
	enum FeedBackReq
	{
		FBR_Length = 0x80,
		FBR_DOF = 0x40,
		FBR_Accelerometer = 0x20,
		FBR_ActuatorData = 0x10,
		FBR_AlarmList = 0x08
	};

	enum SpecialEffectsCommand
	{
		SFX_NO_EFFECT,						// Indicates that we have finished reading in all the command data
		SFX_DIRECT_DISPLACEMENT_DOF,		// (Applicable only with Message ID 100 and 102)
		SFX_DIRECT_DISPLACEMENT_LENGTH,		// 
		SFX_DISPLACEMENT_BUFFET,			// (Not applicable with option 8).(Applicable only with Message ID 100 and 102)
		SFX_DISPLACEMENT_WHITE_NOISE,		// Note: This Option results in displacement-based white noise, which is not recommended for use. Legacy support only. (Applicable only with Message ID 100 and 102)
		SFX_CENTER_OF_GRAVITY,				// (Applicable only with Message ID 102)
		SFX_ON_GROUND,						// (Applicable only with Message ID 102)
		SFX_ACCELERATION_WHITE_NOISE,		// (Applicable only with Message ID 100 and 102).(Applicable only with Message ID 100 and 102)
		SFX_ACCELERATION_BUFFET				// (Not applicable with option 3).(Applicable only with Message ID 100 and 102)
	};

	enum FeedBackFreq
	{
		Freq_Dof = 500,
		Freq_Length = 2000,
		Freq_MotionCue = 60,
		Freq_PlayBack = 0
	};

#pragma endregion

	static uint32_t MotionMode[DataMode_NumModes]
	{
		100, 101, 102, 103
	};

	class PlatformDataManager
	{

		typedef void(PlatformDataManager::* MemberFuncPointer)(void);

#pragma region DataStructs

#pragma region Base Classes

		struct Header
		{
			uint32_t PacketLength;			// Length does not incude size of Header
			uint32_t PacketSequenceCount;
			void SetMotionMode(DataModes _dataMode) { MessageID = MotionMode[_dataMode]; };
			void Initialize(DataModes _dataMode);

		private:
			uint32_t RESERVED;
			uint32_t MessageID;				// MotionMode
		};

		struct  PostHeader : Header
		{
			PostHeader();

			void Initialize(DataModes _dataMode);

			void SetMotionCommandWord(Command_State _newCommandState = CommandState_NO_CHANGE) { MotionCommandWord = _newCommandState; };
			void SetStatusResponseWord(FeedBackReq _requestedFeedBack = FBR_AlarmList, uint8_t _fileIDNumber = 0, uint16_t _updateRate = Freq_Dof);
			inline void SetFeedBackReq(FeedBackReq _requestedFeedback = FBR_AlarmList) { StatusResponseWord = (StatusResponseWord & 0xFFFFFF00) | _requestedFeedback; };
			inline void SetReplayFile(uint8_t _fileIDNumber = 0) { StatusResponseWord = (StatusResponseWord & 0xFFFF00FF) | (_fileIDNumber << 8); };
			inline void SetUpdateRate(uint16_t _updateRate = Freq_Dof) { StatusResponseWord = (StatusResponseWord & 0x0000FFFF) | (_updateRate << 16); };

		private:
			uint32_t MotionCommandWord;	// Command_State
			uint32_t StatusResponseWord;
		};

#pragma endregion

#pragma region Motion Structs

		struct Data_Dof : public PostHeader
		{
			float Roll;		// Radians
			float Pitch;	// Radians
			float Yaw;		// Radians
			float Surge;	// Meters
			float Sway;		// Meters
			float Heave;	// Meters
			uint32_t SpecialEffects;

			void Initialize(DataModes _dataMode);
		};

		struct Data_Length : public PostHeader
		{
			float ActuatorLength_A;		// Meters
			float ActuatorLength_B;		// Meters
			float ActuatorLength_C;		// Meters
			float ActuatorLength_D;		// Meters
			float ActuatorLength_E;		// Meters
			float ActuatorLength_F;		// Meters
			uint32_t SpecialEffects;

			void Initialize(DataModes _dataMode);
		};

		struct Data_MotionCue : public PostHeader
		{
			uint32_t MotionCueingCommandWord = 0;	// if least significant bit is equal to 1 : Freeze: When the MBC receives a Freeze command, motion is faded out over a 5 second period. For more info: see note below class definition.
			float Angle_Roll;			// Radians
			float Angle_Pitch;			// Radians
			float Angle_Yaw;			// Radians
			float Velocity_Roll;		// Rad/Sec
			float Velocity_Pitch;		// Rad/Sec
			float Velocity_Yaw;			// Rad/Sec
			float Acceleration_Roll;	// Rad/Sec^2
			float Acceleration_Pitch;	// Rad/Sec^2
			float Acceleration_Yaw;		// Rad/Sec^2
			float Acceleration_Surge;	// m/Sec^2
			float Acceleration_Sway;	// m/Sec^2
			float Acceleration_Heave;	// m/Sec^2
			uint32_t SpecialEffects;

			void Initialize(DataModes _dataMode);
		};
		/* Note: Freeze Command:
			When the MBC receives a Freeze command, motion is faded out over a 5 second period.
				During this 5 second period, motion commands are increasingly damped.
				Once this time period has expired the MBC no longer responds to motion commands, and is settled at the neutral position.
				At this point the motion system is considered frozen and the frozen status is reported to the HOST.
				When the freeze command is no longer active the MBC gradually begins to accept motion commands over the same 5 second period. */

		struct Data_PlayBack : public PostHeader
		{
			// PlayBack mode has no difference from PostHeader at the moment.
			void Initialize(DataModes _dataMode);
		};

#pragma endregion

#pragma region Special Effects Structs // SFXO_#: Special Effects Option number

		struct SFXO_1_Direct_Displacement_DOF
		{
			float Roll = 0;				// Radians
			float Pitch = 0;			// Radians
			float Yaw = 0;				// Radians
			float Longitudinal = 0;		// Meters
			float Lateral = 0;			// Meters
			float Heave = 0;			// Meters
			uint32_t SpecialEffects = SFX_NO_EFFECT;
		};

		struct SFXO_2_Direct_Displacement_Length
		{
			float Leg_A = 0;	// Meters
			float Leg_B = 0;	// Meters
			float Leg_C = 0;	// Meters
			float Leg_D = 0;	// Meters
			float Leg_E = 0;	// Meters
			float Leg_F = 0;	// Meters
			uint32_t SpecialEffects = SFX_NO_EFFECT;
		};

		// (Cannot be used simultaneously with Special Effects Option 8) 
		struct SFXO_3_Direct_Displacement_Buffet
		{
			struct SineWaveData
			{
				float Frequency = 0;				// Hz
				float Amplitude_X = 0;				// Meters
				float Amplitude_Y = 0;				// Meters
				float Amplitude_Z = 0;				// Meters
			};
			uint32_t TotalNumSineWaves = 0;		// Value range : 0 - 10
			SineWaveData* SubData = nullptr;
			uint32_t SpecialEffects = SFX_NO_EFFECT;
			SFXO_3_Direct_Displacement_Buffet(uint32_t _totalNumSineWaves);
			~SFXO_3_Direct_Displacement_Buffet();
		};

		// NOTE: USE OF OPTION 4 IS NOT RECOMMENDED. IT IS AVAILABLE FOR LEGACY SYSTEMS. OPTION 7 SHOULD BE USED INSTEAD. (Cannot be used simultaneously with Special Effects Option 7)
		struct SFXO_4_Displacement_White_Noise
		{
			struct Amplitude_Data
			{
				float Amplitude_X_3DOF_Pitch = 0;	// X: (m^2/Hz)  Pitch: (R^2/Hz)
				float Amplitude_Y_3DOF_Roll = 0;	// Y: (m^2/Hz)  Roll: (m^2/Hz)
				float Amplitude_Z = 0;				// Z: (m^2/Hz)
			};
			uint32_t TotalNumSignals = 0;		// Value range : 0 - 2
			Amplitude_Data* SubData = nullptr;
			uint32_t SpecialEffects = SFX_NO_EFFECT;
			SFXO_4_Displacement_White_Noise(uint32_t _totalNumSignals);
			~SFXO_4_Displacement_White_Noise();
		};

		// COG : Center of Gravity.  NOTE: Parameters carried in this Option specify the offset from the vehicle CG to the vehicle reference position.
		struct SFXO_5_COG
		{
			float COG_X = 0;		// Meters
			float COG_Y = 0;		// Meters
			float COG_Z = 0;		// Meters
			uint32_t SpecialEffects = SFX_NO_EFFECT;
		};

		// NOTE: This Option is applicable only when APK Motion Cueing is used.
		struct SFXO_6_On_Ground
		{
			uint32_t OnGroundFlag = 0;	// 1 = True, 0 = False
			float Velocity = 0;			// m/s
			uint32_t SpecialEffects = SFX_NO_EFFECT;
		};

		// (Cannot be used simultaneously with Special Effects Option 4)
		struct SFXO_7_Acceleration_White_Noise
		{
			struct Accel_Data
			{
				float Acceleration_X_3DOF_Pitch = 0;	// X: (m/s^2)^2/Hz  Pitch: (R/s^2)^2/Hz
				float LowPass_CutOff_Freq_X = 0;			// Hz
				float HighPass_CutOff_Freq_X = 0;			// Hz
				float Acceleration_Y_3DOF_Roll = 0;		// Y: (m/s^2)^2/Hz  Roll: (R/s^2)^2/Hz
				float LowPass_CutOff_Freq_Y = 0;			// Hz
				float HighPass_CutOff_Freq_Y = 0;			// Hz
				float Acceleration_Z = 0;				// Z: (m/s^2)^2/Hz
				float LowPass_CutOff_Freq_Z = 0;			// Hz
				float HighPass_CutOff_Freq_Z = 0;			// Hz
			};
			uint32_t TotalNumSignals = 0;			// Value range : 0 - 2
			Accel_Data* SubData = nullptr;
			uint32_t SpecialEffects = SFX_NO_EFFECT;

			SFXO_7_Acceleration_White_Noise(uint32_t _totalNumSignals);
			~SFXO_7_Acceleration_White_Noise();
		};

		// (Cannot be used simultaneously with Special Effects Option 3)
		struct SFXO_8_Acceleration_Buffets
		{
			struct Accel_Data
			{
				float Frequency = 0;				// Hz
				float Acceleration_X = 0;			// m/s^2
				float Acceleration_Y = 0;			// m/s^2
				float Acceleration_Z = 0;			// m/s^2
			};

			uint32_t TotalNumSineWaves = 0;		// Value range : 0 - 10
			Accel_Data* SubData = nullptr;
			uint32_t SpecialEffects = SFX_NO_EFFECT;

			SFXO_8_Acceleration_Buffets(uint32_t _totalNumSineWaves);
			~SFXO_8_Acceleration_Buffets();
		};

#pragma endregion

#pragma endregion

#pragma region Private DataMembers

		static uint8_t DataStructSizes[DataMode_NumModes];
		static uint32_t CurrPacketSequenceCount;
		static uint32_t HeaderSize;
		bool DataDirty;

		DataModes		CurrMode;
		uint32_t		DataByteSize;
		uint8_t*		DataBuffer;
		uint8_t*		DataSwapBuffer;
		PostHeader*		DataModeHeaders[DataMode_NumModes];

		SFXO_1_Direct_Displacement_DOF*		DataSFX01;
		SFXO_2_Direct_Displacement_Length*	DataSFX02;
		SFXO_3_Direct_Displacement_Buffet*	DataSFX03;
		SFXO_4_Displacement_White_Noise*	DataSFX04;
		SFXO_5_COG*							DataSFX05;
		SFXO_6_On_Ground*					DataSFX06;
		SFXO_7_Acceleration_White_Noise*	DataSFX07;
		SFXO_8_Acceleration_Buffets*		DataSFX08;

#pragma endregion

#pragma region Private Member Functions

		void Helper_SetUpDefualtData(DataModes _dataMode);
		void SyncPacketSequence();
		uint8_t DefaultOutBuffer(DataModes _dataMode);

#pragma endregion


	public:
		PlatformDataManager();
		~PlatformDataManager();

		// Packet Byte size
		// !! Potentially Not thread safe while SyncBufferChange() is being called !!
		const uint32_t GetDataSize() { return DataByteSize; };
		const uint32_t GetPacketSequence() { return CurrPacketSequenceCount; };

		// !! Not thread safe !!
		// Does not require syncing the buffer afterwards
		void IncrimentPacketSequence();
		// !! Not thread safe !!
		// Does not require syncing the buffer afterwards
		void SetPacketSequence(uint32_t _value);
		// !! Not thread safe if changes were made since last call or last Sync !!
		// This pointer should not change during the life of this object
		const uint8_t* GetDataBufferAddress();
		// !! Not thread safe !!
		// Swaps changes made to data packet into the buffer for data transfer
		void SyncBufferChanges();

		void SetCommandState(Command_State _commandState);
		// IF _newMode != current mode it sets the buffer data to the default values for passed in data mode
		// Return Value: 0: Success, 1: Same Mode(No Change), 2: Failed
		uint8_t SetDataMode(DataModes _newMode);
		// Sets the buffer data to the default values for passed in data mode
		// Return Value: 0: Success, 1: Same Mode(No Change), 2: Failed
		uint8_t SetDefaultData(DataModes _newMode);

		// Return Value: 0: Success, 1: Same Mode(No Change), 2: Failed
		// Requires SyncBufferChanges() to be called in order for changes to take effect.
		uint8_t SetDofData(float _roll, float _pitch, float _yaw, float _surge, float _sway, float _heave);
		// Return Value: 0: Success, 1: Same Mode(No Change), 2: Failed
		// Requires SyncBufferChanges() to be called in order for changes to take effect.
		uint8_t SetLengthData(float _LenA, float _LenB, float _LenC, float _LenD, float _LenE, float _LenF);
		// Return Value: 0: Success, 1: Same Mode(No Change), 2: Failed
		// Requires SyncBufferChanges() to be called in order for changes to take effect.
		uint8_t SetMotionCueData(
			float _angelRoll, float _angelPitch, float _angelYaw,
			float _velocityRoll, float _velocityPitch, float _velocityYaw,
			float _accelRoll, float _accelPitch, float _accelYaw, float _accelSurge, float _accelSway, float _accelHeave);


		inline int GetCurrentDataRate()
		{
			switch (CurrMode)
			{
			case DataManager::DataMode_DOF:
				return Freq_Dof;
			case DataManager::DataMode_Length:
				return Freq_Length;
			case DataManager::DataMode_Motion:
				return Freq_MotionCue;
			case DataManager::DataMode_PlayBack:
				return Freq_PlayBack;
			default:
				return 0;
			}
		}

		// TODO: Add functions to add, set, & remove special effects

	};
}