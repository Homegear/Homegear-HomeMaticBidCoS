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

#include "CUNO.h"
#include "../BidCoSPacket.h"
#include "homegear-base/BaseLib.h"
#include "../GD.h"

namespace BidCoS
{

CUNO::CUNO(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IBidCoSInterface(settings), BaseLib::ITimedQueue(GD::bl)
{
	_bl = GD::bl;
	_out.init(GD::bl);
	_out.setPrefix(GD::out.getPrefix() + "CUNO \"" + settings->id + "\": ");

	signal(SIGPIPE, SIG_IGN);

	_socket = std::unique_ptr<BaseLib::SocketOperations>(new BaseLib::SocketOperations(_bl));

	if(settings->listenThreadPriority == -1)
	{
		settings->listenThreadPriority = 45;
		settings->listenThreadPolicy = SCHED_FIFO;
	}

	_aesHandshake.reset(new AesHandshake(_bl, _out, _myAddress, _rfKey, _oldRfKey, _currentRfKeyIndex));
}

CUNO::~CUNO()
{
	try
	{
		_stopCallbackThread = true;
		GD::bl->threadManager.join(_listenThread);
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

void CUNO::addPeer(PeerInfo peerInfo)
{
	try
	{
		if(peerInfo.address == 0) return;
		_peersMutex.lock();
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
    _peersMutex.unlock();
}

void CUNO::addPeers(std::vector<PeerInfo>& peerInfos)
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

void CUNO::removePeer(int32_t address)
{
	try
	{
		_peersMutex.lock();
		if(_peers.find(address) != _peers.end()) _peers.erase(address);
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
    _peersMutex.unlock();
}

void CUNO::processQueueEntry(int32_t index, int64_t id, std::shared_ptr<BaseLib::ITimedQueueEntry>& entry)
{
	try
	{
		std::shared_ptr<QueueEntry> queueEntry;
		queueEntry = std::dynamic_pointer_cast<QueueEntry>(entry);
		if(!queueEntry || !queueEntry->packet) return;
		send("As" + queueEntry->packet->hexString() + "\n");
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

void CUNO::queuePacket(std::shared_ptr<BidCoSPacket> packet, int64_t sendingTime)
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

void CUNO::sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet)
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
			if(_bl->debugLevel >= 2) _out.printError("Error: Tried to send packet larger than 64 bytes. That is not supported.");
			return;
		}
		std::shared_ptr<BidCoSPacket> bidCoSPacket(std::dynamic_pointer_cast<BidCoSPacket>(packet));
		if(!bidCoSPacket) return;
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

		std::string packetString = packet->hexString();
		if(_bl->debugLevel >= 4) _out.printInfo("Info: Sending (" + _settings->id + "): " + packetString);
		send("As" + packet->hexString() + "\n");
		_lastPacketSent = BaseLib::HelperFunctions::getTime();
		packet->setTimeSending(BaseLib::HelperFunctions::getTime());
		_aesHandshake->setMFrame(bidCoSPacket);
		queuePacket(bidCoSPacket, packet->timeSending() + 200);
		queuePacket(bidCoSPacket, packet->timeSending() + 400);
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

void CUNO::enableUpdateMode()
{
	try
	{
		_updateMode = true;
		send("AR\n");
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

void CUNO::disableUpdateMode()
{
	try
	{
		send("Ar\n");
		_updateMode = false;
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

void CUNO::send(std::string data)
{
	try
    {
    	if(data.size() < 3) return; //Otherwise error in printInfo
    	_sendMutex.lock();
    	if(!_socket->connected() || _stopped)
    	{
    		_out.printWarning(std::string("Warning: !!!Not!!! sending: ") + data.substr(2, data.size() - 3));
    		_sendMutex.unlock();
    		return;
    	}
    	_socket->proofwrite(data);
    	 _sendMutex.unlock();
    	 return;
    }
    catch(const BaseLib::SocketOperationException& ex)
    {
    	_out.printError(ex.what());
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
    _stopped = true;
    _sendMutex.unlock();
}

void CUNO::startListening()
{
	try
	{
		stopListening();
		if(!_aesHandshake) return; //AES is not initialized

		if(!GD::family->getCentral())
		{
			_stopCallbackThread = true;
			_out.printError("Error: Could not get central address. Stopping listening.");
			return;
		}
		_myAddress = GD::family->getCentral()->getAddress();
		_aesHandshake->setMyAddress(_myAddress);

		startQueue(0, 45, SCHED_FIFO);
		_socket = std::unique_ptr<BaseLib::SocketOperations>(new BaseLib::SocketOperations(_bl, _settings->host, _settings->port, _settings->ssl, _settings->caFile, _settings->verifyCertificate));
		_socket->setAutoConnect(false);
		_out.printDebug("Connecting to CUNO with hostname " + _settings->host + " on port " + _settings->port + "...");
		_stopped = false;
		if(_settings->listenThreadPriority > -1) GD::bl->threadManager.start(_listenThread, true, _settings->listenThreadPriority, _settings->listenThreadPolicy, &CUNO::listen, this);
		else GD::bl->threadManager.start(_listenThread, true, &CUNO::listen, this);
		IPhysicalInterface::startListening();
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

void CUNO::reconnect()
{
	try
	{
		_socket->close();
		_out.printDebug("Connecting to CUNO device with hostname " + _settings->host + " on port " + _settings->port + "...");
		_socket->open();
		send("X21\nAr\n");
		_out.printInfo("Connected to CUNO device with hostname " + _settings->host + " on port " + _settings->port + ".");
		_stopped = false;
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

void CUNO::stopListening()
{
	try
	{
		stopQueue(0);
		if(_socket->connected()) send("Ax\nX00\n");
		_stopCallbackThread = true;
		GD::bl->threadManager.join(_listenThread);
		_stopCallbackThread = false;
		_socket->close();
		_stopped = true;
		_sendMutex.unlock(); //In case it is deadlocked - shouldn't happen of course
		IPhysicalInterface::stopListening();
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

void CUNO::listen()
{
    try
    {
    	uint32_t receivedBytes = 0;
    	int32_t bufferMax = 2048;
		std::vector<char> buffer(bufferMax);

        while(!_stopCallbackThread)
        {
        	if(_stopped || !_socket->connected())
        	{
        		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        		if(_stopCallbackThread) return;
        		if(_stopped) _out.printWarning("Warning: Connection to CUNO closed. Trying to reconnect...");
        		reconnect();
        		continue;
        	}
        	std::vector<uint8_t> data;
			try
			{
				do
				{
					receivedBytes = _socket->proofread(&buffer[0], bufferMax);
					if(receivedBytes > 0)
					{
						data.insert(data.end(), &buffer.at(0), &buffer.at(0) + receivedBytes);
						if(data.size() > 1000000)
						{
							_out.printError("Could not read from CUNO: Too much data.");
							break;
						}
					}
				} while(receivedBytes == (unsigned)bufferMax);
			}
			catch(const BaseLib::SocketTimeOutException& ex)
			{
				if(data.empty()) continue; //When receivedBytes is exactly 2048 bytes long, proofread will be called again, time out and the packet is received with a delay of 5 seconds. It doesn't matter as packets this big are only received at start up.
			}
			catch(const BaseLib::SocketClosedException& ex)
			{
				_stopped = true;
				_out.printWarning("Warning: " + ex.what());
				std::this_thread::sleep_for(std::chrono::milliseconds(10000));
				continue;
			}
			catch(const BaseLib::SocketOperationException& ex)
			{
				_stopped = true;
				_out.printError("Error: " + ex.what());
				std::this_thread::sleep_for(std::chrono::milliseconds(10000));
				continue;
			}
			if(data.empty() || data.size() > 1000000) continue;

        	if(_bl->debugLevel >= 6)
        	{
        		_out.printDebug("Debug: Packet received from CUNO. Raw data:");
        		_out.printBinary(data);
        	}

        	processData(data);

        	_lastPacketReceived = BaseLib::HelperFunctions::getTime();
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

void CUNO::processData(std::vector<uint8_t>& data)
{
	try
	{
		if(data.empty()) return;
		std::string packets;
		packets.insert(packets.end(), data.begin(), data.end());

		std::istringstream stringStream(packets);
		std::string packetHex;
		while(std::getline(stringStream, packetHex))
		{
			if(packetHex.size() > 21) //21 is minimal packet length (=10 Byte + CUNO "A")
        	{
				std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(packetHex, BaseLib::HelperFunctions::getTime()));
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
									continue;
								}
								if(_bl->debugLevel >= 5) _out.printDebug("Debug: AES handshake successful.");
								queuePacket(aFrame);
								mFrame->setTimeReceived(BaseLib::HelperFunctions::getTime());
								raisePacketReceived(mFrame);

								continue;
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
									continue;
								}

								// {{{ Remove wrongly queued non AES packets from queue id map
									bool requeue = false;
									{
										std::lock_guard<std::mutex> idGuard(_queueIdsMutex);
										std::map<int32_t, std::set<int64_t>>::iterator idIterator = _queueIds.find(packet->senderAddress());

										if(idIterator != _queueIds.end() && *(idIterator->second.begin()) < mFrame->timeSending() + 300)
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
								continue;
							}
							else if(packet->messageType() == 0x02)
							{
								if(_aesHandshake->handshakeStarted(packet->senderAddress()) && !_aesHandshake->checkAFrame(packet))
								{
									_out.printError("Error: ACK has invalid signature.");
									continue;
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
						if(knowsPeer && (packet->controlByte() & 0x20))
						{
							std::vector<uint8_t> payload { 0 };
							uint8_t controlByte = 0x80;
							if((packet->controlByte() & 2) && wakeUp && packet->messageType() != 0) controlByte |= 1;
							std::shared_ptr<BidCoSPacket> ackPacket(new BidCoSPacket(packet->messageCounter(), controlByte, 0x02, _myAddress, packet->senderAddress(), payload));
							queuePacket(ackPacket);
						}
						raisePacketReceived(packet);
					}
				}
				else if(packet->destinationAddress() == 0 && (packet->controlByte() & 2)) //Packet is wake me up packet
				{
					try
					{
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
				else raisePacketReceived(packet);
				if(_bl->hf.getTime() - _lastAesHandshakeGc > 30000)
				{
					_lastAesHandshakeGc = _bl->hf.getTime();
					_aesHandshake->collectGarbage();
				}
        	}
        	else if(!packetHex.empty())
        	{
        		if(packetHex == "LOVF\n") _out.printWarning("Warning: CUNO with id " + _settings->id + " reached 1% limit. You need to wait, before sending is allowed again.");
        		else if(packetHex == "A") continue;
        		else _out.printWarning("Warning: Too short packet received: " + packetHex);
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
