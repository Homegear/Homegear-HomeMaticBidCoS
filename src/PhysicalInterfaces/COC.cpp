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

#include "COC.h"
#include "../BidCoSPacket.h"
#include "homegear-base/BaseLib.h"
#include "../GD.h"

namespace BidCoS
{

COC::COC(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IBidCoSInterface(settings)
{
	_out.init(GD::bl);
	_out.setPrefix(GD::out.getPrefix() + "COC \"" + settings->id + "\": ");

	stackPrefix = "";
	for(uint32_t i = 1; i < settings->stackPosition; i++)
	{
		stackPrefix.push_back('*');
	}

	_aesHandshake.reset(new AesHandshake(_bl, _out, _myAddress, _rfKey, _oldRfKey, _currentRfKeyIndex));
}

COC::~COC()
{
	try
	{
		if(_socket)
		{
			_socket->removeEventHandler(_eventHandlerSelf);
			_socket->closeDevice();
			_socket.reset();
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

void COC::forceSendPacket(std::shared_ptr<BidCoSPacket> packet)
{
	try
	{
		if(!_socket)
		{
			_out.printError("Error: Couldn't write to COC device, because the device descriptor is not valid: " + _settings->device);
			return;
		}
		std::string packetString = packet->hexString();
		if(_bl->debugLevel >= 4) _out.printInfo("Info: Sending (" + _settings->id + "): " + packetString);
		writeToDevice(stackPrefix + "As" + packetString + "\n" + (_updateMode ? "" : stackPrefix + "Ar\n"));
		_lastPacketSent = BaseLib::HelperFunctions::getTime();
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

void COC::enableUpdateMode()
{
	try
	{
		_updateMode = true;
		writeToDevice(stackPrefix + "AR\n");
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

void COC::disableUpdateMode()
{
	try
	{
		stopListening();
		std::this_thread::sleep_for(std::chrono::milliseconds(2000));
		startListening();
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

void COC::writeToDevice(std::string data)
{
    try
    {
        if(!_socket)
		{
			_out.printError("Error: Couldn't write to COC device, because the device descriptor is not valid: " + _settings->device);
			return;
		}
        _socket->writeLine(data);
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
    _lastPacketSent = BaseLib::HelperFunctions::getTime();
}

void COC::startListening()
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

		IBidCoSInterface::startListening();
		_socket = GD::bl->serialDeviceManager.get(_settings->device);
		if(!_socket) _socket = GD::bl->serialDeviceManager.create(_settings->device, 38400, O_RDWR | O_NOCTTY | O_NDELAY, true, 45);
		if(!_socket) return;
		_eventHandlerSelf = _socket->addEventHandler(this);
		_socket->openDevice();
		if(gpioDefined(2))
		{
			openGPIO(2, false);
			if(!getGPIO(2)) setGPIO(2, true);
			closeGPIO(2);
		}
		if(gpioDefined(1))
		{
			openGPIO(1, false);
			if(!getGPIO(1))
			{
				setGPIO(1, false);
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
				setGPIO(1, true);
				std::this_thread::sleep_for(std::chrono::milliseconds(2000));
			}
			else std::this_thread::sleep_for(std::chrono::milliseconds(2000));
			closeGPIO(1);
		}
		writeToDevice(stackPrefix + "X21\n" + stackPrefix + "Ar\n");
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
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

void COC::stopListening()
{
	try
	{
		IBidCoSInterface::stopListening();
		if(!_socket) return;
		_socket->removeEventHandler(_eventHandlerSelf);
		_socket->closeDevice();
		_socket.reset();
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

void COC::lineReceived(const std::string& data)
{
    try
    {
    	std::string packetHex;
		if(stackPrefix.empty())
		{
			if(data.size() > 0 && data.at(0) == '*') return;
			packetHex = data;
		}
		else
		{
			if(data.size() + 1 <= stackPrefix.size()) return;
			if(data.substr(0, stackPrefix.size()) != stackPrefix || data.at(stackPrefix.size()) == '*') return;
			else packetHex = data.substr(stackPrefix.size());
		}
		if(packetHex.size() > 21) //21 is minimal packet length (=10 Byte + COC "A")
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
			if(packetHex == "LOVF\n") _out.printWarning("Warning: COC with id " + _settings->id + " reached 1% limit. You need to wait, before sending is allowed again.");
			else if(packetHex == "A") return;
			else _out.printWarning("Warning: Too short packet received: " + packetHex);
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

void COC::setup(int32_t userID, int32_t groupID)
{
    try
    {
    	setDevicePermission(userID, groupID);
    	exportGPIO(1);
		setGPIOPermission(1, userID, groupID, false);
		setGPIODirection(1, GPIODirection::OUT);
		exportGPIO(2);
		setGPIOPermission(2, userID, groupID, false);
		setGPIODirection(2, GPIODirection::OUT);
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
