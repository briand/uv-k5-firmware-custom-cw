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

// CW enunciation engine
// Plays the shared enunciation queue (gVoiceID[], ENUNCIATE_CW mode) as
// sidetone only. Unlike macro playback this runs in every modulation and
// never goes near CW_AppUpdate's RF path, so it can't key the transmitter.
//
// For the whole message the BK4819 AF output stays in ALAM, which only
// carries generated tones, so RX audio is muted just like during a voice
// prompt. Elements are gated by the tone frequency word, the same way the
// local sidetone path in cwapp.c does it.

#include <stdint.h>
#include <stdbool.h>

#include "app/cwenunciate.h"
#include "app/cwmacro.h"
#include "app/cpo.h"
#ifdef ENABLE_FMRADIO
#include "app/fm.h"
#include "driver/bk1080.h"
#endif
#include "audio.h"
#include "driver/bk4819.h"
#include "driver/bk4819-regs.h"
#include "driver/keyboard.h"
#include "driver/system.h"
#include "driver/timer.h"
#include "functions.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"

static bool     s_pending     = false;  // queued, waiting for the triggering key to be released
static bool     s_active      = false;
static bool     s_tone_on     = false;
static uint16_t s_phase_start = 0;  // millis when the current element/gap began
static uint16_t s_phase_ms    = 0;  // length of the current element/gap
static uint16_t s_dit_ms      = 0;
static uint8_t  s_pattern     = 0;  // current char morse pattern (LSB-first, 1 = dah)
static uint8_t  s_len         = 0;  // number of elements in current char
static uint8_t  s_elem        = 0;  // current element index within char
static uint16_t s_saved_reg71 = 0;  // REG_71 doubles as the scrambler setting

static void ToneOn(void)
{
    // re-assert the speaker amp, a squelch close may have turned it off
    AUDIO_AudioPathOn();
    BK4819_SetScrambleFrequencyControlWord(gEeprom.CW_TONE_FREQUENCY * 10);
    s_tone_on = true;
}

static void ToneOff(void)
{
    BK4819_SetScrambleFrequencyControlWord(0);
    s_tone_on = false;
}

static void ClearQueue(void)
{
    gVoiceWriteIndex = 0;
    gVoiceReadIndex  = 0;
}

bool CW_EnunciateIsActive(void)
{
    return s_active;
}

void CW_EnunciateStart(void)
{
    if (s_active || s_pending || gEnunciateMode != ENUNCIATE_CW || gVoiceWriteIndex == 0)
        return;

    if (!gEeprom.VOICE_PROMPT
        || gEeprom.CW_KEY_WPM == 0
        || gCurrentFunction == FUNCTION_TRANSMIT
        || gCW_State != CW_INACTIVE
        || gCW_PlaybackActive)
    {
        ClearQueue();
        return;
    }

    // Don't touch the audio yet. A long press runs its action while the key is held and
    // defers the channel save to the release, which reconfigures the radio again
    // (RADIO_SetupRegisters) and would clobber our tone setup mid-message.
    s_pending = true;
}

static void BeginPlayback(void)
{
    s_dit_ms      = 1200U / gEeprom.CW_KEY_WPM;  // PARIS: 50 dits per word
    s_saved_reg71 = BK4819_ReadRegister(BK4819_REG_71);

#ifdef ENABLE_FMRADIO
    if (gFmRadioMode)
        BK1080_Mute(true);
#endif

    if (gCurrentFunction == FUNCTION_POWER_SAVE && gRxIdleMode)
        BK4819_RX_TurnOn();

    if (!AUDIO_IsAudioPathOn())
    {
        AUDIO_AudioPathOn();
        SYSTEM_DelayMs(10);
    }

    BK4819_SetAF(BK4819_AF_ALAM);
    BK4819_SetScrambleFrequencyControlWord(0);
    BK4819_WriteRegister(BK4819_REG_70,
        (gEeprom.CW_SIDETONE_LEVEL == 0 ? 0 : BK4819_REG_70_ENABLE_TONE1) |
        (gEeprom.CW_SIDETONE_LEVEL << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));

#ifdef ENABLE_VOX
    gVoxResumeCountdown = 2000;
#endif

    // first update fetches the first char immediately
    s_tone_on     = false;
    s_len         = 0;
    s_elem        = 0;
    s_phase_ms    = 0;
    s_phase_start = timer_millis_low16();
    s_active      = true;
}

void CW_EnunciateStop(void)
{
    if (s_pending)
    {   // nothing was set up yet, just drop the message
        s_pending = false;
        ClearQueue();
        return;
    }

    if (!s_active)
        return;

    s_active = false;

#ifdef ENABLE_FMRADIO
    if (gFmRadioMode)
        BK1080_Mute(false);
#endif

    if (gCW_CpoActive)
    {   // CPO owns the audio path: leave it in its own idle sidetone state (ALAM, tone
        // generator still enabled, frequency word 0, speaker on), as cwapp's CARRIER_OFF
        // does. Clearing REG_70 in ALAM with a non-zero REG_71 lets RX audio through.
        BK4819_SetScrambleFrequencyControlWord(0);
    }
    else
    {
        if (gRxVfo->Modulation == MODULATION_CW)
        {   // CW never clears REG_70, which pops: leave tone1 enabled with a zero
            // frequency word, as cwapp's sidetone and BK4819_SetupSquelch do
            BK4819_SetScrambleFrequencyControlWord(0);
        }
        else
        {
            BK4819_WriteRegister(BK4819_REG_70, 0);
            BK4819_WriteRegister(BK4819_REG_71, s_saved_reg71);
        }

        // AF can't go ALAM -> MUTE (BK4819_SetAF skips it), so always rebuild
        // the RX demod; the speaker amp below decides whether it's heard
        RADIO_SetModulation(gRxVfo->Modulation);

        if (!gEnableSpeaker)
            AUDIO_AudioPathOff();
    }

    if (gCurrentFunction == FUNCTION_POWER_SAVE && gRxIdleMode)
        BK4819_Sleep();

#ifdef ENABLE_VOX
    gVoxResumeCountdown = 80;
#endif

    ClearQueue();
}

void CW_EnunciateUpdate(void)
{
    if (s_pending)
    {
        if (gCurrentFunction == FUNCTION_TRANSMIT || gPttIsPressed)
        {
            CW_EnunciateStop();
            return;
        }

        // gKeyReading1 clears only after the release ProcessKey(), including any
        // reconfigure it triggers, has returned
        if (gKeyReading1 != KEY_INVALID)
            return;

        s_pending = false;
        BeginPlayback();
    }

    if (!s_active)
        return;

    if (gCurrentFunction == FUNCTION_TRANSMIT || gPttIsPressed)
    {
        CW_EnunciateStop();
        return;
    }

    if (timer_millis_low16_since(s_phase_start) < s_phase_ms)
        return;

    s_phase_start = timer_millis_low16();

    if (s_tone_on)
    {   // element done: 1 dit gap within a char, 3 dits after its last element
        ToneOff();
        s_phase_ms = (++s_elem < s_len) ? s_dit_ms : 3U * s_dit_ms;
        return;
    }

    if (s_elem >= s_len)
    {   // between chars: fetch the next one
        if (gVoiceReadIndex >= gVoiceWriteIndex)
        {
            CW_EnunciateStop();
            return;
        }

        const char ch = (char)gVoiceID[gVoiceReadIndex++];
        s_elem = 0;
        s_len  = 0;

        if (ch == ' ')
        {   // word gap: 7 dits including the char gap already spent
            s_phase_ms = 4U * s_dit_ms;
            return;
        }

        if (!CW_GetMorseForChar(ch, &s_pattern, &s_len))
        {   // unknown char - skip it
            s_phase_ms = 0;
            return;
        }
    }

    ToneOn();
    s_phase_ms = ((s_pattern >> s_elem) & 1U) ? 3U * s_dit_ms : s_dit_ms;
}
