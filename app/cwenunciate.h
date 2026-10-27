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

// CW enunciation: plays the shared voice/enunciation queue as local sidetone
// (never RF), as an alternative to the voice chip's limited vocabulary.
//
// Usage mirrors the voice prompts:
//     AUDIO_SetCWEnunciate("SQL 5");          // or AUDIO_SetFrequencyCW(freq)
//     gAnotherVoiceID = (VOICE_ID_t)0xFE;     // APP starts it after the key beep

#ifndef APP_CWENUNCIATE_H
#define APP_CWENUNCIATE_H

#include <stdbool.h>

// Begin playing the queue (once no key is held). Called by APP via the gAnotherVoiceID handoff.
// Does nothing (and empties the queue) if the VOICE setting is off,
// the radio is transmitting, or the CW keyer / macro playback is busy.
void CW_EnunciateStart(void);

// Drive element timing; call every 1 ms from the main loop.
void CW_EnunciateUpdate(void);

// Abort playback and restore the RX audio path. Safe to call when idle.
void CW_EnunciateStop(void);

bool CW_EnunciateIsActive(void);

#endif // APP_CWENUNCIATE_H
