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

#include "Hm-Mod-Rpi-Pcb.h"
#include "../GD.h"

namespace BidCoS
{
Hm_Mod_Rpi_Pcb::Hm_Mod_Rpi_Pcb(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IBidCoSInterface(settings)
{
	_out.init(GD::bl);
	_out.setPrefix(GD::out.getPrefix() + "HM-MOD-RPI-PCB \"" + settings->id + "\": ");

	if(settings->listenThreadPriority == -1)
	{
		settings->listenThreadPriority = 45;
		settings->listenThreadPolicy = SCHED_FIFO;
	}

	_packetIndex = 0;
	memset(&_termios, 0, sizeof(termios));

	if(!settings)
	{
		_out.printCritical("Critical: Error initializing HM-MOD-RPI-PCB. Settings pointer is empty.");
		return;
	}
}

Hm_Mod_Rpi_Pcb::~Hm_Mod_Rpi_Pcb()
{
	try
	{
		_stopCallbackThread = true;
		GD::bl->threadManager.join(_initThread);
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

void Hm_Mod_Rpi_Pcb::setup(int32_t userID, int32_t groupID, bool setPermissions)
{
	try
	{
		_out.printDebug("Debug: HM-MOD_RPI_PCB: Setting device permissions");
		if(setPermissions) setDevicePermission(userID, groupID);
		_out.printDebug("Debug: HM-MOD_RPI_PCB: Exporting GPIO");
		exportGPIO(1);
		_out.printDebug("Debug: HM-MOD_RPI_PCB: Setting GPIO permissions");
		if(setPermissions) setGPIOPermission(1, userID, groupID, false);
		setGPIODirection(1, BaseLib::Systems::IPhysicalInterface::GPIODirection::Enum::OUT);
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

void Hm_Mod_Rpi_Pcb::enableUpdateMode()
{
	try
	{
		if(!_initComplete || _stopped) return;
		_updateMode = true;
		for(int32_t j = 0; j < 40; j++)
		{
			std::vector<uint8_t> responsePacket;
			std::vector<char> requestPacket;
			std::vector<char> payload{ 0, 6 };
			buildPacket(requestPacket, payload);
			_packetIndex++;
			getResponse(requestPacket, responsePacket, _packetIndex - 1, 0, 4);
			if(responsePacket.size() >= 9  && responsePacket.at(6) == 1)
			{
				break;
			}
			else if(responsePacket.size() == 9 && responsePacket.at(6) == 8)
			{
				//Operation pending
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				continue;
			}
			if(j == 2)
			{
				_out.printError("Error: Could not enable update mode.");
				return;
			}
		}
		for(int32_t j = 0; j < 40; j++)
		{
			std::vector<uint8_t> responsePacket;
			std::vector<char> requestPacket;
			std::vector<char> payload{ 0, 7 };
			payload.push_back(0xE9);
			payload.push_back(0xCA);
			buildPacket(requestPacket, payload);
			_packetIndex++;
			getResponse(requestPacket, responsePacket, _packetIndex - 1, 0, 4);
			if(responsePacket.size() >= 9  && responsePacket.at(6) == 1)
			{
				_out.printInfo("Info: Update mode enabled.");
				break;
			}
			else if(responsePacket.size() == 9 && responsePacket.at(6) == 8)
			{
				//Operation pending
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				continue;
			}
			if(j == 2)
			{
				_out.printError("Error: Could not enable update mode.");
				return;
			}
		}
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
}

void Hm_Mod_Rpi_Pcb::disableUpdateMode()
{
	try
	{
		if(!_initComplete || _stopped) return;
		reconnect();
		_updateMode = false;
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
}

void Hm_Mod_Rpi_Pcb::openDevice()
{
	try
	{
		if(_fileDescriptor->descriptor > -1) closeDevice();

		_lockfile = GD::bl->settings.lockFilePath() + "LCK.." + _settings->device.substr(_settings->device.find_last_of('/') + 1);
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
				_out.printCritical("Device is in use: " + _settings->device);
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

		_fileDescriptor = _bl->fileDescriptorManager.add(open(_settings->device.c_str(), O_RDWR | O_NOCTTY | O_NDELAY));
		if(_fileDescriptor->descriptor == -1)
		{
			_out.printCritical("Couldn't open device \"" + _settings->device + "\": " + strerror(errno));
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

void Hm_Mod_Rpi_Pcb::closeDevice()
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

void Hm_Mod_Rpi_Pcb::setupDevice()
{
	try
	{
		if(_fileDescriptor->descriptor == -1) return;
		memset(&_termios, 0, sizeof(termios));

		_termios.c_cflag = B115200 | CS8 | CREAD;
		_termios.c_iflag = 0;
		_termios.c_oflag = 0;
		_termios.c_lflag = 0;
		_termios.c_cc[VMIN] = 1;
		_termios.c_cc[VTIME] = 0;

		cfsetispeed(&_termios, B115200);
		cfsetospeed(&_termios, B115200);

		if(tcflush(_fileDescriptor->descriptor, TCIFLUSH) == -1) _out.printError("Couldn't flush device " + _settings->device);
		if(tcsetattr(_fileDescriptor->descriptor, TCSANOW, &_termios) == -1) _out.printError("Couldn't set flush device settings: " + _settings->device);

		std::this_thread::sleep_for(std::chrono::milliseconds(2000));

		int flags = fcntl(_fileDescriptor->descriptor, F_GETFL);
		if(!(flags & O_NONBLOCK))
		{
			if(fcntl(_fileDescriptor->descriptor, F_SETFL, flags | O_NONBLOCK) == -1)
			{
				_out.printError("Couldn't set device to non blocking mode: " + _settings->device);
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

void Hm_Mod_Rpi_Pcb::addPeer(PeerInfo peerInfo)
{
	try
	{
		if(peerInfo.address == 0) return;
		_peersMutex.lock();
		_peers[peerInfo.address] = peerInfo;

		if(_initComplete)
		{
			int64_t id;
			std::shared_ptr<BaseLib::ITimedQueueEntry> entry(new AddPeerQueueEntry(peerInfo, AddPeerQueueEntryType::add, BaseLib::HelperFunctions::getTime()));
			enqueue(0, entry, id);
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
}

void Hm_Mod_Rpi_Pcb::addPeers(std::vector<PeerInfo>& peerInfos)
{
	try
	{
		_peersMutex.lock();
		for(std::vector<PeerInfo>::iterator i = peerInfos.begin(); i != peerInfos.end(); ++i)
		{
			if(i->address == 0) continue;
			_peers[i->address] = *i;
			if(_initComplete) sendPeer(*i);
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
}

void Hm_Mod_Rpi_Pcb::processQueueEntry(int32_t index, int64_t id, std::shared_ptr<BaseLib::ITimedQueueEntry>& entry)
{
	try
	{
		std::shared_ptr<AddPeerQueueEntry> queueEntry;
		queueEntry = std::dynamic_pointer_cast<AddPeerQueueEntry>(entry);
		if(!queueEntry) return;
		if(!_initComplete) return;
		if(queueEntry->type == AddPeerQueueEntryType::remove)
		{
			for(int32_t i = 0; i < 40; i++)
			{
				std::vector<uint8_t> responsePacket;
				std::vector<char> requestPacket;
				std::vector<char> payload{ 1, 7 };
				payload.push_back(queueEntry->address >> 16);
				payload.push_back((queueEntry->address >> 8) & 0xFF);
				payload.push_back(queueEntry->address & 0xFF);
				buildPacket(requestPacket, payload);
				_packetIndex++;
				getResponse(requestPacket, responsePacket, _packetIndex - 1, 1, 4);
				if(responsePacket.size() >= 13  && responsePacket.at(6) == 7) break;
				else if(responsePacket.size() == 9 && responsePacket.at(6) == 8)
				{
					//Operation pending
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
					continue;
				}
				if(i == 2)
				{
					_out.printError("Error: Could not remove peer with address 0x" + _bl->hf.getHexString(queueEntry->address, 6));
					_peersMutex.unlock();
					return;
				}
			}
		}
		else if(queueEntry->type == AddPeerQueueEntryType::wakeUp)
		{
			for(int32_t j = 0; j < 40; j++)
			{
				std::vector<uint8_t> responsePacket;
				std::vector<char> requestPacket;
				std::vector<char> payload{ 1, 6 };
				payload.push_back(queueEntry->peerInfo.address >> 16);
				payload.push_back((queueEntry->peerInfo.address >> 8) & 0xFF);
				payload.push_back(queueEntry->peerInfo.address & 0xFF);
				payload.push_back(queueEntry->peerInfo.keyIndex);
				payload.push_back(queueEntry->peerInfo.wakeUp ? 1 : 0); //CCU2 sets this for wake up, too. No idea, what the meaning is.
				payload.push_back(0);
				buildPacket(requestPacket, payload);
				_packetIndex++;
				getResponse(requestPacket, responsePacket, _packetIndex - 1, 1, 4);
				if(responsePacket.size() >= 21  && responsePacket.at(6) == 7) break;
				else if(responsePacket.size() == 9 && responsePacket.at(6) == 8)
				{
					//Operation pending
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
					continue;
				}
				if(j == 2)
				{
					_out.printError("Error: Could not add peer with address 0x" + _bl->hf.getHexString(queueEntry->peerInfo.address, 6));
					return;
				}
			}
		}
		else if(queueEntry->type == AddPeerQueueEntryType::aes)
		{
			if(queueEntry->peerInfo.aesChannels.find(queueEntry->channel) == queueEntry->peerInfo.aesChannels.end()) return;
			for(int32_t j = 0; j < 40; j++)
			{
				std::vector<uint8_t> responsePacket;
				std::vector<char> requestPacket;
				std::vector<char> payload{ 1 };
				if(queueEntry->peerInfo.aesChannels.at(queueEntry->channel)) payload.push_back(9);
				else payload.push_back(0xA);
				payload.push_back(queueEntry->peerInfo.address >> 16);
				payload.push_back((queueEntry->peerInfo.address >> 8) & 0xFF);
				payload.push_back(queueEntry->peerInfo.address & 0xFF);
				payload.push_back(0);
				payload.push_back(queueEntry->channel);
				buildPacket(requestPacket, payload);
				_packetIndex++;
				getResponse(requestPacket, responsePacket, _packetIndex - 1, 1, 4);
				if(responsePacket.size() >= 9  && responsePacket.at(6) == 1) break;
				else if(responsePacket.size() == 9 && responsePacket.at(6) == 8)
				{
					//Operation pending
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
					continue;
				}
				if(j == 2)
				{
					_out.printError("Error: Could not set AES for peer with address 0x" + _bl->hf.getHexString(queueEntry->peerInfo.address, 6));
					return;
				}
			}
		}
		else sendPeer(queueEntry->peerInfo);
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

void Hm_Mod_Rpi_Pcb::sendPeers()
{
	try
	{
		_peersMutex.lock();
		for(std::map<int32_t, PeerInfo>::iterator i = _peers.begin(); i != _peers.end(); ++i)
		{
			sendPeer(i->second);
		}
		_initComplete = true; //Init complete is set here within _peersMutex, so there is no conflict with addPeer() and peers are not sent twice
		_out.printInfo("Info: Peer sending completed.");
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

    //There is no duty cycle => tested with version 1.1.3
    /*std::thead t1(&Hm_Mod_Rpi_Pcb::dutyCycleTest, this, 0x123456);
	t1.detach();*/
}

void Hm_Mod_Rpi_Pcb::sendPeer(PeerInfo& peerInfo)
{
	try
	{
		if(GD::bl->debugLevel > 4) GD::out.printDebug("Debug: Sending peer to LGW \"" + _settings->id + "\": Address " + GD::bl->hf.getHexString(peerInfo.address, 6) + ", AES enabled " + std::to_string(peerInfo.aesEnabled) + ", AES map " + GD::bl->hf.getHexString(peerInfo.getAESChannelMap()) + ".");
		for(int32_t i = 0; i < 2; i++) //The CCU sends this packet two times, I don't know why
		{
			//Get current config
			for(int32_t j = 0; j < 40; j++)
			{
				std::vector<uint8_t> responsePacket;
				std::vector<char> requestPacket;
				std::vector<char> payload{ 1, 6 };
				payload.push_back(peerInfo.address >> 16);
				payload.push_back((peerInfo.address >> 8) & 0xFF);
				payload.push_back(peerInfo.address & 0xFF);
				payload.push_back(0);
				payload.push_back(0);
				payload.push_back(0);
				buildPacket(requestPacket, payload);
				_packetIndex++;
				getResponse(requestPacket, responsePacket, _packetIndex - 1, 1, 4);
				if(responsePacket.size() >= 21  && responsePacket.at(6) == 7) break;
				else if(responsePacket.size() == 9 && responsePacket.at(6) == 8)
				{
					//Operation pending
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
					continue;
				}
				if(j == 2)
				{
					_out.printError("Error: Could not add peer with address 0x" + _bl->hf.getHexString(peerInfo.address, 6));
					return;
				}
			}
		}
		//Reset all channels
		for(int32_t j = 0; j < 40; j++)
		{
			std::vector<uint8_t> responsePacket;
			std::vector<char> requestPacket;
			std::vector<char> payload{ 1, 0xA };
			payload.push_back(peerInfo.address >> 16);
			payload.push_back((peerInfo.address >> 8) & 0xFF);
			payload.push_back(peerInfo.address & 0xFF);
			payload.push_back(0);

			for(std::map<int32_t, bool>::iterator k = peerInfo.aesChannels.begin(); k != peerInfo.aesChannels.end(); ++k)
			{
				payload.push_back(k->first);
			}
			buildPacket(requestPacket, payload);
			_packetIndex++;
			getResponse(requestPacket, responsePacket, _packetIndex - 1, 1, 4);
			if(responsePacket.size() >= 9  && responsePacket.at(6) == 1) break;
			else if(responsePacket.size() == 9 && responsePacket.at(6) == 8)
			{
				//Operation pending
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				continue;
			}
			if(j == 2)
			{
				_out.printError("Error: Could not add peer with address 0x" + _bl->hf.getHexString(peerInfo.address, 6));
				return;
			}
		}
		//Get current config again
		for(int32_t j = 0; j < 40; j++)
		{
			std::vector<uint8_t> responsePacket;
			std::vector<char> requestPacket;
			std::vector<char> payload{ 1, 6 };
			payload.push_back(peerInfo.address >> 16);
			payload.push_back((peerInfo.address >> 8) & 0xFF);
			payload.push_back(peerInfo.address & 0xFF);
			payload.push_back(0);
			payload.push_back(0);
			payload.push_back(0);
			buildPacket(requestPacket, payload);
			_packetIndex++;
			getResponse(requestPacket, responsePacket, _packetIndex - 1, 1, 4);
			if(responsePacket.size() >= 21  && responsePacket.at(6) == 7) break;
			else if(responsePacket.size() == 9 && responsePacket.at(6) == 8)
			{
				//Operation pending
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				continue;
			}
			if(j == 2)
			{
				_out.printError("Error: Could not add peer with address 0x" + _bl->hf.getHexString(peerInfo.address, 6));
				return;
			}
		}
		if(peerInfo.wakeUp)
		{
			//Enable sending of wake up packet or just request config again?
			for(int32_t j = 0; j < 40; j++)
			{
				std::vector<uint8_t> responsePacket;
				std::vector<char> requestPacket;
				std::vector<char> payload{ 1, 6 };
				payload.push_back(peerInfo.address >> 16);
				payload.push_back((peerInfo.address >> 8) & 0xFF);
				payload.push_back(peerInfo.address & 0xFF);
				payload.push_back(0);
				payload.push_back(1);
				payload.push_back(0);
				buildPacket(requestPacket, payload);
				_packetIndex++;
				getResponse(requestPacket, responsePacket, _packetIndex - 1, 1, 4);
				if(responsePacket.size() >= 21  && responsePacket.at(6) == 7) break;
				else if(responsePacket.size() == 9 && responsePacket.at(6) == 8)
				{
					//Operation pending
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
					continue;
				}
				if(j == 2)
				{
					_out.printError("Error: Could not add peer with address 0x" + _bl->hf.getHexString(peerInfo.address, 6));
					return;
				}
			}
		}
		//Set key index and enable sending of wake up packet.
		for(int32_t j = 0; j < 40; j++)
		{
			std::vector<uint8_t> responsePacket;
			std::vector<char> requestPacket;
			std::vector<char> payload{ 1, 6 };
			payload.push_back(peerInfo.address >> 16);
			payload.push_back((peerInfo.address >> 8) & 0xFF);
			payload.push_back(peerInfo.address & 0xFF);
			payload.push_back(peerInfo.keyIndex);
			payload.push_back(peerInfo.wakeUp ? 1 : 0);
			payload.push_back(0);
			buildPacket(requestPacket, payload);
			_packetIndex++;
			getResponse(requestPacket, responsePacket, _packetIndex - 1, 1, 4);
			if(responsePacket.size() >= 21  && responsePacket.at(6) == 7) break;
			else if(responsePacket.size() == 9 && responsePacket.at(6) == 8)
			{
				//Operation pending
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				continue;
			}
			if(j == 2)
			{
				_out.printError("Error: Could not add peer with address 0x" + _bl->hf.getHexString(peerInfo.address, 6));
				return;
			}
		}
		//Enable AES
		if(peerInfo.aesEnabled)
		{
			//Delete old configuration
			for(int32_t j = 0; j < 40; j++)
			{
				std::vector<uint8_t> responsePacket;
				std::vector<char> requestPacket;
				std::vector<char> payload{ 1, 9 };
				payload.push_back(peerInfo.address >> 16);
				payload.push_back((peerInfo.address >> 8) & 0xFF);
				payload.push_back(peerInfo.address & 0xFF);
				payload.push_back(0);

				bool aesEnabled = false;
				for(std::map<int32_t, bool>::iterator k = peerInfo.aesChannels.begin(); k != peerInfo.aesChannels.end(); ++k)
				{
					if(k->second)
					{
						aesEnabled = true;
						payload.push_back(k->first);
					}
				}
				if(!aesEnabled) break;
				buildPacket(requestPacket, payload);
				_packetIndex++;
				getResponse(requestPacket, responsePacket, _packetIndex - 1, 1, 4);
				if(responsePacket.size() >= 9  && responsePacket.at(6) == 1) break;
				else if(responsePacket.size() == 9 && responsePacket.at(6) == 8)
				{
					//Operation pending
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
					continue;
				}
				if(j == 2)
				{
					_out.printError("Error: Could not add peer with address 0x" + _bl->hf.getHexString(peerInfo.address, 6));
					return;
				}
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

void Hm_Mod_Rpi_Pcb::setAES(PeerInfo peerInfo, int32_t channel)
{
	try
	{
		if(!_initComplete || _stopped) return;
		try
		{
			_peersMutex.lock();
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

		if(_initComplete)
		{
			int64_t id;
			std::shared_ptr<BaseLib::ITimedQueueEntry> entry(new AddPeerQueueEntry(peerInfo, channel, AddPeerQueueEntryType::aes, BaseLib::HelperFunctions::getTime()));
			enqueue(0, entry, id);
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

void Hm_Mod_Rpi_Pcb::setWakeUp(PeerInfo peerInfo)
{
	try
	{
		if(!_initComplete || _stopped) return;
		try
		{
			_peersMutex.lock();
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

		if(_initComplete)
		{
			int64_t id;
			std::shared_ptr<BaseLib::ITimedQueueEntry> entry(new AddPeerQueueEntry(peerInfo, AddPeerQueueEntryType::wakeUp, BaseLib::HelperFunctions::getTime()));
			enqueue(0, entry, id);
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

void Hm_Mod_Rpi_Pcb::removePeer(int32_t address)
{
	try
	{
		_peersMutex.lock();
		if(_peers.find(address) == _peers.end())
		{
			_peersMutex.unlock();
			return;
		}
		_peers.erase(address);

		if(_initComplete)
		{
			int64_t id;
			std::shared_ptr<BaseLib::ITimedQueueEntry> entry(new AddPeerQueueEntry(address, AddPeerQueueEntryType::remove, BaseLib::HelperFunctions::getTime()));
			enqueue(0, entry, id);
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
}

void Hm_Mod_Rpi_Pcb::sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet)
{
	try
	{
		if(!packet)
		{
			_out.printWarning("Warning: Packet was nullptr.");
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
		if(bidCoSPacket->messageType() == 0x04 && bidCoSPacket->payload()->size() == 2 && bidCoSPacket->payload()->at(0) == 1) //Set new AES key if necessary
		{
			std::lock_guard<std::mutex> peersGuard(_peersMutex);
			std::map<int32_t, PeerInfo>::iterator peerIterator = _peers.find(bidCoSPacket->destinationAddress());
			if(peerIterator != _peers.end())
			{
				if((bidCoSPacket->payload()->at(1) + 2) / 2 <= peerIterator->second.keyIndex)
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

		std::vector<char> packetBytes = bidCoSPacket->byteArraySigned();
		if(_bl->debugLevel >= 4) _out.printInfo("Info: Sending (" + _settings->id + "): " + _bl->hf.getHexString(packetBytes));

		for(int32_t j = 0; j < 40; j++)
		{
			std::vector<uint8_t> responsePacket;
			std::vector<char> requestPacket;
			std::vector<char> payload;
			payload.reserve(5 + packetBytes.size() - 1);
			payload.push_back(1);
			payload.push_back(2);
			payload.push_back(0);
			payload.push_back(0);
			payload.push_back((bidCoSPacket->controlByte() & 0x10) ? 1 : 0);
			payload.insert(payload.end(), packetBytes.begin() + 1, packetBytes.end());
			buildPacket(requestPacket, payload);
			_packetIndex++;
			getResponse(requestPacket, responsePacket, _packetIndex - 1, 1, 4);
			if(responsePacket.size() == 9  && responsePacket.at(6) == 8)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
				//Resend
				continue;
			}
			else if(responsePacket.size() >= 9  && responsePacket.at(6) != 4)
			{
				//I assume byte 7 with i. e. 0x02 is the message type
				if(responsePacket.at(6) == 0x0D)
				{
					//0x0D is returned, when there is no response to the A003 packet and if the 8002
					//packet doesn't match
					//Example: FD000501BC040D025E14
					_out.printInfo("Info: AES handshake failed for packet, because either the response data of the last handshake packet didn't match or the last handshake packet wasn't received: " + _bl->hf.getHexString(packetBytes));
					return;
				}
				else if(responsePacket.at(6) == 0x03)
				{
					//Example: FD001001B3040302391A8002282BE6FD26EF00C54D
					_out.printDebug("Debug: Packet was sent successfully: " + _bl->hf.getHexString(packetBytes));
				}
				else if(responsePacket.at(6) == 0x0C)
				{
					//Example: FD00140168040C0228128002282BE6FD26EF00938ABE1C163D
					_out.printDebug("Debug: Packet was sent successfully and AES handshake was successful: " + _bl->hf.getHexString(packetBytes));
				}
				if(responsePacket.size() == 9)
				{
					_out.printDebug("Debug: Packet was sent successfully: " + _bl->hf.getHexString(packetBytes));
					break;
				}
				parsePacket(responsePacket);
				break;
			}
			else if(responsePacket.size() == 9  && responsePacket.at(6) == 4)
			{
				//The gateway tries to send the packet three times, when there is no response
				//NACK (0404) is returned
				//NACK is sometimes also returned when the AES handshake wasn't successful (i. e.
				//the handshake after sending a wake up packet)
				_out.printInfo("Info: No answer to packet " + _bl->hf.getHexString(packetBytes));
				return;
			}
			if(j == 2)
			{
				_out.printInfo("Info: No response from HM-MOD-RPI-PCB to packet " + _bl->hf.getHexString(packetBytes));
				return;
			}
		}

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

void Hm_Mod_Rpi_Pcb::getResponse(const std::vector<char>& packet, std::vector<uint8_t>& response, uint8_t messageCounter, uint8_t responseControlByte, uint8_t responseType)
{
	try
    {
		if(packet.size() < 8 || _stopped) return;
		std::lock_guard<std::mutex> getResponseGuard(_getResponseMutex);
		std::shared_ptr<Request> request(new Request(responseControlByte, responseType));
		_requestsMutex.lock();
		_requests[messageCounter] = request;
		_requestsMutex.unlock();
		std::unique_lock<std::mutex> lock(request->mutex);
		send(packet);
		if(!request->conditionVariable.wait_for(lock, std::chrono::milliseconds(5000), [&] { return request->mutexReady; }))
		{
			_out.printError("Error: No response received to packet: " + _bl->hf.getHexString(packet));
		}
		response = request->response;

		_requestsMutex.lock();
		_requests.erase(messageCounter);
		_requestsMutex.unlock();
	}
	catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
        _requestsMutex.unlock();
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
        _requestsMutex.unlock();
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
        _requestsMutex.unlock();
    }
}

void Hm_Mod_Rpi_Pcb::send(std::string hexString)
{
	try
    {
		if(hexString.empty()) return;
		std::vector<char> data(&hexString.at(0), &hexString.at(0) + hexString.size());
		send(data);
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

void Hm_Mod_Rpi_Pcb::send(const std::vector<char>& data)
{
    try
    {
    	if(data.size() < 3) return; //Otherwise error in printInfo
    	if(_fileDescriptor->descriptor == -1 || _stopped)
    	{
    		_out.printWarning("Warning: !!!Not!!! sending: " + _bl->hf.getHexString(data));
    		_sendMutex.unlock();
    		return;
    	}
    	if(_bl->debugLevel >= 5)
        {
            _out.printDebug("Debug: Sending: " + _bl->hf.getHexString(data));
        }
    	int32_t totallySentBytes = 0;
		std::lock_guard<std::mutex> sendGuard(_sendMutex);
		while (totallySentBytes < (signed)data.size())
		{
			int32_t sentBytes = ::write(_fileDescriptor->descriptor, &data.at(0) + totallySentBytes, data.size() - totallySentBytes);
			if(sentBytes <= 0)
			{
				GD::out.printError("Could not send data to client (" + std::to_string(_fileDescriptor->descriptor) + ")" + (sentBytes == -1 ? ": " + std::string(strerror(errno)) : ""));
				break;
			}
			totallySentBytes += sentBytes;
		}
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
}

void Hm_Mod_Rpi_Pcb::doInit()
{
	try
	{
		_packetIndex = 0;

		if(!GD::family->getCentral())
		{
			_stopCallbackThread = true;
			_out.printError("Error: Could not get central address. Stopping listening.");
			return;
		}

		_myAddress = GD::family->getCentral()->getAddress();

		if(_stopped) return;

		// {{{ Reset
		try
		{
			openGPIO(1, false);
			setGPIO(1, false);
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			setGPIO(1, true);
			closeGPIO(1);
		}
		catch(BaseLib::Exception& ex)
		{
			_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		}
		// }}}

		// {{{ Wait for Co_CPU_BL packet
			std::shared_ptr<Request> request(new Request(0, 0));
			_requestsMutex.lock();
			_requests[0] = request;
			_requestsMutex.unlock();
			std::unique_lock<std::mutex> lock(request->mutex);
			_initStarted = true;
			if(!request->conditionVariable.wait_for(lock, std::chrono::milliseconds(30000), [&] { return request->mutexReady; }))
			{
				_out.printError("Error: No init packet received.");
				_stopped = true;
				return;
			}
			lock.unlock();
			std::string packetString(request->response.begin(), request->response.end());
			_requestsMutex.lock();
			_requests.erase(0);
			_requestsMutex.unlock();
			if(request->response.size() != 17)
			{
				_stopped = true;
				_out.printError("Error: First packet has wrong size. Stopping listening.");
				return;
			}
			packetString.clear();
			packetString.insert(packetString.end(), request->response.begin() + 6, request->response.end() - 2);
			if(packetString != "Co_CPU_BL")
			{
				_stopped = true;
				_out.printError("Error: First packet does not contain \"Co_CPU_BL\". Stopping listening.");
				return;
			}

			//The LXCCU waits for 5.5 seconds
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		// }}}

		//1st packet
		if(_stopped) return;
		std::vector<uint8_t> responsePacket;
		std::vector<char> requestPacket;
		std::vector<char> payload{ 0, 3 };
		buildPacket(requestPacket, payload);
		_packetIndex++;
		getResponse(requestPacket, responsePacket, 0, 0, 0);
		if(responsePacket.size() == 18)
		{
			packetString.clear();
			packetString.insert(packetString.end(), responsePacket.begin() + 6, responsePacket.end() - 2);
			if(packetString != "Co_CPU_App")
			{
				_out.printError("Error: Unknown packet received in response to init packet. Reconnecting...");
				_stopped = true;
				return;
			}
			else _out.printDebug("Debug: Co_CPU_App packet received.");
		}
		else
		{
			_out.printError("Error: Unknown packet received in response to init packet. Reconnecting...");
			_stopped = true;
			return;
		}

		//2nd packet - Get firmware version
		if(_stopped) return;
		responsePacket.clear();
		requestPacket.clear();
		payload.clear();
		payload.reserve(2);
		payload.push_back(0);
		payload.push_back(2);
		buildPacket(requestPacket, payload);
		_packetIndex++;
		getResponse(requestPacket, responsePacket, _packetIndex - 1, 0, 4);
		if(responsePacket.size() < 9 || responsePacket.at(6) == 4)
		{
			if(responsePacket.size() >= 9) _out.printError("Error: NACK received in response to init sequence packet (" + BaseLib::HelperFunctions::getHexString(requestPacket) + "). Response was: " + BaseLib::HelperFunctions::getHexString(responsePacket) + ". Reconnecting...");
			_stopped = true;
			return;
		}
		_out.printInfo("Info: Firmware version: " + std::to_string((int32_t)responsePacket.at(10)) + '.' + std::to_string((int32_t)responsePacket.at(11)) + '.' + std::to_string((int32_t)responsePacket.at(12)));

		//3rd packet
		if(_stopped) return;
		responsePacket.clear();
		requestPacket.clear();
		payload.clear();
		payload.reserve(3);
		payload.push_back(0);
		payload.push_back(0xA);
		payload.push_back(0);
		buildPacket(requestPacket, payload);
		_packetIndex++;
		getResponse(requestPacket, responsePacket, _packetIndex - 1, 0, 4);
		if(responsePacket.size() < 9 || responsePacket.at(6) == 4)
		{
			if(responsePacket.size() >= 9) _out.printError("Error: NACK received in response to init sequence packet (" + BaseLib::HelperFunctions::getHexString(requestPacket) + "). Response was: " + BaseLib::HelperFunctions::getHexString(responsePacket) + ". Reconnecting...");
			_stopped = true;
			return;
		}

		//4th packet - Get serial number
		if(_stopped) return;
		responsePacket.clear();
		requestPacket.clear();
		payload.clear();
		payload.reserve(2);
		payload.push_back(0);
		payload.push_back(0xB);
		buildPacket(requestPacket, payload);
		_packetIndex++;
		getResponse(requestPacket, responsePacket, _packetIndex - 1, 0, 4);
		if(responsePacket.size() < 19 || responsePacket.at(6) == 4)
		{
			if(responsePacket.size() >= 9) _out.printError("Error: NACK received in response to init sequence packet (" + BaseLib::HelperFunctions::getHexString(requestPacket) + "). Response was: " + BaseLib::HelperFunctions::getHexString(responsePacket) + ". Reconnecting...");
			_stopped = true;
			return;
		}
		if(responsePacket.at(7) != 0xFF) //Valid response?
		{
			std::string serialNumber((char*)responsePacket.data() + 7, 10);
			_out.printInfo("Info: Serial number: " + serialNumber);
		}

		//5th packet - Set time
		if(_stopped) return;
		responsePacket.clear();
		requestPacket.clear();
		payload.clear();
		const auto timePoint = std::chrono::system_clock::now();
		time_t t = std::chrono::system_clock::to_time_t(timePoint);
		tm* localTime = std::localtime(&t);
		uint32_t time = (uint32_t)t;
		payload.push_back(0);
		payload.push_back(0xE);
		payload.push_back(time >> 24);
		payload.push_back((time >> 16) & 0xFF);
		payload.push_back((time >> 8) & 0xFF);
		payload.push_back(time & 0xFF);
		payload.push_back(localTime->tm_gmtoff / 1800);
		buildPacket(requestPacket, payload);
		_packetIndex++;
		getResponse(requestPacket, responsePacket, _packetIndex - 1, 0, 4);
		if(responsePacket.size() < 9 || responsePacket.at(6) == 4)
		{
			if(responsePacket.size() >= 9) _out.printError("Error: NACK received in response to init sequence packet (" + BaseLib::HelperFunctions::getHexString(requestPacket) + "). Response was: " + BaseLib::HelperFunctions::getHexString(responsePacket) + ". Reconnecting...");
			_stopped = true;
			return;
		}

		//6th packet - Set RF key
		if(_stopped) return;
		responsePacket.clear();
		requestPacket.clear();
		payload.clear();
		payload.reserve(2 + 16 + 1);
		payload.push_back(1);
		payload.push_back(3);
		if(_rfKey.empty())
		{
			payload.push_back(0);
		}
		else
		{
			payload.insert(payload.end(), _rfKey.begin(), _rfKey.end());
			payload.push_back(_currentRfKeyIndex);
		}
		buildPacket(requestPacket, payload);
		_packetIndex++;
		getResponse(requestPacket, responsePacket, _packetIndex - 1, 1, 4);
		if(responsePacket.size() < 9 || responsePacket.at(6) == 4)
		{
			if(responsePacket.size() >= 9) _out.printError("Error: NACK received in response to init sequence packet (" + BaseLib::HelperFunctions::getHexString(requestPacket) + "). Response was: " + BaseLib::HelperFunctions::getHexString(responsePacket) + ". Reconnecting...");
			_stopped = true;
			return;
		}

		//7th packet - Set old RF key
		if(_currentRfKeyIndex > 1 && !_oldRfKey.empty())
		{
			if(_stopped) return;
			responsePacket.clear();
			requestPacket.clear();
			payload.clear();
			payload.reserve(2 + 16 + 1);
			payload.push_back(1);
			payload.push_back(0xF);
			payload.insert(payload.end(), _oldRfKey.begin(), _oldRfKey.end());
			payload.push_back(_currentRfKeyIndex - 1);
			buildPacket(requestPacket, payload);
			_packetIndex++;
			getResponse(requestPacket, responsePacket, _packetIndex - 1, 1, 4);
			if(responsePacket.size() < 9 || responsePacket.at(6) == 4)
			{
				if(responsePacket.size() >= 9) _out.printError("Error: NACK received in response to init sequence packet (" + BaseLib::HelperFunctions::getHexString(requestPacket) + "). Response was: " + BaseLib::HelperFunctions::getHexString(responsePacket) + ". Reconnecting...");
				_stopped = true;
				return;
			}
		}

		//8th packet - Set address
		if(_stopped) return;
		responsePacket.clear();
		requestPacket.clear();
		payload.clear();
		payload.reserve(5);
		payload.push_back(1);
		payload.push_back(0);
		payload.push_back(_myAddress >> 16);
		payload.push_back((_myAddress >> 8) & 0xFF);
		payload.push_back(_myAddress & 0xFF);
		buildPacket(requestPacket, payload);
		_packetIndex++;
		getResponse(requestPacket, responsePacket, _packetIndex - 1, 1, 4);
		if(responsePacket.size() < 9 || responsePacket.at(6) == 4)
		{
			if(responsePacket.size() >= 9) _out.printError("Error: NACK received in response to init sequence packet (" + BaseLib::HelperFunctions::getHexString(requestPacket) + "). Response was: " + BaseLib::HelperFunctions::getHexString(responsePacket) + ". Reconnecting...");
			_stopped = true;
			return;
		}

		//9th packet - Disable update mode
		if(_stopped) return;
		responsePacket.clear();
		requestPacket.clear();
		payload.clear();
		payload.reserve(2);
		payload.push_back(0);
		payload.push_back(6);
		buildPacket(requestPacket, payload);
		_packetIndex++;
		getResponse(requestPacket, responsePacket, _packetIndex - 1, 0, 4);
		if(responsePacket.size() < 9 || responsePacket.at(6) == 4)
		{
			if(responsePacket.size() >= 9) _out.printError("Error: NACK received in response to init sequence packet (" + BaseLib::HelperFunctions::getHexString(requestPacket) + "). Response was: " + BaseLib::HelperFunctions::getHexString(responsePacket) + ". Reconnecting...");
			_stopped = true;
			return;
		}

		if(_stopped) return;
		_out.printInfo("Info: Init queue completed. Sending peers...");
		sendPeers();
		return;
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
}

void Hm_Mod_Rpi_Pcb::startListening()
{
	try
	{
		stopListening();
		if(_rfKey.empty())
		{
			_out.printError("Error: Cannot start listening, because rfKey is not specified.");
			return;
		}
		openDevice();
		if(_fileDescriptor->descriptor == -1) return;
		_out.printDebug("Connecting to HM-MOD-RPI-PCB...");
		_stopped = false;
		if(_settings->listenThreadPriority > -1) GD::bl->threadManager.start(_listenThread, true, _settings->listenThreadPriority, _settings->listenThreadPolicy, &Hm_Mod_Rpi_Pcb::listen, this);
		else GD::bl->threadManager.start(_listenThread, true, &Hm_Mod_Rpi_Pcb::listen, this);
		if(_settings->listenThreadPriority > -1) GD::bl->threadManager.start(_initThread, true, _settings->listenThreadPriority, _settings->listenThreadPolicy, &Hm_Mod_Rpi_Pcb::doInit, this);
		else GD::bl->threadManager.start(_initThread, true, &Hm_Mod_Rpi_Pcb::doInit, this);
		startQueue(0, 0, SCHED_OTHER);
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

void Hm_Mod_Rpi_Pcb::reconnect()
{
	try
	{
		closeDevice();
		GD::bl->threadManager.join(_initThread);
		_requestsMutex.lock();
		_requests.clear();
		_requestsMutex.unlock();
		_initStarted = false;
		_initComplete = false;
		_out.printDebug("Connecting to HM-MOD-RPI-PCB...");
		openDevice();
		_out.printInfo("Connected to HM-MOD-RPI-PCB.");
		_stopped = false;
		if(_settings->listenThreadPriority > -1) GD::bl->threadManager.start(_initThread, true, _settings->listenThreadPriority, _settings->listenThreadPolicy, &Hm_Mod_Rpi_Pcb::doInit, this);
		else GD::bl->threadManager.start(_initThread, true, &Hm_Mod_Rpi_Pcb::doInit, this);
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

void Hm_Mod_Rpi_Pcb::stopListening()
{
	try
	{
		stopQueue(0);
		_stopCallbackThread = true;
		GD::bl->threadManager.join(_initThread);
		GD::bl->threadManager.join(_listenThread);
		_stopCallbackThread = false;
		_stopped = true;
		closeDevice();
		_requestsMutex.lock();
		_requests.clear();
		_requestsMutex.unlock();
		_initStarted = false;
		_initComplete = false;
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

void Hm_Mod_Rpi_Pcb::sendTimePacket()
{
	try
    {
		std::lock_guard<std::mutex> getResponseGuard(_getResponseMutex);
		const auto timePoint = std::chrono::system_clock::now();
		time_t t = std::chrono::system_clock::to_time_t(timePoint);
		tm* localTime = std::localtime(&t);
		uint32_t time = (uint32_t)t;
		std::vector<char> payload{ 0, 0xE };
		payload.push_back(time >> 24);
		payload.push_back((time >> 16) & 0xFF);
		payload.push_back((time >> 8) & 0xFF);
		payload.push_back(time & 0xFF);
		payload.push_back(localTime->tm_gmtoff / 1800);
		std::vector<char> packet;
		buildPacket(packet, payload);
		_packetIndex++;
		send(packet);
		_lastTimePacket = BaseLib::HelperFunctions::getTimeSeconds();
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

void Hm_Mod_Rpi_Pcb::listen()
{
    try
    {
    	while(!_initStarted && !_stopCallbackThread)
    	{
    		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    	}

    	int32_t result = 0;
    	int32_t bytesRead = 0;
		std::vector<char> buffer(2048);
		_lastTimePacket = BaseLib::HelperFunctions::getTimeSeconds();

		std::vector<uint8_t> data;
        while(!_stopCallbackThread)
        {
        	try
        	{
				if(_stopped)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
					if(_stopCallbackThread) return;
					_out.printWarning("Warning: Connection closed (1). Trying to reconnect...");
					reconnect();
					continue;
				}
				try
				{
					if(BaseLib::HelperFunctions::getTimeSeconds() - _lastTimePacket > 1800) sendTimePacket();

					if(_fileDescriptor->descriptor == -1) break;
					timeval timeout;
					timeout.tv_sec = 5;
					timeout.tv_usec = 0;
					fd_set readFileDescriptor;
					FD_ZERO(&readFileDescriptor);
					{
						auto fileDescriptorGuard = GD::bl->fileDescriptorManager.getLock();
						fileDescriptorGuard.lock();
						FD_SET(_fileDescriptor->descriptor, &readFileDescriptor);
					}

					result = select(_fileDescriptor->descriptor + 1, &readFileDescriptor, NULL, NULL, &timeout);
					if(result == 0) continue;
					else if(result == -1)
					{
						if(errno == EINTR) continue;
						_out.printWarning("Warning: Connection closed (2). Trying to reconnect...");
						_stopped = true;
						continue;
					}

					bytesRead = read(_fileDescriptor->descriptor, buffer.data(), buffer.size());
					if(bytesRead <= 0) //read returns 0, when connection is disrupted.
					{
						_out.printWarning("Warning: Connection closed (3). Trying to reconnect...");
						_stopped = true;
						continue;
					}

					if(bytesRead > (signed)buffer.size()) bytesRead = buffer.size();
					data.insert(data.end(), buffer.begin(), buffer.begin() + bytesRead);

					if(data.size() > 100000)
					{
						_out.printError("Could not read from HM-MOD-RPI-PCB: Too much data.");
						data.clear();
						break;
					}
				}
				catch(const BaseLib::SocketTimeOutException& ex)
				{
					if(data.empty()) //When receivedBytes is exactly 2048 bytes long, proofread will be called again, time out and the packet is received with a delay of 5 seconds. It doesn't matter as packets this big are only received at start up.
					{
						continue;
					}
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

				if(_bl->debugLevel >= 5) _out.printDebug("Debug: Packet received. Raw data: " + BaseLib::HelperFunctions::getHexString(data));

				if(data.empty()) continue;
				if(data.size() > 100000)
				{
					data.clear();
					continue;
				}

				processData(data);
				data.clear();

				_lastPacketReceived = BaseLib::HelperFunctions::getTime();
			}
			catch(const std::exception& ex)
			{
				_stopped = true;
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(BaseLib::Exception& ex)
			{
				_stopped = true;
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
			}
			catch(...)
			{
				_stopped = true;
				_out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
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

void Hm_Mod_Rpi_Pcb::buildPacket(std::vector<char>& packet, const std::vector<char>& payload)
{
	try
	{
		std::vector<char> unescapedPacket;
		unescapedPacket.push_back(0xFD);
		int32_t size = payload.size() + 1; //Payload size plus message counter size - control byte
		unescapedPacket.push_back(size >> 8);
		unescapedPacket.push_back(size & 0xFF);
		unescapedPacket.push_back(payload.at(0));
		unescapedPacket.push_back(_packetIndex);
		unescapedPacket.insert(unescapedPacket.end(), payload.begin() + 1, payload.end());
		uint16_t crc = _crc.calculate(unescapedPacket);
		unescapedPacket.push_back(crc >> 8);
		unescapedPacket.push_back(crc & 0xFF);
		escapePacket(unescapedPacket, packet);
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

void Hm_Mod_Rpi_Pcb::escapePacket(const std::vector<char>& unescapedPacket, std::vector<char>& escapedPacket)
{
	try
	{
		escapedPacket.clear();
		if(unescapedPacket.empty()) return;
		escapedPacket.push_back(unescapedPacket[0]);
		for(uint32_t i = 1; i < unescapedPacket.size(); i++)
		{
			if(unescapedPacket[i] == (char)0xFC || unescapedPacket[i] == (char)0xFD)
			{
				escapedPacket.push_back(0xFC);
				escapedPacket.push_back(unescapedPacket[i] & (char)0x7F);
			}
			else escapedPacket.push_back(unescapedPacket[i]);
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

void Hm_Mod_Rpi_Pcb::processPacket(std::vector<uint8_t>& packet)
{
	try
	{
		_out.printDebug(std::string("Debug: Packet received from HM-MOD-RPI-PCB: " + _bl->hf.getHexString(packet)));
		if(packet.size() < 8) return;
		uint16_t crc = _crc.calculate(packet, true);
		if((packet.at(packet.size() - 2) != (crc >> 8) || packet.at(packet.size() - 1) != (crc & 0xFF)))
		{
			_out.printError("Error: CRC (" + BaseLib::HelperFunctions::getHexString(crc, 4) + ") failed on packet received from HM-MOD-RPI-PCB: " + _bl->hf.getHexString(packet));
			return;
		}
		else
		{
			{
				std::unique_lock<std::mutex> requestsGuard(_requestsMutex);
				if(_requests.find(packet.at(4)) != _requests.end())
				{
					std::shared_ptr<Request> request = _requests.at(packet.at(4));
					requestsGuard.unlock();
					if(packet.at(3) == request->getResponseControlByte() && packet.at(5) == request->getResponseType())
					{
						request->response = packet;
						{
							std::lock_guard<std::mutex> lock(request->mutex);
							request->mutexReady = true;
						}
						request->conditionVariable.notify_all();
						return;
					}
				}
			}
			if(_initComplete) parsePacket(packet);
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

void Hm_Mod_Rpi_Pcb::processData(std::vector<uint8_t>& data)
{
	try
	{
		std::vector<uint8_t> packet;
		if(!_packetBuffer.empty()) packet = _packetBuffer;
		for(std::vector<uint8_t>::iterator i = data.begin(); i != data.end(); ++i)
		{
			if(!packet.empty() && *i == 0xfd)
			{
				processPacket(packet);
				packet.clear();
				_escapeByte = false;
			}
			if(*i == 0xfc)
			{
				_escapeByte = true;
				continue;
			}
			if(_escapeByte)
			{
				packet.push_back(*i | 0x80);
				_escapeByte = false;
			}
			else packet.push_back(*i);
		}
		int32_t size = (packet.size() > 5) ? (((int32_t)packet.at(1)) << 8) + packet.at(2) + 5 : 0;
		if(size < 0) size = 0;
		if(size > 0 && size < 8) _out.printWarning("Warning: Too small packet received: " + _bl->hf.getHexString(data));
		else if(size > 255) _out.printWarning("Warning: Too large packet received: " + _bl->hf.getHexString(data));
		else if(packet.size() < 8 || packet.size() < (unsigned)size) _packetBuffer = packet;
		else
		{
			processPacket(packet);
			_packetBuffer.clear();
			_escapeByte = false;
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

void Hm_Mod_Rpi_Pcb::parsePacket(std::vector<uint8_t>& packet)
{
	try
	{
		if(packet.empty()) return;
		if((packet.at(5) == 5 || (packet.at(5) == 4 && packet.at(6) != 7)) && packet.at(3) == 1 && packet.size() >= 20)
		{
			std::vector<uint8_t> binaryPacket({(uint8_t)(packet.size() - 11)});
			binaryPacket.insert(binaryPacket.end(), packet.begin() + 9, packet.end() - 2);
			int32_t rssi = packet.at(8); //Range should be from 0x0B to 0x8A. 0x0B is -11dBm 0x8A -138dBm.
			rssi *= -1;
			//Convert to TI CC1101 format
			if(rssi <= -75) rssi = ((rssi + 74) * 2) + 256;
			else rssi = (rssi + 74) * 2;
			binaryPacket.push_back(rssi);
			std::shared_ptr<BidCoSPacket> bidCoSPacket(new BidCoSPacket(binaryPacket, true, BaseLib::HelperFunctions::getTime()));
			//Don't use (packet.at(6) & 1) here. That bit is set for non-AES packets, too
			//packet.at(6) == 3 and packet.at(7) == 0 is set on pairing packets: FD0020018A0503002494840026219BFD00011000AD4C4551303030333835365803FFFFCB99
			if(packet.at(5) == 5 && ((packet.at(6) & 3) == 3 || (packet.at(6) & 5) == 5))
			{
				//Accept pairing packets from HM-TC-IT-WM-W-EU (version 1.0) and maybe other devices.
				//For these devices the handshake is never executed, but the "failed bit" set anyway: Bug
				if(!(bidCoSPacket->controlByte() & 0x4) || bidCoSPacket->messageType() != 0 || bidCoSPacket->payload()->size() != 17)
				{
					_out.printWarning("Warning: AES handshake failed for packet: " + _bl->hf.getHexString(binaryPacket));
					return;
				}
			}
			else if(_bl->debugLevel >= 5 && packet.at(5) == 5 && (packet.at(6) & 3) == 2)
			{
				_out.printDebug("Debug: AES handshake was successful for packet: " + _bl->hf.getHexString(binaryPacket));
			}
			_lastPacketReceived = BaseLib::HelperFunctions::getTime();
			bool wakeUp = packet.at(5) == 5 && (packet.at(6) & 0x10);

			// {{{ Update AES key index
			std::lock_guard<std::mutex> peersGuard(_peersMutex);
			std::map<int32_t, PeerInfo>::iterator peerIterator = _peers.find(bidCoSPacket->senderAddress());
			if(peerIterator != _peers.end())
			{
				if(bidCoSPacket->messageType() == 0x02 && bidCoSPacket->payload()->size() == 8 && bidCoSPacket->payload()->at(0) == 0x04)
				{
					peerIterator->second.keyIndex = bidCoSPacket->payload()->back() / 2;
				}
			}
			// }}}

			raisePacketReceived(bidCoSPacket);
			if(wakeUp) //Wake up was sent
			{
				_out.printInfo("Info: Detected wake-up packet.");
				std::vector<uint8_t> payload;
				payload.push_back(0x00);
				std::shared_ptr<BidCoSPacket> ok(new BidCoSPacket(bidCoSPacket->messageCounter(), 0x80, 0x02, bidCoSPacket->senderAddress(), _myAddress, payload));
				ok->setTimeReceived(bidCoSPacket->timeReceived() + 1);
				std::this_thread::sleep_for(std::chrono::milliseconds(30));
				raisePacketReceived(ok);
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
