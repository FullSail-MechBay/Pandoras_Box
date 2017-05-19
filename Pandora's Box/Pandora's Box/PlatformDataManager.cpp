#include "PlatformDataManager.h"
#include <intrin.h>
#include <cstring>
#include <functional>

using namespace DataManager;


#pragma region Static Members

uint8_t PlatformDataManager::DataStructSizes[DataMode_NumModes]{
sizeof(Data_Dof),
sizeof(Data_Length),
sizeof(Data_MotionCue),
sizeof(Data_PlayBack)
};

uint32_t PlatformDataManager::CurrPacketSequenceCount = 1;
uint32_t PlatformDataManager::HeaderSize = sizeof(PlatformDataManager::Header);
#pragma endregion

PlatformDataManager::PlatformDataManager()
{
	DataDirty = true;
	CurrMode = DataMode_DOF;
	DataByteSize = 0;
	DataBuffer = nullptr;
	DataSwapBuffer = nullptr;
	DataSFX01 = nullptr;
	DataSFX02 = nullptr;
	DataSFX03 = nullptr;
	DataSFX04 = nullptr;
	DataSFX05 = nullptr;
	DataSFX06 = nullptr;
	DataSFX07 = nullptr;
	DataSFX08 = nullptr;
	for (size_t DataMode = 0; DataMode < DataMode_NumModes; DataMode++)
		DataModeHeaders[DataMode] = nullptr;
	DataBuffer = new uint8_t[DM_DataBufferSize];
	DataSwapBuffer = new uint8_t[DM_DataBufferSize];
	Helper_SetUpDefualtData(CurrMode);
	SyncBufferChanges();
}
PlatformDataManager::~PlatformDataManager()
{
	delete[] DataBuffer;
	delete[] DataSwapBuffer;

	for (size_t mode = 0; mode < DataMode_NumModes; mode++)
		delete DataModeHeaders[mode];

	delete DataSFX01;
	delete DataSFX02;
	delete DataSFX03;
	delete DataSFX04;
	delete DataSFX05;
	delete DataSFX06;
	delete DataSFX07;
	delete DataSFX08;
}

void PlatformDataManager::Header::Initialize(DataModes _dataMode)
{
	PacketLength = DataStructSizes[_dataMode] - HeaderSize;
	PacketSequenceCount = 1;
	RESERVED = 0;
	MessageID = MotionMode[_dataMode];
}

PlatformDataManager::PostHeader::PostHeader()
{
	MotionCommandWord = 0;
	StatusResponseWord = 0;
}
void PlatformDataManager::PostHeader::Initialize(DataModes _dataMode)
{
	Header::Initialize(_dataMode);
	SetMotionCommandWord();
	SetStatusResponseWord();
	if (DataMode_Motion == _dataMode)
		SetUpdateRate(Freq_MotionCue);
}
void PlatformDataManager::PostHeader::SetStatusResponseWord(FeedBackReq _requestedFeedBack, uint8_t _fileIDNumber, uint16_t _updateRate)
{
	SetFeedBackReq(_requestedFeedBack);
	SetReplayFile(_fileIDNumber);
	SetUpdateRate(_updateRate);
}

#pragma region Data Structs

void PlatformDataManager::Data_Dof::Initialize(DataModes _dataMode)
{
	PostHeader::Initialize(_dataMode);

	Roll = 0;
	Pitch = 0;
	Yaw = 0;
	Surge = 0;
	Sway = 0;
	Heave = 0;

	SpecialEffects = SFX_NO_EFFECT;
}
void PlatformDataManager::Data_Length::Initialize(DataModes _dataMode)
{
	PostHeader::Initialize(_dataMode);

	ActuatorLength_A = 0;
	ActuatorLength_B = 0;
	ActuatorLength_C = 0;
	ActuatorLength_D = 0;
	ActuatorLength_E = 0;
	ActuatorLength_F = 0;
	SpecialEffects = SFX_NO_EFFECT;
}
void PlatformDataManager::Data_MotionCue::Initialize(DataModes _dataMode)
{
	PostHeader::Initialize(_dataMode);

	Angle_Roll = 0;
	Angle_Pitch = 0;
	Angle_Yaw = 0;
	Velocity_Roll = 0;
	Velocity_Pitch = 0;
	Velocity_Yaw = 0;
	Acceleration_Roll = 0;
	Acceleration_Pitch = 0;
	Acceleration_Yaw = 0;
	Acceleration_Surge = 0;
	Acceleration_Sway = 0;
	Acceleration_Heave = 0;
	SpecialEffects = SFX_NO_EFFECT;
}
void PlatformDataManager::Data_PlayBack::Initialize(DataModes _dataMode)
{
	PostHeader::Initialize(_dataMode);
}

#pragma endregion


#pragma region Special Effects : Constructors / Destructors

PlatformDataManager::SFXO_3_Direct_Displacement_Buffet::SFXO_3_Direct_Displacement_Buffet(uint32_t _totalNumSineWaves)
{
	TotalNumSineWaves = _totalNumSineWaves;
	SubData = new SineWaveData[_totalNumSineWaves];
}
PlatformDataManager::SFXO_3_Direct_Displacement_Buffet::~SFXO_3_Direct_Displacement_Buffet()
{
	delete[] SubData;
}

PlatformDataManager::SFXO_4_Displacement_White_Noise::SFXO_4_Displacement_White_Noise(uint32_t _totalNumSignals)
{
	TotalNumSignals = _totalNumSignals;
	SubData = new Amplitude_Data[_totalNumSignals];
}
PlatformDataManager::SFXO_4_Displacement_White_Noise::~SFXO_4_Displacement_White_Noise()
{
	delete[] SubData;
}

PlatformDataManager::SFXO_7_Acceleration_White_Noise::SFXO_7_Acceleration_White_Noise(uint32_t _totalNumSignals)
{
	TotalNumSignals = _totalNumSignals;
	SubData = new Accel_Data[_totalNumSignals];
}
PlatformDataManager::SFXO_7_Acceleration_White_Noise::~SFXO_7_Acceleration_White_Noise()
{
	delete[] SubData;
}

PlatformDataManager::SFXO_8_Acceleration_Buffets::SFXO_8_Acceleration_Buffets(uint32_t _totalNumSineWaves)
{
	TotalNumSineWaves = _totalNumSineWaves;
	SubData = new Accel_Data[_totalNumSineWaves];
}
PlatformDataManager::SFXO_8_Acceleration_Buffets::~SFXO_8_Acceleration_Buffets()
{
	delete[] SubData;
}

#pragma endregion

#pragma region Member Functions

void PlatformDataManager::IncrimentPacketSequence()
{
	CurrPacketSequenceCount++;
	SyncPacketSequence();
}
void PlatformDataManager::SetPacketSequence(uint32_t _value)
{
	CurrPacketSequenceCount = _value;
	SyncPacketSequence();
}
const uint8_t* PlatformDataManager::GetDataBufferAddress()
{
	if (DataDirty)
		SyncBufferChanges();
	return DataSwapBuffer;
}
void PlatformDataManager::SyncBufferChanges()
{
	if (!DataDirty)
		return;

	((Header*)DataBuffer)->PacketSequenceCount = CurrPacketSequenceCount;
	DataByteSize = ((Header*)DataBuffer)->PacketLength + HeaderSize;
	size_t numChunks = DataByteSize >> 2;
	for (size_t DataChunk = 0; DataChunk < numChunks; DataChunk++)
		((uint32_t*)DataSwapBuffer)[DataChunk] = _byteswap_ulong(((uint32_t*)DataBuffer)[DataChunk]);
	DataDirty = false;

}

void PlatformDataManager::SetCommandState(Command_State _commandState)
{
	((PostHeader*)DataBuffer)->SetMotionCommandWord(_commandState);

	DataDirty = true;
}
uint8_t PlatformDataManager::SetDataMode(DataModes _newMode)
{
	uint8_t returnCode = 2;
	if (_newMode == CurrMode)
		returnCode = 1;
	else
	{
		returnCode = DefaultOutBuffer(_newMode);
		((PostHeader*)DataBuffer)->SetMotionCommandWord(CommandState_DISENGAGE);
	}

	return returnCode;
}
uint8_t PlatformDataManager::SetDefaultData(DataModes _newMode)
{
	return DefaultOutBuffer(_newMode);
}
uint8_t PlatformDataManager::SetDofData(float _roll, float _pitch, float _yaw, float _surge, float _sway, float _heave)
{
	if (2 != SetDataMode(DataMode_DOF))
	{
		Data_Dof* dofAccess = (Data_Dof*)DataBuffer;
		dofAccess->Roll = _roll;
		dofAccess->Pitch = _pitch;
		dofAccess->Yaw = _yaw;
		dofAccess->Surge = _surge;
		dofAccess->Sway = _sway;
		dofAccess->Heave = _heave;

		DataDirty = true;
		return 0;
	}

	return 2;
}
uint8_t PlatformDataManager::SetLengthData(float _LenA, float _LenB, float _LenC, float _LenD, float _LenE, float _LenF)
{
	if (2 != SetDataMode(DataMode_Length))
	{
		Data_Length* LenAccess = (Data_Length*)DataBuffer;
		LenAccess->ActuatorLength_A = _LenA;
		LenAccess->ActuatorLength_B = _LenB;
		LenAccess->ActuatorLength_C = _LenC;
		LenAccess->ActuatorLength_D = _LenD;
		LenAccess->ActuatorLength_E = _LenE;
		LenAccess->ActuatorLength_F = _LenF;

		DataDirty = true;
		return 0;
	}

	return 2;
}
uint8_t PlatformDataManager::SetMotionCueData(
	float _angelRoll, float _angelPitch, float _angelYaw,
	float _velocityRoll, float _velocityPitch, float _velocityYaw,
	float _accelRoll, float _accelPitch, float _accelYaw, float _accelSurge, float _accelSway, float _accelHeave)
{
	if (2 != SetDataMode(DataMode_Motion))
	{
		Data_MotionCue* MCAccess = (Data_MotionCue*)DataBuffer;
		MCAccess->Angle_Roll = _angelRoll;
		MCAccess->Angle_Pitch = _angelPitch;
		MCAccess->Angle_Yaw = _angelYaw;

		MCAccess->Velocity_Roll = _velocityRoll;
		MCAccess->Velocity_Pitch = _velocityPitch;
		MCAccess->Velocity_Yaw = _velocityYaw;

		MCAccess->Acceleration_Roll = _accelRoll;
		MCAccess->Acceleration_Pitch = _accelPitch;
		MCAccess->Acceleration_Yaw = _accelYaw;
		MCAccess->Acceleration_Surge = _accelSurge;
		MCAccess->Acceleration_Sway = _accelSway;
		MCAccess->Acceleration_Heave = _accelHeave;

		DataDirty = true;
		return 0;
	}

	return 2;
}

#pragma region Private Helper Functions

void PlatformDataManager::Helper_SetUpDefualtData(DataModes _dataMode)
{
	CurrMode = _dataMode;

	if (nullptr == DataModeHeaders[CurrMode])
		switch (CurrMode)
		{
		case DataManager::DataMode_DOF:
			DataModeHeaders[CurrMode] = new Data_Dof();
			((Data_Dof*)DataModeHeaders[CurrMode])->Initialize(CurrMode);
			break;
		case DataManager::DataMode_Length:
			DataModeHeaders[CurrMode] = new Data_Length();
			((Data_Length*)DataModeHeaders[CurrMode])->Initialize(CurrMode);
			break;
		case DataManager::DataMode_Motion:
			DataModeHeaders[CurrMode] = new Data_MotionCue();
			((Data_MotionCue*)DataModeHeaders[CurrMode])->Initialize(CurrMode);
			break;
		case DataManager::DataMode_PlayBack:
			DataModeHeaders[CurrMode] = new Data_PlayBack();
			((Data_PlayBack*)DataModeHeaders[CurrMode])->Initialize(CurrMode);
			break;
		default:
			break;
		}

	std::memcpy(DataBuffer, DataModeHeaders[CurrMode], DataStructSizes[CurrMode]);
	DataDirty = true;
}

void PlatformDataManager::SyncPacketSequence()
{
	((Header*)DataSwapBuffer)->PacketSequenceCount = _byteswap_ulong(((Header*)DataBuffer)->PacketSequenceCount = CurrPacketSequenceCount);
}

uint8_t PlatformDataManager::DefaultOutBuffer(DataModes _dataMode)
{
	Helper_SetUpDefualtData(_dataMode);
	return (_dataMode == CurrMode) ? 0 : 2;
}

#pragma endregion

#pragma endregion