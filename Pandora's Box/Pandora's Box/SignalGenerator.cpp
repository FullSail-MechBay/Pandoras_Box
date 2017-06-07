// tabs=4
//************************************************************
//	COPYRIGHT 2010 Moog Inc. - ALL RIGHTS RESERVED
//
// This file is the product of Moog Inc. and cannot be 
// reproduced, copied, or used in any shape or form without 
// the express written consent of Moog Inc.
//************************************************************
//
//  Defines class for basic signal generator.
//
//	Revision History: See end of file.
//
//*************************************************************

#include <math.h>
#include "SignalGenerator.hpp"


const double PI = 3.1415926535897932384626433832795;
const double TWO_PI = PI * 2.0;
const int POWER_2 = 2;


//---------------------------------------------------------
// Constructor.
//---------------------------------------------------------
CSignalGenerator::CSignalGenerator()
{
    m_eWaveForm = eWAVEFORM_SINE;
    m_dAmplitude = 0;
    m_dFrequency = 0; 
    m_dOffset = 0;
    m_uiSamplesPerSecond = 0;

    ResetCycle();
}

//---------------------------------------------------------
// Destructor.
//---------------------------------------------------------
CSignalGenerator::~CSignalGenerator()
{
}

//---------------------------------------------------------
// Sets the signal generator parameters.
//
// Input:
//      eWaveForm       - waveform shape
//      dFrequency      - waveform frequency (Hz)
//      dAmplitude      - waveform amplitude
//      dOffset         - waveform DC offset
//      uiSamplesPerSecond - number of samples/second of waveform
//
//---------------------------------------------------------
void CSignalGenerator::SetParameters(TWaveForm eWaveForm, 
                        double dFrequency, 
                        double dAmplitude, 
                        double dOffset, 
                        unsigned int uiSamplesPerSecond)
{
    m_eWaveForm = eWaveForm;
    m_dAmplitude = dAmplitude;
    m_dFrequency = dFrequency; 
    m_dOffset = dOffset;
    m_uiSamplesPerSecond = uiSamplesPerSecond;

    // calculate number of datapoints in a single waveform cycle,
    // which is dependent on the uiSamplesPerSecond argument.
	if(m_dFrequency > 0.000001)
	{
	    // calculate the number of waveform data points
		m_uiWaveformDataPoints = (unsigned int)(((double)m_uiSamplesPerSecond * (1.0 / m_dFrequency)) + 0.5);
	}
	else
	{
	    m_dAmplitude = 0.0;
		m_uiWaveformDataPoints = m_uiSamplesPerSecond;
	}

    switch (m_eWaveForm)
    {
    case eWAVEFORM_SINE:
	    // calculate the amount to increment per call
	    m_dDelta = TWO_PI / (double)m_uiWaveformDataPoints;
        break;
    case eWAVEFORM_TRIANGLE:
        m_dDelta = (m_dAmplitude * 4.0) / (double)m_uiWaveformDataPoints;
        break;
    }

    ResetCycle();
}


//---------------------------------------------------------
// Gets next sample. When end of cycle is reached, cycle is
// restarted.
//---------------------------------------------------------
float CSignalGenerator::GetSample()
{
double dNewPosition = 0;

    switch (m_eWaveForm)
    {
    case eWAVEFORM_SINE:
        {
	        double dRadians = (double)m_uiCurrentPosition * m_dDelta;
            dNewPosition = (sin(dRadians) * m_dAmplitude);
        }
        break;
    case eWAVEFORM_TRIANGLE:
        
	    //...First Quarter (Increase)
	    if(m_uiCurrentPosition  < (m_uiWaveformDataPoints / 4))
	    {
		    m_dCurrentAmplitude += m_dDelta;
	    }
	    //...Second and third Quarters (Decrease)
	    else if(m_uiCurrentPosition  < (m_uiWaveformDataPoints * 3 / 4))
	    {
		    m_dCurrentAmplitude -= m_dDelta;
	    }
	    //...Forth Quarter (Increase)
	    else
	    {
		    m_dCurrentAmplitude += m_dDelta;
	    }
        dNewPosition = m_dCurrentAmplitude;
        break;
    }

	if (++m_uiCurrentPosition > m_uiWaveformDataPoints)
	{
        ResetCycle();
	}

    return (float)(dNewPosition+m_dOffset);
}


//-------------------------------------------------------------------
// END OF FILE
//-------------------------------------------------------------------
