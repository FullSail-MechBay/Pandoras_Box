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
	DataOutStep_ms = 0.0f;
	DataDirty = true;
	FirstPacket = true;
	CurrMode = DataMode_DOF;
	DataByteSize = 0;
	DataBuffer = nullptr;
	DataSwapBuffer = nullptr;
	DataBufferDiff = nullptr;
	NewestPacket = nullptr;
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
	DataBufferDiff = new uint8_t[DM_DataBufferSize];
	NewestPacket = new uint8_t[DM_DataBufferSize];
	Helper_SetUpDefualtData(CurrMode);
	SyncBufferChanges();
}
PlatformDataManager::~PlatformDataManager()
{
	delete[] DataBuffer;
	delete[] DataSwapBuffer;
	delete[] DataBufferDiff;
	delete[] NewestPacket;

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
	if (DataMode_MotionCue == _dataMode)
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
	Heave = -0.15f;

	SpecialEffects = SFX_NO_EFFECT;
}
void PlatformDataManager::Data_Length::Initialize(DataModes _dataMode)
{
	PostHeader::Initialize(_dataMode);

	ActuatorLength_A = 0.2f;
	ActuatorLength_B = 0.2f;
	ActuatorLength_C = 0.2f;
	ActuatorLength_D = 0.2f;
	ActuatorLength_E = 0.2f;
	ActuatorLength_F = 0.2f;
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
	TotalNumSineWaves = _totalNumSineWaves < 0 ? 0 : _totalNumSineWaves > 10 ? 10 : _totalNumSineWaves; // keeps total sine waves within proper range.
	SubData = new SineWaveData[_totalNumSineWaves];
}
PlatformDataManager::SFXO_3_Direct_Displacement_Buffet::~SFXO_3_Direct_Displacement_Buffet()
{
	delete[] SubData;
}

PlatformDataManager::SFXO_4_Displacement_White_Noise::SFXO_4_Displacement_White_Noise(uint32_t _totalNumSignals)
{
	TotalNumSignals = _totalNumSignals < 0 ? 0 : _totalNumSignals > 2 ? 2 : _totalNumSignals; // keeps total signals within proper range.
	SubData = new Amplitude_Data[_totalNumSignals];
}
PlatformDataManager::SFXO_4_Displacement_White_Noise::~SFXO_4_Displacement_White_Noise()
{
	delete[] SubData;
}

PlatformDataManager::SFXO_7_Acceleration_White_Noise::SFXO_7_Acceleration_White_Noise(uint32_t _totalNumSignals)
{
	TotalNumSignals = _totalNumSignals < 0 ? 0 : _totalNumSignals > 2 ? 2 : _totalNumSignals; // keeps total signals within proper range.
	SubData = new Accel_Data[_totalNumSignals];
}
PlatformDataManager::SFXO_7_Acceleration_White_Noise::~SFXO_7_Acceleration_White_Noise()
{
	delete[] SubData;
}

PlatformDataManager::SFXO_8_Acceleration_Buffets::SFXO_8_Acceleration_Buffets(uint32_t _totalNumSineWaves)
{
	TotalNumSineWaves = _totalNumSineWaves < 0 ? 0 : _totalNumSineWaves > 10 ? 10 : _totalNumSineWaves; // keeps total sine waves within proper range.
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

void PlatformDataManager::IncDataFrame()
{
	if (-1 >= FrameSlicesRemaining)
		return;
	FrameSlicesRemaining--;
	switch (CurrMode)
	{
	case DataManager::DataMode_DOF:
	{
		Data_Dof* currBuff = (Data_Dof*)DataBuffer;
		if (-1 == FrameSlicesRemaining)
		{
			Data_Dof* latestPacket = (Data_Dof*)NewestPacket;

			currBuff->Roll = latestPacket->Roll;
			currBuff->Pitch = latestPacket->Pitch;
			currBuff->Yaw = latestPacket->Yaw;
			currBuff->Surge = latestPacket->Surge;
			currBuff->Sway = latestPacket->Sway;
			currBuff->Heave = latestPacket->Heave;
		}
		else
		{
			Data_Dof* IncBuff = (Data_Dof*)DataBufferDiff;

			currBuff->Roll += IncBuff->Roll;
			currBuff->Pitch += IncBuff->Pitch;
			currBuff->Yaw += IncBuff->Yaw;
			currBuff->Surge += IncBuff->Surge;
			currBuff->Sway += IncBuff->Sway;
			currBuff->Heave += IncBuff->Heave;
		}
		break;
	}
	case DataManager::DataMode_Length:
	{
		Data_Length* currBuff = (Data_Length*)DataBuffer;
		if (-1 == FrameSlicesRemaining)
		{
			Data_Length* latestPacket = (Data_Length*)NewestPacket;

			currBuff->ActuatorLength_A = latestPacket->ActuatorLength_A;
			currBuff->ActuatorLength_B = latestPacket->ActuatorLength_B;
			currBuff->ActuatorLength_C = latestPacket->ActuatorLength_C;
			currBuff->ActuatorLength_D = latestPacket->ActuatorLength_D;
			currBuff->ActuatorLength_E = latestPacket->ActuatorLength_E;
			currBuff->ActuatorLength_F = latestPacket->ActuatorLength_F;
		}
		else
		{
			Data_Length* IncBuff = (Data_Length*)DataBufferDiff;

			currBuff->ActuatorLength_A += IncBuff->ActuatorLength_A;
			currBuff->ActuatorLength_B += IncBuff->ActuatorLength_B;
			currBuff->ActuatorLength_C += IncBuff->ActuatorLength_C;
			currBuff->ActuatorLength_D += IncBuff->ActuatorLength_D;
			currBuff->ActuatorLength_E += IncBuff->ActuatorLength_E;
			currBuff->ActuatorLength_F += IncBuff->ActuatorLength_F;
		}
		break;
	}
	case DataManager::DataMode_MotionCue:
	{
		/*Data_MotionCue* currBuff = (Data_MotionCue*)DataBuffer;
		Data_MotionCue* IncBuff = (Data_MotionCue*)DataBufferDiff;

		currBuff->Angle_Roll += IncBuff->Angle_Roll;
		currBuff->Angle_Pitch += IncBuff->Angle_Pitch;
		currBuff->Angle_Yaw += IncBuff->Angle_Yaw;

		currBuff->Velocity_Roll += IncBuff->Velocity_Roll;
		currBuff->Velocity_Pitch += IncBuff->Velocity_Pitch;
		currBuff->Velocity_Yaw += IncBuff->Velocity_Yaw;

		currBuff->Acceleration_Roll += IncBuff->Acceleration_Roll;
		currBuff->Acceleration_Pitch += IncBuff->Acceleration_Pitch;
		currBuff->Acceleration_Yaw += IncBuff->Acceleration_Yaw;
		currBuff->Acceleration_Surge += IncBuff->Acceleration_Surge;
		currBuff->Acceleration_Sway += IncBuff->Acceleration_Sway;
		currBuff->Acceleration_Heave += IncBuff->Acceleration_Heave;*/
		break;
	}
	case DataManager::DataMode_PlayBack:
		break;
	case DataManager::DataMode_NumModes:
		break;
	default:
		break;
	}

	DataDirty = true;
}

void PlatformDataManager::SetCommandState(Command_State _commandState)
{
	((PostHeader*)DataBuffer)->SetMotionCommandWord(_commandState);

	DataDirty = true;
}
uint8_t PlatformDataManager::SetDataMode(DataModes _newMode)
{
	uint8_t returnCode = Failed;
	if (_newMode == CurrMode)
		returnCode = NoChange;
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
uint8_t PlatformDataManager::SetDofData(float _roll, float _pitch, float _yaw, float _surge, float _sway, float _heave, float _deltaT)
{
	if (Failed != SetDataMode(DataMode_DOF))
	{
		Data_Dof* dofAccess = (Data_Dof*)DataBuffer;

		if (Helper_CalculateFrameDiff(_deltaT))
		{
			Data_Dof* newest = (Data_Dof*)NewestPacket;
			newest->Roll = _roll;
			newest->Pitch = _pitch;
			newest->Yaw = _yaw;
			newest->Surge = _surge;
			newest->Sway = _sway;
			newest->Heave = _heave;

			Data_Dof* DiffBuffer = (Data_Dof*)DataBufferDiff;
			DiffBuffer->Roll = (newest->Roll - dofAccess->Roll) / FramesDiff;
			DiffBuffer->Pitch = (newest->Pitch - dofAccess->Pitch) / FramesDiff;
			DiffBuffer->Yaw = (newest->Yaw - dofAccess->Yaw) / FramesDiff;
			DiffBuffer->Surge = (newest->Surge - dofAccess->Surge) / FramesDiff;
			DiffBuffer->Sway = (newest->Sway - dofAccess->Sway) / FramesDiff;
			DiffBuffer->Heave = (newest->Heave - dofAccess->Heave) / FramesDiff;
		}
		else
		{
			dofAccess->Roll = _roll;
			dofAccess->Pitch = _pitch;
			dofAccess->Yaw = _yaw;
			dofAccess->Surge = _surge;
			dofAccess->Sway = _sway;
			dofAccess->Heave = _heave;
		}


		DataDirty = true;
		return Success;
	}

	return Failed;
}
uint8_t PlatformDataManager::SetLengthData(float _LenA, float _LenB, float _LenC, float _LenD, float _LenE, float _LenF, float _deltaT)
{
	if (Failed != SetDataMode(DataMode_Length))
	{
		Data_Length* LenAccess = (Data_Length*)DataBuffer;
		if (Helper_CalculateFrameDiff(_deltaT))
		{
			Data_Length* newest = (Data_Length*)NewestPacket;

			newest->ActuatorLength_A = _LenA;
			newest->ActuatorLength_B = _LenB;
			newest->ActuatorLength_C = _LenC;
			newest->ActuatorLength_D = _LenD;
			newest->ActuatorLength_E = _LenE;
			newest->ActuatorLength_F = _LenF;

			Data_Length* DiffBuffer = (Data_Length*)DataBufferDiff;
			DiffBuffer->ActuatorLength_A = (newest->ActuatorLength_A - LenAccess->ActuatorLength_A) / FramesDiff;
			DiffBuffer->ActuatorLength_B = (newest->ActuatorLength_B - LenAccess->ActuatorLength_B) / FramesDiff;
			DiffBuffer->ActuatorLength_C = (newest->ActuatorLength_C - LenAccess->ActuatorLength_C) / FramesDiff;
			DiffBuffer->ActuatorLength_D = (newest->ActuatorLength_D - LenAccess->ActuatorLength_D) / FramesDiff;
			DiffBuffer->ActuatorLength_E = (newest->ActuatorLength_E - LenAccess->ActuatorLength_E) / FramesDiff;
			DiffBuffer->ActuatorLength_F = (newest->ActuatorLength_F - LenAccess->ActuatorLength_F) / FramesDiff;
		}
		else
		{
			LenAccess->ActuatorLength_A = _LenA;
			LenAccess->ActuatorLength_B = _LenB;
			LenAccess->ActuatorLength_C = _LenC;
			LenAccess->ActuatorLength_D = _LenD;
			LenAccess->ActuatorLength_E = _LenE;
			LenAccess->ActuatorLength_F = _LenF;
		}

		DataDirty = true;
		return Success;
	}

	return Failed;
}
uint8_t PlatformDataManager::SetMotionCueData(
	float _angelRoll, float _angelPitch, float _angelYaw,
	float _velocityRoll, float _velocityPitch, float _velocityYaw,
	float _accelRoll, float _accelPitch, float _accelYaw, float _accelSurge, float _accelSway, float _accelHeave,
	float _multiplier)
{
	if (Failed != SetDataMode(DataMode_MotionCue))
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
		return Success;
	}

	return Failed;
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
		case DataManager::DataMode_MotionCue:
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
	std::memset(DataBufferDiff, 0, DM_DataBufferSize);
	std::memset(NewestPacket, 0, DM_DataBufferSize);
	DataDirty = true;
}

void PlatformDataManager::SyncPacketSequence()
{
	((Header*)DataSwapBuffer)->PacketSequenceCount = _byteswap_ulong(((Header*)DataBuffer)->PacketSequenceCount = CurrPacketSequenceCount);
}

uint8_t PlatformDataManager::DefaultOutBuffer(DataModes _dataMode)
{
	Helper_SetUpDefualtData(_dataMode);
	return (_dataMode == CurrMode) ? Success : Failed;
}

bool PlatformDataManager::Helper_CalculateFrameDiff(float _deltaT)
{
	if (_deltaT <= DataOutStep_ms || (DataOutStep_ms < FLT_EPSILON && DataOutStep_ms > -FLT_EPSILON))
	{
		FrameSlicesRemaining = -1;
		FramesDiff = 1.0f;
		return false;
	}

	FramesDiff = _deltaT / (DataOutStep_ms == 0 ? FLT_EPSILON : DataOutStep_ms);
	FrameSlicesRemaining = (int)FramesDiff;
	if (FirstPacket)
	{
		FrameSlicesRemaining = -1;
		FramesDiff = 1.0f;
		FirstPacket = false;
	}

	return true;
}

#pragma endregion

#pragma endregion