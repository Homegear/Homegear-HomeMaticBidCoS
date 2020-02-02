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

#include "HomeMaticCentral.h"
#include "PendingBidCoSQueues.h"
#include <homegear-base/BaseLib.h>
#include "GD.h"
#include "VirtualPeers/HmCcTc.h"

namespace BidCoS
{

HomeMaticCentral::HomeMaticCentral(ICentralEventSink* eventHandler) : BaseLib::Systems::ICentral(BIDCOS_FAMILY_ID, GD::bl, eventHandler)
{
	init();
}

HomeMaticCentral::HomeMaticCentral(uint32_t deviceId, std::string serialNumber, int32_t address, ICentralEventSink* eventHandler) : BaseLib::Systems::ICentral(BIDCOS_FAMILY_ID, GD::bl, deviceId, serialNumber, address, eventHandler)
{
	init();
}

HomeMaticCentral::~HomeMaticCentral()
{
	try
	{
		dispose();
	}
    catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::dispose(bool wait)
{
	try
	{
		if(_disposing) return;
		_disposing = true;

		stopThreads();

		_bidCoSQueueManager.dispose(false);
		_receivedPackets.dispose(false);
		_sentPackets.dispose(false);

		_peersMutex.lock();
		for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::const_iterator i = _peersById.begin(); i != _peersById.end(); ++i)
		{
			try
			{
				i->second->dispose();
			}
			catch(const std::exception& ex)
			{
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
		}
		_peersMutex.unlock();

		GD::out.printDebug("Removing device " + std::to_string(_deviceId) + " from physical device's event queue...");
		for(std::map<std::string, std::shared_ptr<IBidCoSInterface>>::iterator i = GD::physicalInterfaces.begin(); i != GD::physicalInterfaces.end(); ++i)
		{
			//Just to make sure cycle through all physical devices. If event handler is not removed => segfault
			i->second->removeEventHandler(_physicalInterfaceEventhandlers[i->first]);
		}
	}
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::stopThreads()
{
	try
	{
		_bl->threadManager.join(_resetThread);

		_sendPacketThreadMutex.lock();
		_bl->threadManager.join(_sendPacketThread);
		_sendPacketThreadMutex.unlock();

		_sendMultiplePacketsThreadMutex.lock();
		_bl->threadManager.join(_sendMultiplePacketsThread);
		_sendMultiplePacketsThreadMutex.unlock();

		_pairingModeThreadMutex.lock();
		_stopPairingModeThread = true;
		_bl->threadManager.join(_pairingModeThread);
		_pairingModeThreadMutex.unlock();

		_updateFirmwareThreadMutex.lock();
		_bl->threadManager.join(_updateFirmwareThread);
		_updateFirmwareThreadMutex.unlock();

		_stopWorkerThread = true;
		GD::out.printDebug("Debug: Waiting for worker thread of device " + std::to_string(_deviceId) + "...");
		_bl->threadManager.join(_workerThread);
	}
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::init()
{
	try
	{
		if(_initialized) return; //Prevent running init two times
		_initialized = true;

		_stopWorkerThread = false;
		_pairing = false;
		_stopPairingModeThread = false;
		_updateMode = false;

		_messages = std::shared_ptr<BidCoSMessages>(new BidCoSMessages());
		_messageCounter[0] = 0; //Broadcast message counter

		setUpBidCoSMessages();

		for(std::map<std::string, std::shared_ptr<IBidCoSInterface>>::iterator i = GD::physicalInterfaces.begin(); i != GD::physicalInterfaces.end(); ++i)
		{
			_physicalInterfaceEventhandlers[i->first] = i->second->addEventHandler((BaseLib::Systems::IPhysicalInterface::IPhysicalInterfaceEventSink*)this);
		}

		_bl->threadManager.start(_workerThread, true, _bl->settings.workerThreadPriority(), _bl->settings.workerThreadPolicy(), &HomeMaticCentral::worker, this);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::setUpBidCoSMessages()
{
	try
	{
		_messages->add(std::make_shared<BidCoSMessage>(0x00, ACCESSPAIREDTOSENDER, FULLACCESS, &HomeMaticCentral::handlePairingRequest));

		_messages->add(std::make_shared<BidCoSMessage>(0x02, ACCESSPAIREDTOSENDER | ACCESSDESTISME, ACCESSPAIREDTOSENDER | ACCESSDESTISME, &HomeMaticCentral::handleAck));

		_messages->add(std::make_shared<BidCoSMessage>(0x10, ACCESSPAIREDTOSENDER | ACCESSDESTISME, ACCESSPAIREDTOSENDER | ACCESSDESTISME, &HomeMaticCentral::handleConfigParamResponse));

		_messages->add(std::make_shared<BidCoSMessage>(0x3F, ACCESSPAIREDTOSENDER | ACCESSDESTISME, ACCESSPAIREDTOSENDER | ACCESSDESTISME, &HomeMaticCentral::handleTimeRequest));
	}
    catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::loadPeers()
{
	try
	{
		std::shared_ptr<BaseLib::Database::DataTable> rows = _bl->db->getPeers(_deviceId);
		for(BaseLib::Database::DataTable::iterator row = rows->begin(); row != rows->end(); ++row)
		{
			int32_t peerId = row->second.at(0)->intValue;
			GD::out.printMessage("Loading peer " + std::to_string(peerId));
			int32_t address = row->second.at(2)->intValue;
			std::string serialNumber = row->second.at(3)->textValue;
			std::shared_ptr<BidCoSPeer> peer;
			if(serialNumber.substr(0, 3) == "VCD")
			{
				if(row->second.at(4)->intValue == (uint32_t)DeviceType::HMCCTC)
				{
					GD::out.printMessage("Peer is virtual.");
					peer.reset(new HmCcTc(peerId, address, serialNumber, _deviceId, this));
				}
				else
				{
					GD::out.printError("Error: Unknown virtual HM-CC-TC: 0x" + BaseLib::HelperFunctions::getHexString(row->second.at(4)->intValue));
					continue;
				}
			}
			else peer.reset(new BidCoSPeer(peerId, address, row->second.at(3)->textValue, _deviceId, this));
			if(!peer->load(this)) continue;
			PHomegearDevice rpcDevice = peer->getRpcDevice();
			if(!rpcDevice) continue;
			_peersMutex.lock();
			if(peer->getAddress() != _address) _peers[peer->getAddress()] = peer;
			if(!peer->getSerialNumber().empty()) _peersBySerial[peer->getSerialNumber()] = peer;
			_peersById[peerId] = peer;
			_peersMutex.unlock();
			peer->getPhysicalInterface()->addPeer(peer->getPeerInfo());
			if(!peer->getTeamRemoteSerialNumber().empty())
			{
				_peersMutex.lock();
				if(_peersBySerial.find(peer->getTeamRemoteSerialNumber()) == _peersBySerial.end())
				{
					std::shared_ptr<BidCoSPeer> team = createTeam(peer->getTeamRemoteAddress(), peer->getDeviceType(), peer->getTeamRemoteSerialNumber());
					team->setRpcDevice(rpcDevice->group);
					team->initializeCentralConfig();
					team->setID(peer->getID() | (1 << 30));
					team->setInterface(nullptr, peer->getPhysicalInterfaceID());
					_peersBySerial[team->getSerialNumber()] = team;
					_peersById[team->getID()] = team;
				}
				_peersMutex.unlock();
				for(Functions::iterator i = rpcDevice->functions.begin(); i != rpcDevice->functions.end(); ++i)
				{
					if(i->second->hasGroup)
					{
						getPeer(peer->getTeamRemoteSerialNumber())->teamChannels.push_back(std::pair<std::string, uint32_t>(peer->getSerialNumber(), peer->getTeamRemoteChannel()));
						break;
					}
				}
			}
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    	_peersMutex.unlock();
    }
}

void HomeMaticCentral::saveVariables()
{
	try
	{
		if(_deviceId == 0) return;
		saveMessageCounters(); //2
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::loadVariables()
{
	try
	{
		std::shared_ptr<BaseLib::Database::DataTable> rows = _bl->db->getDeviceVariables(_deviceId);
		for(BaseLib::Database::DataTable::iterator row = rows->begin(); row != rows->end(); ++row)
		{
			_variableDatabaseIds[row->second.at(2)->intValue] = row->second.at(0)->intValue;
			switch(row->second.at(2)->intValue)
			{
			case 2:
				unserializeMessageCounters(row->second.at(5)->binaryValue);
				break;
			}
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

uint8_t HomeMaticCentral::getMessageCounter()
{
	_messageCounter[0]++;
	return _messageCounter[0];
}

void HomeMaticCentral::saveMessageCounters()
{
	try
	{
		std::vector<uint8_t> serializedData;
		serializeMessageCounters(serializedData);
		saveVariable(2, serializedData);
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::serializeMessageCounters(std::vector<uint8_t>& encodedData)
{
	try
	{
		BaseLib::BinaryEncoder encoder(_bl);
		encoder.encodeInteger(encodedData, _messageCounter.size());
		for(std::unordered_map<int32_t, uint8_t>::const_iterator i = _messageCounter.begin(); i != _messageCounter.end(); ++i)
		{
			encoder.encodeInteger(encodedData, i->first);
			encoder.encodeByte(encodedData, i->second);
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::unserializeMessageCounters(std::shared_ptr<std::vector<char>> serializedData)
{
	try
	{
		BaseLib::BinaryDecoder decoder(_bl);
		uint32_t position = 0;
		uint32_t messageCounterSize = decoder.decodeInteger(*serializedData, position);
		for(uint32_t i = 0; i < messageCounterSize; i++)
		{
			int32_t index = decoder.decodeInteger(*serializedData, position);
			_messageCounter[index] = decoder.decodeByte(*serializedData, position);
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::savePeers(bool full)
{
	try
	{
		_peersMutex.lock();
		for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
		{
			if(i->second->isTeam()) continue;
			//We are always printing this, because the init script needs it
			GD::out.printMessage("(Shutdown) => Saving HomeMatic BidCoS peer " + std::to_string(i->second->getID()) + " with address 0x" + BaseLib::HelperFunctions::getHexString(i->second->getAddress(), 6));
			i->second->save(full, full, full);
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
	_peersMutex.unlock();
}

std::shared_ptr<BidCoSPeer> HomeMaticCentral::getPeer(int32_t address)
{
	try
	{
		_peersMutex.lock();
		if(_peers.find(address) != _peers.end())
		{
			std::shared_ptr<BidCoSPeer> peer(std::dynamic_pointer_cast<BidCoSPeer>(_peers.at(address)));
			_peersMutex.unlock();
			return peer;
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    _peersMutex.unlock();
    return std::shared_ptr<BidCoSPeer>();
}

std::shared_ptr<BidCoSPeer> HomeMaticCentral::getPeer(uint64_t id)
{
	try
	{
		_peersMutex.lock();
		if(_peersById.find(id) != _peersById.end())
		{
			std::shared_ptr<BidCoSPeer> peer(std::dynamic_pointer_cast<BidCoSPeer>(_peersById.at(id)));
			_peersMutex.unlock();
			return peer;
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    _peersMutex.unlock();
    return std::shared_ptr<BidCoSPeer>();
}

std::shared_ptr<BidCoSPeer> HomeMaticCentral::getPeer(std::string serialNumber)
{
	try
	{
		_peersMutex.lock();
		if(_peersBySerial.find(serialNumber) != _peersBySerial.end())
		{
			std::shared_ptr<BidCoSPeer> peer(std::dynamic_pointer_cast<BidCoSPeer>(_peersBySerial.at(serialNumber)));
			_peersMutex.unlock();
			return peer;
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    _peersMutex.unlock();
    return std::shared_ptr<BidCoSPeer>();
}

std::shared_ptr<BidCoSQueue> HomeMaticCentral::getQueue(int32_t address)
{
	return _bidCoSQueueManager.get(address);
}

bool HomeMaticCentral::isSwitch(uint32_t type)
{
	switch((DeviceType)type)
	{
	case DeviceType::HMESPMSW1PL:
		return true;
	case DeviceType::HMLCSW1PL:
		return true;
	case DeviceType::HMLCSW1PL2:
		return true;
	case DeviceType::HMLCSW1SM:
		return true;
	case DeviceType::HMLCSW2SM:
		return true;
	case DeviceType::HMLCSW4SM:
		return true;
	case DeviceType::HMLCSW4PCB:
		return true;
	case DeviceType::HMLCSW4WM:
		return true;
	case DeviceType::HMLCSW1FM:
		return true;
	case DeviceType::HMLCSWSCHUECO:
		return true;
	case DeviceType::HMLCSWSCHUECO2:
		return true;
	case DeviceType::HMLCSW2FM:
		return true;
	case DeviceType::HMLCSW1PBFM:
		return true;
	case DeviceType::HMLCSW2PBFM:
		return true;
	case DeviceType::HMLCSW4DR:
		return true;
	case DeviceType::HMLCSW2DR:
		return true;
	case DeviceType::HMLCSW1PBUFM:
		return true;
	case DeviceType::HMLCSW4BAPCB:
		return true;
	case DeviceType::HMLCSW1BAPCB:
		return true;
	case DeviceType::HMLCSW1PLOM54:
		return true;
	case DeviceType::HMLCSW1SMATMEGA168:
		return true;
	case DeviceType::HMLCSW4SMATMEGA168:
		return true;
	case DeviceType::HMMODRE8:
		return true;
	case DeviceType::HmEsPmsw1Dr:
		return true;
	case DeviceType::HmEsPmsw1Sm:
		return true;
	case DeviceType::HmLcSw1Dr:
		return true;
	case DeviceType::HmLcSw1PlCtR1:
		return true;
	case DeviceType::HmLcSw1PlCtR2:
		return true;
	case DeviceType::HmLcSw1PlCtR3:
		return true;
	case DeviceType::HmLcSw1PlCtR4:
		return true;
	case DeviceType::HmLcSw1PlCtR5:
		return true;
	default:
		return false;
	}
	return false;
}

bool HomeMaticCentral::isDimmer(uint32_t type)
{
	switch((DeviceType)type)
	{
	case DeviceType::HMLCDIM1TPL:
		return true;
	case DeviceType::HMLCDIM1TPL2:
		return true;
	case DeviceType::HMLCDIM1TCV:
		return true;
	case DeviceType::HMLCDIMSCHUECO:
		return true;
	case DeviceType::HMLCDIMSCHUECO2:
		return true;
	case DeviceType::HMLCDIM2TSM:
		return true;
	case DeviceType::HMLCDIM1TFM:
		return true;
	case DeviceType::HMLCDIM1LPL644:
		return true;
	case DeviceType::HMLCDIM1LCV644:
		return true;
	case DeviceType::HMLCDIM1PWMCV:
		return true;
	case DeviceType::HMLCDIM1TPL644:
		return true;
	case DeviceType::HMLCDIM1TCV644:
		return true;
	case DeviceType::HMLCDIM1TFM644:
		return true;
	case DeviceType::HMLCDIM1TPBUFM:
		return true;
	case DeviceType::HMLCDIM2LSM644:
		return true;
	case DeviceType::HMLCDIM2TSM644:
		return true;
	case DeviceType::HMLCDIM1TFMLF:
		return true;
	default:
		return false;
	}
	return false;
}


void HomeMaticCentral::worker()
{
	try
	{
		while(GD::bl->booting && !_stopWorkerThread)
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}

		std::chrono::milliseconds sleepingTime(10);
		uint32_t counter = 0;
		uint64_t lastPeer = 0;
		//One loop on the Raspberry Pi takes about 30Âµs
		while(!_stopWorkerThread)
		{
			try
			{
				std::this_thread::sleep_for(sleepingTime);
				if(_stopWorkerThread) return;
				if(counter > 10000)
				{
					counter = 0;
					_peersMutex.lock();
					if(_peersById.size() > 0)
					{
						int32_t windowTimePerPeer = _bl->settings.workerThreadWindow() / _peersById.size();
						if(windowTimePerPeer > 2) windowTimePerPeer -= 2;
						sleepingTime = std::chrono::milliseconds(windowTimePerPeer);
					}
					_peersMutex.unlock();
				}
				_peersMutex.lock();
				if(!_peersById.empty())
				{
					if(!_peersById.empty())
					{
						std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator nextPeer = _peersById.find(lastPeer);
						if(nextPeer != _peersById.end())
						{
							nextPeer++;
							if(nextPeer == _peersById.end()) nextPeer = _peersById.begin();
						}
						else nextPeer = _peersById.begin();
						lastPeer = nextPeer->first;
					}
				}
				_peersMutex.unlock();
				std::shared_ptr<BidCoSPeer> peer(getPeer(lastPeer));
				if(peer && !peer->deleting) peer->worker();
				counter++;
			}
			catch(const std::exception& ex)
			{
				_peersMutex.unlock();
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
		}
	}
    catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

bool deleteThis = false;

bool HomeMaticCentral::onPacketReceived(std::string& senderId, std::shared_ptr<BaseLib::Systems::Packet> packet)
{
	try
	{
		if(_disposing) return false;
		std::shared_ptr<BidCoSPacket> bidCoSPacket(std::dynamic_pointer_cast<BidCoSPacket>(packet));
		if(BaseLib::HelperFunctions::getTime() > bidCoSPacket->getTimeReceived() + 5000) GD::out.printError("Error: Packet was processed more than 5 seconds after reception. If your CPU and network load is low, please report this to the Homegear developers.");
		if(_bl->debugLevel >= 4) std::cout << BaseLib::HelperFunctions::getTimeString(bidCoSPacket->getTimeReceived()) << " HomeMatic BidCoS packet received (" << senderId << (bidCoSPacket->rssiDevice() ? std::string(", RSSI: -") + std::to_string((int32_t)(bidCoSPacket->rssiDevice())) + " dBm" : "") << "): " << bidCoSPacket->hexString() << std::endl;
		if(!bidCoSPacket) return false;

		// {{{ Intercept packet
		/*if(bidCoSPacket->senderAddress() == 0x19A4E0 && bidCoSPacket->messageType() == 0x41)
		{
			std::vector<uint8_t> payload({ 0x00, 0x0B, 0x03 });
			std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(0x2F, 0x80, 0x02, _address, bidCoSPacket->senderAddress(), payload));
			sendPacket(GD::defaultPhysicalInterface, packet);
			return true;
		}*/
		// }}}

		/*if(bidCoSPacket->senderAddress() == _address) //Packet spoofed
		{
			std::shared_ptr<BidCoSPeer> peer(getPeer(bidCoSPacket->destinationAddress()));
			if(peer)
			{
				if(senderID != peer->getPhysicalInterfaceID()) return true; //Packet we sent was received by another interface
				if(bidCoSPacket->messageType() == 0x02 || bidCoSPacket->messageType() == 0x03) return true; //Ignore ACK and AES handshake packets.
				GD::out.printWarning("Warning: Central address of packet to peer " + std::to_string(peer->getID()) + " was spoofed. Packet was: " + packet->hexString());
				peer->serviceMessages->set("CENTRAL_ADDRESS_SPOOFED", 1, 0);
				std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string> { "CENTRAL_ADDRESS_SPOOFED" });
				std::shared_ptr<std::vector<PVariable>> values(new std::vector<PVariable> { PVariable(new Variable((int32_t)1)) });
                std::string eventSource = "device-" + std::to_string(peer->getID());
                std::string address = peer->getSerialNumber() + ":0";
				raiseRPCEvent(eventSource, peer->getID(), 0, address, valueKeys, values);
				return true;
			}
			return false;
		}*/
		if(_bl->settings.devLog()) _bl->out.printMessage("Devlog (" + senderId + "): Getting peer for packet " + bidCoSPacket->hexString() + ".");
		std::shared_ptr<BidCoSPeer> peer(getPeer(bidCoSPacket->senderAddress()));
		if(peer && bidCoSPacket->messageType() != 0x02 && bidCoSPacket->messageType() != 0x03)
		{
			if(_bl->settings.devLog()) _bl->out.printMessage("Devlog (" + senderId + "): Packet " + bidCoSPacket->hexString() + " is now passed to checkForBestInterface.");
			peer->checkForBestInterface(senderId, bidCoSPacket->rssiDevice(), bidCoSPacket->messageCounter()); //Ignore ACK and AES handshake packets.
			if(_bl->settings.devLog()) _bl->out.printMessage("Devlog (" + senderId + "): checkForBestInterface finished.");
		}
		std::shared_ptr<IBidCoSInterface> physicalInterface = getPhysicalInterface(bidCoSPacket->senderAddress());
		if(physicalInterface->getID() != senderId) return true;

		// {{{ Handle wrong ACKs
			if(bidCoSPacket->messageType() == 0x02 && bidCoSPacket->destinationAddress() != 0 && bidCoSPacket->payload().size() == 1 && bidCoSPacket->payload().at(0) == 0)
			{
				std::shared_ptr<BidCoSPacket> sentPacket(_sentPackets.get(bidCoSPacket->senderAddress()));
				if(sentPacket && sentPacket->messageCounter() != bidCoSPacket->messageCounter())
				{
					_bl->out.printInfo("Info: Ignoring ACK with wrong message counter.");
					return true;
				}
			}
		// }}}

		bool handled = false;
		if(_bl->settings.devLog()) _bl->out.printMessage("Devlog (" + senderId + "): Packet " + bidCoSPacket->hexString() + " is now passed to _receivedPackets.set.");
		if(_receivedPackets.set(bidCoSPacket->senderAddress(), bidCoSPacket, bidCoSPacket->getTimeReceived())) handled = true;
		else
		{
			std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.get(bidCoSPacket->senderAddress());
			if(queue && queue->getQueueType() == BidCoSQueueType::GETVALUE) handled = false; //Don't handle peer's get value responses
			else
			{
				std::shared_ptr<BidCoSMessage> message = _messages->find(bidCoSPacket);
				if(message && message->checkAccess(bidCoSPacket, queue))
				{
					if(_bl->debugLevel >= 6) GD::out.printDebug("Debug: Device " + std::to_string(_deviceId) + ": Access granted for packet " + bidCoSPacket->hexString());
					message->invokeMessageHandler(senderId, bidCoSPacket);
					handled = true;
				}
			}
		}
		if(_bl->settings.devLog()) _bl->out.printMessage("Devlog (" + senderId + "): _receivedPackets.set finished.");
		if(!peer) return false;
		std::shared_ptr<BidCoSPeer> team;
		if(peer->hasTeam() && bidCoSPacket->senderAddress() == peer->getTeamRemoteAddress()) team = getPeer(peer->getTeamRemoteSerialNumber());
		if(handled)
		{
			//This block is not necessary for teams as teams will never have queues.
			std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.get(bidCoSPacket->senderAddress());
			if(queue && queue->getQueueType() != BidCoSQueueType::PEER)
			{
				peer->setLastPacketReceived();
				peer->serviceMessages->endUnreach();
				peer->setRSSIDevice(bidCoSPacket->rssiDevice());
				return true; //Packet is handled by queue. Don't check if queue is empty!
			}
		}
		if(_bl->settings.devLog()) _bl->out.printMessage("Devlog (" + senderId + "): Packet " + bidCoSPacket->hexString() + " is now passed to the peer.");
		if(team)
		{
			team->packetReceived(bidCoSPacket);
			for(std::vector<std::pair<std::string, uint32_t>>::const_iterator i = team->teamChannels.begin(); i != team->teamChannels.end(); ++i)
			{
				getPeer(i->first)->packetReceived(bidCoSPacket);
			}
		}
		else peer->packetReceived(bidCoSPacket);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return false;
}

std::shared_ptr<IBidCoSInterface> HomeMaticCentral::getPhysicalInterface(int32_t peerAddress)
{
	try
	{
		std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.get(peerAddress);
		if(queue) return queue->getPhysicalInterface();
		std::shared_ptr<BidCoSPeer> peer = getPeer(peerAddress);
		return peer ? peer->getPhysicalInterface() : GD::defaultPhysicalInterface;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return GD::defaultPhysicalInterface;
}

std::shared_ptr<BidCoSQueue> HomeMaticCentral::enqueuePendingQueues(int32_t deviceAddress, bool wait, bool* result)
{
	try
	{
		_enqueuePendingQueuesMutex.lock();
		std::shared_ptr<BidCoSPeer> peer = getPeer(deviceAddress);
		if(!peer || !peer->pendingBidCoSQueues)
		{
			_enqueuePendingQueuesMutex.unlock();
			if(result) *result = true;
			return std::shared_ptr<BidCoSQueue>();
		}
		std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.get(deviceAddress);
		if(!queue) queue = _bidCoSQueueManager.createQueue(peer->getPhysicalInterface(), BidCoSQueueType::DEFAULT, deviceAddress);
		if(!queue)
		{
			_enqueuePendingQueuesMutex.unlock();
			if(result) *result = true;
			return std::shared_ptr<BidCoSQueue>();
		}
		if(!queue->peer) queue->peer = peer;
		if(queue->pendingQueuesEmpty())
		{
			if(peer->getRXModes() & HomegearDevice::ReceiveModes::wakeOnRadio) peer->pendingBidCoSQueues->setWakeOnRadioBit();
			queue->push(peer->pendingBidCoSQueues);
		}
		_enqueuePendingQueuesMutex.unlock();

		if(wait)
		{
			int32_t waitIndex = 0;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			while(!peer->pendingQueuesEmpty() && waitIndex < 50)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				waitIndex++;
			}

			if(result) *result = peer->pendingQueuesEmpty();
		}
		else if(result) *result = true;

		return queue;
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    _enqueuePendingQueuesMutex.unlock();
    if(result) *result = false;
    return std::shared_ptr<BidCoSQueue>();
}

void HomeMaticCentral::enqueuePackets(int32_t deviceAddress, std::shared_ptr<BidCoSQueue> packets, bool pushPendingBidCoSQueues)
{
	try
	{
		std::shared_ptr<BidCoSPeer> peer(getPeer(deviceAddress));
		if(!peer) return;
		std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.createQueue(peer->getPhysicalInterface(), BidCoSQueueType::DEFAULT, deviceAddress);
		queue->push(packets, true, true);
		if(pushPendingBidCoSQueues)
		{
			queue->push(peer->pendingBidCoSQueues);
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::sendPacket(std::shared_ptr<IBidCoSInterface> physicalInterface, std::shared_ptr<BidCoSPacket> packet, bool stealthy)
{
	try
	{
		if(!packet || !physicalInterface) return;
		uint32_t responseDelay = physicalInterface->responseDelay();
		std::shared_ptr<BidCoSPacketInfo> packetInfo = _sentPackets.getInfo(packet->destinationAddress());
		/*int64_t time = _bl->hf.getTime();
		if(_physicalInterface->autoResend())
		{
			if((packet->messageType() == 0x02 && packet->controlByte() == 0x80 && packet->payload().size() == 1 && packet->payload().at(0) == 0)
				|| !((packet->controlByte() & 0x01) && (packet->payload().empty() || (packet->payload().size() == 1 && packet->payload().at(0) == 0))))
			{
				time -= 80;
			}
		}
		if(!stealthy) _sentPackets.set(packet->destinationAddress(), packet, time);*/
		if(!stealthy) _sentPackets.set(packet->destinationAddress(), packet);
		if(packetInfo)
		{
			int64_t timeDifference = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - packetInfo->time;
			if(timeDifference < responseDelay)
			{
				packetInfo->time += responseDelay - timeDifference; //Set to sending time
				std::this_thread::sleep_for(std::chrono::milliseconds(responseDelay - timeDifference));
			}
		}
		if(stealthy) _sentPackets.keepAlive(packet->destinationAddress());
		packetInfo = _receivedPackets.getInfo(packet->destinationAddress());
		if(packetInfo)
		{
			int64_t time = BaseLib::HelperFunctions::getTime();
			int64_t timeDifference = time - packetInfo->time;
			if(timeDifference >= 0 && timeDifference < responseDelay)
			{
				int64_t sleepingTime = responseDelay - timeDifference;
				if(sleepingTime > 1) sleepingTime -= 1;
				packet->setTimeSending(time + sleepingTime);
				std::this_thread::sleep_for(std::chrono::milliseconds(sleepingTime));
			}
			//Set time to now. This is necessary if two packets are sent after each other without a response in between
			packetInfo->time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		}
		else if(_bl->debugLevel > 4) GD::out.printDebug("Debug: Sending packet " + packet->hexString() + " immediately, because it seems it is no response (no packet information found).", 7);
		physicalInterface->sendPacket(packet);
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::sendPacketMultipleTimes(std::shared_ptr<IBidCoSInterface> physicalInterface, std::shared_ptr<BidCoSPacket> packet, int32_t peerAddress, int32_t count, int32_t delay, bool incrementMessageCounter, bool useCentralMessageCounter, bool isThread)
{
	try
	{
		if(!isThread)
		{
			_sendMultiplePacketsThreadMutex.lock();
			_bl->threadManager.join(_sendMultiplePacketsThread);
			_bl->threadManager.start(_sendMultiplePacketsThread, false, &HomeMaticCentral::sendPacketMultipleTimes, this, physicalInterface, packet, peerAddress, count, delay, incrementMessageCounter, useCentralMessageCounter, true);
			_sendMultiplePacketsThreadMutex.unlock();
			return;
		}
		if(!packet || !physicalInterface) return;
		if((packet->controlByte() & 0x20) && delay < 700) delay = 700;
		std::shared_ptr<BidCoSPeer> peer = getPeer(peerAddress);
		if(!peer) return;
		for(int32_t i = 0; i < count; i++)
		{
			_sentPackets.set(packet->destinationAddress(), packet);
			int64_t start = BaseLib::HelperFunctions::getTime();
			physicalInterface->sendPacket(packet);
			if(incrementMessageCounter)
			{
				if(useCentralMessageCounter)
				{
					packet->setMessageCounter(getMessageCounter());
				}
				else
				{
					packet->setMessageCounter(peer->getMessageCounter());
					peer->setMessageCounter(peer->getMessageCounter() + 1);
				}
			}
			int32_t difference = BaseLib::HelperFunctions::getTime() - start;
			if(difference < delay - 10) std::this_thread::sleep_for(std::chrono::milliseconds(delay - difference));
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

std::shared_ptr<BidCoSPeer> HomeMaticCentral::createPeer(int32_t address, int32_t firmwareVersion, uint32_t deviceType, std::string serialNumber, int32_t remoteChannel, int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet, bool save)
{
	try
	{
		std::shared_ptr<BidCoSPeer> peer(new BidCoSPeer(_deviceId, this));
		peer->setAddress(address);
		peer->setFirmwareVersion(firmwareVersion);
		peer->setDeviceType(deviceType);
		peer->setSerialNumber(serialNumber);
		peer->setRemoteChannel(remoteChannel);
		peer->setMessageCounter(messageCounter);
		PHomegearDevice rpcDevice = GD::family->getRpcDevices()->find(deviceType, firmwareVersion);
		if(!rpcDevice) return std::shared_ptr<BidCoSPeer>();
		int32_t dynamicChannelCount = -1;
		if(packet)
		{
			Functions::iterator functionIterator = rpcDevice->functions.find(1);
			if(functionIterator != rpcDevice->functions.end())
			{
				if(functionIterator->second->dynamicChannelCountIndex > -1)
				{
					double dynamicChannelCountSize = functionIterator->second->dynamicChannelCountSize;
					if(dynamicChannelCountSize == 1.0) dynamicChannelCountSize = 0.7;
					std::vector<uint8_t> data = packet->getPosition(23.0, dynamicChannelCountSize, -1);
					if(!data.empty()) dynamicChannelCount = data.at(0);
				}
			}
		}
		rpcDevice = GD::family->getRpcDevices()->find(deviceType, firmwareVersion, dynamicChannelCount);
		if(!rpcDevice) return std::shared_ptr<BidCoSPeer>();
		peer->setRpcDevice(rpcDevice);
		if(dynamicChannelCount > -1) peer->setCountFromSysinfo(dynamicChannelCount);
		if(save) peer->save(true, true, false); //Save and create peerID
		return peer;
	}
    catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return std::shared_ptr<BidCoSPeer>();
}

std::shared_ptr<BidCoSPeer> HomeMaticCentral::createTeam(int32_t address, uint32_t deviceType, std::string serialNumber)
{
	try
	{
		std::shared_ptr<BidCoSPeer> team(new BidCoSPeer(_deviceId, this));
		team->setAddress(address);
		team->setDeviceType(deviceType);
		team->setSerialNumber(serialNumber);
		//Do not save team!!!
		return team;
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return std::shared_ptr<BidCoSPeer>();
}

std::string HomeMaticCentral::handleCliCommand(std::string command)
{
	try
	{
		std::ostringstream stringStream;
		std::vector<std::string> arguments;
		bool showHelp = false;
		if(BaseLib::HelperFunctions::checkCliCommand(command, "test1", "ts1", "", 0, arguments, showHelp))
		{
			std::vector<uint8_t> payload{2, 0};
			std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(getMessageCounter(), 0xA2, 0x58, 0x39A07F, 0x1DA07F, payload));
			GD::defaultPhysicalInterface->sendPacket(packet);
			return "ok\n";
		}
		else if(BaseLib::HelperFunctions::checkCliCommand(command, "test2", "ts2", "", 0, arguments, showHelp))
		{
			std::vector<uint8_t> payload{ 0x80, 0x03, 0x02, 0x0A, 0x12, 0x81, 0x13, 0x85, 0x0A, 0x12, 0x80, 0x13, 0x85, 0x0A, 0x12, 0x0A, 0x0A };
			std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(getMessageCounter(), 0xA2, 0x58, _address, 0x4BD22A, payload));
			GD::defaultPhysicalInterface->sendPacket(packet);
			return "ok\n";
		}
		else if(BaseLib::HelperFunctions::checkCliCommand(command, "help", "h", "", 0, arguments, showHelp))
		{
			stringStream << "List of commands (shortcut in brackets):" << std::endl << std::endl;
			stringStream << "For more information about the individual command type: COMMAND help" << std::endl << std::endl;
			stringStream << "pairing on (pon)\tEnables pairing mode" << std::endl;
			stringStream << "pairing off (pof)\tDisables pairing mode" << std::endl;
			stringStream << "peers list (ls)\t\tList all peers" << std::endl;
			stringStream << "peers add (pa)\t\tManually adds a peer (without pairing it! Only for testing)" << std::endl;
			stringStream << "peers remove (prm)\tRemove a peer (without unpairing)" << std::endl;
			stringStream << "peers reset (prs)\tUnpair a peer and reset it to factory defaults" << std::endl;
			stringStream << "peers select (ps)\tSelect a peer" << std::endl;
			stringStream << "peers setname (pn)\tName a peer" << std::endl;
			stringStream << "peers unpair (pup)\tUnpair a peer" << std::endl;
			stringStream << "peers update (pud)\tUpdates a peer to the newest firmware version" << std::endl;
			stringStream << "unselect (u)\t\tUnselect this device" << std::endl;
			return stringStream.str();
		}
		else if(BaseLib::HelperFunctions::checkCliCommand(command, "pairing on", "pon", "", 0, arguments, showHelp))
		{
			if(showHelp)
			{
				stringStream << "Description: This command enables pairing mode." << std::endl;
				stringStream << "Usage: pairing on [DURATION]" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  DURATION:\tOptional duration in seconds to stay in pairing mode." << std::endl;
				return stringStream.str();
			}
			int32_t duration = arguments.size() > 0 ? BaseLib::Math::getNumber(arguments.at(0)) : 60;
			if(duration < 5 || duration > 3600) return "Invalid duration. Duration has to be greater than 5 and less than 3600.\n";

			setInstallMode(nullptr, true, duration, nullptr, false);
			stringStream << "Pairing mode enabled for " + std::to_string(duration) + " seconds." << std::endl;
			return stringStream.str();
		}
		else if(BaseLib::HelperFunctions::checkCliCommand(command, "pairing off", "pof", "", 0, arguments, showHelp))
		{
			if(showHelp)
			{
				stringStream << "Description: This command disables pairing mode." << std::endl;
				stringStream << "Usage: pairing off" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  There are no parameters." << std::endl;
				return stringStream.str();
			}

			setInstallMode(nullptr, false, -1, nullptr, false);
			stringStream << "Pairing mode disabled." << std::endl;
			return stringStream.str();
		}
		else if(command.compare(0, 9, "peers add") == 0 || command.compare(0, 2, "pa") == 0)
		{
			uint32_t deviceType = (uint32_t)DeviceType::none;
			int32_t peerAddress = 0;
			std::string serialNumber;
			int32_t firmwareVersion = 0;

			std::stringstream stream(command);
			std::string element;
			int32_t offset = (command.at(1) == 'a') ? 0 : 1;
			int32_t index = 0;
			std::shared_ptr<BidCoSPacket> packet;
			while(std::getline(stream, element, ' '))
			{
				if(index < 1 + offset)
				{
					index++;
					continue;
				}
				else if(index == 1 + offset)
				{
					if(element == "help") break;
					if(element.size() == 54)
					{
						index = 6;
						packet.reset(new BidCoSPacket());
						packet->import(element, false);
						peerAddress = packet->senderAddress();
						deviceType = (packet->payload().at(1) << 8) + packet->payload().at(2);
						firmwareVersion = packet->payload().at(0);
						serialNumber.insert(serialNumber.end(), &packet->payload().at(3), &packet->payload().at(3) + 10);
						break;
					}
					else
					{
						int32_t temp = BaseLib::Math::getNumber(element, true);
						if(temp == 0) return "Invalid device type. Device type has to be provided in hexadecimal format.\n";
						deviceType = temp;
					}
				}
				else if(index == 2 + offset)
				{
					peerAddress = BaseLib::Math::getNumber(element, true);
					if(peerAddress == 0 || peerAddress != (peerAddress & 0xFFFFFF)) return "Invalid address. Address has to be provided in hexadecimal format and with a maximum size of 3 bytes. A value of \"0\" is not allowed.\n";
				}
				else if(index == 3 + offset)
				{
					if(element.length() != 10) return "Invalid serial number. Please provide a serial number with a length of 10 characters.\n";
					serialNumber = element;
				}
				else if(index == 4 + offset)
				{
					firmwareVersion = BaseLib::Math::getNumber(element, true);
					if(firmwareVersion == 0) return "Invalid firmware version. The firmware version has to be passed in hexadecimal format.\n";
				}
				index++;
			}
			if(index < 5 + offset)
			{
				stringStream << "Description: This command manually adds a peer without pairing. Please only use this command for testing." << std::endl;
				stringStream << "Usage: peers add DEVICETYPE ADDRESS SERIALNUMBER FIRMWAREVERSION" << std::endl;
				stringStream << "       peers add PAIRINGPACKET" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  DEVICETYPE:\t\tThe 2 byte device type of the peer to add in hexadecimal format. Example: 0039" << std::endl;
				stringStream << "  ADDRESS:\t\tThe 3 byte address of the peer to add in hexadecimal format. Example: 1A03FC" << std::endl;
				stringStream << "  SERIALNUMBER:\t\tThe 10 character long serial number of the peer to add. Example: JEQ0123456" << std::endl;
				stringStream << "  FIRMWAREVERSION:\tThe 1 byte firmware version of the peer to add in hexadecimal format. Example: 1F" << std::endl;
				stringStream << "  PAIRINGPACKET:\tThe 27 byte hex of a pairing packet. Example: 1A0080001F454D1D01231900044B45513030323236393510010100" << std::endl;
				return stringStream.str();
			}
			if(peerExists(peerAddress) || peerExists(serialNumber)) stringStream << "This peer is already paired to this central." << std::endl;
			else
			{
				std::shared_ptr<BidCoSPeer> peer = createPeer(peerAddress, firmwareVersion, deviceType, serialNumber, 0, 0, packet, false);
				if(!peer || !peer->getRpcDevice()) return "Device type not supported.\n";
				try
				{
					_peersMutex.lock();
					if(peer->getAddress() != _address) _peers[peer->getAddress()] = peer;
					if(!peer->getSerialNumber().empty()) _peersBySerial[peer->getSerialNumber()] = peer;
					_peersMutex.unlock();
					peer->save(true, true, false);
					peer->initializeCentralConfig();
					_peersMutex.lock();
					_peersById[peer->getID()] = peer;
					_peersMutex.unlock();
				}
				catch(const std::exception& ex)
				{
					_peersMutex.unlock();
					GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
				}

				stringStream << "Added peer " + std::to_string(peer->getID()) + " with address 0x" << std::hex << peerAddress << " of type 0x" << BaseLib::HelperFunctions::getHexString(deviceType) << " with serial number " << serialNumber << " and firmware version 0x" << firmwareVersion << "." << std::dec << std::endl;
			}
			return stringStream.str();
		}
		else if(command.compare(0, 12, "peers remove") == 0 || command.compare(0, 3, "prm") == 0)
		{
			uint64_t peerID = 0;

			std::stringstream stream(command);
			std::string element;
			int32_t offset = (command.at(1) == 'r') ? 0 : 1;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 1 + offset)
				{
					index++;
					continue;
				}
				else if(index == 1 + offset)
				{
					if(element == "help") break;
					peerID = BaseLib::Math::getNumber(element, false);
					if(peerID == 0) return "Invalid id.\n";
				}
				index++;
			}
			if(index == 1 + offset)
			{
				stringStream << "Description: This command removes a peer without trying to unpair it first." << std::endl;
				stringStream << "Usage: peers remove PEERID" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEERID:\tThe id of the peer to remove. Example: 513" << std::endl;
				return stringStream.str();
			}

			if(!peerExists(peerID)) stringStream << "This peer is not paired to this central." << std::endl;
			else
			{
				deletePeer(peerID);
				stringStream << "Removed peer " << std::to_string(peerID) << "." << std::endl;
			}
			return stringStream.str();
		}
		else if(command.compare(0, 12, "peers unpair") == 0 || command.compare(0, 3, "pup") == 0)
		{
			uint64_t peerID = 0;

			std::stringstream stream(command);
			std::string element;
			int32_t offset = (command.at(1) == 'u') ? 0 : 1;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 1 + offset)
				{
					index++;
					continue;
				}
				else if(index == 1 + offset)
				{
					if(element == "help") break;
					peerID = BaseLib::Math::getNumber(element);
					if(peerID == 0) return "Invalid id.\n";
				}
				index++;
			}
			if(index == 1 + offset)
			{
				stringStream << "Description: This command unpairs a peer." << std::endl;
				stringStream << "Usage: peers unpair PEERID" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEERID:\tThe id of the peer to unpair. Example: 513" << std::endl;
				return stringStream.str();
			}

			std::shared_ptr<BidCoSPeer> peer = getPeer(peerID);
			if(!peer) stringStream << "This peer is not paired to this central." << std::endl;
			else
			{
				bool isVirtual = peer->isVirtual();
				peer.reset();
				stringStream << "Unpairing peer " << peerID << std::endl;
				if(isVirtual) deletePeer(peerID);
				else unpair(peerID, true);
			}
			return stringStream.str();
		}
		else if(command.compare(0, 11, "peers reset") == 0 || command.compare(0, 3, "prs") == 0)
		{
			uint64_t peerID = 0;

			std::stringstream stream(command);
			std::string element;
			int32_t offset = (command.at(1) == 'r') ? 0 : 1;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 1 + offset)
				{
					index++;
					continue;
				}
				else if(index == 1 + offset)
				{
					if(element == "help") break;
					peerID = BaseLib::Math::getNumber(element);
					if(peerID == 0) return "Invalid id.\n";
				}
				index++;
			}
			if(index == 1 + offset)
			{
				stringStream << "Description: This command unpairs a peer and resets it to factory defaults." << std::endl;
				stringStream << "Usage: peers reset PEERID" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEERID:\tThe id of the peer to reset. Example: 513" << std::endl;
				return stringStream.str();
			}

			std::shared_ptr<BidCoSPeer> peer = getPeer(peerID);
			if(!peer) stringStream << "This peer is not paired to this central." << std::endl;
			else
			{
				bool isVirtual = peer->isVirtual();
				peer.reset();
				stringStream << "Resetting peer " << std::to_string(peerID) << std::endl;
				if(isVirtual) deletePeer(peerID);
				else reset(peerID, true);
			}
			return stringStream.str();
		}
		else if(BaseLib::HelperFunctions::checkCliCommand(command, "peers setname", "pn", "", 2, arguments, showHelp))
		{
			if(showHelp)
			{
				stringStream << "Description: This command sets or changes the name of a peer to identify it more easily." << std::endl;
				stringStream << "Usage: peers setname PEERID NAME" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEERID:\tThe id of the peer to set the name for. Example: 513" << std::endl;
				stringStream << "  NAME:\tThe name to set. Example: \"1st floor light switch\"." << std::endl;
				return stringStream.str();
			}

			uint64_t peerID = BaseLib::Math::getNumber(arguments.at(0));
			if(peerID == 0) return "Invalid id.\n";
			std::string name = arguments.at(1);
			for(uint32_t i = 2; i < arguments.size(); i++)
			{
				name += ' ' + arguments.at(i);
			}

			if(!peerExists(peerID)) stringStream << "This peer is not paired to this central." << std::endl;
			else
			{
				std::shared_ptr<BidCoSPeer> peer = getPeer(peerID);
				peer->setName(name);
				stringStream << "Name set to \"" << name << "\"." << std::endl;
			}
			return stringStream.str();
		}
		else if(command.compare(0, 12, "peers update") == 0 || command.compare(0, 3, "pud") == 0)
		{
			uint64_t peerID = 0;
			bool all = false;

			std::stringstream stream(command);
			std::string element;
			int32_t offset = (command.at(1) == 'u') ? 0 : 1;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 1 + offset)
				{
					index++;
					continue;
				}
				else if(index == 1 + offset)
				{
					if(element == "help") break;
					else if(element == "all") all = true;
					else
					{
						peerID = BaseLib::Math::getNumber(element, false);
						if(peerID == 0) return "Invalid id.\n";
					}
				}
				index++;
			}
			if(index == 1 + offset)
			{
				stringStream << "Description: This command updates one or all peers to the newest firmware version available in \"" << _bl->settings.firmwarePath() << "\"." << std::endl;
				stringStream << "Usage: peers update PEERID" << std::endl;
				stringStream << "       peers update all" << std::endl << std::endl;
				stringStream << "Parameters:" << std::endl;
				stringStream << "  PEERID:\tThe id of the peer to update. Example: 513" << std::endl;
				return stringStream.str();
			}

			PVariable result;
			std::vector<uint64_t> ids;
			if(all)
			{
				_peersMutex.lock();
				for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
				{
					std::shared_ptr<BidCoSPeer> peer(std::dynamic_pointer_cast<BidCoSPeer>(i->second));
					if(peer->firmwareUpdateAvailable()) ids.push_back(i->first);
				}
				_peersMutex.unlock();
				if(ids.empty())
				{
					stringStream << "All peers are up to date." << std::endl;
					return stringStream.str();
				}
				result = updateFirmware(nullptr, ids, false);
			}
			else if(!peerExists(peerID)) stringStream << "This peer is not paired to this central." << std::endl;
			else
			{
				std::shared_ptr<BidCoSPeer> peer = getPeer(peerID);
				if(!peer->firmwareUpdateAvailable())
				{
					stringStream << "Peer is up to date." << std::endl;
					return stringStream.str();
				}
				ids.push_back(peerID);
				result = updateFirmware(nullptr, ids, false);
			}
			if(!result) stringStream << "Unknown error." << std::endl;
			else if(result->errorStruct) stringStream << result->structValue->at("faultString")->stringValue << std::endl;
			else stringStream << "Started firmware update(s)... This might take a long time. Use the RPC function \"getUpdateStatus\" or see the log for details." << std::endl;
			return stringStream.str();
		}
		else if(BaseLib::HelperFunctions::checkCliCommand(command, "peers list", "pl", "ls", 0, arguments, showHelp))
		{
			try
			{
				if(showHelp)
				{
					stringStream << "Description: This command lists information about all peers." << std::endl;
					stringStream << "Usage: peers list [FILTERTYPE] [FILTERVALUE]" << std::endl << std::endl;
					stringStream << "Parameters:" << std::endl;
					stringStream << "  FILTERTYPE:\tSee filter types below." << std::endl;
					stringStream << "  FILTERVALUE:\tDepends on the filter type. If a number is required, it has to be in hexadecimal format." << std::endl << std::endl;
					stringStream << "Filter types:" << std::endl;
					stringStream << "  ID: Filter by id." << std::endl;
					stringStream << "      FILTERVALUE: The id of the peer to filter (e. g. 513)." << std::endl;
					stringStream << "  ADDRESS: Filter by address." << std::endl;
					stringStream << "      FILTERVALUE: The 3 byte address of the peer to filter (e. g. 1DA44D)." << std::endl;
					stringStream << "  SERIAL: Filter by serial number." << std::endl;
					stringStream << "      FILTERVALUE: The serial number of the peer to filter (e. g. JEQ0554309)." << std::endl;
					stringStream << "  NAME: Filter by name." << std::endl;
					stringStream << "      FILTERVALUE: The part of the name to search for (e. g. \"1st floor\")." << std::endl;
					stringStream << "  TYPE: Filter by device type." << std::endl;
					stringStream << "      FILTERVALUE: The 2 byte device type in hexadecimal format." << std::endl;
					stringStream << "  CONFIGPENDING: List peers with pending config." << std::endl;
					stringStream << "      FILTERVALUE: empty" << std::endl;
					stringStream << "  UNREACH: List all unreachable peers." << std::endl;
					stringStream << "      FILTERVALUE: empty" << std::endl;
					stringStream << "  LOWBAT: Lists all peers with low battery." << std::endl;
					stringStream << "      FILTERVALUE: empty" << std::endl;
					return stringStream.str();
				}

				std::string filterType = arguments.size() > 1 ? BaseLib::HelperFunctions::toLower(arguments.at(0)) : "";
				std::string filterValue = arguments.size() > 1 ? BaseLib::HelperFunctions::toLower(arguments.at(1)) : "";
				if(filterType == "name") BaseLib::HelperFunctions::toLower(filterValue);

				if(_peers.empty())
				{
					stringStream << "No peers are paired to this central." << std::endl;
					return stringStream.str();
				}
				bool firmwareUpdates = false;
				std::string bar(" │ ");
				const int32_t idWidth = 11;
				const int32_t nameWidth = 25;
				const int32_t addressWidth = 8;
				const int32_t serialWidth = 13;
				const int32_t typeWidth1 = 4;
				const int32_t typeWidth2 = 25;
				const int32_t firmwareWidth = 8;
				const int32_t configPendingWidth = 14;
				const int32_t unreachWidth = 7;
				const int32_t lowbatWidth = 7;
				std::string nameHeader("Name");
				nameHeader.resize(nameWidth, ' ');
				std::string typeStringHeader("Type String");
				typeStringHeader.resize(typeWidth2, ' ');
				stringStream << std::setfill(' ')
					<< std::setw(idWidth) << "ID" << bar
					<< nameHeader << bar
					<< std::setw(addressWidth) << "Address" << bar
					<< std::setw(serialWidth) << "Serial Number" << bar
					<< std::setw(typeWidth1) << "Type" << bar
					<< typeStringHeader << bar
					<< std::setw(firmwareWidth) << "Firmware" << bar
					<< std::setw(configPendingWidth) << "Config Pending" << bar
					<< std::setw(unreachWidth) << "Unreach" << bar
					<< std::setw(unreachWidth) << "Low Bat"
					<< std::endl;
				stringStream << "────────────┼───────────────────────────┼──────────┼───────────────┼──────┼───────────────────────────┼──────────┼────────────────┼─────────┼────────" << std::endl;
				stringStream << std::setfill(' ')
					<< std::setw(idWidth) << " " << bar
					<< std::setw(nameWidth) << " " << bar
					<< std::setw(addressWidth) << " " << bar
					<< std::setw(serialWidth) << " " << bar
					<< std::setw(typeWidth1) << " " << bar
					<< std::setw(typeWidth2) << " " << bar
					<< std::setw(firmwareWidth) << " " << bar
					<< std::setw(configPendingWidth) << " " << bar
					<< std::setw(unreachWidth) << " " << bar
					<< std::setw(lowbatWidth) << " "
					<< std::endl;
				_peersMutex.lock();
				for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::iterator i = _peersById.begin(); i != _peersById.end(); ++i)
				{
					std::shared_ptr<BidCoSPeer> peer(std::dynamic_pointer_cast<BidCoSPeer>(i->second));
					if(filterType == "id")
					{
						uint64_t id = BaseLib::Math::getNumber(filterValue, false);
						if(i->second->getID() != id) continue;
					}
					else if(filterType == "name")
					{
						std::string name = i->second->getName();
						if((signed)BaseLib::HelperFunctions::toLower(name).find(filterValue) == (signed)std::string::npos) continue;
					}
					else if(filterType == "address")
					{
						int32_t address = BaseLib::Math::getNumber(filterValue, true);
						std::string addressHex = BaseLib::HelperFunctions::getHexString(i->second->getAddress(), 6);
						if((i->second->getAddress() != address) && ((signed)BaseLib::HelperFunctions::toLower(addressHex).find(filterValue) == (signed)std::string::npos)) continue;
					}
					else if(filterType == "serial")
					{
						std::string serial = i->second->getSerialNumber();
						if((signed)BaseLib::HelperFunctions::toLower(serial).find(filterValue) == (signed)std::string::npos) continue;
					}
					else if(filterType == "type")
					{
						int32_t deviceType = BaseLib::Math::getNumber(filterValue, true);
						std::string deviceTypeHex = BaseLib::HelperFunctions::getHexString(i->second->getDeviceType(), 4);
						if(((int32_t)i->second->getDeviceType() != deviceType) && ((signed)BaseLib::HelperFunctions::toLower(deviceTypeHex).find(filterValue) == (signed)std::string::npos)) continue;
					}
					else if(filterType == "configpending")
					{
						if(i->second->serviceMessages)
						{
							if(!i->second->serviceMessages->getConfigPending()) continue;
						}
					}
					else if(filterType == "unreach")
					{
						if(i->second->serviceMessages)
						{
							if(!i->second->serviceMessages->getUnreach()) continue;
						}
					}
					else if(filterType == "lowbat")
					{
						if(i->second->serviceMessages)
						{
							if(!i->second->serviceMessages->getLowbat()) continue;
						}
					}

					uint64_t currentID = i->second->getID();
					std::string idString = (currentID > 999999) ? "0x" + BaseLib::HelperFunctions::getHexString(currentID, 8) : std::to_string(currentID);
					stringStream << std::setw(idWidth) << std::setfill(' ') << idString << bar;
					std::string name = i->second->getName();
					size_t nameSize = BaseLib::HelperFunctions::utf8StringSize(name);
					if(nameSize > (unsigned)nameWidth)
					{
						name = BaseLib::HelperFunctions::utf8Substring(name, 0, nameWidth - 3);
						name += "...";
					}
					else name.resize(nameWidth + (name.size() - nameSize), ' ');
					stringStream << name << bar
						<< std::setw(addressWidth) << BaseLib::HelperFunctions::getHexString(i->second->getAddress(), 6) << bar
						<< std::setw(serialWidth) << i->second->getSerialNumber() << bar
						<< std::setw(typeWidth1) << BaseLib::HelperFunctions::getHexString(i->second->getDeviceType(), 4) << bar;
					if(i->second->getRpcDevice())
					{
						std::string typeID = i->second->getRpcTypeString();
						if(typeID.size() > (unsigned)typeWidth2)
						{
							typeID.resize(typeWidth2 - 3);
							typeID += "...";
						}
						else typeID.resize(typeWidth2, ' ');
						stringStream << typeID << bar;
					}
					else stringStream << std::setw(typeWidth2) << " " << bar;
					if(i->second->getFirmwareVersion() == 0) stringStream << std::setfill(' ') << std::setw(firmwareWidth) << "?" << bar;
					else if(i->second->firmwareUpdateAvailable())
					{
						stringStream << std::setfill(' ') << std::setw(firmwareWidth) << ("*" + BaseLib::HelperFunctions::getHexString(i->second->getFirmwareVersion() >> 4) + "." + BaseLib::HelperFunctions::getHexString(i->second->getFirmwareVersion() & 0x0F)) << bar;
						firmwareUpdates = true;
					}
					else stringStream << std::setfill(' ') << std::setw(firmwareWidth) << (BaseLib::HelperFunctions::getHexString(i->second->getFirmwareVersion() >> 4) + "." + BaseLib::HelperFunctions::getHexString(i->second->getFirmwareVersion() & 0x0F)) << bar;
					if(i->second->serviceMessages)
					{
						std::string configPending(i->second->serviceMessages->getConfigPending() ? "Yes" : "No");
						std::string unreachable(i->second->serviceMessages->getUnreach() ? "Yes" : "No");
						std::string lowbat(i->second->serviceMessages->getLowbat() ? "Yes" : "No");
						stringStream << std::setfill(' ') << std::setw(configPendingWidth) << configPending << bar;
						stringStream << std::setfill(' ') << std::setw(unreachWidth) << unreachable << bar;
						stringStream << std::setfill(' ') << std::setw(lowbatWidth) << lowbat;
					}
					stringStream << std::endl << std::dec;
				}
				_peersMutex.unlock();
				stringStream << "────────────┴───────────────────────────┴──────────┴───────────────┴──────┴───────────────────────────┴──────────┴────────────────┴─────────┴────────" << std::endl;
				if(firmwareUpdates) stringStream << std::endl << "*: Firmware update available." << std::endl;

				return stringStream.str();
			}
			catch(const std::exception& ex)
			{
				_peersMutex.unlock();
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
		}
		else return "Unknown command.\n";
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return "Error executing command. See log file for more details.\n";
}

void HomeMaticCentral::updateFirmwares(std::vector<uint64_t> ids)
{
	try
	{
		if(_updateMode || _bl->deviceUpdateInfo.currentDevice > 0) return;
		_bl->deviceUpdateInfo.updateMutex.lock();
		_bl->deviceUpdateInfo.devicesToUpdate = ids.size();
		_bl->deviceUpdateInfo.currentUpdate = 0;
		for(std::vector<uint64_t>::iterator i = ids.begin(); i != ids.end(); ++i)
		{
			_bl->deviceUpdateInfo.currentDeviceProgress = 0;
			_bl->deviceUpdateInfo.currentUpdate++;
			_bl->deviceUpdateInfo.currentDevice = *i;
			updateFirmware(*i);
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
	_bl->deviceUpdateInfo.reset();
	_bl->deviceUpdateInfo.updateMutex.unlock();
}

void HomeMaticCentral::updateFirmware(uint64_t id)
{
	std::shared_ptr<IBidCoSInterface> physicalInterface;
	std::string oldPhysicalInterfaceID;
	std::shared_ptr<BidCoSPeer> peer;
	try
	{
		if(_updateMode) return;
		peer = getPeer(id);
		if(!peer) return;
		oldPhysicalInterfaceID = peer->getPhysicalInterfaceID();
		physicalInterface = peer->getPhysicalInterface();
		if(!physicalInterface->firmwareUpdatesSupported())
		{
			if(GD::defaultPhysicalInterface->firmwareUpdatesSupported())
			{
				GD::out.printInfo("Info: Using the default physical interface " + GD::defaultPhysicalInterface->getID() + " because the peer's interface doesn't support firmware updates.");
				physicalInterface = GD::defaultPhysicalInterface;
			}
			else
			{
				for(std::map<std::string, std::shared_ptr<IBidCoSInterface>>::iterator i = GD::physicalInterfaces.begin(); i != GD::physicalInterfaces.end(); ++i)
				{
					if(i->second->firmwareUpdatesSupported())
					{
						GD::out.printInfo("Info: Using physical interface " + i->second->getID() + " because the peer's interface doesn't support firmware updates.");
						physicalInterface = i->second;
						break;
					}
				}
			}
		}
		if(!physicalInterface->firmwareUpdatesSupported())
		{
			GD::out.printInfo("Info: Not updating peer with id " + std::to_string(id) + ". No physical interface supports firmware updates.");
			_bl->deviceUpdateInfo.results[id].first = 9;
			_bl->deviceUpdateInfo.results[id].second = "No physical interface supports firmware updates.";
			_updateMutex.unlock();
			_updateMode = false;
			return;
		}
		_updateMode = true;
		_updateMutex.lock();
		GD::out.printInfo("Starting firmware update for peer " + std::to_string(peer->getID()) + " (address 0x" + BaseLib::HelperFunctions::getHexString(peer->getAddress(), 6) + "). Interface: " + physicalInterface->getID());
		std::string filenamePrefix = BaseLib::HelperFunctions::getHexString((int32_t)0, 4) + "." + BaseLib::HelperFunctions::getHexString(peer->getDeviceType(), 8);
		std::string versionFile(_bl->settings.firmwarePath() + filenamePrefix + ".version");
		if(!BaseLib::Io::fileExists(versionFile))
		{
			GD::out.printInfo("Info: Not updating peer with id " + std::to_string(id) + ". No version info file found.");
			_bl->deviceUpdateInfo.results[id].first = 2;
			_bl->deviceUpdateInfo.results[id].second = "No version file found.";
			_updateMutex.unlock();
			_updateMode = false;
			return;
		}
		std::string firmwareFile(_bl->settings.firmwarePath() + filenamePrefix + ".fw");
		if(!BaseLib::Io::fileExists(firmwareFile))
		{
			GD::out.printInfo("Info: Not updating peer with id " + std::to_string(id) + ". No firmware file found.");
			_bl->deviceUpdateInfo.results[id].first = 3;
			_bl->deviceUpdateInfo.results[id].second = "No firmware file found.";
			_updateMutex.unlock();
			_updateMode = false;
			return;
		}
		int32_t firmwareVersion = peer->getNewFirmwareVersion();
		if(peer->getFirmwareVersion() >= firmwareVersion)
		{
			_bl->deviceUpdateInfo.results[id].first = 0;
			_bl->deviceUpdateInfo.results[id].second = "Already up to date.";
			GD::out.printInfo("Info: Not updating peer with id " + std::to_string(id) + ". Peer firmware is already up to date.");
			_updateMutex.unlock();
			_updateMode = false;
			return;
		}
		std::string oldVersionString = peer->getFirmwareVersionString(peer->getFirmwareVersion());
		std::string versionString = peer->getFirmwareVersionString(firmwareVersion);

		std::string firmwareHex;
		try
		{
			firmwareHex = BaseLib::Io::getFileContent(firmwareFile);
		}
		catch(const std::exception& ex)
		{
			GD::out.printError("Error: Could not open firmware file: " + firmwareFile + ": " + ex.what());
			_bl->deviceUpdateInfo.results[id].first = 4;
			_bl->deviceUpdateInfo.results[id].second = "Could not open firmware file.";
			_updateMutex.unlock();
			_updateMode = false;
			return;
		}
		catch(...)
		{
			GD::out.printError("Error: Could not open firmware file: " + firmwareFile + ".");
			_bl->deviceUpdateInfo.results[id].first = 4;
			_bl->deviceUpdateInfo.results[id].second = "Could not open firmware file.";
			_updateMutex.unlock();
			_updateMode = false;
			return;
		}
		std::vector<uint8_t> firmware = _bl->hf.getUBinary(firmwareHex);
		GD::out.printDebug("Debug: Size of firmware is: " + std::to_string(firmware.size()) + " bytes.");
		if(firmware.size() < 4)
		{
			_bl->deviceUpdateInfo.results[id].first = 5;
			_bl->deviceUpdateInfo.results[id].second = "Firmware file has wrong format.";
			GD::out.printError("Error: Could not read firmware file: " + firmwareFile + ": Wrong format.");
			_updateMutex.unlock();
			_updateMode = false;
			return;
		}

		std::vector<uint8_t> currentBlock;
		std::vector<std::vector<uint8_t>> blocks;
		int32_t pos = 0;
		while(pos + 1 < (signed)firmware.size())
		{
			int32_t blockSize = (firmware.at(pos) << 8) + firmware.at(pos + 1);
			GD::out.printDebug("Debug: Current block size is: " + std::to_string(blockSize) + " bytes.");
			pos += 2;
			if(pos + blockSize > (signed)firmware.size() || blockSize > 1024)
			{
				_bl->deviceUpdateInfo.results[id].first = 5;
				_bl->deviceUpdateInfo.results[id].second = "Firmware file has wrong format.";
				GD::out.printError("Error: Could not read firmware file: " + firmwareFile + ": Wrong format.");
				_updateMutex.unlock();
				_updateMode = false;
				return;
			}
			currentBlock.clear();
			currentBlock.insert(currentBlock.begin(), firmware.begin() + pos, firmware.begin() + pos + blockSize);
			blocks.push_back(currentBlock);
			pos += blockSize;
		}

		int32_t waitIndex = 0;
		std::shared_ptr<BidCoSPacket> receivedPacket;
		if(peer->getPhysicalInterfaceID() != physicalInterface->getID()) peer->setPhysicalInterfaceID(physicalInterface->getID());

		//if(!manual)
		//{
			//bool responseReceived = false;
			//for(int32_t retries = 0; retries < 10; retries++)
			//{
				std::vector<uint8_t> payload({0xCA});
				std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(getMessageCounter(), 0x30, 0x11, _address, peer->getAddress(), payload, true));
				//int64_t time = BaseLib::HelperFunctions::getTime();
				physicalInterface->sendPacket(packet);
				_sentPackets.set(packet->destinationAddress(), packet);
				/*waitIndex = 0;
				while(waitIndex < 100) //Wait, wait, wait. The WOR preamble alone needs 360ms with the CUL! And AES handshakes need time, too.
				{
					receivedPacket = _receivedPackets.get(peer->getAddress());
					if(receivedPacket && receivedPacket->getTimeReceived() > time && receivedPacket->payload().size() >= 1 && receivedPacket->destinationAddress() == _address && receivedPacket->messageType() == 2)
					{
						GD::out.printInfo("Info: Enter bootloader packet was accepted by peer.");
						responseReceived = true;
						break;
					}
					else if(receivedPacket) GD::out.printWarning("Warning: No ACK received in response to bootloader packet. Received packet was: " + receivedPacket->hexString());
					std::this_thread::sleep_for(std::chrono::milliseconds(20));
					waitIndex++;
				}
				if(responseReceived) break;*/
			//}
			/*if(!responseReceived)
			{
				peer->setPhysicalInterfaceID(oldPhysicalInterfaceID);
				_updateMutex.unlock();
				_updateMode = false;
				_bl->deviceUpdateInfo.results[id].first = 6;
				_bl->deviceUpdateInfo.results[id].second = "Device did not respond to enter-bootloader packet.";
				GD::out.printWarning("Warning: Device did not enter bootloader.");
				return;
			}*/
		//}

		int64_t time = BaseLib::HelperFunctions::getTime();
		GD::out.printInfo("Info: Now waiting for update request from peer " + std::to_string(peer->getID()) + ".");
		int32_t retries = 0;
		for(retries = 0; retries < 2; retries++)
		{
			bool requestReceived = false;
			while(waitIndex < 1000)
			{
				receivedPacket = _receivedPackets.get(peer->getAddress());
				if(receivedPacket && receivedPacket->getTimeReceived() > time && receivedPacket->payload().size() > 1 && receivedPacket->payload().at(0) == 0 && receivedPacket->destinationAddress() == 0 && receivedPacket->messageType() == 0x10)
				{
					std::string serialNumber((char*)&receivedPacket->payload().at(1), receivedPacket->payload().size() - 1);
					if(serialNumber == peer->getSerialNumber())
					{
						GD::out.printInfo("Info: Update request received from peer " + std::to_string(peer->getID()) + ".");
						requestReceived = true;
						break;
					}
					else GD::out.printWarning("Warning: Update request received, but serial number does not match. Serial number in update packet: " + serialNumber + ". Expected serial number: " + peer->getSerialNumber());
				}
				else if(receivedPacket && receivedPacket->getTimeReceived() > time && receivedPacket->messageType() != 0x02) GD::out.printWarning("Warning: Received packet is no update request: " + receivedPacket->hexString());
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				waitIndex++;
			}
			if(!requestReceived || !receivedPacket)
			{
				peer->setPhysicalInterfaceID(oldPhysicalInterfaceID);
				_updateMutex.unlock();
				_updateMode = false;
				_bl->deviceUpdateInfo.results[id].first = 7;
				_bl->deviceUpdateInfo.results[id].second = "No update request received.";
				GD::out.printWarning("Warning: No update request received.");
				return;
			}

			std::vector<uint8_t> payload({0x10, 0x5B, 0x11, 0xF8, 0x15, 0x47}); //TI CC1100 register settings
			std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(0x42, 0, 0xCB, 0, peer->getAddress(), payload, true));
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			physicalInterface->sendPacket(packet);
			_sentPackets.set(packet->destinationAddress(), packet);

			GD::out.printInfo("Info: Enabling update mode.");
			physicalInterface->enableUpdateMode();

			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			packet = std::shared_ptr<BidCoSPacket>(new BidCoSPacket(0x43, 0x20, 0xCB, 0, peer->getAddress(), payload, true));
			physicalInterface->sendPacket(packet);
			_sentPackets.set(packet->destinationAddress(), packet);

			requestReceived = false;
			waitIndex = 0;
			while(waitIndex < 100)
			{
				receivedPacket = _receivedPackets.get(peer->getAddress());
				if(receivedPacket && receivedPacket->payload().size() == 1 && receivedPacket->payload().at(0) == 0 && receivedPacket->destinationAddress() == 0 && receivedPacket->controlByte() == 0 && receivedPacket->messageType() == 2)
				{
					requestReceived = true;
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				waitIndex++;
			}
			if(requestReceived) break;
			GD::out.printInfo("Info: Disabling update mode.");
			physicalInterface->disableUpdateMode();
			std::this_thread::sleep_for(std::chrono::milliseconds(4000));
		}
		if(retries == 2 || !receivedPacket)
		{
			peer->setPhysicalInterfaceID(oldPhysicalInterfaceID);
			_updateMutex.unlock();
			_updateMode = false;
			_bl->deviceUpdateInfo.results[id].first = 7;
			_bl->deviceUpdateInfo.results[id].second = "No update request received.";
			GD::out.printError("Error: No update request received.");
			std::this_thread::sleep_for(std::chrono::milliseconds(7000));
			return;
		}

		GD::out.printInfo("Info: Updating peer " + std::to_string(id) + " from version " + oldVersionString + " to version " + versionString + ".");

		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		uint8_t messageCounter = receivedPacket->messageCounter() + 1;
		int32_t blockCounter = 1;
		for(std::vector<std::vector<uint8_t>>::iterator i = blocks.begin(); i != blocks.end(); ++i)
		{
			GD::out.printInfo("Info: Sending block " + std::to_string(blockCounter) + " of " + std::to_string(blocks.size()) + "...");
			_bl->deviceUpdateInfo.currentDeviceProgress = (blockCounter * 100) / blocks.size();
			blockCounter++;
			int32_t retries = 0;
			for(retries = 0; retries < 10; retries++)
			{
				int32_t pos = 0;
				std::vector<uint8_t> payload;
				while(pos < (signed)i->size())
				{
					payload.clear();
					if(pos == 0)
					{
						payload.push_back(i->size() >> 8);
						payload.push_back(i->size() & 0xFF);
					}
					if(i->size() - pos >= 35)
					{
						payload.insert(payload.end(), i->begin() + pos, i->begin() + pos + 35);
						pos += 35;
					}
					else
					{
						payload.insert(payload.end(), i->begin() + pos, i->begin() + pos + (i->size() - pos));
						pos += (i->size() - pos);
					}
					uint8_t controlByte = (pos < (signed)i->size()) ? 0 : 0x20;
					std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(messageCounter, controlByte, 0xCA, 0, peer->getAddress(), payload, true));
					physicalInterface->sendPacket(packet);
					_sentPackets.set(packet->destinationAddress(), packet);
					std::this_thread::sleep_for(std::chrono::milliseconds(55));
				}
				waitIndex = 0;
				bool okReceived = false;
				while(waitIndex < 15)
				{
					receivedPacket = _receivedPackets.get(peer->getAddress());
					if(receivedPacket && receivedPacket->messageCounter() == messageCounter && receivedPacket->payload().size() == 1 && receivedPacket->payload().at(0) == 0 && receivedPacket->destinationAddress() == 0 && receivedPacket->controlByte() == 0 && receivedPacket->messageType() == 2)
					{
						messageCounter++;
						okReceived = true;
						break;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(20));
					waitIndex++;
				}
				if(okReceived) break;
			}
			if(retries == 10)
			{
				GD::out.printInfo("Info: Disabling update mode.");
				physicalInterface->disableUpdateMode();
				peer->setPhysicalInterfaceID(oldPhysicalInterfaceID);
				_updateMutex.unlock();
				_updateMode = false;
				_bl->deviceUpdateInfo.results[id].first = 8;
				_bl->deviceUpdateInfo.results[id].second = "Too many communication errors.";
				GD::out.printError("Error: Too many communication errors.");
				std::this_thread::sleep_for(std::chrono::milliseconds(7000));
				return;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(55));
		}
		peer->setFirmwareVersion(firmwareVersion);
		_bl->deviceUpdateInfo.results[id].first = 0;
		_bl->deviceUpdateInfo.results[id].second = "Update successful.";
		GD::out.printInfo("Info: Peer " + std::to_string(id) + " was successfully updated to firmware version " + versionString + ".");
		GD::out.printInfo("Info: Disabling update mode.");
		physicalInterface->disableUpdateMode();
		peer->setPhysicalInterfaceID(oldPhysicalInterfaceID);
		_updateMutex.unlock();
		_updateMode = false;
		std::this_thread::sleep_for(std::chrono::milliseconds(7000));
		return;
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    _bl->deviceUpdateInfo.results[id].first = 1;
	_bl->deviceUpdateInfo.results[id].second = "Unknown error.";
    GD::out.printInfo("Info: Disabling update mode.");
    peer->setPhysicalInterfaceID(oldPhysicalInterfaceID);
    if(physicalInterface) physicalInterface->disableUpdateMode();
    _updateMutex.unlock();
    _updateMode = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(7000));
}

int32_t HomeMaticCentral::getUniqueAddress(int32_t seed)
{
	try
	{
		uint32_t i = 0;
		while((_peers.find(seed) != _peers.end()) && i++ < 200000)
		{
			seed += 9345;
			if(seed > 16777215) seed -= 16777216;
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
	return seed;
}

std::string HomeMaticCentral::getUniqueSerialNumber(std::string seedPrefix, uint32_t seedNumber)
{
	try
	{
		if(seedPrefix.size() > 3) throw BaseLib::Exception("seedPrefix is too long.");
		uint32_t i = 0;
		int32_t numberSize = 10 - seedPrefix.size();
		std::ostringstream stringstream;
		stringstream << seedPrefix << std::setw(numberSize) << std::setfill('0') << std::dec << seedNumber;
		std::string temp2 = stringstream.str();
		while((_peersBySerial.find(temp2) != _peersBySerial.end()) && i++ < 100000)
		{
			stringstream.str(std::string());
			stringstream.clear();
			seedNumber += 73;
			if(seedNumber > 9999999) seedNumber -= 10000000;
			std::ostringstream stringstream;
			stringstream << seedPrefix << std::setw(numberSize) << std::setfill('0') << std::dec << seedNumber;
			temp2 = stringstream.str();
		}
		return temp2;
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
	return "";
}

void HomeMaticCentral::addHomegearFeaturesHMCCVD(std::shared_ptr<BidCoSPeer> peer, int32_t channel, bool pushPendingBidCoSQueues)
{
	try
	{
		std::shared_ptr<BidCoSPeer> tc;
		uint64_t hmcctcId = peer->getVirtualPeerId();
		if(hmcctcId != 0) tc = getPeer(hmcctcId);
		if(!tc)
		{
			int32_t hmcctcAddress = getUniqueAddress((0x39 << 16) + (peer->getAddress() & 0xFF00) + (peer->getAddress() & 0xFF));
			if(peer->hasPeers(1) && !peer->getPeer(1, hmcctcAddress)) return; //Already linked to a HM-CC-TC
			std::string temp = peer->getSerialNumber().substr(3);
			std::string serialNumber = getUniqueSerialNumber("VCD", BaseLib::Math::getNumber(temp));
			tc.reset(new HmCcTc(_deviceId, this));
			tc->setAddress(hmcctcAddress);
			tc->setFirmwareVersion(0x10);
			tc->setDeviceType((uint32_t)DeviceType::HMCCTC);
			tc->setSerialNumber(serialNumber);
			PHomegearDevice rpcDevice = GD::family->getRpcDevices()->find(tc->getDeviceType(), 0x10);
			if(!rpcDevice)
			{
				GD::out.printError("Error: Could not create virtual peer of type HM-CC-TC.");
				return;
			}
			tc->setRpcDevice(rpcDevice);
			try
			{
				_peersMutex.lock();
				if(!tc->getSerialNumber().empty()) _peersBySerial[tc->getSerialNumber()] = tc;
				_peersMutex.unlock();
				tc->save(true, true, false);
				tc->initializeCentralConfig();
				_peersMutex.lock();
				_peersById[tc->getID()] = tc;
				_peersMutex.unlock();
			}
			catch(const std::exception& ex)
			{
				_peersMutex.unlock();
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			std::shared_ptr<BaseLib::Systems::BasicPeer> hmccvd(new BaseLib::Systems::BasicPeer());
			hmccvd->id = peer->getID();
			hmccvd->address = peer->getAddress();
			hmccvd->serialNumber = peer->getSerialNumber();
			hmccvd->channel = 1;
			tc->addPeer(2, hmccvd);
		}
		std::shared_ptr<BaseLib::Systems::BasicPeer> hmcctc(new BaseLib::Systems::BasicPeer());
		hmcctc->id = tc->getID();
		hmcctc->address = tc->getAddress();
		hmcctc->serialNumber = tc->getSerialNumber();
		hmcctc->channel = 2;
		hmcctc->isSender = true;
		hmcctc->isVirtual = true;
		peer->addPeer(1, hmcctc);

		std::shared_ptr<BidCoSQueue> pendingQueue(new BidCoSQueue(peer->getPhysicalInterface(), BidCoSQueueType::CONFIG));
		pendingQueue->noSending = true;

		std::vector<uint8_t> payload;
		//CONFIG_ADD_PEER
		payload.push_back(0x01);
		payload.push_back(0x01);
		payload.push_back(tc->getAddress() >> 16);
		payload.push_back((tc->getAddress() >> 8) & 0xFF);
		payload.push_back(tc->getAddress() & 0xFF);
		payload.push_back(0x02);
		payload.push_back(0);
		std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(getMessageCounter(), 0xA0, 0x01, _address, peer->getAddress(), payload));
		pendingQueue->push(configPacket);
		pendingQueue->push(_messages->find(0x02));

		peer->pendingBidCoSQueues->push(pendingQueue);
		peer->serviceMessages->setConfigPending(true);
		if(pushPendingBidCoSQueues)
		{
			std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.createQueue(peer->getPhysicalInterface(), BidCoSQueueType::CONFIG, peer->getAddress());
			queue->push(peer->pendingBidCoSQueues);
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::addHomegearFeaturesRemote(std::shared_ptr<BidCoSPeer> peer, int32_t channel, bool pushPendingBidCoSQueues)
{
	try
	{
		if(!peer) return;
		Functions functions;
		if(channel == -1)
		{
			std::shared_ptr<HomegearDevice> rpcDevice = peer->getRpcDevice();
			for(Functions::iterator i = rpcDevice->functions.begin(); i != rpcDevice->functions.end(); ++i)
			{
				if(!peer->hasPeers(i->first) || peer->getPeer(i->first, _address))
				{
					functions[i->first] = i->second;
				}
			}
			if(functions.empty()) return; //All channels are already paired to actors
		}
		std::shared_ptr<BaseLib::Systems::BasicPeer> switchPeer;
		if(channel > -1)
		{
			switchPeer.reset(new BaseLib::Systems::BasicPeer());
			switchPeer->id = 0xFFFFFFFFFFFFFFFF;
			switchPeer->address = _address;
			switchPeer->serialNumber = _serialNumber;
			switchPeer->channel = channel;
			switchPeer->isVirtual = true;
			peer->addPeer(channel, switchPeer);
		}
		else
		{
			for(Functions::iterator i = functions.begin(); i != functions.end(); ++i)
			{
				if(i->second->type != "KEY" && i->second->type != "MOTION_DETECTOR" && i->second->type != "SWITCH_INTERFACE" && i->second->type != "PULSE_SENSOR") continue;
				switchPeer.reset(new BaseLib::Systems::BasicPeer());
				switchPeer->id = 0xFFFFFFFFFFFFFFFF;
				switchPeer->address = _address;
				switchPeer->serialNumber = _serialNumber;
				switchPeer->channel = i->first;
				switchPeer->isVirtual = true;
				peer->addPeer(i->first, switchPeer);
			}
		}

		PVariable paramset(new Variable(VariableType::tStruct));
		if(peer->getDeviceType() == (uint32_t)DeviceType::HMRC19 || peer->getDeviceType() == (uint32_t)DeviceType::HMRC19B || peer->getDeviceType() == (uint32_t)DeviceType::HMRC19SW)
		{
			paramset->structValue->insert(StructElement("LCD_SYMBOL", PVariable(new Variable(2))));
			paramset->structValue->insert(StructElement("LCD_LEVEL_INTERP", PVariable(new Variable(1))));
		}

		std::vector<uint8_t> payload;
		if(channel > -1)
		{
			std::shared_ptr<BidCoSQueue> pendingQueue(new BidCoSQueue(peer->getPhysicalInterface(), BidCoSQueueType::CONFIG));
			pendingQueue->noSending = true;

			payload.clear();
			payload.push_back(channel);
			payload.push_back(0x01);
			payload.push_back(_address >> 16);
			payload.push_back((_address >> 8) & 0xFF);
			payload.push_back(_address & 0xFF);
			payload.push_back(channel);
			payload.push_back(0);
			std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(getMessageCounter(), 0xA0, 0x01, _address, peer->getAddress(), payload));
			pendingQueue->push(configPacket);
			pendingQueue->push(_messages->find(0x02));
			peer->pendingBidCoSQueues->push(pendingQueue);
			peer->serviceMessages->setConfigPending(true);

			//putParamset pushes the packets on pendingQueues, but does not send immediately
			if(!paramset->structValue->empty()) peer->putParamset(nullptr, channel, ParameterGroup::Type::Enum::link, 0xFFFFFFFFFFFFFFFF, channel, paramset, true);
		}
		else
		{
			for(Functions::iterator i = functions.begin(); i != functions.end(); ++i)
			{
				if(i->second->type != "KEY" && i->second->type != "MOTION_DETECTOR" && i->second->type != "SWITCH_INTERFACE" && i->second->type != "PULSE_SENSOR") continue;
				std::shared_ptr<BidCoSQueue> pendingQueue(new BidCoSQueue(peer->getPhysicalInterface(), BidCoSQueueType::CONFIG));
				pendingQueue->noSending = true;

				payload.clear();
				payload.push_back(i->first);
				payload.push_back(0x01);
				payload.push_back(_address >> 16);
				payload.push_back((_address >> 8) & 0xFF);
				payload.push_back(_address & 0xFF);
				payload.push_back(i->first);
				payload.push_back(0);
				std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(getMessageCounter(), 0xA0, 0x01, _address, peer->getAddress(), payload));
				pendingQueue->push(configPacket);
				pendingQueue->push(_messages->find(0x02));
				peer->pendingBidCoSQueues->push(pendingQueue);
				peer->serviceMessages->setConfigPending(true);

				//putParamset pushes the packets on pendingQueues, but does not send immediately
				if(!paramset->structValue->empty()) peer->putParamset(nullptr, i->first, ParameterGroup::Type::Enum::link, 0xFFFFFFFFFFFFFFFF, i->first, paramset, true);
			}
		}

		if(pushPendingBidCoSQueues)
		{
			std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.createQueue(peer->getPhysicalInterface(), BidCoSQueueType::CONFIG, peer->getAddress());
			queue->push(peer->pendingBidCoSQueues);
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::addHomegearFeaturesSwitch(std::shared_ptr<BidCoSPeer> peer, int32_t channel, bool pushPendingBidCoSQueues)
{
	try
	{
		if(!peer) return;
		Functions functions;
		if(channel == -1)
		{
			std::shared_ptr<HomegearDevice> rpcDevice = peer->getRpcDevice();
			for(Functions::iterator i = rpcDevice->functions.begin(); i != rpcDevice->functions.end(); ++i)
			{
				//Covers the case, that channel numbering is not continuous. A little overdoing, probably.
				functions[i->first] = i->second;
			}
			if(functions.empty()) return; //All channels are already paired to actors
		}
		std::shared_ptr<BaseLib::Systems::BasicPeer> switchPeer;
		if(channel > -1)
		{
			switchPeer.reset(new BaseLib::Systems::BasicPeer());
			switchPeer->id = 0xFFFFFFFFFFFFFFFF;
			switchPeer->address = _address;
			switchPeer->serialNumber = _serialNumber;
			switchPeer->channel = channel;
			switchPeer->isVirtual = true;
			peer->addPeer(channel, switchPeer);
		}
		else
		{
			for(Functions::iterator i = functions.begin(); i != functions.end(); ++i)
			{
				if(i->second->type != "DIMMER" && i->second->type != "SWITCH") continue;
				switchPeer.reset(new BaseLib::Systems::BasicPeer());
				switchPeer->id = 0xFFFFFFFFFFFFFFFF;
				switchPeer->address = _address;
				switchPeer->serialNumber = _serialNumber;
				switchPeer->channel = i->first;
				switchPeer->isVirtual = true;
				peer->addPeer(i->first, switchPeer);
			}
		}

		std::vector<uint8_t> payload;
		if(channel > -1)
		{
			std::shared_ptr<BidCoSQueue> pendingQueue(new BidCoSQueue(peer->getPhysicalInterface(), BidCoSQueueType::CONFIG));
			pendingQueue->noSending = true;

			payload.clear();
			payload.push_back(channel);
			payload.push_back(0x01);
			payload.push_back(_address >> 16);
			payload.push_back((_address >> 8) & 0xFF);
			payload.push_back(_address & 0xFF);
			payload.push_back(channel);
			payload.push_back(0);
			std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(getMessageCounter(), 0xA0, 0x01, _address, peer->getAddress(), payload));
			pendingQueue->push(configPacket);
			pendingQueue->push(_messages->find(0x02));
			peer->pendingBidCoSQueues->push(pendingQueue);
			peer->serviceMessages->setConfigPending(true);
		}
		else
		{
			for(Functions::iterator i = functions.begin(); i != functions.end(); ++i)
			{
				if(i->second->type != "DIMMER" && i->second->type != "SWITCH") continue;
				std::shared_ptr<BidCoSQueue> pendingQueue(new BidCoSQueue(peer->getPhysicalInterface(), BidCoSQueueType::CONFIG));
				pendingQueue->noSending = true;

				payload.clear();
				payload.push_back(i->first);
				payload.push_back(0x01);
				payload.push_back(_address >> 16);
				payload.push_back((_address >> 8) & 0xFF);
				payload.push_back(_address & 0xFF);
				payload.push_back(i->first);
				payload.push_back(0);
				std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(getMessageCounter(), 0xA0, 0x01, _address, peer->getAddress(), payload));
				pendingQueue->push(configPacket);
				pendingQueue->push(_messages->find(0x02));
				peer->pendingBidCoSQueues->push(pendingQueue);
				peer->serviceMessages->setConfigPending(true);
			}
		}

		if(pushPendingBidCoSQueues)
		{
			std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.createQueue(peer->getPhysicalInterface(), BidCoSQueueType::CONFIG, peer->getAddress());
			queue->push(peer->pendingBidCoSQueues);
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::addHomegearFeaturesMotionDetector(std::shared_ptr<BidCoSPeer> peer, int32_t channel, bool pushPendingBidCoSQueues)
{
	try
	{
		addHomegearFeaturesRemote(peer, channel, pushPendingBidCoSQueues);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::addHomegearFeaturesHMCCRTDN(std::shared_ptr<BidCoSPeer> peer, int32_t channel, bool pushPendingBidCoSQueues)
{
	try
	{
		if(!peer) return;
		if(channel == -1) channel = 3;
		if(peer->hasPeers(channel) && !peer->getPeer(channel, _address)) return;
		GD::out.printInfo("Info: Adding Homegear features to HM-CC-RT-DN/HM-TC-IT-WM-W-EU.");
		std::shared_ptr<BaseLib::Systems::BasicPeer> switchPeer;

		switchPeer.reset(new BaseLib::Systems::BasicPeer());
		switchPeer->id = 0xFFFFFFFFFFFFFFFF;
		switchPeer->address = _address;
		switchPeer->serialNumber = _serialNumber;
		switchPeer->channel = channel;
		switchPeer->isVirtual = true;
		peer->addPeer(channel, switchPeer);

		std::vector<uint8_t> payload;
		std::shared_ptr<BidCoSQueue> pendingQueue(new BidCoSQueue(peer->getPhysicalInterface(), BidCoSQueueType::CONFIG));
		pendingQueue->noSending = true;

		payload.clear();
		payload.push_back(channel);
		payload.push_back(0x01);
		payload.push_back(_address >> 16);
		payload.push_back((_address >> 8) & 0xFF);
		payload.push_back(_address & 0xFF);
		payload.push_back(channel);
		payload.push_back(0);
		std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(getMessageCounter(), 0xA0, 0x01, _address, peer->getAddress(), payload));
		pendingQueue->push(configPacket);
		pendingQueue->push(_messages->find(0x02));
		peer->pendingBidCoSQueues->push(pendingQueue);
		peer->serviceMessages->setConfigPending(true);

		if(pushPendingBidCoSQueues)
		{
			std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.createQueue(peer->getPhysicalInterface(), BidCoSQueueType::CONFIG, peer->getAddress());
			queue->push(peer->pendingBidCoSQueues);
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::addHomegearFeatures(std::shared_ptr<BidCoSPeer> peer, int32_t channel, bool pushPendingBidCoSQueues)
{
	try
	{
		GD::out.printDebug("Debug: Adding homegear features. Device type: 0x" + BaseLib::HelperFunctions::getHexString((int32_t)peer->getDeviceType()));
		if(peer->getDeviceType() == (uint32_t)DeviceType::HMCCVD) addHomegearFeaturesHMCCVD(peer, channel, pushPendingBidCoSQueues);
		else if(peer->getDeviceType() == (uint32_t)DeviceType::HMPB4DISWM ||
				peer->getDeviceType() == (uint32_t)DeviceType::HmPb4DisWm2 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRC4 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRC8 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRC4B ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRCP1 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRCSEC3 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRCSEC3B ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRCKEY3 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRCKEY3B ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMPBI4FM ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMPB4WM ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMPB2WM ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMPB2FM ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRC12 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRC12B ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRC12SW ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRC19 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRC19B ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRC19SW ||
				peer->getDeviceType() == (uint32_t)DeviceType::RCH ||
				peer->getDeviceType() == (uint32_t)DeviceType::ATENT ||
				peer->getDeviceType() == (uint32_t)DeviceType::ZELSTGRMHS4 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRC42 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRCSEC42 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMRCKEY42 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMPB6WM55 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMSWI3FM ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMSWI3FMSCHUECO ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMSWI3FMROTO ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMSENEP ||
				peer->getDeviceType() == (uint32_t)DeviceType::HmRc43 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HmRc43D ||
				peer->getDeviceType() == (uint32_t)DeviceType::HmRcSec43 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HmRcKey43 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HmRc2PbuFm ||
				peer->getDeviceType() == (uint32_t)DeviceType::HmRcDisHXEu ||
				peer->getDeviceType() == (uint32_t)DeviceType::BrcH ||
				peer->getDeviceType() == (uint32_t)DeviceType::HmPb2Wm55 ||
				peer->getDeviceType() == (uint32_t)DeviceType::HmPb2Wm552) addHomegearFeaturesRemote(peer, channel, pushPendingBidCoSQueues);
		else if(peer->getDeviceType() == (uint32_t)DeviceType::HMSECMDIR ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMSECMDIRSCHUECO ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMSENMDIRSM ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMSENMDIRO ||
				peer->getDeviceType() == (uint32_t)DeviceType::HmSenMdirWm55) addHomegearFeaturesMotionDetector(peer, channel, pushPendingBidCoSQueues);
		else if(peer->getDeviceType() == (uint32_t)DeviceType::HMCCRTDN ||
				peer->getDeviceType() == (uint32_t)DeviceType::HMCCRTDNBOM ||
				peer->getDeviceType() == (uint32_t)DeviceType::HmTcItWmWEu) addHomegearFeaturesHMCCRTDN(peer, channel, pushPendingBidCoSQueues);
		else if(HomeMaticCentral::isDimmer(peer->getDeviceType()) || HomeMaticCentral::isSwitch(peer->getDeviceType())) addHomegearFeaturesSwitch(peer, channel, pushPendingBidCoSQueues);
		else GD::out.printDebug("Debug: No homegear features to add.");
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::deletePeer(uint64_t id)
{
	try
	{
		std::shared_ptr<BidCoSPeer> peer(getPeer(id));
		if(!peer || peer->isTeam()) return;
		peer->deleting = true;
		PVariable deviceAddresses(new Variable(VariableType::tArray));
		deviceAddresses->arrayValue->push_back(PVariable(new Variable(peer->getSerialNumber())));
		PHomegearDevice rpcDevice = peer->getRpcDevice();
		for(Functions::iterator i = rpcDevice->functions.begin(); i != rpcDevice->functions.end(); ++i)
		{
			deviceAddresses->arrayValue->push_back(PVariable(new Variable(peer->getSerialNumber() + ":" + std::to_string(i->first))));
		}
		PVariable deviceInfo(new Variable(VariableType::tStruct));
		deviceInfo->structValue->insert(StructElement("ID", PVariable(new Variable((int32_t)peer->getID()))));
		PVariable channels(new Variable(VariableType::tArray));
		deviceInfo->structValue->insert(StructElement("CHANNELS", channels));
		for(Functions::iterator i = rpcDevice->functions.begin(); i != rpcDevice->functions.end(); ++i)
		{
			channels->arrayValue->push_back(PVariable(new Variable(i->first)));
		}
        std::vector<uint64_t> newIds{ id };
		raiseRPCDeleteDevices(newIds, deviceAddresses, deviceInfo);

		uint64_t virtualPeerId = peer->getVirtualPeerId();
		peer->getPhysicalInterface()->removePeer(peer->getAddress());

		{
			std::lock_guard<std::mutex> peersGuard(_peersMutex);
			if(_peersBySerial.find(peer->getSerialNumber()) != _peersBySerial.end()) _peersBySerial.erase(peer->getSerialNumber());
			if(_peersById.find(id) != _peersById.end()) _peersById.erase(id);
			if(_peers.find(peer->getAddress()) != _peers.end()) _peers.erase(peer->getAddress());
		}

		removePeerFromTeam(peer);

		int32_t i = 0;
		while(peer.use_count() > 1 && i < 600)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			i++;
		}
		if(i == 600) GD::out.printError("Error: Peer deletion took too long.");

		peer->deleteFromDatabase();
		GD::out.printMessage("Removed HomeMatic BidCoS peer " + std::to_string(peer->getID()));
		peer.reset();
		if(virtualPeerId > 0) deletePeer(virtualPeerId);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::reset(uint64_t id, bool defer)
{
	try
	{
		std::shared_ptr<BidCoSPeer> peer(getPeer(id));
		if(!peer || peer->isTeam()) return;
		std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.createQueue(peer->getPhysicalInterface(), BidCoSQueueType::UNPAIRING, peer->getAddress());
		std::shared_ptr<BidCoSQueue> pendingQueue(new BidCoSQueue(peer->getPhysicalInterface(), BidCoSQueueType::UNPAIRING));
		pendingQueue->noSending = true;

		uint8_t configByte = 0xA0;
		if(peer->getRXModes() & HomegearDevice::ReceiveModes::wakeOnRadio) configByte |= 0x10;

		std::vector<uint8_t> payload;

		//CONFIG_START
		payload.push_back(0x04);
		payload.push_back(0x00);
		std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(getMessageCounter(), configByte, 0x11, _address, peer->getAddress(), payload));
		pendingQueue->push(configPacket);
		pendingQueue->push(_messages->find(0x02));

		if(defer)
		{
			while(!peer->pendingBidCoSQueues->empty()) peer->pendingBidCoSQueues->pop();
			peer->pendingBidCoSQueues->push(pendingQueue);
			peer->serviceMessages->setConfigPending(true);
			queue->push(peer->pendingBidCoSQueues);
		}
		else queue->push(pendingQueue, true, true);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::unpair(uint64_t id, bool defer)
{
	try
	{
		std::shared_ptr<BidCoSPeer> peer(getPeer(id));
		if(!peer || peer->isTeam()) return;
		std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.createQueue(peer->getPhysicalInterface(), BidCoSQueueType::UNPAIRING, peer->getAddress());
		std::shared_ptr<BidCoSQueue> pendingQueue(new BidCoSQueue(peer->getPhysicalInterface(), BidCoSQueueType::UNPAIRING));
		pendingQueue->noSending = true;

		uint8_t configByte = 0xA0;
		if(peer->getRXModes() & HomegearDevice::ReceiveModes::wakeOnRadio) configByte |= 0x10;
		std::vector<uint8_t> payload;

		//CONFIG_START
		payload.push_back(0);
		payload.push_back(0x05);
		payload.push_back(0);
		payload.push_back(0);
		payload.push_back(0);
		payload.push_back(0);
		payload.push_back(0);
		std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(getMessageCounter(), configByte, 0x01, _address, peer->getAddress(), payload));
		pendingQueue->push(configPacket);
		pendingQueue->push(_messages->find(0x02));
		payload.clear();

		//CONFIG_WRITE_INDEX
		payload.push_back(0);
		payload.push_back(0x08);
		payload.push_back(0x02);
		payload.push_back(0);
		payload.push_back(0x0A);
		payload.push_back(0);
		payload.push_back(0x0B);
		payload.push_back(0);
		payload.push_back(0x0C);
		payload.push_back(0);
		configPacket = std::shared_ptr<BidCoSPacket>(new BidCoSPacket(getMessageCounter(), 0xA0, 0x01, _address, peer->getAddress(), payload));
		pendingQueue->push(configPacket);
		pendingQueue->push(_messages->find(0x02));
		payload.clear();

		//END_CONFIG
		payload.push_back(0);
		payload.push_back(0x06);
		configPacket = std::shared_ptr<BidCoSPacket>(new BidCoSPacket(getMessageCounter(), 0xA0, 0x01, _address, peer->getAddress(), payload));
		pendingQueue->push(configPacket);
		pendingQueue->push(_messages->find(0x02));
		payload.clear();

		if(defer)
		{
			while(!peer->pendingBidCoSQueues->empty()) peer->pendingBidCoSQueues->pop();
			peer->pendingBidCoSQueues->push(pendingQueue);
			peer->serviceMessages->setConfigPending(true);
			queue->push(peer->pendingBidCoSQueues);
		}
		else queue->push(pendingQueue, true, true);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::handlePairingRequest(const std::string& interfaceId, int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet)
{
	try
	{
		if(packet->destinationAddress() != 0 && packet->destinationAddress() != _address)
		{
            std::lock_guard<std::mutex> newPeersGuard(_newPeersMutex);
            _pairingMessages.emplace_back(std::make_shared<PairingMessage>("l10n.homematicBidcos.pairing.alreadyPaired", std::list<std::string>{ BaseLib::HelperFunctions::getHexString(packet->destinationAddress()) }));
			GD::out.printError("Error: Pairing packet rejected, because this peer is already paired to central with address 0x" + BaseLib::HelperFunctions::getHexString(packet->destinationAddress(), 6) + ".");
			return;
		}
		if(packet->payload().size() < 17)
		{
			GD::out.printError("Error: Pairing packet is too small (payload size has to be at least 17).");
			return;
		}

		std::string serialNumber;
		serialNumber.reserve(10);
		for(uint32_t i = 3; i < 13; i++)
		{
			serialNumber.push_back((char)packet->payload().at(i));
		}
		uint32_t deviceType = (packet->payload().at(1) << 8) + packet->payload().at(2);

		std::shared_ptr<BidCoSPeer> peer(getPeer(packet->senderAddress()));
		if(peer && (peer->getSerialNumber() != serialNumber || peer->getDeviceType() != deviceType))
		{
            std::lock_guard<std::mutex> newPeersGuard(_newPeersMutex);
            _pairingMessages.emplace_back(std::make_shared<PairingMessage>("l10n.homematicBidcos.pairing.samePeer"));
			GD::out.printError("Error: Pairing packet rejected, because a peer with the same address but different serial number or device type is already paired to this central.");
			return;
		}

		if((packet->controlByte() & 0x20) && packet->destinationAddress() == _address) sendOK(packet->messageCounter(), packet->senderAddress());

		std::vector<uint8_t> payload;

		std::shared_ptr<BidCoSQueue> queue;
		PHomegearDevice rpcDevice;
		if(!peer && _pairing)
		{
			queue = _bidCoSQueueManager.createQueue(getPhysicalInterface(packet->senderAddress()), BidCoSQueueType::PAIRING, packet->senderAddress());

			//Do not save here
			queue->peer = createPeer(packet->senderAddress(), packet->payload().at(0), deviceType, serialNumber, 0, 0, packet, false);
			if(!queue->peer)
			{
                std::lock_guard<std::mutex> newPeersGuard(_newPeersMutex);
                _pairingMessages.emplace_back(std::make_shared<PairingMessage>("l10n.homematicBidcos.pairing.unsupportedDeviceType", std::list<std::string>{ BaseLib::HelperFunctions::getHexString(deviceType, 4) }));
				GD::out.printWarning("Warning: Device type not supported: 0x" + BaseLib::HelperFunctions::getHexString(deviceType, 4) + ", firmware version: 0x" + BaseLib::HelperFunctions::getHexString(packet->payload().at(0), 2) + ". Sender address 0x" + BaseLib::HelperFunctions::getHexString(packet->senderAddress(), 6) + ".");
				return;
			}
			peer = queue->peer;
			rpcDevice = peer->getRpcDevice();
			if(!rpcDevice)
			{
                std::lock_guard<std::mutex> newPeersGuard(_newPeersMutex);
                _pairingMessages.emplace_back(std::make_shared<PairingMessage>("l10n.homematicBidcos.pairing.unsupportedDeviceType", std::list<std::string>{ BaseLib::HelperFunctions::getHexString(deviceType, 4) }));
				GD::out.printWarning("Warning: Device type not supported. Sender address 0x" + BaseLib::HelperFunctions::getHexString(packet->senderAddress(), 6) + ".");
				return;
			}
			peer->setInterface(std::make_shared<RpcClientInfo>(), interfaceId);
			peer->getPhysicalInterface()->addPeer(peer->getPeerInfo());

			//CONFIG_START
			payload.push_back(0);
			payload.push_back(0x05);
			payload.push_back(0);
			payload.push_back(0);
			payload.push_back(0);
			payload.push_back(0);
			payload.push_back(0);
			std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(getMessageCounter(), 0xA0, 0x01, _address, packet->senderAddress(), payload));
			queue->push(configPacket);
			queue->push(_messages->find(0x02));
			payload.clear();

			//CONFIG_WRITE_INDEX
			payload.push_back(0);
			payload.push_back(0x08);
			payload.push_back(0x02);
			PParameter internalKeysVisible = rpcDevice->functions.at(0)->configParameters->getParameter("INTERNAL_KEYS_VISIBLE");
			if(internalKeysVisible)
			{
				std::vector<uint8_t> data;
				data.push_back(1);
				internalKeysVisible->adjustBitPosition(data);
				payload.push_back(data.at(0) | 0x01);
			}
			else payload.push_back(0x01);
			payload.push_back(0x0A);
			payload.push_back(_address >> 16);
			payload.push_back(0x0B);
			payload.push_back((_address >> 8) & 0xFF);
			payload.push_back(0x0C);
			payload.push_back(_address & 0xFF);
			configPacket = std::shared_ptr<BidCoSPacket>(new BidCoSPacket(getMessageCounter(), 0xA0, 0x01, _address, packet->senderAddress(), payload));
			queue->push(configPacket);
			queue->push(_messages->find(0x02));
			payload.clear();

			//END_CONFIG
			payload.push_back(0);
			payload.push_back(0x06);
			configPacket = std::shared_ptr<BidCoSPacket>(new BidCoSPacket(getMessageCounter(), 0xA0, 0x01, _address, packet->senderAddress(), payload));
			queue->push(configPacket);
			queue->push(_messages->find(0x02));
			payload.clear();

			//Don't check for rxModes here! All rxModes are allowed.
			//if(!peerExists(packet->senderAddress())) //Only request config when peer is not already paired to central
			//{
				for(Functions::iterator i = rpcDevice->functions.begin(); i != rpcDevice->functions.end(); ++i)
				{
					std::shared_ptr<BidCoSQueue> pendingQueue;
					int32_t channel = i->first;
					//Walk through all lists to request master config if necessary
					if(!rpcDevice->functions.at(channel)->configParameters->parameters.empty())
					{
						PParameterGroup masterSet = rpcDevice->functions.at(channel)->configParameters;
						for(Lists::iterator k = masterSet->lists.begin(); k != masterSet->lists.end(); ++k)
						{
							pendingQueue.reset(new BidCoSQueue(peer->getPhysicalInterface(), BidCoSQueueType::CONFIG));
							pendingQueue->noSending = true;
							payload.push_back(channel);
							payload.push_back(0x04);
							payload.push_back(0);
							payload.push_back(0);
							payload.push_back(0);
							payload.push_back(0);
							payload.push_back(k->first);
							configPacket = std::shared_ptr<BidCoSPacket>(new BidCoSPacket(getMessageCounter(), 0xA0, 0x01, _address, packet->senderAddress(), payload));
							pendingQueue->push(configPacket);
							pendingQueue->push(_messages->find(0x10));
							payload.clear();
							peer->pendingBidCoSQueues->push(pendingQueue);
							peer->serviceMessages->setConfigPending(true);
						}
					}

					if(!rpcDevice->functions[channel]->linkReceiverFunctionTypes.empty() || !rpcDevice->functions[channel]->linkSenderFunctionTypes.empty())
					{
						pendingQueue.reset(new BidCoSQueue(peer->getPhysicalInterface(), BidCoSQueueType::CONFIG));
						pendingQueue->noSending = true;
						payload.push_back(channel);
						payload.push_back(0x03);
						configPacket = std::shared_ptr<BidCoSPacket>(new BidCoSPacket(getMessageCounter(), 0xA0, 0x01, _address, packet->senderAddress(), payload));
						pendingQueue->push(configPacket);
						pendingQueue->push(_messages->find(0x10));
						payload.clear();
						peer->pendingBidCoSQueues->push(pendingQueue);
						peer->serviceMessages->setConfigPending(true);
					}
				}
			//}
		}
		//not in pairing mode
		else queue = _bidCoSQueueManager.createQueue(peer->getPhysicalInterface(), BidCoSQueueType::DEFAULT, packet->senderAddress());

		if(!peer)
		{
            std::lock_guard<std::mutex> newPeersGuard(_newPeersMutex);
            _pairingMessages.emplace_back(std::make_shared<PairingMessage>("l10n.homematicBidcos.pairing.unknownError"));
			GD::out.printError("Error handling pairing packet: Peer is nullptr. This shouldn't have happened. Something went very wrong.");
			return;
		}

		if(peer->pendingBidCoSQueues && !peer->pendingBidCoSQueues->empty()) GD::out.printInfo("Info: Pushing pending queues.");
		queue->push(peer->pendingBidCoSQueues); //This pushes the just generated queue and the already existent pending queue onto the queue
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::handleTimeRequest(const std::string& interfaceId, int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet)
{
	try
	{
		std::vector<uint8_t> payload;
		payload.push_back(0x02);
		const auto timePoint = std::chrono::system_clock::now();
		time_t t = std::chrono::system_clock::to_time_t(timePoint);
		std::tm localTime;
		localtime_r(&t, &localTime);
		uint32_t time = (uint32_t)(t - 946684800);
		payload.push_back(localTime.tm_gmtoff / 1800);
		payload.push_back(time >> 24);
		payload.push_back((time >> 16) & 0xFF);
		payload.push_back((time >> 8) & 0xFF);
		payload.push_back(time & 0xFF);
		std::shared_ptr<BidCoSPacket> timePacket(new BidCoSPacket(messageCounter, 0x80, 0x3F, _address, packet->senderAddress(), payload));
		sendPacket(getPhysicalInterface(packet->senderAddress()), timePacket);
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::sendOK(int32_t messageCounter, int32_t destinationAddress, std::vector<uint8_t> payload)
{
	try
	{
		if(payload.empty()) payload.push_back(0x00);
		std::shared_ptr<BidCoSPacket> ok(new BidCoSPacket(messageCounter, 0x80, 0x02, _address, destinationAddress, payload));
		sendPacket(getPhysicalInterface(destinationAddress), ok);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}


void HomeMaticCentral::sendRequestConfig(int32_t address, uint8_t localChannel, uint8_t list, int32_t remoteAddress, uint8_t remoteChannel)
{
	try
	{
		std::shared_ptr<BidCoSPeer> peer(getPeer(address));
		if(!peer) return;
		bool oldQueue = true;
		std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.get(address);
		if(!queue)
		{
			oldQueue = false;
			queue = _bidCoSQueueManager.createQueue(peer->getPhysicalInterface(), BidCoSQueueType::CONFIG, address);
		}
		std::shared_ptr<BidCoSQueue> pendingQueue(new BidCoSQueue(peer->getPhysicalInterface(), BidCoSQueueType::CONFIG));
		pendingQueue->noSending = true;

		std::vector<uint8_t> payload;

		//CONFIG_WRITE_INDEX
		payload.push_back(localChannel);
		payload.push_back(0x04);
		payload.push_back(remoteAddress >> 16);
		payload.push_back((remoteAddress >> 8) & 0xFF);
		payload.push_back(remoteAddress & 0xFF);
		payload.push_back(remoteChannel);
		payload.push_back(list);
		std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(getMessageCounter(), 0xA0, 0x01, _address, address, payload));
		pendingQueue->push(packet);
		pendingQueue->push(_messages->find(0x10));
		payload.clear();

		peer->pendingBidCoSQueues->push(pendingQueue);
		peer->serviceMessages->setConfigPending(true);
		if(!oldQueue) queue->push(peer->pendingBidCoSQueues);
		else if(queue->pendingQueuesEmpty()) queue->push(peer->pendingBidCoSQueues);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::handleConfigParamResponse(const std::string& interfaceId, int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet)
{
	try
	{
		std::shared_ptr<BidCoSPeer> peer(getPeer(packet->senderAddress()));
		if(!peer) return;
		PHomegearDevice rpcDevice = peer->getRpcDevice();
		//Config changed in device
		if(packet->payload().size() > 7 && packet->payload().at(0) == 0x05)
		{
			if(packet->controlByte() & 0x20) sendOK(packet->messageCounter(), packet->senderAddress());
			if(packet->payload().size() == 8) return; //End packet
			int32_t list = packet->payload().at(6);
			int32_t channel = packet->payload().at(1); //??? Not sure if this really is the channel
			int32_t remoteAddress = (packet->payload().at(2) << 16) + (packet->payload().at(3) << 8) + packet->payload().at(4);
			int32_t remoteChannel = (remoteAddress == 0) ? 0 : packet->payload().at(5);
			ParameterGroup::Type::Enum type = (remoteAddress != 0) ? ParameterGroup::Type::link : ParameterGroup::Type::config;
			int32_t startIndex = packet->payload().at(7);
			int32_t endIndex = startIndex + packet->payload().size() - 9;
			Functions::iterator functionIterator = rpcDevice->functions.find(channel);
			if(functionIterator == rpcDevice->functions.end())
			{
				GD::out.printError("Error: Received config for non existant parameter set.");
				return;
			}
			PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
			if(!parameterGroup || parameterGroup->parameters.empty())
			{
				GD::out.printError("Error: Received config for non existant parameter set.");
				return;
			}
			std::vector<PParameter> packetParameters;
			parameterGroup->getIndices(startIndex, endIndex, list, packetParameters);
			for(std::vector<PParameter>::iterator i = packetParameters.begin(); i != packetParameters.end(); ++i)
			{
				if(!(*i)->id.empty())
				{
					double position = ((*i)->physical->index - startIndex) + 8 + 9;
					if(position < 0)
					{
						GD::out.printError("Error: Packet position is negative. Device: " + BaseLib::HelperFunctions::getHexString(peer->getAddress()) + " Serial number: " + peer->getSerialNumber() + " Channel: " + std::to_string(channel) + " List: " + std::to_string((*i)->physical->list) + " Parameter index: " + std::to_string((*i)->physical->index));
						continue;
					}
					BaseLib::Systems::RpcConfigurationParameter* parameter = nullptr;
					if(type == ParameterGroup::Type::config) parameter = &peer->configCentral[channel][(*i)->id];
					//type == link
					else if(peer->getPeer(channel, remoteAddress, remoteChannel)) parameter = &peer->linksCentral[channel][remoteAddress][remoteChannel][(*i)->id];
					if(!parameter) continue;

					if(position < 9 + 8)
					{
						uint32_t byteOffset = 9 + 8 - position;
						uint32_t missingBytes = (*i)->physical->size - byteOffset;
						if(missingBytes >= (*i)->physical->size)
						{
							GD::out.printError("Error: Device tried to set parameter with more bytes than specified. Device: " + BaseLib::HelperFunctions::getHexString(peer->getAddress()) + " Serial number: " + peer->getSerialNumber() + " Channel: " + std::to_string(channel) + " List: " + std::to_string((*i)->physical->list) + " Parameter index: " + std::to_string((*i)->physical->index));
							continue;
						}
						std::vector<uint8_t> parameterData = parameter->getBinaryData();
						std::vector<uint8_t> partialParameterData = parameter->getPartialBinaryData();
						while(partialParameterData.size() < (*i)->physical->size) partialParameterData.push_back(0);
						position = 9 + 8;
						std::vector<uint8_t> data = packet->getPosition(position, missingBytes, (*i)->physical->mask);
						for(uint32_t j = 0; j < byteOffset; j++) parameterData.at(j) = partialParameterData.at(j);
						for(uint32_t j = byteOffset; j < (*i)->physical->size; j++) parameterData.at(j) = data.at(j - byteOffset);
						parameter->setPartialBinaryData(partialParameterData);
						parameter->setBinaryData(parameterData);
						//Don't clear partialData - packet might be resent
						peer->saveParameter(parameter->databaseId, type, channel, (*i)->id, parameterData, remoteAddress, remoteChannel);
						if(_bl->debugLevel >= 4) GD::out.printInfo("Info: Parameter " + (*i)->id + " of device 0x" + BaseLib::HelperFunctions::getHexString(peer->getAddress()) + " at index " + std::to_string((*i)->physical->index) + " and packet index " + std::to_string(position) + " with size " + std::to_string((*i)->physical->size) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(parameterData) + " after being partially set in the last packet.");
					}
					else if(position + (int32_t)(*i)->physical->size >= packet->length())
					{
						std::vector<uint8_t> partialParameterData = packet->getPosition(position, (*i)->physical->size, (*i)->physical->mask);
						parameter->setPartialBinaryData(partialParameterData);
						if(_bl->debugLevel >= 4) GD::out.printInfo("Info: Parameter " + (*i)->id + " of device 0x" + BaseLib::HelperFunctions::getHexString(peer->getAddress()) + " at index " + std::to_string((*i)->physical->index) + " and packet index " + std::to_string(position) + " with size " + std::to_string((*i)->physical->size) + " was partially set to 0x" + BaseLib::HelperFunctions::getHexString(partialParameterData) + ".");
					}
					else
					{
						std::vector<uint8_t> parameterData = packet->getPosition(position, (*i)->physical->size, (*i)->physical->mask);
						parameter->setBinaryData(parameterData);
						peer->saveParameter(parameter->databaseId, type, channel, (*i)->id, parameterData, remoteAddress, remoteChannel);
						if(_bl->debugLevel >= 4) GD::out.printInfo("Info: Parameter " + (*i)->id + " of device 0x" + BaseLib::HelperFunctions::getHexString(peer->getAddress()) + " at index " + std::to_string((*i)->physical->index) + " and packet index " + std::to_string(position) + " with size " + std::to_string((*i)->physical->size) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(parameterData) + ".");
					}
				}
				else GD::out.printError("Error: Device tried to set parameter without id. Device: " + BaseLib::HelperFunctions::getHexString(peer->getAddress()) + " Serial number: " + peer->getSerialNumber() + " Channel: " + std::to_string(channel) + " List: " + std::to_string((*i)->physical->list) + " Parameter index: " + std::to_string((*i)->physical->index));
			}
			return;
		}
		std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.get(packet->senderAddress());
		if(!queue || queue->isEmpty()) return;
		if(packet->controlByte() & 0x04) return; //Ignore broadcast packets
		//Config was requested by central
		std::shared_ptr<BidCoSPacket> sentPacket(_sentPackets.get(packet->senderAddress()));
		bool continuousData = false;
		bool multiPacket = false;
		bool multiPacketEnd = false;
		//Peer request
		if(sentPacket && sentPacket->payload().size() >= 2 && sentPacket->payload().at(1) == 0x03)
		{
			int32_t localChannel = sentPacket->payload().at(0);
			PFunction rpcFunction = rpcDevice->functions[localChannel];
			bool peerFound = false;
			if(packet->payload().size() >= 5)
			{
				for(uint32_t i = 1; i < packet->payload().size() - 1; i += 4)
				{
					int32_t peerAddress = (packet->payload().at(i) << 16) + (packet->payload().at(i + 1) << 8) + packet->payload().at(i + 2);
					if(peerAddress != 0)
					{
						peerFound = true;
						int32_t remoteChannel = packet->payload().at(i + 3);
						if(rpcFunction->hasGroup) //Peer is team
						{
							//Don't add team if address is peer's, because then the team already exists
							if(peerAddress != peer->getAddress())
							{
								GD::out.printInfo("Info: Adding peer 0x" + BaseLib::HelperFunctions::getHexString(peer->getAddress()) + " to group with address 0x" + BaseLib::HelperFunctions::getHexString(peerAddress) + ".");
								addPeerToTeam(peer, localChannel, peerAddress, remoteChannel);
							}
						}
						else //Normal peer
						{
							std::shared_ptr<BaseLib::Systems::BasicPeer> newPeer(new BaseLib::Systems::BasicPeer());
							newPeer->address = peerAddress;
							newPeer->channel = remoteChannel;
							peer->addPeer(localChannel, newPeer);
							if(rpcDevice->functions.find(localChannel) == rpcDevice->functions.end()) continue;

							PParameterGroup parameterGroup = rpcFunction->getParameterGroup(ParameterGroup::Type::Enum::link);
							if(!parameterGroup || parameterGroup->parameters.empty()) continue;
							for(Lists::iterator k = parameterGroup->lists.begin(); k != parameterGroup->lists.end(); ++k)
							{
								sendRequestConfig(peer->getAddress(), localChannel, k->first, newPeer->address, newPeer->channel);
							}
						}
					}
					else if(i == 1 && rpcFunction->hasGroup)
					{
						//Peer (smoke detector) has team but no peer. Add peer to it's own team:
						GD::out.printInfo("Info: Peer has no group set. Resetting group to default.");
						setTeam(nullptr, peer->getID(), -1, 0, -1, true, true);
					}
				}
			}
			if(rpcFunction->hasGroup && !peerFound)
			{
				//Peer has no team yet so set it needs to be defined
				setTeam(nullptr, peer->getSerialNumber(), localChannel, "", 0, true, false);
			}
			if(packet->payload().size() >= 2 && packet->payload().at(0) == 0x03 && packet->payload().at(1) == 0x00)
			{
				//multiPacketEnd was received unexpectedly. Because of the delay it was received after the peer request packet was sent.
				//As the peer request is popped already, queue it again.
				//Example from HM-LC-Sw1PBU-FM:
				//1375093199833 Sending: 1072A001FD00011BD5D100040000000000
				//1375093199988 Received: 1472A0101BD5D1FD00010202810AFD0B000C0115FF <= not a multi packet response
				//1375093200079 Sending: 0A728002FD00011BD5D100 <= After the ok we are not expecting another packet
				//1375093200171 Sending: 0B73A001FD00011BD5D10103 <= So we are sending the peer request
				//1375093200227 Received: 0C73A0101BD5D1FD0001030000 <= And a little later receive multiPacketEnd unexpectedly
				std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.get(packet->senderAddress());
				if(queue)
				{
					queue->pushFront(sentPacket);
					return;
				}
			}
		}
		else if(sentPacket && sentPacket->payload().size() >= 7 && sentPacket->payload().at(1) == 0x04) //Config request
		{
			int32_t channel = sentPacket->payload().at(0);
			int32_t list = sentPacket->payload().at(6);
			int32_t remoteAddress = (sentPacket->payload().at(2) << 16) + (sentPacket->payload().at(3) << 8) + sentPacket->payload().at(4);
			int32_t remoteChannel = (remoteAddress == 0) ? 0 : sentPacket->payload().at(5);
			ParameterGroup::Type::Enum type = (remoteAddress != 0) ? ParameterGroup::Type::link : ParameterGroup::Type::config;
			PVariable parametersToEnforce;
			if(!peer->getPairingComplete()) parametersToEnforce.reset(new Variable(VariableType::tStruct));
			if((packet->controlByte() & 0x20) && (packet->payload().at(0) == 3)) continuousData = true;
			if(!continuousData && (packet->payload().at(packet->payload().size() - 2) != 0 || packet->payload().at(packet->payload().size() - 1) != 0)) multiPacket = true;
			//Some devices have a payload size of 3
			if(continuousData && packet->payload().size() == 3 && packet->payload().at(1) == 0 && packet->payload().at(2) == 0) multiPacketEnd = true;
			//And some a payload size of 2
			if(continuousData && packet->payload().size() == 2 && packet->payload().at(1) == 0) multiPacketEnd = true;
			if(continuousData && !multiPacketEnd)
			{
				int32_t startIndex = packet->payload().at(1);
				int32_t endIndex = startIndex + packet->payload().size() - 3;
				Functions::iterator functionIterator = rpcDevice->functions.find(channel);
				PParameterGroup parameterGroup;
				if(functionIterator != rpcDevice->functions.end()) parameterGroup = functionIterator->second->getParameterGroup(type);
				if(!parameterGroup || parameterGroup->parameters.empty())
				{
					GD::out.printError("Error: Received config for non existant parameter set.");
				}
				else
				{
					std::vector<PParameter> packetParameters;
					parameterGroup->getIndices(startIndex, endIndex, list, packetParameters);
					for(std::vector<PParameter>::iterator i = packetParameters.begin(); i != packetParameters.end(); ++i)
					{
						if(!(*i)->id.empty())
						{
							double position = ((*i)->physical->index - startIndex) + 2 + 9;
							BaseLib::Systems::RpcConfigurationParameter* parameter = nullptr;
							if(type == ParameterGroup::Type::config) parameter = &peer->configCentral[channel][(*i)->id];
							//type == link
							else if(peer->getPeer(channel, remoteAddress, remoteChannel)) parameter = &peer->linksCentral[channel][remoteAddress][remoteChannel][(*i)->id];
							if(!parameter) continue;
							if(position < 9 + 2)
							{
								uint32_t byteOffset = 9 + 2 - position;
								uint32_t missingBytes = (*i)->physical->size - byteOffset;
								if(missingBytes >= (*i)->physical->size)
								{
									GD::out.printError("Error: Device tried to set parameter with more bytes than specified. Device: " + BaseLib::HelperFunctions::getHexString(peer->getAddress()) + " Serial number: " + peer->getSerialNumber() + " Channel: " + std::to_string(channel) + " List: " + std::to_string((*i)->physical->list) + " Parameter index: " + std::to_string((*i)->physical->index));
									continue;
								}
								std::vector<uint8_t> parameterData = parameter->getBinaryData();
								std::vector<uint8_t> partialParameterData = parameter->getPartialBinaryData();
								while(partialParameterData.size() < (*i)->physical->size) partialParameterData.push_back(0);
								position = 9 + 2;
								std::vector<uint8_t> data = packet->getPosition(position, missingBytes, (*i)->physical->mask);
								for(uint32_t j = 0; j < byteOffset; j++)
								{
									if(j >= parameterData.size()) parameterData.push_back(partialParameterData.at(j));
									else parameterData.at(j) = partialParameterData.at(j);
								}
								for(uint32_t j = byteOffset; j < (*i)->physical->size; j++)
								{
									if(j >= parameterData.size()) parameterData.push_back(data.at(j - byteOffset));
									else parameterData.at(j) = data.at(j - byteOffset);
								}
								parameter->setBinaryData(parameterData);
								parameter->setPartialBinaryData(partialParameterData);
								//Don't clear partialData - packet might be resent
								peer->saveParameter(parameter->databaseId, type, channel, (*i)->id, parameterData, remoteAddress, remoteChannel);
								if(type == ParameterGroup::Type::config && !peer->getPairingComplete() && (*i)->logical->setToValueOnPairingExists)
								{
									parametersToEnforce->structValue->insert(StructElement((*i)->id, (*i)->logical->getSetToValueOnPairing()));
								}
								if(_bl->debugLevel >= 5) GD::out.printDebug("Debug: Parameter " + (*i)->id + " of device 0x" + BaseLib::HelperFunctions::getHexString(peer->getAddress()) + " at index " + std::to_string((*i)->physical->index) + " and packet index " + std::to_string(position) + " with size " + std::to_string((*i)->physical->size) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(parameterData) + " after being partially set in the last packet.");
							}
							else if(position + (int32_t)(*i)->physical->size >= packet->length())
							{
								std::vector<uint8_t> partialParameterData = packet->getPosition(position, (*i)->physical->size, (*i)->physical->mask);
								parameter->setPartialBinaryData(partialParameterData);
								if(_bl->debugLevel >= 5) GD::out.printDebug("Debug: Parameter " + (*i)->id + " of device 0x" + BaseLib::HelperFunctions::getHexString(peer->getAddress()) + " at index " + std::to_string((*i)->physical->index) + " and packet index " + std::to_string(position) + " with size " + std::to_string((*i)->physical->size) + " was partially set to 0x" + BaseLib::HelperFunctions::getHexString(partialParameterData) + ".");
							}
							else
							{
								std::vector<uint8_t> parameterData = packet->getPosition(position, (*i)->physical->size, (*i)->physical->mask);
								parameter->setBinaryData(parameterData);
								peer->saveParameter(parameter->databaseId, type, channel, (*i)->id, parameterData, remoteAddress, remoteChannel);
								if(type == ParameterGroup::Type::config && !peer->getPairingComplete() && (*i)->logical->setToValueOnPairingExists)
								{
									parametersToEnforce->structValue->insert(StructElement((*i)->id, (*i)->logical->getSetToValueOnPairing()));
								}
								if(_bl->debugLevel >= 5) GD::out.printDebug("Debug: Parameter " + (*i)->id + " of device 0x" + BaseLib::HelperFunctions::getHexString(peer->getAddress()) + " at index " + std::to_string((*i)->physical->index) + " and packet index " + std::to_string(position) + " with size " + std::to_string((*i)->physical->size) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(parameterData) + ".");
							}
						}
						else GD::out.printError("Error: Device tried to set parameter without id. Device: " + BaseLib::HelperFunctions::getHexString(peer->getAddress()) + " Serial number: " + peer->getSerialNumber() + " Channel: " + std::to_string(channel) + " List: " + std::to_string((*i)->physical->list) + " Parameter index: " + std::to_string((*i)->physical->index));
					}
				}
			}
			else if(!multiPacketEnd)
			{
				Functions::iterator functionIterator = rpcDevice->functions.find(channel);
				PParameterGroup parameterGroup;
				if(functionIterator != rpcDevice->functions.end()) parameterGroup = functionIterator->second->getParameterGroup(type);
				if(!parameterGroup || parameterGroup->parameters.empty())
				{
					GD::out.printError("Error: Received config for non existant parameter set.");
				}
				else
				{
					int32_t length = multiPacket ? packet->payload().size() : packet->payload().size() - 2;
					for(int32_t i = 1; i < length; i += 2)
					{
						int32_t index = packet->payload().at(i);
						std::vector<PParameter> packetParameters;
						parameterGroup->getIndices(index, index, list, packetParameters);
						for(std::vector<PParameter>::iterator j = packetParameters.begin(); j != packetParameters.end(); ++j)
						{
							if(!(*j)->id.empty())
							{
								double position = std::fmod((*j)->physical->index, 1) + 9 + i + 1;
								double size = (*j)->physical->size;
								if(size > 1.0) size = 1.0; //Reading more than one byte doesn't make any sense
								uint8_t data = packet->getPosition(position, size, (*j)->physical->mask).at(0);
								BaseLib::Systems::RpcConfigurationParameter* configParam = nullptr;
								if(type == ParameterGroup::Type::config)
								{
									configParam = &peer->configCentral[channel][(*j)->id];
								}
								else if(peer->getPeer(channel, remoteAddress, remoteChannel))
								{
									configParam = &peer->linksCentral[channel][remoteAddress][remoteChannel][(*j)->id];
								}
								if(configParam)
								{
									std::vector<uint8_t> parameterData = configParam->getBinaryData();
									while(index - (*j)->physical->startIndex >= parameterData.size())
									{
										parameterData.push_back(0);
									}
									parameterData.at(index - (*j)->physical->startIndex) = data;
									if(!peer->getPairingComplete() && (*j)->logical->setToValueOnPairingExists)
									{
										parametersToEnforce->structValue->insert(StructElement((*j)->id, (*j)->logical->getSetToValueOnPairing()));
									}
									configParam->setBinaryData(parameterData);
									peer->saveParameter(configParam->databaseId, type, channel, (*j)->id, parameterData, remoteAddress, remoteChannel);
									if(_bl->debugLevel >= 5) GD::out.printDebug("Debug: Parameter " + (*j)->id + " of device 0x" + BaseLib::HelperFunctions::getHexString(peer->getAddress()) + " at index " + std::to_string((*j)->physical->index) + " and packet index " + std::to_string(position) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(data) + ".");
								}
							}
							else GD::out.printError("Error: Device tried to set parameter without id. Device: " + BaseLib::HelperFunctions::getHexString(peer->getAddress()) + " Serial number: " + peer->getSerialNumber() + " Channel: " + std::to_string(channel) + " List: " + std::to_string((*j)->physical->list) + " Parameter index: " + std::to_string((*j)->physical->index));
						}
					}
				}
			}
			if(!peer->getPairingComplete() && !parametersToEnforce->structValue->empty()) peer->putParamset(nullptr, channel, type, 0, -1, parametersToEnforce, true);
		}
		if((continuousData || multiPacket) && !multiPacketEnd && (packet->controlByte() & 0x20)) //Multiple config response packets
		{
			//Sending stealthy does not set sentPacket
			//The packet is queued as it triggers the next response
			//If no response is coming, the packet needs to be resent
			std::vector<uint8_t> payload;
			payload.push_back(0x00);
			std::shared_ptr<BidCoSPacket> ok(new BidCoSPacket(packet->messageCounter(), 0x80, 0x02, _address, packet->senderAddress(), payload));
			queue->pushFront(ok, true, false);
			//No popping from queue to stay at the same position until all response packets are received!!!
			//The popping is done with the last packet, which has no BIDI control bit set. See https://sathya.de/HMCWiki/index.php/Examples:Message_Counter
		}
		else
		{
			//Sometimes the peer requires an ok after the end packet
			if((multiPacketEnd || !multiPacket) && (packet->controlByte() & 0x20))
			{
				//Send ok once. Do not queue it!!!
				sendOK(packet->messageCounter(), packet->senderAddress());
			}
			queue->pop(); //Messages are not popped by default.
		}
		if(queue->isEmpty())
		{
			if(!peer->getPairingComplete())
			{
				addHomegearFeatures(peer, -1, true);
				peer->setPairingComplete(true);
			}
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::resetTeam(std::shared_ptr<BidCoSPeer> peer, uint32_t channel)
{
	try
	{
		removePeerFromTeam(peer);

		std::string teamSerialNumber('*' + peer->getSerialNumber());
		std::shared_ptr<BidCoSPeer> team = getPeer(teamSerialNumber);
		bool teamCreated = false;
		PHomegearDevice rpcDevice = peer->getRpcDevice();
		if(!team)
		{
			team = createTeam(peer->getAddress(), peer->getDeviceType(), teamSerialNumber);
			team->setRpcDevice(rpcDevice->group);
			team->setID(peer->getID() | (1 << 30));
			team->initializeCentralConfig();
			_peersMutex.lock();
			_peersBySerial[team->getSerialNumber()] = team;
			_peersById[team->getID()] = team;
			_peersMutex.unlock();
			teamCreated = true;
		}
		peer->setTeamRemoteAddress(team->getAddress());
		peer->setTeamRemoteID(team->getID());
		peer->setTeamRemoteSerialNumber(team->getSerialNumber());
		peer->setTeamChannel(channel);

		PHomegearDevice teamRpcDevice = team->getRpcDevice();
		for(Functions::iterator j = teamRpcDevice->functions.begin(); j != teamRpcDevice->functions.end(); ++j)
		{
			if(j->second->groupId == rpcDevice->functions.at(channel)->groupId)
			{
				peer->setTeamRemoteChannel(j->first);
				break;
			}
		}
		team->teamChannels.push_back(std::pair<std::string, uint32_t>(peer->getSerialNumber(), channel));
		if(teamCreated)
		{
			PVariable deviceDescriptions(new Variable(VariableType::tArray));
			deviceDescriptions->arrayValue = team->getDeviceDescriptions(nullptr, true, std::map<std::string, bool>());
			std::vector<uint64_t> newIds{ team->getID() };
			raiseRPCNewDevices(newIds, deviceDescriptions);
		}
		else raiseRPCUpdateDevice(team->getID(), peer->getTeamRemoteChannel(), team->getSerialNumber() + ":" + std::to_string(peer->getTeamRemoteChannel()), 2);
	}
	catch(const std::exception& ex)
    {
		_peersMutex.unlock();
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::addPeerToTeam(std::shared_ptr<BidCoSPeer> peer, int32_t channel, int32_t teamAddress, uint32_t teamChannel)
{
	try
	{
		std::shared_ptr<BidCoSPeer> teamPeer(getPeer(teamAddress));
		if(teamPeer)
		{
			//Team should be known
			addPeerToTeam(peer, channel, teamChannel, '*' + teamPeer->getSerialNumber());
		}
		else
		{
			removePeerFromTeam(peer);

			peer->setTeamRemoteAddress(teamAddress);
			peer->setTeamChannel(channel);
			peer->setTeamRemoteChannel(teamChannel);
			peer->setTeamRemoteID(0);
			peer->setTeamRemoteSerialNumber("");
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::addPeerToTeam(std::shared_ptr<BidCoSPeer> peer, int32_t channel, uint32_t teamChannel, std::string teamSerialNumber)
{
	try
	{
		std::shared_ptr<BidCoSPeer> team = getPeer(teamSerialNumber);
		if(!team) return;
		PHomegearDevice teamRpcDevice = team->getRpcDevice();
		Functions::iterator functionIterator = teamRpcDevice->functions.find(teamChannel);
		if(functionIterator == teamRpcDevice->functions.end()) return;
		if(functionIterator->second->groupId != teamRpcDevice->functions[channel]->groupId) return;

		removePeerFromTeam(peer);

		peer->setTeamRemoteAddress(team->getAddress());
		peer->setTeamRemoteID(team->getID());
		peer->setTeamRemoteSerialNumber(teamSerialNumber);
		peer->setTeamChannel(channel);
		peer->setTeamRemoteChannel(teamChannel);
		team->teamChannels.push_back(std::pair<std::string, uint32_t>(peer->getSerialNumber(), channel));
		raiseRPCUpdateDevice(team->getID(), teamChannel, team->getSerialNumber() + ":" + std::to_string(teamChannel), 2);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::removePeerFromTeam(std::shared_ptr<BidCoSPeer> peer)
{
	try
	{
		if(peer->getTeamRemoteSerialNumber().empty()) return;
		std::shared_ptr<BidCoSPeer> oldTeam = getPeer(peer->getTeamRemoteSerialNumber());
		if(!oldTeam) return; //No old team, e. g. on pairing
		//Remove peer from old team
		for(std::vector<std::pair<std::string, uint32_t>>::iterator i = oldTeam->teamChannels.begin(); i != oldTeam->teamChannels.end(); ++i)
		{
			if(i->first == peer->getSerialNumber() && (signed)i->second == peer->getTeamChannel())
			{
				oldTeam->teamChannels.erase(i);
				break;
			}
		}
		//Delete team if there are no peers anymore
		if(oldTeam->teamChannels.empty())
		{
			oldTeam->deleting = true;
			_peersMutex.lock();
			try
			{
				_peersBySerial.erase(oldTeam->getSerialNumber());
				_peersById.erase(oldTeam->getID());
			}
			catch(const std::exception& ex)
			{
				GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			_peersMutex.unlock();

			PVariable deviceAddresses(new Variable(VariableType::tArray));
			deviceAddresses->arrayValue->push_back(PVariable(new Variable(oldTeam->getSerialNumber())));
			PHomegearDevice teamRpcDevice = oldTeam->getRpcDevice();

			PVariable deviceInfo(new Variable(VariableType::tStruct));
			deviceInfo->structValue->insert(StructElement("ID", PVariable(new Variable((int32_t)oldTeam->getID()))));
			PVariable channels(new Variable(VariableType::tArray));
			deviceInfo->structValue->insert(StructElement("CHANNELS", channels));

			for(Functions::iterator i = teamRpcDevice->functions.begin(); i != teamRpcDevice->functions.end(); ++i)
			{
				deviceAddresses->arrayValue->push_back(PVariable(new Variable(oldTeam->getSerialNumber() + ":" + std::to_string(i->first))));
				channels->arrayValue->push_back(PVariable(new Variable(i->first)));
			}

			std::vector<uint64_t> deletedIds{ oldTeam->getID() };
			raiseRPCDeleteDevices(deletedIds, deviceAddresses, deviceInfo);
		}
		else raiseRPCUpdateDevice(oldTeam->getID(), peer->getTeamRemoteChannel(), oldTeam->getSerialNumber() + ":" + std::to_string(peer->getTeamRemoteChannel()), 2);
		peer->setTeamRemoteSerialNumber("");
		peer->setTeamRemoteID(0);
		peer->setTeamRemoteAddress(0);
		peer->setTeamRemoteChannel(0);
		return;
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void HomeMaticCentral::handleAck(const std::string& interfaceId, int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet)
{
	try
	{
		std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.get(packet->senderAddress());
		if(!queue) return;
		std::shared_ptr<BidCoSPacket> sentPacket(_sentPackets.get(packet->senderAddress()));
		if(packet->payload().at(0) == 0x80 || packet->payload().at(0) == 0x84)
		{
			if(_bl->debugLevel >= 2)
			{
				if(sentPacket) GD::out.printError("Error: NACK received from 0x" + BaseLib::HelperFunctions::getHexString(packet->senderAddress()) + " in response to " + sentPacket->hexString() + ".");
				else GD::out.printError("Error: NACK received from 0x" + BaseLib::HelperFunctions::getHexString(packet->senderAddress()));
				if(queue->getQueueType() == BidCoSQueueType::PAIRING) GD::out.printError("Try resetting the device to factory defaults before pairing it to this central.");
			}
			if(queue->getQueueType() == BidCoSQueueType::PAIRING) queue->clear(); //Abort
			else queue->pop(); //Otherwise the queue might persist forever. NACKS shouldn't be received when not pairing
			return;
		}
		bool aesKeyChanged = false;
		if(queue->getQueueType() == BidCoSQueueType::PAIRING)
		{
			if(sentPacket && sentPacket->messageType() == 0x01 && sentPacket->payload().at(0) == 0x00 && sentPacket->payload().at(1) == 0x06)
			{
				if(!peerExists(packet->senderAddress()))
				{
					if(!queue->peer) return;
					try
					{
						_peersMutex.lock();
						_peers[queue->peer->getAddress()] = queue->peer;
						if(!queue->peer->getSerialNumber().empty()) _peersBySerial[queue->peer->getSerialNumber()] = queue->peer;
						_peersMutex.unlock();
						queue->peer->save(true, true, false);
						queue->peer->initializeCentralConfig();
						_peersMutex.lock();
						_peersById[queue->peer->getID()] = queue->peer;
						_peersMutex.unlock();
					}
					catch(const std::exception& ex)
					{
						_peersMutex.unlock();
						GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
					}
					setInstallMode(nullptr, false, -1, nullptr, false);
					if(queue->peer->getRXModes() & HomegearDevice::ReceiveModes::wakeOnRadio) queue->setWakeOnRadioBit();
					PVariable deviceDescriptions(new Variable(VariableType::tArray));
					deviceDescriptions->arrayValue = queue->peer->getDeviceDescriptions(nullptr, true, std::map<std::string, bool>());
					std::vector<uint64_t> newIds{ queue->peer->getID() };
					raiseRPCNewDevices(newIds, deviceDescriptions);

                    {
                        auto pairingState = std::make_shared<PairingState>();
                        pairingState->peerId = queue->peer->getID();
                        pairingState->state = "success";
                        std::lock_guard<std::mutex> newPeersGuard(_newPeersMutex);
                        _newPeers[BaseLib::HelperFunctions::getTime()].emplace_back(std::move(pairingState));
                    }

                    GD::out.printMessage("Added peer 0x" + BaseLib::HelperFunctions::getHexString(queue->peer->getAddress()) + ".");
					std::shared_ptr<HomegearDevice> rpcDevice = queue->peer->getRpcDevice();
					for(Functions::iterator i = rpcDevice->functions.begin(); i != rpcDevice->functions.end(); ++i)
					{
						if(i->second->encryptionEnabledByDefault)
						{
							queue->peer->peerInfoPacketsEnabled = false;
							PVariable variables(new Variable(VariableType::tStruct));
							variables->structValue->insert(StructElement("AES_ACTIVE", PVariable(new Variable(true))));
							queue->peer->putParamset(nullptr, i->first, ParameterGroup::Type::config, 0, -1, variables, true);
							queue->peer->peerInfoPacketsEnabled = true;
						}
						if(i->second->hasGroup)
						{
							GD::out.printInfo("Info: Creating group for channel " + std::to_string(i->first) + ".");
							resetTeam(queue->peer, i->first);
							//Check if a peer exists with this devices team and if yes add it to the team.
							//As the serial number of the team was unknown up to this point, this couldn't
							//be done before.
							try
							{
								std::vector<std::shared_ptr<BidCoSPeer>> peers;
								_peersMutex.lock();
								for(std::map<uint64_t, std::shared_ptr<BaseLib::Systems::Peer>>::const_iterator j = _peersById.begin(); j != _peersById.end(); ++j)
								{
									std::shared_ptr<BidCoSPeer> peer(std::dynamic_pointer_cast<BidCoSPeer>(j->second));
									if(peer->getTeamRemoteAddress() == queue->peer->getAddress() && j->second->getAddress() != queue->peer->getAddress())
									{
										//Needed to avoid deadlocks
										peers.push_back(peer);
									}
								}
								_peersMutex.unlock();
								for(std::vector<std::shared_ptr<BidCoSPeer>>::iterator j = peers.begin(); j != peers.end(); ++j)
								{
									GD::out.printInfo("Info: Adding device 0x" + BaseLib::HelperFunctions::getHexString((*j)->getAddress()) + " to group " + queue->peer->getTeamRemoteSerialNumber() + ".");
									addPeerToTeam(*j, (*j)->getTeamChannel(), (*j)->getTeamRemoteChannel(), queue->peer->getTeamRemoteSerialNumber());
								}
							}
							catch(const std::exception& ex)
							{
								_peersMutex.unlock();
								GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
							}
							break;
						}
					}
				}
			}
		}
		else if(queue->getQueueType() == BidCoSQueueType::UNPAIRING)
		{
			if((sentPacket && sentPacket->messageType() == 0x01 && sentPacket->payload().at(0) == 0x00 && sentPacket->payload().at(1) == 0x06) || (sentPacket && sentPacket->messageType() == 0x11 && sentPacket->payload().at(0) == 0x04 && sentPacket->payload().at(1) == 0x00))
			{
				std::shared_ptr<BidCoSPeer> peer = getPeer(packet->senderAddress());
				if(peer)
                {
                    uint64_t peerId = peer->getID();
                    peer.reset();
                    deletePeer(peerId);
                }
			}
		}
		else if(queue->getQueueType() == BidCoSQueueType::SETAESKEY)
		{
			std::shared_ptr<BidCoSPeer> peer = getPeer(packet->senderAddress());
			if(peer && queue->getQueue()->size() < 2)
			{
				peer->setAESKeyIndex(peer->getPhysicalInterface()->getCurrentRFKeyIndex());
				aesKeyChanged = true;
				GD::out.printInfo("Info: Successfully changed AES key of peer " + std::to_string(peer->getID()) + ". Key index now is: " + std::to_string(peer->getAESKeyIndex()));
			}
		}
		queue->pop(); //Messages are not popped by default.
		if(aesKeyChanged)
		{
			std::shared_ptr<BidCoSPeer> peer = getPeer(packet->senderAddress());
			peer->getPhysicalInterface()->addPeer(peer->getPeerInfo());
		}

		if(queue->isEmpty())
		{
			std::shared_ptr<BidCoSPeer> peer = getPeer(packet->senderAddress());
			if(peer && !peer->getPairingComplete())
			{
				addHomegearFeatures(peer, -1, true);
				peer->setPairingComplete(true);
			}
		}
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

PVariable HomeMaticCentral::addDevice(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber)
{
	try
	{
		if(serialNumber.empty()) return Variable::createError(-2, "Serial number is empty.");
		if(serialNumber.size() != 10) return Variable::createError(-2, "Serial number length is not 10.");

		bool oldPairingModeState = _pairing;
		if(!_pairing) _pairing = true;

		std::vector<uint8_t> payload;
		payload.push_back(0x01);
		payload.push_back(serialNumber.size());
		payload.insert(payload.end(), serialNumber.begin(), serialNumber.end());
		std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(0, 0x84, 0x01, _address, 0, payload));

		int32_t i = 0;
		std::shared_ptr<BidCoSPeer> peer;
		while(!peer && i < 3)
		{
			packet->setMessageCounter(getMessageCounter());

            {
                std::lock_guard<std::mutex> sendPacketThreadGuard(_sendPacketThreadMutex);
                _bl->threadManager.join(_sendPacketThread);
                _bl->threadManager.start(_sendPacketThread, false, &HomeMaticCentral::sendPacket, this, GD::defaultPhysicalInterface, packet, false);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
			peer = getPeer(serialNumber);
			i++;
		}

		_pairing = oldPairingModeState;

		if(!peer)
		{
			return Variable::createError(-1, "No response from device.");
		}
		else
		{
			return peer->getDeviceDescription(clientInfo, -1, std::map<std::string, bool>());
		}
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return Variable::createError(-32500, "Unknown application error.");
}

PVariable HomeMaticCentral::addLink(BaseLib::PRpcClientInfo clientInfo, std::string senderSerialNumber, int32_t senderChannelIndex, std::string receiverSerialNumber, int32_t receiverChannelIndex, std::string name, std::string description)
{
	try
	{
		if(senderSerialNumber.empty()) return Variable::createError(-2, "Given sender address is empty.");
		if(receiverSerialNumber.empty()) return Variable::createError(-2, "Given receiver address is empty.");
		std::shared_ptr<BidCoSPeer> sender = getPeer(senderSerialNumber);
		std::shared_ptr<BidCoSPeer> receiver = getPeer(receiverSerialNumber);
		if(!sender) return Variable::createError(-2, "Sender device not found.");
		if(!receiver) return Variable::createError(-2, "Receiver device not found.");
		return addLink(clientInfo, sender->getID(), senderChannelIndex, receiver->getID(), receiverChannelIndex, name, description);
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return Variable::createError(-32500, "Unknown application error.");
}

PVariable HomeMaticCentral::addLink(BaseLib::PRpcClientInfo clientInfo, uint64_t senderID, int32_t senderChannelIndex, uint64_t receiverID, int32_t receiverChannelIndex, std::string name, std::string description)
{
	try
	{
		if(senderID == 0) return Variable::createError(-2, "Sender id is not set.");
		if(receiverID == 0) return Variable::createError(-2, "Receiver is not set.");
		std::shared_ptr<BidCoSPeer> sender = getPeer(senderID);
		std::shared_ptr<BidCoSPeer> receiver = getPeer(receiverID);
		if(!sender) return Variable::createError(-2, "Sender device not found.");
		if(!receiver) return Variable::createError(-2, "Receiver device not found.");
		if(senderChannelIndex < 0) senderChannelIndex = 0;
		if(receiverChannelIndex < 0) receiverChannelIndex = 0;
		std::shared_ptr<HomegearDevice> senderRpcDevice = sender->getRpcDevice();
		std::shared_ptr<HomegearDevice> receiverRpcDevice = receiver->getRpcDevice();
		Functions::iterator senderFunctionIterator = senderRpcDevice->functions.find(senderChannelIndex);
		if(senderFunctionIterator == senderRpcDevice->functions.end()) return Variable::createError(-2, "Sender channel not found.");
		Functions::iterator receiverFunctionIterator = receiverRpcDevice->functions.find(receiverChannelIndex);
		if(receiverFunctionIterator == receiverRpcDevice->functions.end()) return Variable::createError(-2, "Receiver channel not found.");
		PFunction senderFunction = senderFunctionIterator->second;
		PFunction receiverFunction = receiverFunctionIterator->second;
		if(senderFunction->linkSenderFunctionTypes.size() == 0 || receiverFunction->linkReceiverFunctionTypes.size() == 0) return Variable::createError(-6, "Link not supported.");
		bool validLink = false;
		for(LinkFunctionTypes::iterator i = senderFunction->linkSenderFunctionTypes.begin(); i != senderFunction->linkSenderFunctionTypes.end(); ++i)
		{
			for(LinkFunctionTypes::iterator j = receiverFunction->linkReceiverFunctionTypes.begin(); j != receiverFunction->linkReceiverFunctionTypes.end(); ++j)
			{
				if(*i == *j)
				{
					validLink = true;
					break;
				}
			}
			if(validLink) break;
		}
		if(!validLink) return Variable::createError(-6, "Link not supported.");

		std::shared_ptr<BaseLib::Systems::BasicPeer> senderPeer(new BaseLib::Systems::BasicPeer());
		senderPeer->address = sender->getAddress();
		senderPeer->channel = senderChannelIndex;
		senderPeer->id = sender->getID();
		senderPeer->serialNumber = sender->getSerialNumber();
		senderPeer->isSender = true;
		senderPeer->linkDescription = description;
		senderPeer->linkName = name;

		std::shared_ptr<BaseLib::Systems::BasicPeer> receiverPeer(new BaseLib::Systems::BasicPeer());
		receiverPeer->address = receiver->getAddress();
		receiverPeer->channel = receiverChannelIndex;
		receiverPeer->id = receiver->getID();
		receiverPeer->serialNumber = receiver->getSerialNumber();
		receiverPeer->linkDescription = description;
		receiverPeer->linkName = name;

		sender->addPeer(senderChannelIndex, receiverPeer);
		receiver->addPeer(receiverChannelIndex, senderPeer);

		if(!sender->isVirtual())
		{
			std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.createQueue(sender->getPhysicalInterface(), BidCoSQueueType::CONFIG, sender->getAddress());
			std::shared_ptr<BidCoSQueue> pendingQueue(new BidCoSQueue(sender->getPhysicalInterface(), BidCoSQueueType::CONFIG));
			pendingQueue->noSending = true;

			std::vector<uint8_t> payload;

			uint8_t configByte = 0xA0;
			if(sender->getRXModes() & HomegearDevice::ReceiveModes::wakeOnRadio) configByte |= 0x10;
			std::shared_ptr<BaseLib::Systems::BasicPeer> hiddenPeer(sender->getVirtualPeer(senderChannelIndex));
			if(hiddenPeer)
			{
				sender->removePeer(senderChannelIndex, hiddenPeer->address, hiddenPeer->channel);
				if(sender->getVirtualPeerId() == 0) deletePeer(hiddenPeer->id);

				payload.clear();
				payload.push_back(senderChannelIndex);
				payload.push_back(0x02);
				payload.push_back(hiddenPeer->address >> 16);
				payload.push_back((hiddenPeer->address >> 8) & 0xFF);
				payload.push_back(hiddenPeer->address & 0xFF);
				payload.push_back(hiddenPeer->channel);
				payload.push_back(0);
				std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(getMessageCounter(), configByte, 0x01, _address, sender->getAddress(), payload));
				pendingQueue->push(configPacket);
				pendingQueue->push(_messages->find(0x02));
				configByte = 0xA0;
			}

			payload.clear();
			//CONFIG_ADD_PEER
			payload.push_back(senderChannelIndex);
			payload.push_back(0x01);
			payload.push_back(receiver->getAddress() >> 16);
			payload.push_back((receiver->getAddress() >> 8) & 0xFF);
			payload.push_back(receiver->getAddress() & 0xFF);
			payload.push_back(receiverChannelIndex);
			payload.push_back(0);
			std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(getMessageCounter(), configByte, 0x01, _address, sender->getAddress(), payload));
			pendingQueue->push(configPacket);
			pendingQueue->push(_messages->find(0x02));

			sender->pendingBidCoSQueues->push(pendingQueue);
			sender->serviceMessages->setConfigPending(true);

			PParameterGroup senderParameterGroup = senderFunction->getParameterGroup(ParameterGroup::Type::Enum::link);
			if(senderParameterGroup && !senderParameterGroup->parameters.empty())
			{
				PVariable paramset(new Variable(VariableType::tStruct));
				std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>* linkConfig = &sender->linksCentral.at(senderChannelIndex).at(receiver->getAddress()).at(receiverChannelIndex);
				for(std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator i = linkConfig->begin(); i != linkConfig->end(); ++i)
				{
					std::vector<uint8_t> parameterData = i->second.getBinaryData();
					paramset->structValue->insert(StructElement(i->first, i->second.rpcParameter->convertFromPacket(parameterData)));
				}
				//putParamset pushes the packets on pendingQueues, but does not send immediately
				sender->putParamset(clientInfo, senderChannelIndex, ParameterGroup::Type::Enum::link, receiverID, receiverChannelIndex, paramset, true);

				Scenarios::iterator scenarioIterator = receiverFunction->linkParameters->scenarios.find("default");
				if(scenarioIterator != receiverFunction->linkParameters->scenarios.end())
				{
					paramset.reset(new Variable(VariableType::tStruct));
					for(ScenarioEntries::iterator i = scenarioIterator->second->scenarioEntries.begin(); i != scenarioIterator->second->scenarioEntries.end(); ++i)
					{
						PParameter parameter = senderFunction->linkParameters->getParameter(i->first);
						if(parameter)
						{
							paramset->structValue->insert(StructElement(i->first, Variable::fromString(i->second, parameter->logical->type)));
						}
					}
					//putParamset pushes the packets on pendingQueues, but does not send immediately
					sender->putParamset(clientInfo, senderChannelIndex, ParameterGroup::Type::Enum::link, receiverID, receiverChannelIndex, paramset, true);
				}
			}

			queue->push(sender->pendingBidCoSQueues);

			raiseRPCUpdateDevice(sender->getID(), senderChannelIndex, sender->getSerialNumber() + ":" + std::to_string(senderChannelIndex), 1);

			int32_t waitIndex = 0;
			while(_bidCoSQueueManager.get(sender->getAddress()) && waitIndex < 50)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				waitIndex++;
			}
			if(!_bidCoSQueueManager.get(sender->getAddress())) sender->serviceMessages->setConfigPending(false);
		}
		else raiseRPCUpdateDevice(sender->getID(), senderChannelIndex, sender->getSerialNumber() + ":" + std::to_string(senderChannelIndex), 1);

		if(!receiver->isVirtual())
		{
			std::shared_ptr<BidCoSQueue>  queue = _bidCoSQueueManager.createQueue(receiver->getPhysicalInterface(), BidCoSQueueType::CONFIG, receiver->getAddress());
			std::shared_ptr<BidCoSQueue> pendingQueue(new BidCoSQueue(receiver->getPhysicalInterface(), BidCoSQueueType::CONFIG));
			pendingQueue->noSending = true;

			std::vector<uint8_t> payload;

			uint8_t configByte = 0xA0;
			if(receiver->getRXModes() & HomegearDevice::ReceiveModes::wakeOnRadio) configByte |= 0x10;

			if(receiver->getDeviceType() != (uint32_t)DeviceType::HMCCRTDN &&
				receiver->getDeviceType() != (uint32_t)DeviceType::HMCCRTDNBOM &&
				receiver->getDeviceType() != (uint32_t)DeviceType::HmTcItWmWEu &&
				!HomeMaticCentral::isSwitch(receiver->getDeviceType()) &&
				!HomeMaticCentral::isDimmer(receiver->getDeviceType()))
			{
				std::shared_ptr<BaseLib::Systems::BasicPeer> hiddenPeer = receiver->getVirtualPeer(receiverChannelIndex);
				if(hiddenPeer)
				{
					sender->removePeer(receiverChannelIndex, hiddenPeer->address, hiddenPeer->channel);
					if(sender->getVirtualPeerId() == 0) deletePeer(hiddenPeer->id);

					payload.clear();
					payload.push_back(receiverChannelIndex);
					payload.push_back(0x02);
					payload.push_back(hiddenPeer->address >> 16);
					payload.push_back((hiddenPeer->address >> 8) & 0xFF);
					payload.push_back(hiddenPeer->address & 0xFF);
					payload.push_back(hiddenPeer->channel);
					payload.push_back(0);
					std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(getMessageCounter(), configByte, 0x01, _address, receiver->getAddress(), payload));
					pendingQueue->push(configPacket);
					pendingQueue->push(_messages->find(0x02));
					configByte = 0xA0;
				}
			}

			payload.clear();
			//CONFIG_ADD_PEER
			payload.push_back(receiverChannelIndex);
			payload.push_back(0x01);
			payload.push_back(sender->getAddress() >> 16);
			payload.push_back((sender->getAddress() >> 8) & 0xFF);
			payload.push_back(sender->getAddress() & 0xFF);
			payload.push_back(senderChannelIndex);
			payload.push_back(0);
			std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(getMessageCounter(), configByte, 0x01, _address, receiver->getAddress(), payload));
			pendingQueue->push(configPacket);
			pendingQueue->push(_messages->find(0x02));

			receiver->pendingBidCoSQueues->push(pendingQueue);
			receiver->serviceMessages->setConfigPending(true);

			PParameterGroup receiverParameterGroup = receiverFunction->getParameterGroup(ParameterGroup::Type::Enum::link);
			if(receiverParameterGroup && !receiverParameterGroup->parameters.empty())
			{
				PVariable paramset(new Variable(VariableType::tStruct));
				std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>* linkConfig = &receiver->linksCentral.at(receiverChannelIndex).at(sender->getAddress()).at(senderChannelIndex);
				for(std::unordered_map<std::string, BaseLib::Systems::RpcConfigurationParameter>::iterator i = linkConfig->begin(); i != linkConfig->end(); ++i)
				{
					std::vector<uint8_t> parameterData = i->second.getBinaryData();
					paramset->structValue->insert(StructElement(i->first, i->second.rpcParameter->convertFromPacket(parameterData)));
				}
				//putParamset pushes the packets on pendingQueues, but does not send immediately
				receiver->putParamset(clientInfo, receiverChannelIndex, ParameterGroup::Type::Enum::link, senderID, senderChannelIndex, paramset, true);

				Scenarios::iterator scenarioIterator = senderFunction->linkParameters->scenarios.find("default");
				if(scenarioIterator != senderFunction->linkParameters->scenarios.end())
				{
					paramset.reset(new Variable(VariableType::tStruct));
					for(ScenarioEntries::iterator i = scenarioIterator->second->scenarioEntries.begin(); i != scenarioIterator->second->scenarioEntries.end(); ++i)
					{
						PParameter parameter = receiverFunction->linkParameters->getParameter(i->first);
						if(parameter)
						{
							paramset->structValue->insert(StructElement(i->first, Variable::fromString(i->second, parameter->logical->type)));
						}
					}
					//putParamset pushes the packets on pendingQueues, but does not send immediately
					receiver->putParamset(clientInfo, receiverChannelIndex, ParameterGroup::Type::Enum::link, senderID, senderChannelIndex, paramset, true);
				}
			}

			queue->push(receiver->pendingBidCoSQueues);

			raiseRPCUpdateDevice(receiver->getID(), receiverChannelIndex, receiver->getSerialNumber() + ":" + std::to_string(receiverChannelIndex), 1);

			int32_t waitIndex = 0;
			while(_bidCoSQueueManager.get(receiver->getAddress()) && waitIndex < 50)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				waitIndex++;
			}
			if(!_bidCoSQueueManager.get(receiver->getAddress())) receiver->serviceMessages->setConfigPending(false);
		}
		else raiseRPCUpdateDevice(receiver->getID(), receiverChannelIndex, receiver->getSerialNumber() + ":" + std::to_string(receiverChannelIndex), 1);

		//Check, if channel is part of a group and if that's the case add link for the grouped channel
		int32_t channelGroupedWith = sender->getChannelGroupedWith(senderChannelIndex);
		//I'm assuming that senderChannelIndex is always the first of the two grouped channels
		if(channelGroupedWith > senderChannelIndex)
		{
			return addLink(clientInfo, senderID, channelGroupedWith, receiverID, receiverChannelIndex, name, description);
		}
		else
		{
			return PVariable(new Variable(VariableType::tVoid));
		}
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return Variable::createError(-32500, "Unknown application error.");
}

PVariable HomeMaticCentral::removeLink(BaseLib::PRpcClientInfo clientInfo, std::string senderSerialNumber, int32_t senderChannelIndex, std::string receiverSerialNumber, int32_t receiverChannelIndex)
{
	try
	{
		if(senderSerialNumber.empty()) return Variable::createError(-2, "Given sender address is empty.");
		if(receiverSerialNumber.empty()) return Variable::createError(-2, "Given receiver address is empty.");
		std::shared_ptr<BidCoSPeer> sender = getPeer(senderSerialNumber);
		std::shared_ptr<BidCoSPeer> receiver = getPeer(receiverSerialNumber);
		if(!sender) return Variable::createError(-2, "Sender device not found.");
		if(!receiver) return Variable::createError(-2, "Receiver device not found.");
		return removeLink(clientInfo, sender->getID(), senderChannelIndex, receiver->getID(), receiverChannelIndex);
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return Variable::createError(-32500, "Unknown application error.");
}

PVariable HomeMaticCentral::removeLink(BaseLib::PRpcClientInfo clientInfo, uint64_t senderID, int32_t senderChannelIndex, uint64_t receiverID, int32_t receiverChannelIndex)
{
	try
	{
		if(senderID == 0) return Variable::createError(-2, "Sender id is not set.");
		if(receiverID == 0) return Variable::createError(-2, "Receiver id is not set.");
		std::shared_ptr<BidCoSPeer> sender = getPeer(senderID);
		std::shared_ptr<BidCoSPeer> receiver = getPeer(receiverID);
		if(!sender) return Variable::createError(-2, "Sender device not found.");
		if(!receiver) return Variable::createError(-2, "Receiver device not found.");
		if(senderChannelIndex < 0) senderChannelIndex = 0;
		if(receiverChannelIndex < 0) receiverChannelIndex = 0;
		std::string senderSerialNumber = sender->getSerialNumber();
		std::string receiverSerialNumber = receiver->getSerialNumber();
		std::shared_ptr<HomegearDevice> senderRpcDevice = sender->getRpcDevice();
		std::shared_ptr<HomegearDevice> receiverRpcDevice = receiver->getRpcDevice();
		if(senderRpcDevice->functions.find(senderChannelIndex) == senderRpcDevice->functions.end()) return Variable::createError(-2, "Sender channel not found.");
		if(receiverRpcDevice->functions.find(receiverChannelIndex) == receiverRpcDevice->functions.end()) return Variable::createError(-2, "Receiver channel not found.");
		if(!sender->getPeer(senderChannelIndex, receiver->getAddress()) && !receiver->getPeer(receiverChannelIndex, sender->getAddress())) return Variable::createError(-6, "Devices are not paired to each other.");

		sender->removePeer(senderChannelIndex, receiver->getAddress(), receiverChannelIndex);
		receiver->removePeer(receiverChannelIndex, sender->getAddress(), senderChannelIndex);

		if(!sender->isVirtual())
		{
			std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.createQueue(sender->getPhysicalInterface(), BidCoSQueueType::CONFIG, sender->getAddress());
			std::shared_ptr<BidCoSQueue> pendingQueue(new BidCoSQueue(sender->getPhysicalInterface(), BidCoSQueueType::CONFIG));
			pendingQueue->noSending = true;

			uint8_t configByte = 0xA0;
			if(sender->getRXModes() & HomegearDevice::ReceiveModes::wakeOnRadio) configByte |= 0x10;

			std::vector<uint8_t> payload;
			payload.push_back(senderChannelIndex);
			payload.push_back(0x02);
			payload.push_back(receiver->getAddress() >> 16);
			payload.push_back((receiver->getAddress() >> 8) & 0xFF);
			payload.push_back(receiver->getAddress() & 0xFF);
			payload.push_back(receiverChannelIndex);
			payload.push_back(0);
			std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(getMessageCounter(), configByte, 0x01, _address, sender->getAddress(), payload));
			pendingQueue->push(configPacket);
			pendingQueue->push(_messages->find(0x02));

			sender->pendingBidCoSQueues->push(pendingQueue);
			sender->serviceMessages->setConfigPending(true);
			queue->push(sender->pendingBidCoSQueues);

			addHomegearFeatures(sender, senderChannelIndex, false);

			raiseRPCUpdateDevice(sender->getID(), senderChannelIndex, senderSerialNumber + ":" + std::to_string(senderChannelIndex), 1);

			int32_t waitIndex = 0;
			while(_bidCoSQueueManager.get(sender->getAddress()) && waitIndex < 50)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				waitIndex++;
			}
			if(!_bidCoSQueueManager.get(sender->getAddress())) sender->serviceMessages->setConfigPending(false);
		}
		else raiseRPCUpdateDevice(sender->getID(), senderChannelIndex, senderSerialNumber + ":" + std::to_string(senderChannelIndex), 1);

		if(!receiver->isVirtual())
		{
			std::shared_ptr<BidCoSQueue> queue = _bidCoSQueueManager.createQueue(receiver->getPhysicalInterface(), BidCoSQueueType::CONFIG, receiver->getAddress());
			std::shared_ptr<BidCoSQueue> pendingQueue(new BidCoSQueue(receiver->getPhysicalInterface(), BidCoSQueueType::CONFIG));
			pendingQueue->noSending = true;

			uint8_t configByte = 0xA0;
			if(receiver->getRXModes() & HomegearDevice::ReceiveModes::wakeOnRadio) configByte |= 0x10;

			std::vector<uint8_t> payload;
			payload.clear();
			payload.push_back(receiverChannelIndex);
			payload.push_back(0x02);
			payload.push_back(sender->getAddress() >> 16);
			payload.push_back((sender->getAddress() >> 8) & 0xFF);
			payload.push_back(sender->getAddress() & 0xFF);
			payload.push_back(senderChannelIndex);
			payload.push_back(0);
			std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(getMessageCounter(), configByte, 0x01, _address, receiver->getAddress(), payload));
			pendingQueue->push(configPacket);
			pendingQueue->push(_messages->find(0x02));

			receiver->pendingBidCoSQueues->push(pendingQueue);
			receiver->serviceMessages->setConfigPending(true);
			queue->push(receiver->pendingBidCoSQueues);

			if(receiver->getDeviceType() != (uint32_t)DeviceType::HMCCRTDN &&
				receiver->getDeviceType() != (uint32_t)DeviceType::HMCCRTDNBOM &&
				receiver->getDeviceType() != (uint32_t)DeviceType::HmTcItWmWEu &&
				!HomeMaticCentral::isSwitch(receiver->getDeviceType()) &&
				!HomeMaticCentral::isDimmer(receiver->getDeviceType()))
			{
				addHomegearFeatures(receiver, receiverChannelIndex, false);
			}

			raiseRPCUpdateDevice(receiver->getID(), receiverChannelIndex, receiverSerialNumber + ":" + std::to_string(receiverChannelIndex), 1);

			int32_t waitIndex = 0;
			while(_bidCoSQueueManager.get(receiver->getAddress()) && waitIndex < 50)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				waitIndex++;
			}
			if(!_bidCoSQueueManager.get(receiver->getAddress())) receiver->serviceMessages->setConfigPending(false);
		}
		else raiseRPCUpdateDevice(receiver->getID(), receiverChannelIndex, receiverSerialNumber + ":" + std::to_string(receiverChannelIndex), 1);

		return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
	return Variable::createError(-32500, "Unknown application error.");
}

PVariable HomeMaticCentral::deleteDevice(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t flags)
{
	try
	{
		if(serialNumber.empty()) return Variable::createError(-2, "Unknown device.");
		if(serialNumber[0] == '*') return Variable::createError(-2, "Cannot delete virtual device.");

        uint64_t peerId = 0;

        {
            std::shared_ptr<BidCoSPeer> peer = getPeer(serialNumber);
            if(!peer) return PVariable(new Variable(VariableType::tVoid));
            peerId = peer->getID();
        }

		return deleteDevice(clientInfo, peerId, flags);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable HomeMaticCentral::deleteDevice(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, int32_t flags)
{
	try
	{
		if(peerID == 0) return Variable::createError(-2, "Unknown device.");
		if(peerID >= 0x40000000) return Variable::createError(-2, "Cannot delete virtual device.");
		std::shared_ptr<BidCoSPeer> peer = getPeer(peerID);
		if(!peer) return PVariable(new Variable(VariableType::tVoid));
		uint64_t id = peer->getID();

		bool defer = flags & 0x04;
		bool force = flags & 0x02;
		//Reset
		if(flags & 0x01)
		{
			_bl->threadManager.join(_resetThread);
			_bl->threadManager.start(_resetThread, false, &HomeMaticCentral::reset, this, id, defer);
		}
		else
		{
			_bl->threadManager.join(_resetThread);
			_bl->threadManager.start(_resetThread, false, &HomeMaticCentral::unpair, this, id, defer);
		}
		//Force delete
		if(force)
        {
            uint64_t peerId = peer->getID();
            peer.reset();
            deletePeer(peerId);
        }
		else
		{
			int32_t waitIndex = 0;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			while(_bidCoSQueueManager.get(peer->getAddress()) && peerExists(id) && waitIndex < 50)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				waitIndex++;
			}
		}

		if(!defer && !force && peerExists(id)) return Variable::createError(-1, "No answer from device.");

		return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable HomeMaticCentral::getPairingState(BaseLib::PRpcClientInfo clientInfo)
{
    try
    {
        auto states = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);

		states->structValue->emplace("pairingModeEnabled", std::make_shared<BaseLib::Variable>(_pairing));
		states->structValue->emplace("pairingModeEndTime", std::make_shared<BaseLib::Variable>(BaseLib::HelperFunctions::getTimeSeconds() + _timeLeftInPairingMode));

        {
            std::lock_guard<std::mutex> newPeersGuard(_newPeersMutex);

            auto pairingMessages = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tArray);
            pairingMessages->arrayValue->reserve(_pairingMessages.size());
            for(auto& message : _pairingMessages)
            {
                auto pairingMessage = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
                pairingMessage->structValue->emplace("messageId", std::make_shared<BaseLib::Variable>(message->messageId));
                auto variables = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tArray);
                variables->arrayValue->reserve(message->variables.size());
                for(auto& variable : message->variables)
                {
                    variables->arrayValue->emplace_back(std::make_shared<BaseLib::Variable>(variable));
                }
                pairingMessage->structValue->emplace("variables", variables);
                pairingMessages->arrayValue->push_back(pairingMessage);
            }
            states->structValue->emplace("general", std::move(pairingMessages));

            for(auto& element : _newPeers)
            {
                for(auto& peer : element.second)
                {
                    auto peerState = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tStruct);
                    peerState->structValue->emplace("state", std::make_shared<BaseLib::Variable>(peer->state));
                    peerState->structValue->emplace("messageId", std::make_shared<BaseLib::Variable>(peer->messageId));
                    auto variables = std::make_shared<BaseLib::Variable>(BaseLib::VariableType::tArray);
                    variables->arrayValue->reserve(peer->variables.size());
                    for(auto& variable : peer->variables)
                    {
                        variables->arrayValue->emplace_back(std::make_shared<BaseLib::Variable>(variable));
                    }
                    peerState->structValue->emplace("variables", variables);
                    states->structValue->emplace(std::to_string(peer->peerId), std::move(peerState));
                }
            }
        }

        return states;
    }
    catch(const std::exception& ex)
    {
        _bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable HomeMaticCentral::setTeam(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t channel, std::string teamSerialNumber, int32_t teamChannel, bool force, bool burst)
{
	try
	{
		std::shared_ptr<BidCoSPeer> peer(getPeer(serialNumber));
		if(!peer) return Variable::createError(-2, "Unknown device.");
		uint64_t teamID = 0;
		if(!teamSerialNumber.empty())
		{
			std::shared_ptr<BidCoSPeer> team(getPeer(teamSerialNumber));
			if(!team) return Variable::createError(-2, "Group does not exist.");
			teamID = team->getID();
		}
		return setTeam(clientInfo, peer->getID(), channel, teamID, teamChannel, force, burst);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable HomeMaticCentral::setTeam(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, int32_t channel, uint64_t teamID, int32_t teamChannel, bool force, bool burst)
{
	try
	{
		if(teamChannel < 0) teamChannel = 0;
		std::shared_ptr<BidCoSPeer> peer(getPeer(peerID));
		if(!peer) return Variable::createError(-2, "Unknown device.");
		int32_t oldTeamAddress = peer->getTeamRemoteAddress();
		int32_t oldTeamChannel = peer->getTeamRemoteChannel();
		if(oldTeamChannel < 0) oldTeamChannel = 0;
		if(teamID == 0) //Reset team to default
		{
			if(!force && !peer->getTeamRemoteSerialNumber().empty() && peer->getTeamRemoteSerialNumber().substr(1) == peer->getSerialNumber() && peer->getTeamChannel() == channel)
			{
				//Team already is default
				return PVariable(new Variable(VariableType::tVoid));
			}
			int32_t newChannel = -1;
			std::shared_ptr<HomegearDevice> rpcDevice = peer->getRpcDevice();
			//Get first channel which has a team
			for(Functions::iterator i = rpcDevice->functions.begin(); i != rpcDevice->functions.end(); ++i)
			{
				if(i->second->hasGroup)
				{
					newChannel = i->first;
					break;
				}
			}
			if(newChannel < 0) return Variable::createError(-6, "There are no group channels for this device.");
			if(channel < 0) channel = newChannel;
			resetTeam(peer, newChannel);
		}
		else //Set new team
		{
			if(channel < 0) channel = 0;

			std::shared_ptr<HomegearDevice> rpcDevice = peer->getRpcDevice();
			Functions::iterator functionIteratorPeer = rpcDevice->functions.find(channel);
			if(functionIteratorPeer == rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel.");
			if(!functionIteratorPeer->second->hasGroup) return Variable::createError(-6, "Channel does not support groups.");

			//Don't create team if not existent!
			std::shared_ptr<BidCoSPeer> team = getPeer(teamID);
			if(!team) return Variable::createError(-2, "Group does not exist.");
			if(!force && !peer->getTeamRemoteSerialNumber().empty() && peer->getTeamRemoteSerialNumber() == team->getSerialNumber() && peer->getTeamChannel() == channel && peer->getTeamRemoteChannel() == teamChannel)
			{
				//Peer already is member of this team
				return PVariable(new Variable(VariableType::tVoid));
			}
			std::shared_ptr<HomegearDevice> teamRpcDevice = team->getRpcDevice();
			Functions::iterator functionIteratorTeam = teamRpcDevice->functions.find(teamChannel);
			if(functionIteratorTeam == teamRpcDevice->functions.end()) return Variable::createError(-2, "Unknown group channel.");
			if(functionIteratorTeam->second->groupId != functionIteratorPeer->second->groupId) return Variable::createError(-6, "Peer channel is not compatible to group channel.");

			addPeerToTeam(peer, channel, teamChannel, team->getSerialNumber());
		}

		std::shared_ptr<BidCoSQueue> queue(new BidCoSQueue(peer->getPhysicalInterface(), BidCoSQueueType::CONFIG));
		queue->noSending = true;

		uint8_t configByte = 0xA0;
		if(burst && (peer->getRXModes() & HomegearDevice::ReceiveModes::wakeOnRadio)) configByte |= 0x10;

		std::vector<uint8_t> payload;
		if(oldTeamAddress != 0 && oldTeamAddress != peer->getTeamRemoteAddress())
		{
			payload.push_back(channel);
			payload.push_back(0x02);
			payload.push_back(oldTeamAddress >> 16);
			payload.push_back((oldTeamAddress >> 8) & 0xFF);
			payload.push_back(oldTeamAddress & 0xFF);
			payload.push_back(oldTeamChannel);
			payload.push_back(0);
			std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(peer->getMessageCounter(), configByte, 0x01, _address, peer->getAddress(), payload));
			peer->setMessageCounter(peer->getMessageCounter() + 1);
			queue->push(packet);
			queue->push(getMessages()->find(0x02));
			configByte = 0xA0;
		}

		payload.clear();
		payload.push_back(channel);
		payload.push_back(0x01);
		payload.push_back(peer->getTeamRemoteAddress() >> 16);
		payload.push_back((peer->getTeamRemoteAddress() >> 8) & 0xFF);
		payload.push_back(peer->getTeamRemoteAddress() & 0xFF);
		payload.push_back(peer->getTeamRemoteChannel());
		payload.push_back(0);
		std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(peer->getMessageCounter(), configByte, 0x01, _address, peer->getAddress(), payload));
		peer->setMessageCounter(peer->getMessageCounter() + 1);
		queue->push(packet);
		queue->push(getMessages()->find(0x02));

		peer->pendingBidCoSQueues->push(queue);
		peer->serviceMessages->setConfigPending(true);
		if((peer->getRXModes() & HomegearDevice::ReceiveModes::Enum::always) || (peer->getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeOnRadio)) enqueuePendingQueues(peer->getAddress());
		else GD::out.printDebug("Debug: Packet was queued and will be sent with next wake me up packet.");
		raiseRPCUpdateDevice(peer->getID(), channel, peer->getSerialNumber() + ":" + std::to_string(channel), 2);
		return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable HomeMaticCentral::getInstallMode(BaseLib::PRpcClientInfo clientInfo)
{
	try
	{
		return PVariable(new Variable(_timeLeftInPairingMode));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

void HomeMaticCentral::pairingModeTimer(int32_t duration, bool debugOutput)
{
	try
	{
		_pairing = true;
		if(debugOutput) GD::out.printInfo("Info: Pairing mode enabled.");
		_timeLeftInPairingMode = duration;
		int64_t startTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		int64_t timePassed = 0;
		while(timePassed < ((int64_t)duration * 1000) && !_stopPairingModeThread)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
			timePassed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() - startTime;
			_timeLeftInPairingMode = duration - (timePassed / 1000);
		}
		_timeLeftInPairingMode = 0;
		_pairing = false;
		if(debugOutput) GD::out.printInfo("Info: Pairing mode disabled.");
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

PVariable HomeMaticCentral::activateLinkParamset(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t channel, std::string remoteSerialNumber, int32_t remoteChannel, bool longPress)
{
	try
	{
		std::shared_ptr<BidCoSPeer> peer(getPeer(serialNumber));
		if(!peer) return Variable::createError(-2, "Unknown device.");
		uint64_t remoteID = 0;
		if(!remoteSerialNumber.empty())
		{
			std::shared_ptr<BidCoSPeer> remotePeer(getPeer(remoteSerialNumber));
			if(!remotePeer)
			{
				if(remoteSerialNumber != _serialNumber) return Variable::createError(-3, "Remote peer is unknown.");
			}
			else remoteID = remotePeer->getID();
		}
		return peer->activateLinkParamset(clientInfo, channel, remoteID, remoteChannel, longPress);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable HomeMaticCentral::activateLinkParamset(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, int32_t channel, uint64_t remoteID, int32_t remoteChannel, bool longPress)
{
	try
	{
		std::shared_ptr<BidCoSPeer> peer(getPeer(peerID));
		if(!peer) return Variable::createError(-2, "Unknown device.");
		return peer->activateLinkParamset(clientInfo, channel, remoteID, remoteChannel, longPress);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable HomeMaticCentral::putParamset(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t channel, ParameterGroup::Type::Enum type, std::string remoteSerialNumber, int32_t remoteChannel, PVariable paramset)
{
	try
	{
		std::shared_ptr<BidCoSPeer> peer(getPeer(serialNumber));
		if(!peer) return Variable::createError(-2, "Unknown device.");
		uint64_t remoteID = 0;
		if(!remoteSerialNumber.empty())
		{
			std::shared_ptr<BidCoSPeer> remotePeer(getPeer(remoteSerialNumber));
			if(!remotePeer)
			{
				if(remoteSerialNumber != _serialNumber) return Variable::createError(-3, "Remote peer is unknown.");
			}
			else remoteID = remotePeer->getID();
		}
		PVariable result = peer->putParamset(clientInfo, channel, type, remoteID, remoteChannel, paramset, false);
		if(result->errorStruct) return result;
		int32_t waitIndex = 0;
		while(_bidCoSQueueManager.get(peer->getAddress()) && waitIndex < 50)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			waitIndex++;
		}
		if(!_bidCoSQueueManager.get(peer->getAddress())) peer->serviceMessages->setConfigPending(false);
		return result;
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable HomeMaticCentral::putParamset(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable paramset, bool checkAcls)
{
	try
	{
		std::shared_ptr<BidCoSPeer> peer(getPeer(peerID));
		if(!peer) return Variable::createError(-2, "Unknown device.");
		PVariable result = peer->putParamset(clientInfo, channel, type, remoteID, remoteChannel, paramset, checkAcls);
		if(result->errorStruct) return result;
		int32_t waitIndex = 0;
		while(_bidCoSQueueManager.get(peer->getAddress()) && waitIndex < 50)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			waitIndex++;
		}
		if(!_bidCoSQueueManager.get(peer->getAddress())) peer->serviceMessages->setConfigPending(false);
		return result;
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable HomeMaticCentral::setInstallMode(BaseLib::PRpcClientInfo clientInfo, bool on, uint32_t duration, BaseLib::PVariable metadata, bool debugOutput)
{
	try
	{
		_pairingModeThreadMutex.lock();
		if(_disposing)
		{
			_pairingModeThreadMutex.unlock();
			return Variable::createError(-32500, "Central is disposing.");
		}
		_stopPairingModeThread = true;
		_bl->threadManager.join(_pairingModeThread);
		_stopPairingModeThread = false;
		_timeLeftInPairingMode = 0;
		if(on && duration >= 5)
		{
            {
                std::lock_guard<std::mutex> newPeersGuard(_newPeersMutex);
                _newPeers.clear();
                _pairingMessages.clear();
            }

			_timeLeftInPairingMode = duration; //It's important to set it here, because the thread often doesn't completely initialize before getInstallMode requests _timeLeftInPairingMode
			_bl->threadManager.start(_pairingModeThread, false, &HomeMaticCentral::pairingModeTimer, this, duration, debugOutput);
		}
		_pairingModeThreadMutex.unlock();
		return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    _pairingModeThreadMutex.unlock();
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable HomeMaticCentral::updateFirmware(BaseLib::PRpcClientInfo clientInfo, std::vector<uint64_t> ids, bool manual)
{
	try
	{
		if(_updateMode || _bl->deviceUpdateInfo.currentDevice > 0) return Variable::createError(-32500, "Central is already already updating a device. Please wait until the current update is finished.");
		_updateFirmwareThreadMutex.lock();
		if(_disposing)
		{
			_updateFirmwareThreadMutex.unlock();
			return Variable::createError(-32500, "Central is disposing.");
		}
		_bl->threadManager.join(_updateFirmwareThread);
		_bl->threadManager.start(_updateFirmwareThread, false, &HomeMaticCentral::updateFirmwares, this, ids);
		_updateFirmwareThreadMutex.unlock();
		return PVariable(new Variable(true));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    _updateFirmwareThreadMutex.unlock();
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable HomeMaticCentral::setInterface(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, std::string interfaceID)
{
	try
	{
		std::shared_ptr<BidCoSPeer> peer(getPeer(peerID));
		if(!peer) return Variable::createError(-2, "Unknown device.");
		return peer->setInterface(clientInfo, interfaceID);
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return Variable::createError(-32500, "Unknown application error.");
}
}
