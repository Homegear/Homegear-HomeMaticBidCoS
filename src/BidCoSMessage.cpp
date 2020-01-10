/* Copyright 2013-2019 Homegear GmbH
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

#include "BidCoSMessage.h"
#include "HomeMaticCentral.h"
#include "GD.h"

namespace BidCoS
{
BidCoSMessage::BidCoSMessage()
{
}

BidCoSMessage::BidCoSMessage(int32_t messageType, int32_t access, void (HomeMaticCentral::*messageHandler)(const std::string&, int32_t, std::shared_ptr<BidCoSPacket>)) : _messageType(messageType), _access(access), _messageHandler(messageHandler)
{
}

BidCoSMessage::BidCoSMessage(int32_t messageType, int32_t access, int32_t accessPairing, void (HomeMaticCentral::*messageHandler)(const std::string&, int32_t, std::shared_ptr<BidCoSPacket>)) : _messageType(messageType), _access(access), _accessPairing(accessPairing), _messageHandler(messageHandler)
{
}

BidCoSMessage::~BidCoSMessage()
{
}

void BidCoSMessage::invokeMessageHandler(const std::string& interfaceId, const std::shared_ptr<BidCoSPacket>& packet)
{
	try
	{
		std::shared_ptr<HomeMaticCentral> central(std::dynamic_pointer_cast<HomeMaticCentral>(GD::family->getCentral()));
		if(!central || _messageHandler == nullptr || packet == nullptr) return;
		((central.get())->*(_messageHandler))(interfaceId, packet->messageCounter(), packet);
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

bool BidCoSMessage::typeIsEqual(int32_t messageType)
{
	try
	{
		if(_messageType == -1) return true; //Match any
		if(_messageType != messageType) return false;
		return true;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return false;
}

bool BidCoSMessage::typeIsEqual(std::shared_ptr<BidCoSPacket> packet)
{
	try
	{
		if(_messageType != packet->messageType()) return false;
		return true;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return false;
}

bool BidCoSMessage::typeIsEqual(std::shared_ptr<BidCoSMessage> message)
{
	try
	{
		if(_messageType != message->getMessageType()) return false;
		return true;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return false;
}

bool BidCoSMessage::typeIsEqual(std::shared_ptr<BidCoSMessage> message, std::shared_ptr<BidCoSPacket> packet)
{
	try
	{
		if(message->getMessageType() != packet->messageType()) return false;
		return true;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return false;
}

bool BidCoSMessage::checkAccess(std::shared_ptr<BidCoSPacket> packet, std::shared_ptr<BidCoSQueue> queue)
{
	try
	{
		std::shared_ptr<HomeMaticCentral> central(std::dynamic_pointer_cast<HomeMaticCentral>(GD::family->getCentral()));
		if(!central || !packet) return false;

		int32_t access = central->isInPairingMode() ? _accessPairing : _access;
		if(access == NOACCESS) return false;
		if(queue && !queue->isEmpty() && packet->destinationAddress() == central->getAddress())
		{
			if(queue->front()->getType() == QueueEntryType::PACKET || (queue->front()->getType() == QueueEntryType::MESSAGE && !typeIsEqual(queue->front()->getMessage())))
			{
				//queue->pop(); //Popping takes place here to be able to process resent messages.
				BidCoSQueueEntry* entry = queue->second();
				if(entry && entry->getType() == QueueEntryType::MESSAGE && !typeIsEqual(entry->getMessage())) return false;
                if(packet->messageType() == 2 && packet->payload().size() == 1 && packet->payload().at(0) == 0x80)
                {
                    GD::out.printWarning("Warning: NACK received from 0x" + BaseLib::HelperFunctions::getHexString(packet->senderAddress(), 6) + ". Popping from queue anyway. If the device doesn't seem to work, please reset it to factory defaults and pair it again to Homegear.");
                }
				queue->pop();
			}
		}
		if(access & FULLACCESS) return true;
		if((access & ACCESSDESTISME) && packet->destinationAddress() != central->getAddress())
		{
			//GD::out.printMessage( "Access denied, because the destination address is not me: " << packet->hexString() << std::endl;
			return false;
		}
		if((access & ACCESSUNPAIRING) && queue != nullptr && queue->getQueueType() == BidCoSQueueType::UNPAIRING)
		{
			return true;
		}
		if(access & ACCESSPAIREDTOSENDER)
		{
			std::shared_ptr<BidCoSPeer> currentPeer;
			if(central->isInPairingMode() && queue && queue->peer && queue->peer->getAddress() == packet->senderAddress()) currentPeer = queue->peer;
			if(!currentPeer) currentPeer = central->getPeer(packet->senderAddress());
			if(!currentPeer) return false;
		}
		if((access & ACCESSCENTRAL) && central->getAddress() != packet->senderAddress())
		{
			//GD::out.printMessage( "Access denied, because it is only granted to a paired central: " << packet->hexString() << std::endl;
			return false;
		}
		return true;
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return false;
}
}
