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

#include <string.h>

#include "app/cwmacro.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "settings.h"
#include "ui/cpo.h"
#include "ui/helper.h"

void UI_DisplayCPO(void)
{
	char String[24];
	const unsigned int len = strlen(gCW_TX_Display);
	const unsigned int idx = (len > 20) ? len - 20 : 0;

	UI_DisplayClear();
	UI_PrintString("Code Practice", 0, 127, 0, 8);
	if (len > 0) {
		sprintf_(String, "%s", gCW_TX_Display + idx);
		UI_PrintStringSmallNormal(String, 2, 0, 3);
	}
	sprintf_(String, "%u WPM", gEeprom.CW_KEY_WPM);
	UI_PrintStringSmallNormal(String, 2, 0, 7);
	ST7565_BlitFullScreen();
}
