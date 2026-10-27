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

// CW speed popup on the main screen, after KF1BER's k1 popup. The CW SPEED action
// opens it (or closes it when already open), up/down change WPM live, and MENU,
// EXIT, keying, TX or 2 s idle close it. Any other key closes it and then does its
// usual job.

#ifndef APP_CWPOPUP_H
#define APP_CWPOPUP_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/keyboard.h"

bool CW_Popup_IsOpen(void);
void CW_Popup_Speed(void);    // key action: open speed adjust, or close it when already open
void CW_Popup_Close(void);    // e.g. the keyer just keyed an element

bool CW_Popup_ProcessKey(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);  // true when the key was used
void CW_Popup_OnAction(uint8_t action);  // a side key action is about to run
void CW_Popup_Tick10ms(void);

#endif
