/* Copyright 2013-2016 Sathya Laufer
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

#ifndef BIDCOSMESSAGE_H
#define BIDCOSMESSAGE_H

#include <homegear-base/BaseLib.h>
#include "BidCoSPacket.h"

#include <iostream>
#include <vector>
#include <map>
#include <memory>

namespace BidCoS
{

class HomeMaticCentral;
class BidCoSQueue;

enum MessageAccess { NOACCESS = 0x00, ACCESSPAIREDTOSENDER = 0x01, ACCESSDESTISME = 0x02, ACCESSCENTRAL = 0x04, ACCESSUNPAIRING = 0x08, FULLACCESS = 0x80 };

class BidCoSMessage
{
    public:
        BidCoSMessage();
        BidCoSMessage(int32_t messageType, int32_t access, void (HomeMaticCentral::*messageHandler)(int32_t, std::shared_ptr<BidCoSPacket>));
        BidCoSMessage(int32_t messageType, int32_t access, int32_t accessPairing, void (HomeMaticCentral::*messageHandler)(int32_t, std::shared_ptr<BidCoSPacket>));
        virtual ~BidCoSMessage();

        int32_t getMessageType() { return _messageType; }
        void setMessageType(int32_t messageType) { _messageType = messageType; }
        int32_t getMessageAccess() { return _access; }
        void setMessageAccess(int32_t access) { _access = access; }
        int32_t getMessageAccessPairing() { return _accessPairing; }
        void setMessageAccessPairing(int32_t accessPairing) { _accessPairing = accessPairing; }
        void invokeMessageHandler(std::shared_ptr<BidCoSPacket> packet);
        bool checkAccess(std::shared_ptr<BidCoSPacket> packet, std::shared_ptr<BidCoSQueue> queue);
        void setMessageCounter(std::shared_ptr<BidCoSPacket> packet);
        bool typeIsEqual(std::shared_ptr<BidCoSPacket> packet);
        bool typeIsEqual(std::shared_ptr<BidCoSMessage> message, std::shared_ptr<BidCoSPacket> packet);
        bool typeIsEqual(std::shared_ptr<BidCoSMessage> message);
        bool typeIsEqual(int32_t messageType);
    protected:
        int32_t _messageType = -1;
        int32_t _access = 0;
        int32_t _accessPairing = 0;
        void (HomeMaticCentral::*_messageHandler)(int32_t, std::shared_ptr<BidCoSPacket>) = nullptr;
    private:
};
}
#endif // BIDCOSMESSAGE_H
