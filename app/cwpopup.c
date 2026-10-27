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

// CW speed popup state and its key handling

#include "app/cwkeyer.h"
#include "app/cwpopup.h"
#include "audio.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
#include "ui/ui.h"

#define CW_POPUP_SHOW_10MS     200  // 2 s after the last key press
#define CW_WPM_MIN             10   // same range as the CWwpm menu
#define CW_WPM_MAX             45

static bool    s_open;
static uint8_t s_10ms;           // idle time left, counted on the 10 ms tick
static bool    s_speed_changed;  // saved once on close instead of on every step

bool CW_Popup_IsOpen(void)
{
	return s_open;
}

// The new speed is already live, so it's kept (and saved) however the popup closes
void CW_Popup_Close(void)
{
	if (!s_open)
		return;

	s_open = false;
	gUpdateDisplay = true;

	if (s_speed_changed) {
		s_speed_changed = false;
		gRequestSaveSettings = true;
	}
}

// Also restarts the idle time
static void Show(void)
{
	s_open = true;
	s_10ms = CW_POPUP_SHOW_10MS;
	gUpdateDisplay = true;

#ifdef ENABLE_CW_ENUNCIATE
	// a '5' (five dits) sent at the current speed, so the speed can be judged by ear
	// (only plays with VOICE on); while up/down auto-repeats it waits for the release
	AUDIO_SetCWEnunciate("5");
	gAnotherVoiceID = (VOICE_ID_t)0xFE;
#endif
}

void CW_Popup_Speed(void)
{
	if (s_open)
		CW_Popup_Close();
	else if (gScreenToDisplay == DISPLAY_MAIN)
		Show();
	else
		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;  // up/down only reach the main screen
}

static void StepSpeed(int8_t direction)
{
	const int wpm = gEeprom.CW_KEY_WPM + direction;

	if (wpm < CW_WPM_MIN || wpm > CW_WPM_MAX) {
		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}

	gEeprom.CW_KEY_WPM = wpm;
	CW_UpdateWPM();
	s_speed_changed = true;
}

bool CW_Popup_ProcessKey(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	// PTT keeps keying; side keys go to their actions, see CW_Popup_OnAction
	if (!s_open || Key == KEY_PTT || Key == KEY_SIDE1 || Key == KEY_SIDE2)
		return false;

	if (Key == KEY_MENU || Key == KEY_EXIT) {
		// close on release so the release doesn't reach the main screen
		if (!bKeyPressed && !bKeyHeld)
			CW_Popup_Close();
		return true;
	}

	if (Key == KEY_UP || Key == KEY_DOWN) {
		if (bKeyPressed) {  // first press and every auto-repeat while held
			StepSpeed((Key == KEY_UP) ? 1 : -1);
			Show();
		}
		return true;
	}

	// any other key closes the popup, then does its usual job
	if (bKeyPressed && !bKeyHeld)
		CW_Popup_Close();

	return false;
}

void CW_Popup_OnAction(uint8_t action)
{
	// CW SPEED closes the popup itself
	if (action != ACTION_OPT_CW_SPEED)
		CW_Popup_Close();
}

void CW_Popup_Tick10ms(void)
{
	if (!s_open)
		return;

	// PTT in any mode closes it like keying does
	if (gCurrentFunction == FUNCTION_TRANSMIT) {
		CW_Popup_Close();
		return;
	}

#ifdef ENABLE_CW_ENUNCIATE
	// with prompts on the screen may not be seen, so it stays open until a key or keying
	// closes it, and the speed can be stepped at leisure
	if (gEeprom.VOICE_PROMPT)
		return;
#endif

	if (--s_10ms == 0)
		CW_Popup_Close();
}
