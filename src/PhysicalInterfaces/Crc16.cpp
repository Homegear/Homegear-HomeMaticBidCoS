/* Copyright 2013-2017 Sathya Laufer
 *
 * Homegear is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Homegear is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Homegear.  If not, see <http://www.gnu.org/licenses/>.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 * You must obey the GNU General Public License in all respects
 * for all of the code used other than OpenSSL.  If you modify
 * file(s) with this exception, you may extend this exception to your
 * version of the file(s), but you are not obligated to do so.  If you
 * do not wish to do so, delete this exception statement from your
 * version.  If you delete this exception statement from all source
 * files in the program, then also delete it here.
 */

#include "Crc16.h"

namespace BidCoS
{
CRC16::CRC16()
{
	if(_crcTable.empty()) initCRCTable();
}

void CRC16::initCRCTable()
{
	uint32_t bit, crc;

	for (uint32_t i = 0; i < 256; i++)
	{
		crc = i << 8;

		for (uint32_t j = 0; j < 8; j++) {

			bit = crc & 0x8000;
			crc <<= 1;
			if(bit) crc ^= 0x8005;
		}

		crc &= 0xFFFF;
		_crcTable[i]= crc;
	}
}

uint16_t CRC16::calculate(const std::vector<char>& data, bool ignoreLastTwoBytes)
{
	int32_t size = ignoreLastTwoBytes ? data.size() - 2 : data.size();
	uint16_t crc = 0xd77f;
	for(int32_t i = 0; i < size; i++)
	{
		crc = (crc << 8) ^ _crcTable[((crc >> 8) & 0xff) ^ (uint8_t)data[i]];
	}

    return crc;
}

uint16_t CRC16::calculate(const std::vector<uint8_t>& data, bool ignoreLastTwoBytes)
{
	int32_t size = ignoreLastTwoBytes ? data.size() - 2 : data.size();
	uint16_t crc = 0xd77f;
	for(int32_t i = 0; i < size; i++)
	{
		crc = (crc << 8) ^ _crcTable[((crc >> 8) & 0xff) ^ data[i]];
	}

    return crc;
}
}
