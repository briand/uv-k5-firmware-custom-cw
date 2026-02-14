/* Copyright 2026 NR7Y
 * https://github.com/briand
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

// CW application-level update loop and end-of-transmission handling
// Extracted from app.c to give CW its own independent PTT / EOT path.

#include <stdint.h>
#include <stdbool.h>

#include "app/cwapp.h"
#include "app/cwkeyer.h"
#include "app/cwmacro.h"
#include "app/app.h"
#include "app/menu.h"
#include "audio.h"
#include "driver/bk4819.h"
#include "driver/bk4819-regs.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "bsp/dp32g030/gpio.h"
#include "functions.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#ifdef ENABLE_CODE_PRACTICE
#include "app/cpo.h"
#endif
#ifdef ENABLE_FLASHLIGHT
#include "app/flashlight.h"
#endif

// ---------------------------------------------------------------------------
// CW_EndTxNow  –  end CW transmission immediately
// ---------------------------------------------------------------------------
void CW_EndTxNow(void)
{
	// Call the common end-of-transmission (sends tail, resets TX regs)
	APP_EndTransmission();

	// Go straight to FOREGROUND
	FUNCTION_Select(FUNCTION_FOREGROUND);

	gFlagEndTransmission = false;

#ifdef ENABLE_VOX
	gVOX_NoiseDetected = false;
#endif

	RADIO_SetVfoState(VFO_STATE_NORMAL);

	gUpdateStatus  = true;
	gUpdateDisplay = true;
}

// ---------------------------------------------------------------------------
// CW_AppUpdate  –  called every 1 ms from main.c (gated by TIMERBASE0 ISR flag)
// ---------------------------------------------------------------------------
void CW_AppUpdate(void)
{
	if (gF_LOCK)  // don't init or run the keyer in "hidden menu" tech mode
		return;

	if (!(gTxVfo->Modulation == MODULATION_CW
#ifdef ENABLE_CODE_PRACTICE
		|| gCW_CpoActive
#endif
	))
	{
		// Not in CW mode – but if we were transmitting CW, auto-suspend
		if (gCW_State == CW_TRANSMITTING)
		{
			UART_Send("!!! CW Auto-auto Suspend\r\n", 21);
			RADIO_CW_Suspend();
			gCW_SuspendCounter_1ms = cw_suspend_limit_1ms;
		}
		return;
	}

	// ---- poll the keyer / playback engine for the next action ----
	CW_Action_t action;
	if (gCW_PlaybackActive)
		action = CW_PlaybackHandleState();
	else
		action = CW_HandleState();

	// ---- local-only sidetone path (no RF) ----
	// Used when recording a macro, reading ADC, breakin disabled, or code practice
	if (gCW_Recording || gCW_AdcReadActive || !gEeprom.CW_BREAKIN_ENABLE
#ifdef ENABLE_CODE_PRACTICE
		|| gCW_CpoActive
#endif
		) {
		switch (action)
		{
			case CW_ACTION_CARRIER_ON:
				AUDIO_AudioPathOn();
				BK4819_SetAF(BK4819_AF_ALAM);
				BK4819_WriteRegister(BK4819_REG_70,
					BK4819_REG_70_ENABLE_TONE1 |
					(gEeprom.CW_SIDETONE_LEVEL << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));
				BK4819_SetScrambleFrequencyControlWord(gEeprom.CW_TONE_FREQUENCY * 10);
				#ifdef ENABLE_FLASHLIGHT
				if (gCW_FlashlightSending) {
					GPIO_SetBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
				}
				#endif
				gCW_TxDisplayHoldoff_10ms = 200;
			break;

			case CW_ACTION_CARRIER_OFF:
				BK4819_SetScrambleFrequencyControlWord(0);
				#ifdef ENABLE_FLASHLIGHT
				if (gCW_FlashlightSending) {
					GPIO_ClearBit(&GPIOC->DATA, GPIOC_PIN_FLASHLIGHT);
				}
				#endif
				#ifdef ENABLE_CODE_PRACTICE
				if (gCW_CpoActive)
					BK4819_SetAF(BK4819_AF_MUTE);
				else
				#endif
					RADIO_SetModulation(gRxVfo->Modulation);
				gCW_TxDisplayHoldoff_10ms = 200;
			break;

			default:
			break;
		}
		// don't let RF happen
		action = CW_ACTION_NONE;
	}

	// ---- RF transmit path ----
	switch (action)
	{
		case CW_ACTION_CARRIER_ON:
			gTxTimerCountdown_500ms = 0;
			gCW_TxDisplayHoldoff_10ms = 200;
			gPttIsPressed = true;

			if (gCW_State == CW_INACTIVE)
			{
				UART_Send("CW Start\r\n", 10);
				RADIO_PrepareTX();
			}
			else if (gCW_State == CW_SUSPENDED) {
				RADIO_CW_BeginResume();
			}
			// if already CW_TRANSMITTING: no-op
		break;

		case CW_ACTION_CARRIER_OFF:
			// only suspend once, from active TX
			if (gCW_State == CW_TRANSMITTING) {
				RADIO_CW_Suspend();
				gCW_SuspendCounter_1ms = 0;
			}
			gCW_TxDisplayHoldoff_10ms = 200;
		break;

		case CW_ACTION_CARRIER_HOLD_ON:
			gPttIsPressed = true;
			gTxTimerCountdown_500ms = 0;

			// if hold arrives while suspended (shouldn't happen), resume once
			if (gCW_State == CW_SUSPENDED) {
				RADIO_CW_BeginResume();
			}
			gCW_SuspendCounter_1ms = 0;
		break;

		case CW_ACTION_NONE:
		default:
		break;
	}

	// ---- suspend timeout → end TX ----
	if (gCW_State == CW_SUSPENDED)
	{
		if (++gCW_SuspendCounter_1ms >= cw_suspend_limit_1ms) {
			CW_EndTxNow();
		}
	}
}
