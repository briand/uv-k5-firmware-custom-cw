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
#ifdef ENABLE_CW_ENUNCIATE
#include "app/cwenunciate.h"
#endif
#include "app/cwmacro.h"
#include "app/cwpopup.h"
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
#include "driver/system.h"
#include "driver/timer.h"
#include "app/cpo.h"
#ifdef ENABLE_FLASHLIGHT
#include "app/flashlight.h"
#endif

// ---------------------------------------------------------------------------
// CW_EndTxNow  –  end CW transmission immediately and return to monitor
// ---------------------------------------------------------------------------
void CW_EndTxNow(void)
{
    // Clear CW state when ending transmission entirely
    gCW_State = CW_INACTIVE;

	// Call the common end-of-transmission (sends tail, resets TX regs)
	APP_EndTransmission();

	// Go straight to FOREGROUND
	FUNCTION_Select(FUNCTION_FOREGROUND);

	gFlagEndTransmission = false;

#ifdef ENABLE_VOX
	gVOX_NoiseDetected = false;
#endif

	RADIO_SetVfoState(VFO_STATE_NORMAL);  // only variables
    RADIO_SelectVfos();                   // only variables
    // Respect the user's monitor-mode preference: if they turned monitor off
    // while in CW, return to FOREGROUND and wait for squelch; otherwise
    // immediately go back to open-squelch monitor listening.
    if (gMonitor)
        APP_StartListening(FUNCTION_MONITOR);
    // else: already in FUNCTION_FOREGROUND; CheckForIncoming() will open
    // audio when a signal arrives and trips the squelch interrupt.
}

// ---------------------------------------------------------------------------
// Stuck key transmit timeout (after KF1BER's k1 cwguard). Normal sending always
// pauses between characters, so keying that runs CW_GUARD_LIMIT_MS without a
// break of two dits or more is a stuck key or paddle (a shorted dit paddle keys
// endless dits, which a plain key-down timer misses). TX drops and keying stays
// locked out until the key has been up for CW_GUARD_RELEASE_MS.
// ---------------------------------------------------------------------------
#define CW_GUARD_LIMIT_MS    7000
#define CW_GUARD_RELEASE_MS  1000

static bool     s_guard_keyed;      // the last action left the carrier on
static bool     s_guard_break = true;  // the key has been up for a character break since the last run
static bool     s_guard_locked;     // tripped, waiting for the key to be released
static uint16_t s_guard_run_start;  // first element after the last character break
static uint16_t s_guard_off_since;  // when the carrier last went off

// Returns the action to apply: unchanged normally, CW_ACTION_CARRIER_OFF on the
// tripping call, CW_ACTION_NONE while locked out. *tripped is set on the tripping call.
// Only short spans are measured, so the 16-bit millis never wraps under it: the
// break and release checks run every poll while the key is up.
static CW_Action_t CW_GuardFilter(CW_Action_t action, bool *tripped)
{
	const bool     keyed = (action == CW_ACTION_CARRIER_ON || action == CW_ACTION_CARRIER_HOLD_ON);
	const uint16_t now   = timer_millis_low16();

	*tripped = false;

	if (!keyed) {
		if (s_guard_keyed)
			s_guard_off_since = now;
		s_guard_keyed = false;

		const uint16_t off_ms = timer_millis_low16_since(s_guard_off_since);
		if (off_ms >= 2400u / gEeprom.CW_KEY_WPM)  // two dits of silence
			s_guard_break = true;
		if (s_guard_locked && off_ms >= CW_GUARD_RELEASE_MS) {
			s_guard_locked = false;
			return action;
		}
		return s_guard_locked ? CW_ACTION_NONE : action;
	}

	if (!s_guard_keyed && s_guard_break) {
		s_guard_break = false;
		s_guard_run_start = now;
	}
	s_guard_keyed = true;

	if (s_guard_locked)
		return CW_ACTION_NONE;

	if (timer_millis_low16_since(s_guard_run_start) >= CW_GUARD_LIMIT_MS) {
		s_guard_locked = true;
		*tripped = true;
		return CW_ACTION_CARRIER_OFF;
	}

	return action;
}

// ---------------------------------------------------------------------------
// CW_AppUpdate  –  called from main.c at approx 1ms cadence (sometimes a little longer)
// ---------------------------------------------------------------------------
void CW_AppUpdate(void)
{
	if (gCW_AdcReadActive)  // CW_CRD mode: PTT pin is an output; no keyer FSM activity
		return;

	if (!(gTxVfo->Modulation == MODULATION_CW
		|| gCW_CpoActive
	))
	{
		// Not in CW mode – paranoid check to end CW TX
		if (gCW_State != CW_INACTIVE)
		{
			CW_EndTxNow();
		}
        return;  // not in CW mode, nothing else to do
	}

	// ---- poll the keyer / playback engine for the next action ----
	CW_Action_t action;
	if (gCW_PlaybackActive)
		action = CW_PlaybackHandleState();
	else
		action = CW_HandleState();

	// ---- transmit timeout: a stuck key or paddle drops TX and locks keying out ----
	bool guardTripped;
	action = CW_GuardFilter(action, &guardTripped);
	if (guardTripped)
		CW_StopPlayback();

	if (action == CW_ACTION_CARRIER_ON) {
		// keying closes the speed popup at the new speed
		CW_Popup_Close();
#ifdef ENABLE_CW_ENUNCIATE
		// keying or macro playback takes over the sidetone from an enunciation
		CW_EnunciateStop();
#endif
	}

	// ---- local-only sidetone path (no RF) ----
	// Used when recording a macro, reading ADC, breakin disabled, or code practice
	if ( !gEeprom.CW_BREAKIN_ENABLE || gCW_Recording || gCW_AdcReadActive
		|| gCW_CpoActive
		) {
		switch (action)
		{
			case CW_ACTION_CARRIER_ON:
			if (gCW_State == CW_INACTIVE && !AUDIO_IsAudioPathOn()) 
			{
				AUDIO_AudioPathOn();
				SYSTEM_DelayMs(10);
			}
			if ((gCW_CpoActive && gCW_CpoBacklightOn) 
			|| (!gCW_CpoActive && (gSetting_backlight_on_tx_rx & BACKLIGHT_ON_TR_TX)))	 
			{
					BACKLIGHT_TurnOn();
			}
			BK4819_SetAF(BK4819_AF_ALAM);
			BK4819_WriteRegister(BK4819_REG_70,
				(gEeprom.CW_SIDETONE_LEVEL == 0 ? 0 : BK4819_REG_70_ENABLE_TONE1) |
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
				if (!gCW_CpoActive)  // just stay in ALAM for CPO
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
			if(gSetting_backlight_on_tx_rx & BACKLIGHT_ON_TR_TX)
				BACKLIGHT_TurnOn();

			if (gCW_State == CW_INACTIVE)
			{
				if(!AUDIO_IsAudioPathOn())
				{
					AUDIO_AudioPathOn();
					SYSTEM_DelayMs(20);
				}
				RADIO_PrepareTX();
			}
			else if (gCW_State == CW_SUSPENDED) {
				RADIO_CW_BeginResume();
                gCW_SuspendCounter_1ms = 0;
			}
			// if already CW_TRANSMITTING: no-op
		break;

		case CW_ACTION_CARRIER_OFF:
			// only suspend once, from active TX
			if (gCW_State == CW_TRANSMITTING) {
				RADIO_CW_Suspend();
				gCW_SuspendCounter_1ms = timer_millis_low16();
			}
			gCW_TxDisplayHoldoff_10ms = 200;
		break;

		case CW_ACTION_CARRIER_HOLD_ON:
			gPttIsPressed = true;
			gTxTimerCountdown_500ms = 0;
			gCW_TxDisplayHoldoff_10ms = 200;

			// if hold arrives while suspended (shouldn't happen), resume once
			if (gCW_State == CW_SUSPENDED) {
				RADIO_CW_BeginResume();
			}
			gCW_SuspendCounter_1ms = timer_millis_low16();
		break;

		case CW_ACTION_NONE:
			if(gCW_State == CW_TRANSMITTING) {
				// if we've been transmitting but now have no carrier, suspend
				RADIO_CW_Suspend();
				gCW_SuspendCounter_1ms = timer_millis_low16();
			}
		default:
		break;
	}

	// a timeout drops TX now instead of waiting out the hang time
	if (guardTripped && gCW_State != CW_INACTIVE) {
		gPttIsPressed = false;
		CW_EndTxNow();
	}

	// ---- suspend timeout → end TX ----
	if (gCW_State == CW_SUSPENDED)
	{
		if (timer_millis_low16_since(gCW_SuspendCounter_1ms) >= cw_suspend_limit_1ms) {
            gCW_SuspendCounter_1ms = 0;
            gCW_TxDisplayHoldoff_10ms = 200;
            gPttIsPressed = false;
			CW_EndTxNow();
		}
	}
}
