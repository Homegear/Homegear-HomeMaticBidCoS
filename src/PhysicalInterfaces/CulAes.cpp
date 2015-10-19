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

#include "CulAes.h"
#include "../BidCoSPacket.h"
#include "../homegear-base/BaseLib.h"
#include "../GD.h"

namespace BidCoS
{

CulAes::CulAes(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IBidCoSInterface(settings), BaseLib::IQueue(GD::bl)
{
	_bl = GD::bl;
	_out.init(GD::bl);
	_out.setPrefix(GD::out.getPrefix() + "CUL \"" + settings->id + "\": ");

	if(settings->listenThreadPriority == -1)
	{
		settings->listenThreadPriority = 45;
		settings->listenThreadPolicy = SCHED_FIFO;
	}

	if(!settings->rfKey.empty() && settings->currentRFKeyIndex == 0)
	{
		_out.printWarning("Warning: currentRFKeyIndex in physicalinterfaces.conf is not set. Setting it to \"1\".");
		settings->currentRFKeyIndex = 1;
	}

	std::vector<uint8_t> rfKey;
	std::vector<uint8_t> oldRfKey;
	if(!settings->rfKey.empty())
	{
		rfKey = _bl->hf.getUBinary(settings->rfKey);
		if(rfKey.size() != 16)
		{
			_out.printError("Error: The RF AES key specified in physicalinterfaces.conf for communication with your BidCoS devices is not a valid hexadecimal string.");
			rfKey.clear();
			return;
		}
	}

	if(!settings->oldRFKey.empty())
	{
		oldRfKey = _bl->hf.getUBinary(settings->oldRFKey);
		if(oldRfKey.size() != 16)
		{
			_out.printError("Error: The old RF AES key specified in physicalinterfaces.conf for communication with your BidCoS devices is not a valid hexadecimal string.");
			oldRfKey.clear();
		}
	}

	if(!oldRfKey.empty() && settings->currentRFKeyIndex == 1)
	{
		_out.printWarning("Warning: The RF AES key index specified in physicalinterfaces.conf for communication with your BidCoS devices is \"1\" but \"OldRFKey\" is specified. That is not possible. Increase the key index to \"2\".");
		oldRfKey.clear();
	}

	if(!oldRfKey.empty() && rfKey.empty())
	{
		oldRfKey.clear();
		if(settings->currentRFKeyIndex > 0)
		{
			_out.printWarning("Warning: The RF AES key index specified in physicalinterfaces.conf for communication with your BidCoS devices is greater than \"0\" but no AES key is specified. Setting it to \"0\".");
			settings->currentRFKeyIndex = 0;
		}
	}

	if(oldRfKey.empty() && settings->currentRFKeyIndex > 1)
	{
		_out.printWarning("Warning: The RF AES key index specified in physicalinterfaces.conf for communication with your BidCoS devices is larger than \"1\" but \"OldRFKey\" is not specified. Please set your old RF key or set key index to \"1\".");
	}

	if(settings->currentRFKeyIndex > 253)
	{
		_out.printError("Error: The RF AES key index specified in physicalinterfaces.conf for communication with your BidCoS devices is greater than \"253\". That is not allowed.");
		settings->currentRFKeyIndex = 253;
	}

	_aesHandshake.reset(new AesHandshake(_bl, _out, _myAddress, rfKey, oldRfKey, settings->currentRFKeyIndex));
}

CulAes::~CulAes()
{
	try
	{
		if(_listenThread.joinable())
		{
			_stopCallbackThread = true;
			_listenThread.join();
		}
		closeDevice();
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

void CulAes::addPeer(PeerInfo peerInfo)
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

void CulAes::addPeers(std::vector<PeerInfo>& peerInfos)
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

void CulAes::removePeer(int32_t address)
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

void CulAes::processQueueEntry(int32_t index, std::shared_ptr<BaseLib::IQueueEntry>& entry)
{
	try
	{
		std::shared_ptr<QueueEntry> queueEntry;
		queueEntry = std::dynamic_pointer_cast<QueueEntry>(entry);
		if(!queueEntry || !queueEntry->packet) return;
		int64_t timeToSleep = 0;
		int64_t time = BaseLib::HelperFunctions::getTime();
		if(queueEntry->sendingTime > 0) timeToSleep = queueEntry->sendingTime - time;
		if(timeToSleep > 0) std::this_thread::sleep_for(std::chrono::milliseconds(timeToSleep));
		sendPacket(queueEntry->packet);
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

void CulAes::queuePacket(std::shared_ptr<BidCoSPacket> packet)
{
	try
	{
		int64_t sendingTime = packet->timeReceived();
		if(sendingTime <= 0) sendingTime = BaseLib::HelperFunctions::getTime();
		sendingTime = sendingTime + _settings->responseDelay;
		std::shared_ptr<BaseLib::IQueueEntry> entry(new QueueEntry(packet, sendingTime));
		if(!enqueue(0, entry)) _out.printError("Error: Too many packets are queued to be processed. Your packet processing is too slow. Dropping packet.");
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

void CulAes::sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet)
{
	try
	{
		if(!packet)
		{
			_out.printWarning("Warning: Packet was nullptr.");
			return;
		}
		if(_fileDescriptor->descriptor == -1) throw(BaseLib::Exception("Couldn't write to CUL device, because the file descriptor is not valid: " + _settings->device));
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
		if(bidCoSPacket->messageType() == 0x04 && bidCoSPacket->payload()->size() == 2 && bidCoSPacket->payload()->at(0) == 1) //Set new AES key
		{
			if(!_aesHandshake->generateKeyChangePacket(bidCoSPacket)) return;
		}

		writeToDevice("As" + packet->hexString() + "\n", true);
		packet->setTimeSending(BaseLib::HelperFunctions::getTime());
		_aesHandshake->setMFrame(bidCoSPacket);
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

void CulAes::enableUpdateMode()
{
	try
	{
		_updateMode = true;
		writeToDevice("AR\n", false);
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

void CulAes::disableUpdateMode()
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

void CulAes::openDevice()
{
	try
	{
		if(_fileDescriptor->descriptor > -1) closeDevice();

		_lockfile = "/var/lock" + _settings->device.substr(_settings->device.find_last_of('/')) + ".lock";
		int lockfileDescriptor = open(_lockfile.c_str(), O_WRONLY | O_EXCL | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
		if(lockfileDescriptor == -1)
		{
			if(errno != EEXIST)
			{
				_out.printCritical("Couldn't create lockfile " + _lockfile + ": " + strerror(errno));
				return;
			}

			int processID = 0;
			std::ifstream lockfileStream(_lockfile.c_str());
			lockfileStream >> processID;
			if(getpid() != processID && kill(processID, 0) == 0)
			{
				_out.printCritical("CUL device is in use: " + _settings->device);
				return;
			}
			unlink(_lockfile.c_str());
			lockfileDescriptor = open(_lockfile.c_str(), O_WRONLY | O_EXCL | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
			if(lockfileDescriptor == -1)
			{
				_out.printCritical("Couldn't create lockfile " + _lockfile + ": " + strerror(errno));
				return;
			}
		}
		dprintf(lockfileDescriptor, "%10i", getpid());
		close(lockfileDescriptor);
		//std::string chmod("chmod 666 " + _lockfile);
		//system(chmod.c_str());

		_fileDescriptor = _bl->fileDescriptorManager.add(open(_settings->device.c_str(), O_RDWR | O_NOCTTY));
		if(_fileDescriptor->descriptor == -1)
		{
			_out.printCritical("Couldn't open CUL device \"" + _settings->device + "\": " + strerror(errno));
			return;
		}

		setupDevice();
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

void CulAes::closeDevice()
{
	try
	{
		_bl->fileDescriptorManager.close(_fileDescriptor);
		unlink(_lockfile.c_str());
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

void CulAes::setupDevice()
{
	try
	{
		if(_fileDescriptor->descriptor == -1) return;
		memset(&_termios, 0, sizeof(termios));

		_termios.c_cflag = B38400 | CS8 | CREAD;
		_termios.c_iflag = 0;
		_termios.c_oflag = 0;
		_termios.c_lflag = 0;
		_termios.c_cc[VMIN] = 1;
		_termios.c_cc[VTIME] = 0;

		cfsetispeed(&_termios, B38400);
		cfsetospeed(&_termios, B38400);

		if(tcflush(_fileDescriptor->descriptor, TCIFLUSH) == -1) throw(BaseLib::Exception("Couldn't flush CUL device " + _settings->device));
		if(tcsetattr(_fileDescriptor->descriptor, TCSANOW, &_termios) == -1) throw(BaseLib::Exception("Couldn't set CUL device settings: " + _settings->device));

		std::this_thread::sleep_for(std::chrono::milliseconds(2000));

		int flags = fcntl(_fileDescriptor->descriptor, F_GETFL);
		if(!(flags & O_NONBLOCK))
		{
			if(fcntl(_fileDescriptor->descriptor, F_SETFL, flags | O_NONBLOCK) == -1)
			{
				throw(BaseLib::Exception("Couldn't set CUL device to non blocking mode: " + _settings->device));
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

std::string CulAes::readFromDevice()
{
	try
	{
		if(_stopped) return "";
		if(_fileDescriptor->descriptor == -1)
		{
			_out.printCritical("Couldn't read from CUL device, because the file descriptor is not valid: " + _settings->device + ". Trying to reopen...");
			closeDevice();
			std::this_thread::sleep_for(std::chrono::milliseconds(5000));
			openDevice();
			if(!isOpen()) return "";
			writeToDevice("X21\nAr\n", false);
		}
		std::string packet;
		int32_t i;
		char localBuffer[1] = "";
		fd_set readFileDescriptor;
		FD_ZERO(&readFileDescriptor);
		FD_SET(_fileDescriptor->descriptor, &readFileDescriptor);

		while((!_stopCallbackThread && localBuffer[0] != '\n' && _fileDescriptor->descriptor > -1))
		{
			FD_ZERO(&readFileDescriptor);
			FD_SET(_fileDescriptor->descriptor, &readFileDescriptor);
			//Timeout needs to be set every time, so don't put it outside of the while loop
			timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 500000;
			i = select(_fileDescriptor->descriptor + 1, &readFileDescriptor, NULL, NULL, &timeout);
			switch(i)
			{
				case 0: //Timeout
					if(!_stopCallbackThread) continue;
					else return "";
				case -1:
					_out.printError("Error reading from CUL device: " + _settings->device);
					return "";
				case 1:
					break;
				default:
					_out.printError("Error reading from CUL device: " + _settings->device);
					return "";
			}

			i = read(_fileDescriptor->descriptor, localBuffer, 1);
			if(i == -1)
			{
				if(errno == EAGAIN) continue;
				_out.printError("Error reading from CUL device: " + _settings->device);
				return "";
			}
			packet.push_back(localBuffer[0]);
			if(packet.size() > 200)
			{
				_out.printError("CUL was disconnected.");
				closeDevice();
				return "";
			}
		}
		return packet;
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
	return "";
}

void CulAes::writeToDevice(std::string data, bool printSending)
{
    try
    {
    	if(_stopped) return;
        if(_fileDescriptor->descriptor == -1) throw(BaseLib::Exception("Couldn't write to CUL device, because the file descriptor is not valid: " + _settings->device));
        int32_t bytesWritten = 0;
        int32_t i;
        if(_bl->debugLevel > 3 && printSending)
        {
            _out.printInfo("Info: Sending (" + _settings->id + "): " + data.substr(2, data.size() - 3));
        }
        _sendMutex.lock();
        while(bytesWritten < (signed)data.length())
        {
            i = write(_fileDescriptor->descriptor, data.c_str() + bytesWritten, data.length() - bytesWritten);
            if(i == -1)
            {
                if(errno == EAGAIN) continue;
                throw(BaseLib::Exception("Error writing to CUL device (3, " + std::to_string(errno) + "): " + _settings->device));
            }
            bytesWritten += i;
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
    _sendMutex.unlock();
    _lastPacketSent = BaseLib::HelperFunctions::getTime();
}

void CulAes::startListening()
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
		_myAddress = GD::family->getCentral()->physicalAddress();
		_aesHandshake->setMyAddress(_myAddress);

		startQueue(0, 45, SCHED_FIFO);
		openDevice();
		if(_fileDescriptor->descriptor == -1) return;
		_stopped = false;
		writeToDevice("X21\nAr\n", false);
		std::this_thread::sleep_for(std::chrono::milliseconds(400));
		_listenThread = std::thread(&CulAes::listen, this);
		BaseLib::Threads::setThreadPriority(_bl, _listenThread.native_handle(), _settings->listenThreadPriority, _settings->listenThreadPolicy);
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

void CulAes::stopListening()
{
	try
	{
		if(_listenThread.joinable())
		{
			_stopCallbackThread = true;
			_listenThread.join();
		}
		_stopCallbackThread = false;
		if(_fileDescriptor->descriptor > -1)
		{
			//Other X commands than 00 seem to slow down data processing
			writeToDevice("Ax\nX00\n", false);
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			closeDevice();
		}
		_stopped = true;
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

void CulAes::listen()
{
    try
    {
    	std::thread temp;
        while(!_stopCallbackThread)
        {
        	if(_stopped)
        	{
        		std::this_thread::sleep_for(std::chrono::milliseconds(200));
        		if(_stopCallbackThread) return;
        		continue;
        	}
        	std::string packetHex = readFromDevice();
        	if(packetHex.size() > 21) //21 is minimal packet length (=10 Byte + CUL "A")
        	{
				std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(packetHex, BaseLib::HelperFunctions::getTime()));
				if(packet->destinationAddress() == _myAddress)
				{
					bool aesHandshake = false;
					_peersMutex.lock();
					try
					{
						std::map<int32_t, PeerInfo>::iterator peerIterator = _peers.find(packet->senderAddress());
						if(peerIterator != _peers.end())
						{
							if(packet->messageType() == 0x03)
							{
								std::shared_ptr<BidCoSPacket> mFrame;
								std::shared_ptr<BidCoSPacket> aFrame = _aesHandshake->getAFrame(packet, mFrame, peerIterator->second.keyIndex);
								if(!aFrame)
								{
									if(mFrame) _out.printError("Error: AES handshake failed for packet: " + mFrame->hexString());
									else _out.printError("Error: No m-Frame found for r-Frame.");
									_peersMutex.unlock();
									continue;
								}
								if(_bl->debugLevel >= 5) _out.printDebug("Debug: AES handshake successful.");
								queuePacket(aFrame);
								raisePacketReceived(mFrame);
								_peersMutex.unlock();
								continue;
							}
							else if(packet->messageType() == 0x02 && packet->payload()->size() == 8 && packet->payload()->at(0) == 0x04)
							{
								std::shared_ptr<BidCoSPacket> mFrame;
								std::shared_ptr<BidCoSPacket> rFrame = _aesHandshake->getRFrame(packet, mFrame, peerIterator->second.keyIndex);
								if(!rFrame)
								{
									if(mFrame) _out.printError("Error: AES handshake failed for packet: " + mFrame->hexString());
									else _out.printError("Error: No m-Frame found for c-Frame.");
									_peersMutex.unlock();
									continue;
								}
								queuePacket(rFrame);
								_peersMutex.unlock();
								continue;
							}
							else if(packet->messageType() == 0x02)
							{
								if(!_aesHandshake->checkAFrame(packet))
								{
									_out.printError("Error: ACK has invalid signature.");
									_peersMutex.unlock();
									continue;
								}
							}
							else
							{
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
					_peersMutex.unlock();
					if(aesHandshake)
					{
						if(_bl->debugLevel >= 5) _out.printDebug("Debug: Doing AES handshake.");
						queuePacket(_aesHandshake->getCFrame(packet));
					}
					else
					{
						/*if(packet->controlByte() & 0x40)
						{
							std::vector<uint8_t> payload(5);
							payload.push_back(0);
							std::shared_ptr<BidCoSPacket> ackPacket(new BidCoSPacket(packet->messageCounter(), 0x80, 0x02, _myAddress, packet->senderAddress(), payload));
							queuePacket(ackPacket);
						}*/
						raisePacketReceived(packet);
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
        		if(packetHex == "LOVF") _out.printWarning("Warning: CUL with id " + _settings->id + " reached 1% limit. You need to wait, before sending is allowed again.");
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

void CulAes::setup(int32_t userID, int32_t groupID)
{
    try
    {
    	setDevicePermission(userID, groupID);
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
