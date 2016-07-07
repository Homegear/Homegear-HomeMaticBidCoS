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

#include "IBidCoSInterface.h"
#include "../GD.h"
#include "../BidCoSPacket.h"

namespace BidCoS
{

std::vector<char> IBidCoSInterface::PeerInfo::getAESChannelMap()
{
	std::vector<char> map;
	try
	{
		for(std::map<int32_t, bool>::iterator i = aesChannels.begin(); i != aesChannels.end(); ++i)
		{
			int32_t byte = i->first / 8;
			if((signed)map.size() < (byte + 1)) map.resize(byte + 1, 0);
			if(i->second) map.at(byte) |= (1 << (i->first % 8));
		}
		std::reverse(map.begin(), map.end());
	}
    catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return map;
}

IBidCoSInterface::IBidCoSInterface(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IPhysicalInterface(GD::bl, settings), BaseLib::ITimedQueue(GD::bl)
{
	_bl = GD::bl;
	_currentRfKeyIndex = GD::settings->getNumber("currentrfkeyindex");
	if(_currentRfKeyIndex < 0) _currentRfKeyIndex = 0;
	_rfKeyHex = GD::settings->get("rfkey");
	_oldRfKeyHex = GD::settings->get("oldrfkey");
	BaseLib::HelperFunctions::toLower(_rfKeyHex);
	BaseLib::HelperFunctions::toLower(_oldRfKeyHex);

	if(settings->listenThreadPriority == -1)
	{
		settings->listenThreadPriority = 0;
		settings->listenThreadPolicy = SCHED_OTHER;
	}

	if(_rfKeyHex.empty())
	{
		_out.printError("Error: No RF AES key specified in homematicbidcos.conf for communication with your BidCoS devices.");
	}

	if(!_rfKeyHex.empty())
	{
		_rfKey = _bl->hf.getUBinary(_rfKeyHex);
		if(_rfKey.size() != 16)
		{
			_out.printError("Error: The RF AES key specified in homematicbidcos.conf for communication with your BidCoS devices is not a valid hexadecimal string.");
			_rfKey.clear();
		}
	}

	if(!_oldRfKeyHex.empty())
	{
		_oldRfKey = _bl->hf.getUBinary(_oldRfKeyHex);
		if(_oldRfKey.size() != 16)
		{
			_out.printError("Error: The old RF AES key specified in homematicbidcos.conf for communication with your BidCoS devices is not a valid hexadecimal string.");
			_oldRfKey.clear();
		}
	}

	if(!_rfKey.empty() && _currentRfKeyIndex == 0)
	{
		_out.printWarning("Warning: currentRFKeyIndex in homematicbidcos.conf is not set. Setting it to \"1\".");
		_currentRfKeyIndex = 1;
	}

	if(!_oldRfKey.empty() && _currentRfKeyIndex == 1)
	{
		_out.printWarning("Warning: The RF AES key index specified in homematicbidcos.conf for communication with your BidCoS devices is \"1\" but \"OldRFKey\" is specified. That is not possible. Increase the key index to \"2\".");
		_oldRfKey.clear();
	}

	if(!_oldRfKey.empty() && _rfKey.empty())
	{
		_oldRfKey.clear();
		if(_currentRfKeyIndex > 0)
		{
			_out.printWarning("Warning: The RF AES key index specified in homematicbidcos.conf for communication with your BidCoS devices is greater than \"0\" but no AES key is specified. Setting it to \"0\".");
			_currentRfKeyIndex = 0;
		}
	}

	if(_oldRfKey.empty() && _currentRfKeyIndex > 1)
	{
		_out.printWarning("Warning: The RF AES key index specified in homematicbidcos.conf for communication with your BidCoS devices is larger than \"1\" but \"OldRFKey\" is not specified. Please set your old RF key or - only if there is no old key - set key index to \"1\".");
	}

	if(_currentRfKeyIndex > 253)
	{
		_out.printError("Error: The RF AES key index specified in homematicbidcos.conf for communication with your BidCoS devices is greater than \"253\". That is not allowed.");
		_currentRfKeyIndex = 253;
	}

	_aesHandshake.reset(new AesHandshake(_bl, _out, _myAddress, _rfKey, _oldRfKey, _currentRfKeyIndex));
}

IBidCoSInterface::~IBidCoSInterface()
{

}

void IBidCoSInterface::addPeer(PeerInfo peerInfo)
{
	try
	{
		if(peerInfo.address == 0) return;
		std::lock_guard<std::mutex> peersGuard(_peersMutex);
		//Remove old peer first. removePeer() is not called, so we don't need to unlock _peersMutex
		if(_peers.find(peerInfo.address) != _peers.end()) _peers.erase(peerInfo.address);
		_peers[peerInfo.address] = peerInfo;
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void IBidCoSInterface::addPeers(std::vector<PeerInfo>& peerInfos)
{
	try
	{
		for(std::vector<PeerInfo>::iterator i = peerInfos.begin(); i != peerInfos.end(); ++i)
		{
			addPeer(*i);
		}
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void IBidCoSInterface::removePeer(int32_t address)
{
	try
	{
		std::lock_guard<std::mutex> peersGuard(_peersMutex);
		if(_peers.find(address) != _peers.end())
		{
			_peers.erase(address);
			std::lock_guard<std::mutex> idGuard(_queueIdsMutex);
			std::map<int32_t, std::set<int64_t>>::iterator idIterator = _queueIds.find(address);

			if(idIterator != _queueIds.end())
			{
				for(std::set<int64_t>::iterator queueId = idIterator->second.begin(); queueId != idIterator->second.end(); ++queueId)
				{
					removeQueueEntry(0, *queueId);
				}
				_queueIds.erase(idIterator);
			}
		}
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}



void IBidCoSInterface::setWakeUp(PeerInfo peerInfo)
{
	addPeer(peerInfo);
}

void IBidCoSInterface::setAES(PeerInfo peerInfo, int32_t channel)
{
	addPeer(peerInfo);
}

void IBidCoSInterface::startListening()
{
	try
	{
		IPhysicalInterface::startListening();
		startQueue(0, 45, SCHED_FIFO);
	}
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void IBidCoSInterface::stopListening()
{
	try
	{
		IPhysicalInterface::stopListening();
		stopQueue(0);
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void IBidCoSInterface::processQueueEntry(int32_t index, int64_t id, std::shared_ptr<BaseLib::ITimedQueueEntry>& entry)
{
	try
	{
		std::shared_ptr<QueueEntry> queueEntry;
		queueEntry = std::dynamic_pointer_cast<QueueEntry>(entry);
		if(!queueEntry || !queueEntry->packet) return;
		forceSendPacket(queueEntry->packet);
		queueEntry->packet->setTimeSending(BaseLib::HelperFunctions::getTime());

		// {{{ Remove packet from queue id map
			std::lock_guard<std::mutex> idGuard(_queueIdsMutex);
			std::map<int32_t, std::set<int64_t>>::iterator idIterator = _queueIds.find(queueEntry->packet->destinationAddress());
			if(idIterator == _queueIds.end()) return;
			idIterator->second.erase(id);
			if(idIterator->second.empty()) _queueIds.erase(idIterator);
		// }}}
	}
    catch(const std::exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
	}
}

void IBidCoSInterface::queuePacket(std::shared_ptr<BidCoSPacket> packet, int64_t sendingTime)
{
	try
	{
		if(sendingTime == 0)
		{
			sendingTime = packet->timeReceived();
			if(sendingTime <= 0) sendingTime = BaseLib::HelperFunctions::getTime();
			sendingTime = sendingTime + _settings->responseDelay;
		}
		std::shared_ptr<BaseLib::ITimedQueueEntry> entry(new QueueEntry(sendingTime, packet));
		int64_t id;
		if(!enqueue(0, entry, id)) _out.printError("Error: Too many packets are queued to be processed. Your packet processing is too slow. Dropping packet.");

		// {{{ Add packet to queue id map
			std::lock_guard<std::mutex> idGuard(_queueIdsMutex);
			_queueIds[packet->destinationAddress()].insert(id);
		// }}}
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void IBidCoSInterface::processReceivedPacket(std::shared_ptr<BidCoSPacket> packet)
{
	try
	{
		if(packet->destinationAddress() == _myAddress)
		{
			bool aesHandshake = false;
			bool wakeUp = false;
			bool knowsPeer = false;
			try
			{
				std::lock_guard<std::mutex> peersGuard(_peersMutex);
				std::map<int32_t, PeerInfo>::iterator peerIterator = _peers.find(packet->senderAddress());
				if(peerIterator != _peers.end())
				{
					knowsPeer = true;
					wakeUp = peerIterator->second.wakeUp;
					if(packet->messageType() == 0x03)
					{
						std::shared_ptr<BidCoSPacket> mFrame;
						std::shared_ptr<BidCoSPacket> aFrame = _aesHandshake->getAFrame(packet, mFrame, peerIterator->second.keyIndex, wakeUp);
						if(!aFrame)
						{
							if(mFrame) _out.printError("Error: AES handshake failed for packet: " + mFrame->hexString());
							else _out.printError("Error: No m-Frame found for r-Frame.");
							return;
						}
						if(_bl->debugLevel >= 5) _out.printDebug("Debug: AES handshake successful.");
						queuePacket(aFrame);
						mFrame->setTimeReceived(BaseLib::HelperFunctions::getTime());
						raisePacketReceived(mFrame);
						return;
					}
					else if(packet->messageType() == 0x02 && packet->payload()->size() == 8 && packet->payload()->at(0) == 0x04)
					{
						peerIterator->second.keyIndex = packet->payload()->back() / 2;
						std::shared_ptr<BidCoSPacket> mFrame;
						std::shared_ptr<BidCoSPacket> rFrame = _aesHandshake->getRFrame(packet, mFrame, peerIterator->second.keyIndex);
						if(!rFrame)
						{
							if(mFrame) _out.printError("Error: AES handshake failed for packet: " + mFrame->hexString());
							else _out.printError("Error: No m-Frame found for c-Frame.");
							return;
						}

						// {{{ Remove wrongly queued non AES packets from queue id map
							bool requeue = false;
							{
								std::lock_guard<std::mutex> idGuard(_queueIdsMutex);
								std::map<int32_t, std::set<int64_t>>::iterator idIterator = _queueIds.find(packet->senderAddress());

								if(idIterator != _queueIds.end() && *(idIterator->second.begin()) < mFrame->timeSending() + 595)
								{
									requeue = true;
									for(std::set<int64_t>::iterator queueId = idIterator->second.begin(); queueId != idIterator->second.end(); ++queueId)
									{
										removeQueueEntry(0, *queueId);
									}
									_queueIds.erase(idIterator);
								}
							}
							if(requeue)
							{
								queuePacket(mFrame, mFrame->timeSending() + 600);
								queuePacket(mFrame, mFrame->timeSending() + 1200);
							}
						// }}}

						queuePacket(rFrame);
						return;
					}
					else if(packet->messageType() == 0x02)
					{
						if(_aesHandshake->handshakeStarted(packet->senderAddress()) && !_aesHandshake->checkAFrame(packet))
						{
							_out.printError("Error: ACK has invalid signature.");
							return;
						}

						// {{{ Remove packet from queue id map
						{
							std::lock_guard<std::mutex> idGuard(_queueIdsMutex);
							std::map<int32_t, std::set<int64_t>>::iterator idIterator = _queueIds.find(packet->senderAddress());
							if(idIterator != _queueIds.end())
							{
								for(std::set<int64_t>::iterator queueId = idIterator->second.begin(); queueId != idIterator->second.end(); ++queueId)
								{
									removeQueueEntry(0, *queueId);
								}
								_queueIds.erase(idIterator);
							}
						}
						// }}}
					}
					else
					{
						// {{{ Remove packet from queue id map
						{
							std::lock_guard<std::mutex> idGuard(_queueIdsMutex);
							std::map<int32_t, std::set<int64_t>>::iterator idIterator = _queueIds.find(packet->senderAddress());
							if(idIterator != _queueIds.end())
							{
								for(std::set<int64_t>::iterator queueId = idIterator->second.begin(); queueId != idIterator->second.end(); ++queueId)
								{
									removeQueueEntry(0, *queueId);
								}
								_queueIds.erase(idIterator);
							}
						}
						// }}}

						if(packet->payload()->size() > 1)
						{
							//Packet type 0x4X has channel at index 0 all other types at index 1
							if((packet->messageType() & 0xF0) == 0x40 && peerIterator->second.aesChannels[packet->payload()->at(0) & 0x3F]) aesHandshake = true;
							else if(peerIterator->second.aesChannels[packet->payload()->at(1) & 0x3F]) aesHandshake = true;
						}
						else if(packet->payload()->size() == 1 && (packet->messageType() & 0xF0) == 0x40 && peerIterator->second.aesChannels[packet->payload()->at(0) & 0x3F]) aesHandshake = true;
						else if(peerIterator->second.aesChannels[0]) aesHandshake = true;
					}
				}
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
			if(aesHandshake)
			{
				if(_bl->debugLevel >= 5) _out.printDebug("Debug: Doing AES handshake.");
				queuePacket(_aesHandshake->getCFrame(packet));
			}
			else
			{
				if(knowsPeer && packet->destinationAddress() == _myAddress && (packet->controlByte() & 0x20))
				{
					std::vector<uint8_t> payload { 0 };
					uint8_t controlByte = 0x80;
					if((packet->controlByte() & 2) && wakeUp && packet->messageType() != 0) controlByte |= 1;
					std::shared_ptr<BidCoSPacket> ackPacket(new BidCoSPacket(packet->messageCounter(), controlByte, 0x02, _myAddress, packet->senderAddress(), payload));
					std::cerr << "Queuing ACK packet" << std::endl;
					queuePacket(ackPacket);
				}
				raisePacketReceived(packet);
			}
		}
		else if(packet->destinationAddress() == 0 && (packet->controlByte() & 2)) //Packet is wake me up packet
		{
			try
			{
				// {{{ Remove packet from queue id map
				{
					std::lock_guard<std::mutex> idGuard(_queueIdsMutex);
					std::map<int32_t, std::set<int64_t>>::iterator idIterator = _queueIds.find(packet->senderAddress());
					if(idIterator != _queueIds.end())
					{
						for(std::set<int64_t>::iterator queueId = idIterator->second.begin(); queueId != idIterator->second.end(); ++queueId)
						{
							removeQueueEntry(0, *queueId);
						}
						_queueIds.erase(idIterator);
					}
				}
				// }}}
				std::lock_guard<std::mutex> peersGuard(_peersMutex);
				std::map<int32_t, PeerInfo>::iterator peerIterator = _peers.find(packet->senderAddress());
				if(peerIterator != _peers.end() && peerIterator->second.wakeUp)
				{
					std::vector<uint8_t> payload;
					std::shared_ptr<BidCoSPacket> wakeUpPacket(new BidCoSPacket(packet->messageCounter(), 0xA1, 0x12, _myAddress, packet->senderAddress(), payload));
					queuePacket(wakeUpPacket);
				}
				raisePacketReceived(packet);
			}
			catch(const std::exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
			}
		}
		else
		{
			// {{{ Remove packet from queue id map
			{
				std::lock_guard<std::mutex> idGuard(_queueIdsMutex);
				std::map<int32_t, std::set<int64_t>>::iterator idIterator = _queueIds.find(packet->senderAddress());
				if(idIterator != _queueIds.end())
				{
					for(std::set<int64_t>::iterator queueId = idIterator->second.begin(); queueId != idIterator->second.end(); ++queueId)
					{
						removeQueueEntry(0, *queueId);
					}
					_queueIds.erase(idIterator);
				}
			}
			// }}}
			raisePacketReceived(packet);
		}
		if(_bl->hf.getTime() - _lastAesHandshakeGc > 30000)
		{
			_lastAesHandshakeGc = _bl->hf.getTime();
			_aesHandshake->collectGarbage();
		}
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void IBidCoSInterface::sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet)
{
	try
	{
		if(!packet)
		{
			_out.printWarning("Warning: Packet was nullptr.");
			return;
		}
		if(packet->payload()->size() > 54)
		{
			_out.printError("Error: Tried to send packet larger than 64 bytes. That is not supported.");
			return;
		}
		std::shared_ptr<BidCoSPacket> bidCoSPacket(std::dynamic_pointer_cast<BidCoSPacket>(packet));
		if(!bidCoSPacket) return;

		// {{{ Remove packet from queue id map
		{
			std::lock_guard<std::mutex> idGuard(_queueIdsMutex);
			std::map<int32_t, std::set<int64_t>>::iterator idIterator = _queueIds.find(bidCoSPacket->senderAddress());
			if(idIterator != _queueIds.end())
			{
				for(std::set<int64_t>::iterator queueId = idIterator->second.begin(); queueId != idIterator->second.end(); ++queueId)
				{
					removeQueueEntry(0, *queueId);
				}
				_queueIds.erase(idIterator);
			}
		}
		// }}}

		if(_updateMode && !bidCoSPacket->isUpdatePacket())
		{
			_out.printInfo("Info: Can't send packet to BidCoS peer with address 0x" + BaseLib::HelperFunctions::getHexString(packet->destinationAddress(), 6) + ", because update mode is enabled.");
			return;
		}
		if(bidCoSPacket->messageType() == 0x02 && packet->senderAddress() == _myAddress && bidCoSPacket->controlByte() == 0x80 && bidCoSPacket->payload()->size() == 1 && bidCoSPacket->payload()->at(0) == 0)
		{
			_out.printDebug("Debug: Ignoring ACK packet.", 6);
			_lastPacketSent = BaseLib::HelperFunctions::getTime();
			return;
		}
		if((bidCoSPacket->controlByte() & 0x01) && packet->senderAddress() == _myAddress && (bidCoSPacket->payload()->empty() || (bidCoSPacket->payload()->size() == 1 && bidCoSPacket->payload()->at(0) == 0)))
		{
			_out.printDebug("Debug: Ignoring wake up packet.", 6);
			_lastPacketSent = BaseLib::HelperFunctions::getTime();
			return;
		}
		if(bidCoSPacket->messageType() == 0x04 && bidCoSPacket->payload()->size() == 2 && bidCoSPacket->payload()->at(0) == 1) //Set new AES key
		{
			std::lock_guard<std::mutex> peersGuard(_peersMutex);
			std::map<int32_t, PeerInfo>::iterator peerIterator = _peers.find(bidCoSPacket->destinationAddress());
			if(peerIterator != _peers.end())
			{
				if((bidCoSPacket->payload()->at(1) + 2) / 2 > peerIterator->second.keyIndex)
				{
					if(!_aesHandshake->generateKeyChangePacket(bidCoSPacket)) return;
				}
				else
				{
					_out.printInfo("Info: Ignoring AES key update packet, because a key with this index is already set.");
					std::vector<uint8_t> payload { 0 };
					std::shared_ptr<BidCoSPacket> ackPacket(new BidCoSPacket(bidCoSPacket->messageCounter(), 0x80, 0x02, bidCoSPacket->destinationAddress(), _myAddress, payload));
					raisePacketReceived(ackPacket);
					return;
				}
			}

		}

		if(!isOpen())
		{
			if(!_initComplete) _out.printWarning(std::string("Warning: !!!Not!!! sending packet, because init sequence is not complete: ") + bidCoSPacket->hexString());
			else _out.printWarning(std::string("Warning: !!!Not!!! sending packet, because device is not connected or opened: ") + bidCoSPacket->hexString());
			return;
		}

		forceSendPacket(bidCoSPacket);
		packet->setTimeSending(BaseLib::HelperFunctions::getTime());
		_aesHandshake->setMFrame(bidCoSPacket);
		if(!_updateMode && bidCoSPacket->messageType() != 0x11)
		{
			if(bidCoSPacket->controlByte() & 0x10)
			{
				queuePacket(bidCoSPacket, packet->timeSending() + 560);
				queuePacket(bidCoSPacket, packet->timeSending() + 1120);
			}
			else
			{
				queuePacket(bidCoSPacket, packet->timeSending() + 200);
				queuePacket(bidCoSPacket, packet->timeSending() + 400);
			}
		}
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

}
