/* Copyright 2013-2015 Sathya Laufer
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

#ifndef AESHANDSHAKE_H
#define AESHANDSHAKE_H

#include "homegear-base/BaseLib.h"

#include <gcrypt.h>

namespace BidCoS
{

class BidCoSPacket;

class AesHandshake
{
	public:
		AesHandshake(BaseLib::Obj* baseLib, BaseLib::Output& out, int32_t address, std::vector<uint8_t> rfKey, std::vector<uint8_t> oldRfKey, uint32_t currentRfKeyIndex);
        virtual ~AesHandshake();

        void setMyAddress(int32_t address) { _myAddress = address; }
        void collectGarbage();
        void setMFrame(std::shared_ptr<BidCoSPacket> mFrame);
        std::shared_ptr<BidCoSPacket> getCFrame(std::shared_ptr<BidCoSPacket> mFrame);
        std::shared_ptr<BidCoSPacket> getRFrame(std::shared_ptr<BidCoSPacket> cFrame, std::shared_ptr<BidCoSPacket>& mFrame, uint32_t keyIndex);
        std::shared_ptr<BidCoSPacket> getAFrame(std::shared_ptr<BidCoSPacket> rFrame, std::shared_ptr<BidCoSPacket>& mFrame, uint32_t keyIndex);
        bool checkAFrame(std::shared_ptr<BidCoSPacket> aFrame);
        bool generateKeyChangePacket(std::shared_ptr<BidCoSPacket> keyChangeTemplate);
    private:
        class HandshakeInfo
		{
		public:
        	HandshakeInfo() {}
        	virtual ~HandshakeInfo() {}

        	std::shared_ptr<BidCoSPacket> mFrame;
        	std::shared_ptr<BidCoSPacket> cFrame;
        	std::shared_ptr<std::vector<uint8_t>> pd;
		};

        BaseLib::Obj* _bl = nullptr;
        BaseLib::Output _out;
        int32_t _myAddress = 0x1C6940;
        std::vector<uint8_t> _rfKey;
        std::vector<uint8_t> _oldRfKey;
        uint32_t _currentRfKeyIndex = 0;
        std::mutex _encryptMutex;
        std::mutex _decryptMutex;
        std::mutex _keyChangeMutex;
        gcry_cipher_hd_t _encryptHandle = nullptr;
        gcry_cipher_hd_t _encryptHandleKeyChange = nullptr;
        gcry_cipher_hd_t _decryptHandle = nullptr;
        std::mutex _handshakeInfoMutex;
        std::map<int32_t, HandshakeInfo> _handshakeInfoRequest;
        std::map<int32_t, HandshakeInfo> _handshakeInfoResponse;

        void getKey(std::vector<uint8_t>& key, uint32_t keyIndex);
};

}

#endif
