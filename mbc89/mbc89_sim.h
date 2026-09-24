//***************************************************************************************************
//* HEADER - mbc89_sim.h - Simulated plug/voltage/temperature sensors for the mbc89 MSX port         *
//***************************************************************************************************
//* Simulated Plug/Voltage/Temperature systemarray fields: a plug flag, then two 3-ASCII-digit "XX.X" *
//* (decimal point dropped) fields for voltage and temperature, ready for CS2/config-channel          *
//* reporting to consume directly.                                                                     *
//***************************************************************************************************
//* Datei: mbc89_sim.h                                                                                *
//***************************************************************************************************
//* MSX (c) 2026 by Dr.-Ing. Thomas Wiesner                                                           *
//***************************************************************************************************
//* VERSION: 24/09/26                                                                                 *
//***************************************************************************************************
#pragma once

#include "msxgl.h"

// [0]=plug 0/1, [1..3]=voltage digits ("XX.X" minus the dot), [4..6]=temperature digits (same).
extern u8 g_SimArr[7];

void Sim_SetPlugged(bool plugged);
bool Sim_IsPlugged(void);

void Sim_SetVoltage(u16 deciVolts);		// e.g. 145 = 14.5 V
u16  Sim_GetVoltage(void);

void Sim_SetTemperature(i16 deciDeg);		// e.g. 235 = 23.5 degC, negative allowed
i16  Sim_GetTemperature(void);

// Undervoltage threshold: below ~11.0 V -> red status LED.
bool Sim_IsUndervoltage(void);
