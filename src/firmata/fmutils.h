// QFirmata - a Firmata library for QML
//
// Copyright 2016 - Calle Laakkonen
// 
// QFirmata is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// QFirmata is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Foobar.  If not, see <http://www.gnu.org/licenses/>.

#ifndef FIRMATA_UTILS_H
#define FIRMATA_UTILS_H

//! Return bits 0..6 of a 14 bit value
inline uint8_t lsb14(uint16_t v) { return v & 0x7f; }

//! Return bits 6..12 of a 14 bit value
inline uint8_t msb14(uint16_t v) { return (v>>7) & 0x7f; }

//! Extract a 14bit integer from 2 7-bit bytes
inline uint16_t unpack14(const uint8_t *data)
{
	return
		((*(data+0) & 0x7f)<<0) |
		((*(data+1) & 0x7f)<<7)
		;
}

//! Extract a 28bit integer from 4 7-bit bytes
inline uint32_t unpack28(const uint8_t *data)
{
	return
		((*(data+0) & 0x7f)<<0) |
		((*(data+1) & 0x7f)<<7) |
		((*(data+2) & 0x7f)<<14) |
		((*(data+3) & 0x7f)<<21)
		;
}

#endif

