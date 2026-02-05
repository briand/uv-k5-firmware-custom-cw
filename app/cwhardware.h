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

 #ifndef APP_CWHARDWARE_H
#define APP_CWHARDWARE_H

#include <stdint.h>
#include <stdbool.h>

// Normalized paddle input + edge flags
typedef struct {
    bool dit;
    bool dah;
    bool dit_rise;
    bool dah_rise;
} CW_Input;

// Read raw inputs for a specific mode
bool CW_ReadKeysForMode(uint8_t mode, bool *dit_out, bool *dah_out);

// Read normalized paddle inputs (computes edges)
void CW_ReadKeys(CW_Input *in);

// Configure port pins for paddle interface
void CW_ConfigurePortGround(bool enable);
void CW_ConfigurePortRing(bool enable);
void CW_ConfigureADCforCECPaddles(bool enable);

// Reset hardware-sampled state (call from keyer init)
void CW_HW_ResetKeySamples(void);

#endif // APP_CWHARDWARE_H
