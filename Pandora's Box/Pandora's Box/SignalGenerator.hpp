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

#ifndef _SIGNALGENERATOR_HPP_
#define _SIGNALGENERATOR_HPP_


class CSignalGenerator
{
public:
    typedef enum
    {
        eWAVEFORM_NONE = 0,        
        eWAVEFORM_SINE,
        eWAVEFORM_TRIANGLE,
    } TWaveForm;

private:
    TWaveForm   m_eWaveForm;
    double      m_dAmplitude;
    double      m_dFrequency;
    double      m_dOffset;
    unsigned int m_uiSamplesPerSecond;

    unsigned int m_uiWaveformDataPoints;
    double m_dDelta;
    unsigned int m_uiCurrentPosition;

    double m_dCurrentAmplitude;

public:
    CSignalGenerator();
    ~CSignalGenerator();

    void SetParameters(TWaveForm eWaveForm, 
                        double dFrequency, 
                        double dAmplitude, 
                        double dOffset, 
                        unsigned int uiSamplesPerSecond);

    void ResetCycle() {m_uiCurrentPosition = 1;m_dCurrentAmplitude = 0;}

    float GetSample();
};


#endif // _SIGNALGENERATOR_HPP_


//-------------------------------------------------------------------
// END OF FILE
//-------------------------------------------------------------------
