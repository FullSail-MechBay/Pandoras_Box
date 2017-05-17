#include "MBCtoHostReader.h"
#include <cstring>

using namespace DataManager;

uint32_t MBCtoHostReader::ReaderDataBufferSize = sizeof(StatusResponse)
+ sizeof(OSD_1_DOF) + sizeof(OSD_2_Length)
+ sizeof(OSD_3_Actuator) + sizeof(OSD_4_Accelerometer_Data);

enum StatusWordBitParser
{
	SWBP_EncodedMachineState = 0x0000000F,
	SWBP_MotionCommandMode = 0x00000030,
	SWBP_MDA_TuningFileNumber = 0x0000FF00,
	SWBP_UpdateRate = 0xFFFF0000,
};
enum StatusBitOffsets
{
	SBO_MCM = 4,
	SBO_MDA = 8,
	SBO_UDR = 16
};

MachineState StatusResponse::GetMachineState()
{
	return (MachineState)(MachineStatus & SWBP_EncodedMachineState);
}
MotionCommandMode StatusResponse::GetMotionCommandMode()
{
	return (MotionCommandMode)((MachineStatus & SWBP_MotionCommandMode) >> SBO_MCM);
}
uint8_t StatusResponse::GetMDATuningFileNum()
{
	return (MachineStatus & SWBP_MDA_TuningFileNumber) >> SBO_MDA;
}
uint16_t StatusResponse::GetUpdateRate()
{
	return (MachineStatus & SWBP_UpdateRate) >> SBO_UDR;
}

MBCtoHostReader::MBCtoHostReader()
{
	Buffer = new uint8_t[ReaderDataBufferSize];
	memset(Buffer, 0, ReaderDataBufferSize);

	for (size_t OSD = 0; OSD < OSDW_NumOptions; OSD++)
		BufferLocations[OSD] = nullptr;
}
MBCtoHostReader::~MBCtoHostReader()
{
	delete[] Buffer;
}

const MBC_Header* MBCtoHostReader::GetHeaderInfo()
{
	if (200 == ((MBC_Header*)Buffer[0])->MessageID)
		return ((MBC_Header*)Buffer[0]);
	return nullptr;
}
const StatusResponse* MBCtoHostReader::GetStatusResponse()
{
	if (GetHeaderInfo())
		return (StatusResponse*)Buffer;
	return nullptr;
}
const OSD_1_DOF* MBCtoHostReader::GetOSD_1()
{
	return (OSD_1_DOF*)Healper_CheckForStruct(OSDW_DOF);
}
const OSD_2_Length* MBCtoHostReader::GetOSD_2()
{
	return (OSD_2_Length*)Healper_CheckForStruct(OSDW_DOF);
}
const OSD_3_Actuator* MBCtoHostReader::GetOSD_3()
{
	return (OSD_3_Actuator*)Healper_CheckForStruct(OSDW_DOF);
}
const OSD_4_Accelerometer_Data* MBCtoHostReader::GetOSD_4()
{
	return (OSD_4_Accelerometer_Data*)Healper_CheckForStruct(OSDW_DOF);
}


const uint32_t* MBCtoHostReader::Healper_CheckForStruct(Optional_Status_Data _OSDW)
{
	const StatusResponse* statusInfo = GetStatusResponse();
	const uint32_t* bufferLoc = nullptr;
	if (statusInfo)
	{
		const uint32_t* OSDWord = &(statusInfo->OptionalStatusData);
		while (*OSDWord != OSDW_None)
		{
			if (*OSDWord == _OSDW)
			{
				bufferLoc = ++OSDWord;
				break;
			}
			Healper_GetNextWord(OSDWord);
		}
	}

	return bufferLoc;
}
void MBCtoHostReader::Healper_GetNextWord(const uint32_t* _word)
{
	switch (*_word)
	{
	case OSDW_DOF:
		_word = &((OSD_1_DOF*)(++_word))->OptionalStatusData;
		break;
	case OSDW_Length:
		_word = &((OSD_2_Length*)(++_word))->OptionalStatusData;
		break;
	case OSDW_Data:
		_word = &((OSD_3_Actuator*)(++_word))->OptionalStatusData;
		break;
	case OSDW_Accelerometer:
		_word = &((OSD_4_Accelerometer_Data*)(++_word))->OptionalStatusData;
		break;
	default:
		_word = nullptr;
		break;
	}
}