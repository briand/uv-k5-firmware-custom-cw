NR7Y CW firmware README

# v1.0
> [!IMPORTANT]
> This firmware only works on "V1" hardware. How do you know? Well, you kinda don't. If you bought your radio early last year, it's V1. If you bought it late last year, it could be V2. If you bought it in 2026, it could be V3! Most (all?) V3 radios are marked as such on the label under the battery. Some vendors specifically promote that their listing a V3, but not all.
> 
> If the flashing process doesn't work, you know you don't have a V1. You should still be able to flash back to the original factory firmware; online sources have it available. I will work on a V2/V3 compatible version of the firmware soon. Note: Bootloader version is not the same as radio hardware version; it's ok if you see "bootloader version 2.xx" during flashing.

Upload using your favorite Quansheng flash tool. I use k5tool (https://github.com/qrp73/K5TOOL). This link also has recovery information on the landing README page, if needed. 

To flash directly from your Chrome browser use https://egzumer.github.io/uvtools/.

> [!NOTE]
> After flashing, I *strongly advise* resetting the eeprom to ensure there are no lingering settings from other firmware versions. 
 1. Hold down PTT and Side Button 1 while turning the radio on.
 2. Release buttons, menu will be automatically presented.
 3. Go up to Reset and pick ALL. 

## Changelist from beta6 -> beta7 (1.0)
- Status bar shows **BKIN** indicator when CW break-in is enabled
- Add several punctuation characters to the on-screen CW decoder and macro system (`.`, `,`, `=`, `-`)
- 3ms debounce added to CW key inputs to reduce spurious elements from contact noise and RFI
- Fixed a bug where ADC-read (CEC cable test) mode would trigger RF transmission (#23)
- Re-apply monitor-mode setting based on modulation when the VFO is reconfigured such going between VFO A/B or mem to VFO mode
- Add a short pre-delay when beginning to send while the squelch is closed (audio path off), preventing the leading edge of the first dit from being cut off
- Minor ADC read cleanup
- Keep backlight on during all sending (change via menu 39 BltTRX)

_Previous beta changelists moved to the bottom of this doc_

# CW (Morse Code) Firmware Mod Guide

This guide describes the CW (Continuous Wave / Morse Code) menu options added to the UV-K5 firmware when `ENABLE_CW_MODULATOR` is enabled.

## Accessing CW Mode

1. Set your VFO modulation to **CW** mode (in the modulation menu), or long-press **0 (FM)** to change modulations
2. Configure CW parameters in the menu system (Menu > CWfreq, CWvol, etc.)
3. Press PTT is a CW straight key by default

## Iambic Key Options

You can perform keying in these ways:
- Buttons (PTT and Side Button 1)
- Perform a small rework inside the radio, then attach a normal paddle - rework instructions: [NR7Y CW Firmware - Paddle Rework.pdf](<NR7Y CW Firmware - Paddle Rework.pdf>)
- Build a "CEC cable" with 2 resistors, and use the cable on a normal paddle - example cable instructions: (https://www.m5duk.com/2025/05/20/use-a-standard-cw-paddle-on-a-quansheng-uv-k5-uv-k6/)

## Code Practice Oscillator (CPO)

The CPO mode provides local practice: no RX or TX, sidetone only, on-screen decode while you send.
- Launch by holding **5** (NOAA reception is disabled)
- Exit by tapping **EXIT**
- Use the Up/Down keys to adjust WPM, which will save on exit.
- Tap **'*'** to toggle backlight
- Tap **4** to toggle flashlight sending

### Filtering

Menu option #9 (Filter) includes an extra setting for 1.7k bandwidth. Cycle through filter settings by holding ***7***

## CW Menu Options

#### `CWfreq` - CW Sidetone Frequency

Sets the audio frequency for the CW sidetone you hear locally when transmitting. This is also used as the BFO (Beat Frequency Oscillator) offset when receiving CW signals.

#### `CWvol` - CW Sidetone Volume Level

Controls the volume level of the CW sidetone you hear when transmitting. The lowest value is "OFF"

#### `CWkmod` - CW Keyer Mode

Selects the keyer mode when using paddle inputs (dual-lever key). These modes are modeled after Elecraft mode A/B behavior. 

**Note:** Keyer mode only applies when using paddle inputs (buttons or port paddles). When using handkey modes, the keyer is disabled and this setting has no effect.

#### `CWwpm` - CW Speed (Words Per Minute)

Sets the sending speed for the automatic iambic keyer in Words Per Minute.

**Note:** Speed only applies when using paddle inputs with the iambic keyer enabled. Handkey modes follow your manual keying speed.

#### `CWkin` - CW Key Input Configuration

Configures how the CW input signals are connected and interpreted. This is the most complex setting as it determines which physical inputs are used and how they're mapped.

**Note:** The port modes (PTT+port, tip/ring settings) require an iambic key rework inside the radio. If you have already done the original straight key mod and wish to keep it, this will continue to work with the "PTT HandKey" mode or PTT/Side1 modes.

**Note:** When the radio is in a port/tip/ring or CEC mode, serial communication **will not work**, including programming with CHIRP. To use the serial port, switch modulation mode away from CW, or switch to a VFO where CW is not the current mode. This does not affect firmware flash programming.

**CEC cable input** is now available here. Because of the internal way this is accomplished, the PTT button cannot be used while in this mode. To make your own CEC cable, see [https://www.m5duk.com/2025/05/20/use-a-standard-cw-paddle-on-a-quansheng-uv-k5-uv-k6/](M5DUK's Helpful Article) among others. In short, wire a cable where the paddle tip goes through a 10k resistor to radio tip, and paddle ring goes through a 20k resistor **also** to radio tip, and wire sleeve-to-sleeve.

#### `CWmsg1` / `CWmsg2` / `CWmsg3` / `CWmsg4` - CW Message Recording and Playback

Messages 1-4 may store up to 46 characters for playback (not including spaces).

Messages start empty - enter the menu and use arrows to change macro option:
- record new? - Select with 'menu' button to begin recording
- play - Select with 'menu' button to begin playback

### CW Message Recording
- Recordings are made using the currently programmed keyer settings and wpm. Attempting to record while not in CW modulation or without a keyer (while in handkey mode) will not work.
- RF is not transmitted while recording. 
- While recording is in progress, the display will show the most recently recorded characters, and a macro character count. 
- To save the macro when complete, Push the 'menu' button. 
- To exit the recording without saving, push the 'exit' button.

### CW Message Playback
There are two ways to playback the CW messages:
- Enter the menu selection for the given message, and change using up/down to 'Play', then select with 'menu' button. Playback will begin. Change using up/down to 'Repeat' and select with the 'menu' button to start repeating the message.
- Assign playback to an action button (side buttons or Menu button):
  - In the menu system choose one of menu items 23 through 27 to assign playback or repeat to side button 1 or 2 short press, 1 or 2 long press, or Menu button long press. Keep in mind that Side Button 1 is unavailable for macro sending when set as a keyer button. The assigned action is ignored.
  - After choosing the button menu item, from the action list pick "PLAY CW MSG1/2/3/4" to play the message one time for each button push, or pick "REPEAT CW MSG1/2/3/4" to activate a repeating playback for the given message.
- During message playback, the display will show the characters being sent, and a flashing arrow on the left side to indicate a message is being played. If Repeat mode is activated, the message will begin sending again after the delay time has expired.
- Message playback can be interrupted by tapping a keyer key or any keyboard key. This will also stop repeating.

#### `CWmrpt` - CW Message Repeat

Selects number of seconds to delay before sending the message again, when in repeat.

#### `CWbkin` - CW Break-In

Controls break-in. When OFF, RF TX is blocked but sidetone still plays so you can hear yourself key while monitoring RX. When BKIN is active, RF will send on keying. An indicator is displayed at the top of the screen (and the radio will enter TX mode and light the red LED when sending).

#### `CWcrd` - ADC Read Check (hidden menu)
**(in the hidden tech menu - turn on radio while holding PTT and SIDE1 to access)**

Shows the live ADC reading from the 3.5mm port for CEC cable tuning. Use this to measure your 10k and 20k resistors if you are having problems detecting keys properly. Enter this mode and hold down each paddle key separately to see the readings, which update every half second (holding both keys at once will show a third reading, which is not useful). Capture the two paddle readings, then set the values in the below 20k/10k menus.

#### `CWcLo` / `CWcHi` - CEC ADC thresholds (hidden menu)
**(in the hidden tech menu - turn on radio while holding PTT and SIDE1 to access)**

Set the ADC thresholds for your CEC cable. After capturing values with the Read Check, store the higher number in CWcHi, and the lower in CWcLo.

**Note:** Please file an issue if the detected values are significantly different than the defaults. Field reports may help tune these best.

#### Menu `CWkin` Input Options In Detail:

**1. PTT HandKey** (Default)
- Use PTT button as a straight key

**2. PTT+port HandKey**
- PTT button as CW key (straight key mode)
- Also accepts a straight key in the 3.5mm port (with iambic rework)

**Note:** _Each of the options below has a primary (dah/dit) and reversed (dit/dah) version - set how you prefer_

**PTT dah SD1 dit / PTT dit SD1 dah** (Buttons)
- PTT sends dah or dit
- Side button 1 (SD1) = sends dit or dah
- Automatic iambic keyer enabled (for this and all the below options)

**PTT+tip dah/dah, ring dit/dah** (Reworked Port)
- 3.5mm port TIP or PTT button sends one element
- 3.5mm port RING pin sends other element
- Requires the internal rework
- Leaves SD1 button free for other use

**PTT+tip dah/dit, SD1+ring dit/dit** (Reworked port and Buttons)
- PTT OR Tip = DAH/dit
- Side button 1 (SD1) OR Ring = DIT/dah
- Both radio buttons AND external port paddles work simultaneously

**9. CEC Cable**
- ADC-based input using a CEC cable. The PTT button is ignored in this mode.

**10. CEC Cable Reversed**
- Same as above with paddle orientation reversed.

# Prior Beta Changelists

## Changelist from beta5 -> beta6
- Monitor mode is available in CW/SSB - these modes default to monitor on / squelch open. If a button is mapped to MONITOR, it can be used to close the squelch. This works inverse to FM mode. If modulation is changed away and back to CW/SSB, squelch will be back open again (not saved).
- Slightly modified the keyer rework. It now uses a different location inside the radio for the RING connection, to avoid requiring soldering to a very small surface mount transistor leg. The rework instructions are updated accordingly. The default firmware build now follows the updated rework; for radios where the beta rework was applied use the "ring b15" firmware binary.
- Fixed a bug where changing frequency didn't actually change the transmit frequency internally
- If no additional bugs are reported, I'll re-release this version as 1.0

## Changelist from beta4 -> beta5
The "smooth operator" release - the only feature here is an improvement in audio behavior when going between RX/TX, switching frequencies, and modes like menu and keyer changes. These previously caused popping noises, and I've been able to eliminate most or all of them. After that, we're down to polishing and bugfixes; this is likely last beta before I call it 1.0.

There are no changes to the Chirp module - the one attached to this release is the same as beta4, either will work.

- Reduce or eliminate most audio pops while going between RX/TX and other operations in CW
- Correctly remove and apply keyer config when changing between modulations
- Correctly remove and apply keyer config going in/out of CPO, regardless of prior modulation
- Fix some stuck key or weird transmission states when changing modes and immediately hitting PTT or a paddle key
- Turn off all the transmission settings that don't apply to CW when in CW (compander, scramble, roger, pttid)
- Correctly test for stuck key when enabling the PTT+tip handkey mode
- Code refactor; cleaned up the CW integration so it's more modular, and use a 1ms deadline timer

## Changelist from beta3 -> beta4
Beta3 was not very healthy. If you had CEC cable mode set, and turned the radio off in a non-CW mode, it would boot in a broken repeating TX death spiral. Also, PTT handkey mode didn't even work! Fixed all that.

- CHIRP driver fields for Mode, Step, and Filter (need to check "show extra fields" to see Filter)
- Toggle Cross-mode on long-press 8 (talk to SSB operators with injected sidetone so they hear your sending and you hear them on-frequency)
- Cycle filters on long-press 7
- Don't do Roger on TX tail in CW
- Hold open squelch on SSB modulation
- Fixed beta3 TX-boot-bug
- Fixed PTT handkey mode

## Changelist from beta2 -> beta3
- CHIRP module for loading and storing all CW settings and messages (Thanks @u77345!)
- Code Practice Oscillator mode
- Available macro message count 2 -> 4 (Thanks @u77345)
- Message repeat sending, with delay setting
- Support for CEC resistor-network paddle cable
- BreakIn menu setting to disable keyer TX
- Fix: de-initialize the keyer port when switching to non-CW modulation or VFO
- Fix: (Issue #4) apply sidetone frequency change to RX upon menu confirm 
- Probably some other fixes I can't remember

## Changelist from beta1 -> beta2
- Recording and playback of macro messages
- On-screen decoder while sending via keyer
- Increased deglitch to resolve RFI-induced extra elements (extra dit when you send a dah, and vice versa)
- Fixed inconsistent sidetone volume setting
- Don't allow setting a port-based keyer mode if no rework or if keys appear stuck
- Fix keyer menu naming "sleeve" -> "ring" for port
- Main screen: show RSSI signal bar in RX mode instead of AGC debug data
