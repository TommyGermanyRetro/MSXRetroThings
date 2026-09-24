//***************************************************************************************************
//* SOURCE - mbc89_sim.c - Simulated plug/voltage/temperature sensors for the mbc89 MSX port         *
//***************************************************************************************************
//* Datei: mbc89_sim.c                                                                                *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
//* VERSION: 17/09/26                                                                                 *
//***************************************************************************************************
#include "mbc89_sim.h"

u8 g_SimArr[7];

static u16 s_Voltage;		// deciVolts
static i16 s_Temperature;	// deciDeg

//=====================================================================================================
void Sim_SetPlugged(bool plugged)
{
	g_SimArr[0] = plugged ? 1 : 0;
}
//-----------------------------------------------------------------------------------------------------
bool Sim_IsPlugged(void)
{
	return g_SimArr[0] != 0;
}

//=====================================================================================================
void Sim_SetVoltage(u16 deciVolts)
{
	if(deciVolts > 999) deciVolts = 999;
	s_Voltage = deciVolts;
	g_SimArr[1] = '0' + (deciVolts / 100) % 10;
	g_SimArr[2] = '0' + (deciVolts / 10) % 10;
	g_SimArr[3] = '0' + deciVolts % 10;
}
//-----------------------------------------------------------------------------------------------------
u16 Sim_GetVoltage(void)
{
	return s_Voltage;
}

//=====================================================================================================
void Sim_SetTemperature(i16 deciDeg)
{
	u16 abs_val;
	if(deciDeg < -999) deciDeg = -999;
	if(deciDeg > 999) deciDeg = 999;
	s_Temperature = deciDeg;
	// Negative values show the sign in place of the tens digit (whole degrees only, no tenths).
	if(deciDeg < 0)
	{
		abs_val = -deciDeg;
		g_SimArr[4] = '-';
		g_SimArr[5] = '0' + (abs_val / 10) % 10;
		g_SimArr[6] = '0' + abs_val % 10;
	}
	else
	{
		g_SimArr[4] = '0' + (deciDeg / 100) % 10;
		g_SimArr[5] = '0' + (deciDeg / 10) % 10;
		g_SimArr[6] = '0' + deciDeg % 10;
	}
}
//-----------------------------------------------------------------------------------------------------
i16 Sim_GetTemperature(void)
{
	return s_Temperature;
}

//=====================================================================================================
bool Sim_IsUndervoltage(void)
{
	return s_Voltage < 110;
}
