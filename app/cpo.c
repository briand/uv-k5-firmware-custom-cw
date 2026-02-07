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

// Code practice (CPO) app skeleton

#include "app/cpo.h"
#include "audio.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/ui.h"
#ifdef ENABLE_CW_MODULATOR
#include "app/cwkeyer.h"
#include "app/cwmacro.h"
#endif

#ifdef ENABLE_CODE_PRACTICE

bool gCpoActive = false;
static bool s_needs_redraw = false;
static bool s_backlight_on = false;

void CPO_Enter(void)
{
    CW_KeyerReconfigure(true);
	gCpoActive = true;
	s_needs_redraw = true;
		s_backlight_on = false;
	gRequestDisplayScreen = DISPLAY_CPO;
	gUpdateDisplay = true;

	gMonitor = false;
	BK4819_SetAF(BK4819_AF_MUTE);
	//AUDIO_AudioPathOff();
}

void CPO_Exit(void)
{
    CW_KeyerReconfigure(false);
	gCpoActive = false;
	gRequestDisplayScreen = DISPLAY_MAIN;
	gUpdateDisplay = true;
	gUpdateStatus = true;
	gFlagReconfigureVfos = true;  // keyer will be turned back on if we're in CW modulation
}

void CPO_Tick(void)
{
	if (!gCpoActive) {
		return;
	}

	if (s_backlight_on) {
		gBacklightCountdown_500ms = 2;
	}

	if (s_needs_redraw | gCW_TX_DisplayUpdated) {
		s_needs_redraw = false;
		gRequestDisplayScreen = DISPLAY_CPO;
		gUpdateDisplay = true;
	}
}

void CPO_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	if (!bKeyPressed || bKeyHeld) {
		return;
	}

	switch (Key) {
	case KEY_UP:
		if (gEeprom.CW_KEY_WPM < 30) {
			gEeprom.CW_KEY_WPM++;
#ifdef ENABLE_CW_MODULATOR
			CW_UpdateWPM();
#endif
			gUpdateDisplay = true;
		}
		break;

	case KEY_DOWN:
		if (gEeprom.CW_KEY_WPM > 10) {
			gEeprom.CW_KEY_WPM--;
#ifdef ENABLE_CW_MODULATOR
			CW_UpdateWPM();
#endif
			gUpdateDisplay = true;
		}
		break;

	case KEY_STAR:
		s_backlight_on = !s_backlight_on;
		if (s_backlight_on) {
			BACKLIGHT_TurnOn();
			gBacklightCountdown_500ms = 2;
		} else {
			BACKLIGHT_TurnOff();
		}
		break;

	default:
		break;
	}
}

#endif
