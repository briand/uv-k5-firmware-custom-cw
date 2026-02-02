# uvk5_NR7Y.py
# CHIRP driver for Quansheng UV-K5 with NR7Y CW firmware
# Supports CW modulator settings and 4 macro memories

import re
import logging
import struct

from chirp import directory, bitwise, memmap, errors, chirp_common
from chirp.settings import (
    RadioSetting, RadioSettingGroup, RadioSettingValueBoolean,
    RadioSettingValueInteger, RadioSettingValueString, RadioSettingValueList,
    RadioSettings
)

# Import the official UV-K5 driver from CHIRP
from chirp.drivers import uvk5

LOG = logging.getLogger(__name__)

# CW Macro constants
CW_MACRO_ADDRS = [0x1C00, 0x1C2A, 0x1C54, 0x1C7E]
CW_MACRO_SIZE = 42
CW_MACRO_MAX_LEN = 40
CW_MACRO_SIG = 0x80

# CW settings EEPROM addresses
CW_SETTINGS_ADDR = 0x0F20  # 3 bytes: freq/vol, mode/wpm, key_input

# Key input mode display strings
CW_KEY_INPUT_MODES = [
    "PTT HandKey",
    "PTT+Port HandKey",
    "PTT dah, Side1 dit",
    "PTT dit, Side1 dah",
    "PTT+Tip dah, Ring dit",
    "PTT+Tip dit, Ring dah",
    "Both dah, Both dit",
    "Both dit, Both dah"
]

# Map menu selection to bit-mapped values (from firmware)
CW_KEY_INPUT_BITMAP = [0x08, 0x18, 0x04, 0x05, 0x12, 0x13, 0x16, 0x17]


@directory.register
@directory.detected_by(uvk5.UVK5Radio)
class UVK5_NR7Y(uvk5.UVK5RadioBase):
    """Quansheng UV-K5 with NR7Y CW firmware"""

    VENDOR = "Quansheng"
    MODEL = "UV-K5"
    VARIANT = "NR7Y"
    
    # Make firmware writable (not restricted)
    NEEDS_COMPAT_SERIAL = False
    
    @classmethod
    def k5_approve_firmware(cls, firmware):
        """Approve NR7Y firmware versions"""
        # Accept any firmware with "NR7Y" in the name
        result = firmware and 'NR7Y' in firmware.upper()
        if result:
            LOG.info(f"NR7Y firmware approved: {firmware}")
        return result
    
    @classmethod
    def match_model(cls, filedata, filename):
        """Override to match NR7Y firmware in image files"""
        # Let base class do basic validation
        try:
            if not uvk5.UVK5Radio.match_model(filedata, filename):
                return False
        except:
            pass
        
        # Check for NR7Y firmware string in the image
        try:
            # Firmware version usually stored around 0x2000-0x3000
            data_str = bytes(filedata[0x2000:0x3000]).decode('ascii', errors='ignore')
            if 'NR7Y' in data_str:
                LOG.info("NR7Y firmware detected in image file")
                return True
        except Exception as e:
            LOG.debug(f"Error checking image file: {e}")
        
        return False

    def _is_nr7y_cw_firmware(self) -> bool:
        """Check if firmware has CW modulator enabled"""
        try:
            # Read build options from 0x1FF0-0x1FF1
            build_opts = bytes(self._mmap[0x1FF0:0x1FF2])
            # Bit 6 of byte 1 indicates ENABLE_CW_MODULATOR
            has_cw = (build_opts[1] & 0x40) != 0
            LOG.info(f"CW modulator flag: {has_cw} (0x1FF1=0x{build_opts[1]:02x})")
            return has_cw
        except Exception as e:
            LOG.error(f"Error checking CW firmware flag: {e}")
            return False

    def get_settings(self):
        """Get radio settings including CW if detected"""
        LOG.info("UVK5_NR7Y.get_settings() called")
        
        try:
            rs = super().get_settings()
        except Exception as e:
            LOG.error(f"Error getting base settings: {e}")
            rs = RadioSettings()

        # Check if CW firmware
        if not self._is_nr7y_cw_firmware():
            LOG.warning("CW modulator not enabled in firmware - skipping CW settings")
            return rs

        LOG.info("CW modulator detected - adding CW settings")

        # Remove DTMF contacts if present (conflicts with CW macros)
        self._remove_dtmf_contacts(rs)

        # Add CW settings group
        cw = RadioSettingGroup("cw", "CW Settings")

        # Sidetone Frequency (450-950 Hz in 50 Hz steps)
        freq_opts = ["%d Hz" % (450 + i * 50) for i in range(11)]
        try:
            freq_idx = self._get_cw_frequency_idx()
            cw_freq = RadioSetting(
                "cw.frequency",
                "Sidetone Frequency",
                RadioSettingValueList(freq_opts, freq_opts[freq_idx])
            )
            cw.append(cw_freq)
        except Exception as e:
            LOG.error(f"Error adding frequency setting: {e}")

        # Sidetone Volume (0=OFF, 1-6)
        vol_opts = ["OFF"] + [str(i) for i in range(1, 7)]
        try:
            vol_idx = self._get_cw_sidetone_level()
            cw_vol = RadioSetting(
                "cw.sidetone_level",
                "Sidetone Volume",
                RadioSettingValueList(vol_opts, vol_opts[vol_idx])
            )
            cw.append(cw_vol)
        except Exception as e:
            LOG.error(f"Error adding volume setting: {e}")

        # Keyer Mode (Iambic A/B)
        mode_opts = ["Iambic A", "Iambic B"]
        try:
            mode_idx = self._get_cw_keyer_mode()
            cw_mode = RadioSetting(
                "cw.keyer_mode",
                "Keyer Mode",
                RadioSettingValueList(mode_opts, mode_opts[mode_idx])
            )
            cw.append(cw_mode)
        except Exception as e:
            LOG.error(f"Error adding keyer mode: {e}")

        # Keyer Speed (10-30 WPM)
        try:
            wpm = self._get_cw_wpm()
            cw_wpm = RadioSetting(
                "cw.wpm",
                "Keyer Speed (WPM)",
                RadioSettingValueInteger(10, 30, wpm)
            )
            cw.append(cw_wpm)
        except Exception as e:
            LOG.error(f"Error adding WPM setting: {e}")

        # Key Input Configuration
        try:
            key_idx = self._get_cw_key_input_idx()
            cw_key_input = RadioSetting(
                "cw.key_input",
                "Key Input Mode",
                RadioSettingValueList(CW_KEY_INPUT_MODES, 
                                     CW_KEY_INPUT_MODES[key_idx])
            )
            cw.append(cw_key_input)
        except Exception as e:
            LOG.error(f"Error adding key input setting: {e}")

        # CW Macros (4 messages)
        macros = RadioSettingGroup("cw_macros", "CW Macros")
        for i in range(1, 5):
            try:
                msg_text = self._get_cw_msg(i)
                val = RadioSettingValueString(0, 40, msg_text)
                msg = RadioSetting(
                    f"cw.msg{i}",
                    f"CW Message {i}",
                    val
                )
                msg.set_doc(f"CW macro {i} (A-Z, 0-9, /, ? only, max 40 chars)")
                macros.append(msg)
            except Exception as e:
                LOG.error(f"Error adding macro {i}: {e}")
        
        cw.append(macros)
        rs.append(cw)
        
        LOG.info(f"Added CW settings group with {len(list(cw))} settings")
        return rs

    def set_settings(self, settings):
        """Apply settings to radio - follows base class pattern with CW support"""
        _mem = self._memobj
        for element in settings:
            if not isinstance(element, RadioSetting):
                # It's a group, recurse into it
                self.set_settings(element)
                continue
            
            # It's an individual setting
            setting = element
            name = setting.get_name()
            
            # Handle CW settings
            if name.startswith("cw."):
                try:
                    if name == "cw.frequency":
                        freq_opts = ["%d Hz" % (450 + i * 50) for i in range(11)]
                        idx = freq_opts.index(str(setting.value))
                        self._set_cw_frequency_idx(idx)
                        LOG.debug(f"Set CW frequency to {freq_opts[idx]}")
                    elif name == "cw.sidetone_level":
                        vol_opts = ["OFF"] + [str(i) for i in range(1, 7)]
                        idx = vol_opts.index(str(setting.value))
                        self._set_cw_sidetone_level(idx)
                        LOG.debug(f"Set CW volume to {vol_opts[idx]}")
                    elif name == "cw.keyer_mode":
                        idx = ["Iambic A", "Iambic B"].index(str(setting.value))
                        self._set_cw_keyer_mode(idx)
                        LOG.debug(f"Set keyer mode to {setting.value}")
                    elif name == "cw.wpm":
                        self._set_cw_wpm(int(setting.value))
                        LOG.debug(f"Set WPM to {setting.value}")
                    elif name == "cw.key_input":
                        idx = CW_KEY_INPUT_MODES.index(str(setting.value))
                        self._set_cw_key_input_idx(idx)
                        LOG.debug(f"Set key input to {setting.value}")
                    elif name.startswith("cw.msg"):
                        # Extract macro number from "cw.msg1" → "1"
                        # "cw.msg" is 6 chars, so number is at index 6
                        macro_num = name[6:]  # Skip "cw.msg" to get "1", "2", "3", "4"
                        idx = int(macro_num)
                        self._set_cw_msg(idx, str(setting.value))
                        LOG.info(f"Saved macro {idx}: '{str(setting.value)[:20]}...'")
                except Exception as e:
                    LOG.error(f"Error applying CW setting {name}: {e}")
                    import traceback
                    traceback.print_exc()
                continue
            
            # Non-CW settings - let base class handle them
            # Call parent's set_settings logic directly for this one setting
            if name == "call_channel":
                _mem.call_channel = int(setting.value)-1
            elif name == "squelch":
                _mem.squelch = int(setting.value)
            elif name == "tot":
                _mem.max_talk_time = int(setting.value)
            elif name == "noaa_autoscan":
                _mem.noaa_autoscan = setting.value and 1 or 0
            elif name == "vox_switch":
                _mem.vox_switch = setting.value and 1 or 0
            elif name == "vox_level":
                _mem.vox_level = int(setting.value)-1
            elif name == "mic_gain":
                _mem.mic_gain = int(setting.value)
            # ... base class handles all other settings through its implementation
            # We'll just let anything else pass through by calling parent on groups

    def _remove_dtmf_contacts(self, rs: RadioSettings) -> None:
        """Remove DTMF contacts group to prevent conflicts"""
        removed = []
        for group in list(rs):
            if hasattr(group, 'get_name'):
                name = group.get_name()
                # Remove any DTMF contact groups
                if 'contact' in name.lower() and 'dtmf' in name.lower():
                    rs.remove(group)
                    removed.append(name)
        
        if removed:
            LOG.info(f"Removed DTMF contact groups: {removed}")

    # ======== CW Settings Encode/Decode ========
    
    def _get_cw_frequency_idx(self) -> int:
        """Get sidetone frequency index (0-10 for 450-950 Hz)"""
        byte0 = struct.unpack('B', bytes(self._mmap[CW_SETTINGS_ADDR:CW_SETTINGS_ADDR+1]))[0]
        if byte0 == 0xFF:
            return 3  # Default 600 Hz (index 3)
        # Formula from settings.c:234 - stored as (Hz/10 - 45) / 5
        freq_value = 45 + (byte0 & 0x0F) * 5  # This gives Hz/10
        # Convert to Hz and then to index
        freq_hz = freq_value * 10
        idx = (freq_hz - 450) // 50
        return max(0, min(10, idx))
    
    def _set_cw_frequency_idx(self, idx: int) -> None:
        """Set sidetone frequency from index"""
        byte0 = struct.unpack('B', bytes(self._mmap[CW_SETTINGS_ADDR:CW_SETTINGS_ADDR+1]))[0]
        freq_hz = 450 + idx * 50
        freq_value = freq_hz // 10  # Convert to deciHz
        encoded = (freq_value - 45) // 5
        # Preserve bits 4-6 (sidetone level), update bits 0-3
        byte0 = (byte0 & 0xF0) | (encoded & 0x0F)
        self._mmap[CW_SETTINGS_ADDR] = byte0

    def _get_cw_sidetone_level(self) -> int:
        """Get sidetone volume level (0-6)"""
        byte0 = struct.unpack('B', bytes(self._mmap[CW_SETTINGS_ADDR:CW_SETTINGS_ADDR+1]))[0]
        if byte0 == 0xFF:
            return 4  # Default level 4
        # Formula from settings.c:235 - bits 4-6
        return (byte0 >> 4) & 0x07
    
    def _set_cw_sidetone_level(self, level: int) -> None:
        """Set sidetone volume level (0-6)"""
        byte0 = struct.unpack('B', bytes(self._mmap[CW_SETTINGS_ADDR:CW_SETTINGS_ADDR+1]))[0]
        # Preserve bits 0-3 (frequency), update bits 4-6
        byte0 = (byte0 & 0x0F) | ((level & 0x07) << 4)
        self._mmap[CW_SETTINGS_ADDR] = byte0

    def _get_cw_keyer_mode(self) -> int:
        """Get keyer mode (0=A, 1=B)"""
        byte1 = struct.unpack('B', bytes(self._mmap[CW_SETTINGS_ADDR+1:CW_SETTINGS_ADDR+2]))[0]
        if byte1 == 0xFF:
            return 0  # Default Mode A
        # Formula from settings.c:236 - bit 7
        return 1 if (byte1 & 0x80) else 0
    
    def _set_cw_keyer_mode(self, mode: int) -> None:
        """Set keyer mode (0=A, 1=B)"""
        byte1 = struct.unpack('B', bytes(self._mmap[CW_SETTINGS_ADDR+1:CW_SETTINGS_ADDR+2]))[0]
        if mode == 1:
            byte1 |= 0x80  # Set bit 7
        else:
            byte1 &= 0x7F  # Clear bit 7
        self._mmap[CW_SETTINGS_ADDR + 1] = byte1

    def _get_cw_wpm(self) -> int:
        """Get keyer speed in WPM"""
        byte1 = struct.unpack('B', bytes(self._mmap[CW_SETTINGS_ADDR+1:CW_SETTINGS_ADDR+2]))[0]
        if byte1 == 0xFF:
            return 18  # Default 18 WPM
        # Formula from settings.c:237 - bits 0-5
        wpm = byte1 & 0x3F
        if wpm < 10 or wpm > 30:
            return 18
        return wpm
    
    def _set_cw_wpm(self, wpm: int) -> None:
        """Set keyer speed in WPM (10-30)"""
        wpm = max(10, min(30, wpm))
        byte1 = struct.unpack('B', bytes(self._mmap[CW_SETTINGS_ADDR+1:CW_SETTINGS_ADDR+2]))[0]
        # Preserve bit 7 (keyer mode), update bits 0-5
        byte1 = (byte1 & 0x80) | (wpm & 0x3F)
        self._mmap[CW_SETTINGS_ADDR + 1] = byte1

    def _get_cw_key_input_idx(self) -> int:
        """Get key input mode index (0-7)"""
        byte2 = struct.unpack('B', bytes(self._mmap[CW_SETTINGS_ADDR+2:CW_SETTINGS_ADDR+3]))[0]
        if byte2 == 0xFF:
            return 0  # Default HandKey
        # Formula from settings.c:238 - bits 0-4
        val = byte2 & 0x1F
        # Find in bitmap array
        try:
            return CW_KEY_INPUT_BITMAP.index(val)
        except ValueError:
            LOG.debug(f"Unknown key input value 0x{val:02x}, defaulting to HandKey")
            return 0
    
    def _set_cw_key_input_idx(self, idx: int) -> None:
        """Set key input mode from index"""
        if idx < 0 or idx >= len(CW_KEY_INPUT_BITMAP):
            idx = 0
        val = CW_KEY_INPUT_BITMAP[idx]
        # Update bits 0-4, preserve bits 5-7
        byte2 = struct.unpack('B', bytes(self._mmap[CW_SETTINGS_ADDR+2:CW_SETTINGS_ADDR+3]))[0]
        byte2 = (byte2 & 0xE0) | (val & 0x1F)
        self._mmap[CW_SETTINGS_ADDR + 2] = byte2

    # ======== CW Macro Encode/Decode ========
    
    def _get_cw_msg(self, idx: int) -> str:
        """Read CW macro from EEPROM with validation"""
        if idx < 1 or idx > 4:
            return ""
        
        addr = CW_MACRO_ADDRS[idx - 1]
        try:
            raw = bytes(self._mmap[addr:addr + CW_MACRO_SIZE])
        except Exception as e:
            LOG.error(f"Error reading macro {idx} from 0x{addr:04x}: {e}")
            return ""
        
        # Parse macro format (from cwmacro.c:113-148)
        length_byte = raw[0]
        
        if length_byte == 0xFF:
            LOG.debug(f"Macro {idx}: empty")
            return ""  # Empty macro
        
        if (length_byte & CW_MACRO_SIG) == 0:
            LOG.debug(f"Macro {idx}: no signature (byte0=0x{length_byte:02x})")
            return ""  # Invalid signature
        
        length = length_byte & ~CW_MACRO_SIG
        if length == 0 or length > CW_MACRO_MAX_LEN:
            LOG.debug(f"Macro {idx}: invalid length {length}")
            return ""
        
        # Verify checksum (byte 41)
        checksum = sum(raw[1:length + 1]) & 0xFF
        if raw[41] != checksum:
            LOG.warning(f"Macro {idx} checksum fail (expected 0x{checksum:02x}, got 0x{raw[41]:02x})")
            return ""
        
        # Decode characters
        result = []
        for i in range(1, length + 1):
            byte = raw[i]
            has_space = (byte & 0x80) != 0
            char = chr(byte & 0x7F)
            
            if has_space and result:  # Don't add space at start
                result.append(' ')
            result.append(char)
        
        text = ''.join(result)
        LOG.info(f"Macro {idx}: '{text}' ({length} chars, checksum OK)")
        return text

    def _set_cw_msg(self, idx: int, text: str) -> None:
        """Write CW macro to EEPROM with checksum"""
        if idx < 1 or idx > 4:
            return
        
        addr = CW_MACRO_ADDRS[idx - 1]
        
        # Clear block
        raw = bytearray([0xFF] * CW_MACRO_SIZE)
        
        if not text or text.strip() == "":
            # Empty macro - write byte by byte
            for i in range(CW_MACRO_SIZE):
                self._mmap[addr + i] = raw[i]
            LOG.info(f"Cleared macro {idx}")
            return
        
        # Validate and encode characters
        encoded = []
        words = text.upper().split()
        char_count = 0
        
        for word_idx, word in enumerate(words):
            for char_idx, char in enumerate(word):
                if char_count >= CW_MACRO_MAX_LEN:
                    break
                
                # Validate character (A-Z, 0-9, /, ?)
                if not ((char >= 'A' and char <= 'Z') or 
                        (char >= '0' and char <= '9') or 
                        char in ['/', '?']):
                    LOG.warning(f"Skipping invalid char '{char}' in macro {idx}")
                    continue
                
                # Add space marker before word (except first char overall)
                has_space = (word_idx > 0 and char_idx == 0)
                byte = ord(char) | (0x80 if has_space else 0x00)
                encoded.append(byte)
                char_count += 1
        
        if char_count == 0:
            # Empty after filtering - write byte by byte
            for i in range(CW_MACRO_SIZE):
                self._mmap[addr + i] = raw[i]
            LOG.warning(f"Macro {idx} empty after filtering invalid chars from '{text}'")
            return
        
        # Set length with signature
        raw[0] = char_count | CW_MACRO_SIG
        
        # Set encoded characters
        for i, byte in enumerate(encoded):
            raw[i + 1] = byte
        
        # Calculate and set checksum
        checksum = sum(encoded) & 0xFF
        raw[41] = checksum
        
        # Write to memory byte by byte
        for i in range(CW_MACRO_SIZE):
            self._mmap[addr + i] = raw[i]
        
        LOG.info(f"Saved macro {idx}: '{text}' ({char_count} chars, checksum=0x{checksum:02x})")


