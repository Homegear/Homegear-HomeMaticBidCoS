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

#include "HM-CFG-LAN.h"
#include "../GD.h"

namespace BidCoS
{

HM_CFG_LAN::HM_CFG_LAN(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IBidCoSInterface(settings)
{
	_out.init(GD::bl);
	_out.setPrefix(GD::out.getPrefix() + "LAN-Konfigurationsadapter \"" + settings->id + "\": ");

	signal(SIGPIPE, SIG_IGN);

	_socket = std::unique_ptr<BaseLib::TcpSocket>(new BaseLib::TcpSocket(_bl));

	if(!settings)
	{
		_out.printCritical("Critical: Error initializing HM-CFG-LAN. Settings pointer is empty.");
		return;
	}
	if(!settings->lanKey.empty())
	{
		_useAES = true;
		_out.printInfo("Info: Enabling AES encryption for communication with HM-CFG-LAN.");
	}
	else
	{
		_useAES = false;
		_out.printInfo("Info: Disabling AES encryption for communication with HM-CFG-LAN.");
	}

	_reconnecting = false;
}

HM_CFG_LAN::~HM_CFG_LAN()
{
	try
	{
		_stopCallbackThread = true;
		GD::bl->threadManager.join(_listenThread);
		GD::bl->threadManager.join(_reconnectThread);
		if(_useAES) aesCleanup();
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

std::string HM_CFG_LAN::getPeerInfoPacket(PeerInfo& peerInfo)
{
	try
	{
		std::string packetHex = std::string("+") + BaseLib::HelperFunctions::getHexString(peerInfo.address, 6) + ",";
		if(peerInfo.aesEnabled)
		{
			packetHex += peerInfo.wakeUp ? "03," : "01,";
			packetHex += BaseLib::HelperFunctions::getHexString(peerInfo.keyIndex, 2) + ",";
			packetHex += BaseLib::HelperFunctions::getHexString(peerInfo.getAESChannelMap()) + ",";
		}
		else
		{
			packetHex += peerInfo.wakeUp ? "02," : "00,";
			packetHex += "00,";
		}
		packetHex += "\r\n";
		return packetHex;
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
    return "";
}

void HM_CFG_LAN::addPeer(PeerInfo peerInfo)
{
	try
	{
		if(peerInfo.address == 0) return;
		_peersMutex.lock();
		//Remove old peer first. removePeer() is not called, so we don't need to unlock _peersMutex
		if(_peers.find(peerInfo.address) != _peers.end()) _peers.erase(peerInfo.address);
		if(_initComplete)
		{
			std::string packetHex = std::string("-") + BaseLib::HelperFunctions::getHexString(peerInfo.address, 6) + "\r\n";
			send(packetHex);
		}
		_peers[peerInfo.address] = peerInfo;
		std::string packetHex = getPeerInfoPacket(peerInfo);
		if(_initComplete) send(packetHex);
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

void HM_CFG_LAN::addPeers(std::vector<PeerInfo>& peerInfos)
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

void HM_CFG_LAN::sendPeers()
{
	try
	{
		_peersMutex.lock();
		for(std::map<int32_t, PeerInfo>::iterator i = _peers.begin(); i != _peers.end(); ++i)
		{
			send(getPeerInfoPacket(i->second));
		}
		_out.printInfo("Info: Initialization completed.");
		_initComplete = true; //Init complete is set here within _peersMutex, so there is no conflict with addPeer() and peers are not sent twice
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

void HM_CFG_LAN::removePeer(int32_t address)
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
			std::string packetHex = std::string("-") + BaseLib::HelperFunctions::getHexString(address, 6) + "\r\n";
			send(packetHex);
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

void HM_CFG_LAN::sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet)
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

		int64_t currentTimeMilliseconds = BaseLib::HelperFunctions::getTime();
		uint32_t currentTime = currentTimeMilliseconds & 0xFFFFFFFF;
		std::string packetString = packet->hexString();
		if(_bl->debugLevel >= 4) _out.printInfo("Info: Sending (" + _settings->id + "): " + packetString);
		std::string hexString = "S" + BaseLib::HelperFunctions::getHexString(currentTime, 8) + ",00,00000000,01," + BaseLib::HelperFunctions::getHexString(currentTimeMilliseconds - _startUpTime, 8) + "," + packetString.substr(2) + "\r\n";
		send(hexString, false);
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

void HM_CFG_LAN::send(std::string hexString, bool raw)
{
	try
    {
		if(hexString.empty()) return;
		std::vector<char> data(&hexString.at(0), &hexString.at(0) + hexString.size());
		send(data, raw);
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

void HM_CFG_LAN::send(std::vector<char>& data, bool raw)
{
    try
    {
    	if(data.size() < 3) return; //Otherwise error in printInfo
    	std::lock_guard<std::mutex> sendGuard(_sendMutex);
    	std::vector<char> encryptedData;
    	if(_useAES && !raw) encryptedData = encrypt(data);
    	if(!_socket->connected() || _stopped)
    	{
    		_out.printWarning(std::string("Warning: !!!Not!!! sending") + ((_useAES && !raw) ? " (encrypted)" : "") + ": " + std::string(&data.at(0), data.size() - 2));
    		return;
    	}
    	if(_bl->debugLevel >= 5)
        {
            _out.printInfo(std::string("Debug: Sending") + ((_useAES && !raw) ? " (encrypted)" : "") + ": " + std::string(&data.at(0), data.size() - 2));
        }
    	(_useAES && !raw) ? _socket->proofwrite(encryptedData) : _socket->proofwrite(data);
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
}

void HM_CFG_LAN::startListening()
{
	try
	{
		stopListening();
		if(_rfKey.empty())
		{
			_out.printError("Error: Cannot start listening , because rfKey is not specified.");
			return;
		}
		if(_useAES) aesInit();
		_socket = std::unique_ptr<BaseLib::TcpSocket>(new BaseLib::TcpSocket(_bl, _settings->host, _settings->port, _settings->ssl, _settings->caFile, _settings->verifyCertificate));
		_out.printDebug("Debug: Connecting to HM-CFG-LAN with hostname " + _settings->host + " on port " + _settings->port + "...");
		//_socket->open();
		//_out.printInfo("Connected to HM-CFG-LAN device with Hostname " + _settings->host + " on port " + _settings->port + ".");
		_stopped = false;
		if(_settings->listenThreadPriority > -1) GD::bl->threadManager.start(_listenThread, true, _settings->listenThreadPriority, _settings->listenThreadPolicy, &HM_CFG_LAN::listen, this);
		else GD::bl->threadManager.start(_listenThread, true, &HM_CFG_LAN::listen, this);
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

void HM_CFG_LAN::reconnectThread()
{
	try
	{
		_stopped = true;
		_missedKeepAliveResponses = 0;
		std::lock_guard<std::mutex> sendGuard(_sendMutex);
		std::lock_guard<std::mutex> listenGuard(_listenMutex);
		_socket->close();
		if(_useAES) aesCleanup();

		if(_rfKey.empty())
		{
			_out.printError("Error: Cannot start listening , because rfKey is not specified.");
			_reconnecting = false;
			return;
		}
		if(_useAES) aesInit();
		createInitCommandQueue();
		_out.printDebug("Debug: Connecting to HM-CFG-LAN with hostname " + _settings->host + " on port " + _settings->port + "...");
		_socket->open();
		_hostname = _settings->host;
		_ipAddress = _socket->getIpAddress();
		_out.printInfo("Connected to HM-CFG-LAN device with Hostname " + _settings->host + " on port " + _settings->port + ".");
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
    _reconnecting = false;
}

void HM_CFG_LAN::reconnect()
{
	try
	{
		std::lock_guard<std::mutex> reconnectGuard(_reconnectMutex);
		if(_reconnecting) return;
		_reconnecting = true;
		GD::bl->threadManager.join(_reconnectThread);
		GD::bl->threadManager.start(_reconnectThread, true, &HM_CFG_LAN::reconnectThread, this);
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

void HM_CFG_LAN::stopListening()
{
	try
	{
		_stopped = true;
		{
			std::lock_guard<std::mutex> reconnectGuard(_reconnectMutex);
			GD::bl->threadManager.join(_reconnectThread);
		}
		_stopCallbackThread = true;
		GD::bl->threadManager.join(_listenThread);
		_stopCallbackThread = false;
		_socket->close();
		if(_useAES) aesCleanup();
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

void HM_CFG_LAN::createInitCommandQueue()
{
	try
	{
		_initStarted = GD::bl->hf.getTime();
		_initComplete = false;
		_initCommandQueue.clear();

		if(!GD::family->getCentral())
		{
			_stopCallbackThread = true;
			_out.printError("Error: Could not get central address. Stopping listening.");
			return;
		}

		std::vector<char> packet = {'A'};
		_myAddress = GD::family->getCentral()->getAddress();
		std::string hexString = BaseLib::HelperFunctions::getHexString(_myAddress, 6) + "\r\n";
		packet.insert(packet.end(), hexString.begin(), hexString.end());
		_initCommandQueue.push_back(packet);

		packet.clear();
		hexString = "C\r\n";
		if(!_rfKey.empty())
		{
			hexString += "Y01," + BaseLib::HelperFunctions::getHexString(_currentRfKeyIndex, 2) + "," + BaseLib::HelperFunctions::getHexString(_rfKey) + "\r\n";
		}
		else hexString += "Y01,00,\r\n";
		packet.insert(packet.end(), hexString.begin(), hexString.end());
		_initCommandQueue.push_back(packet);

		packet.clear();
		if(!_oldRfKey.empty())
		{
			hexString = "Y02," + BaseLib::HelperFunctions::getHexString(_currentRfKeyIndex - 1, 2) + "," + BaseLib::HelperFunctions::getHexString(_oldRfKey) + "\r\n";
		}
		else hexString = "Y02,00,\r\n";
		packet.insert(packet.end(), hexString.begin(), hexString.end());
		_initCommandQueue.push_back(packet);

		packet.clear();
		hexString = "Y03,00,\r\n";
		packet.insert(packet.end(), hexString.begin(), hexString.end());
		_initCommandQueue.push_back(packet);

		packet.clear();
		const auto timePoint = std::chrono::system_clock::now();
		time_t t = std::chrono::system_clock::to_time_t(timePoint);
		tm* localTime = std::localtime(&t);
		uint32_t time = (uint32_t)(t - 946684800) + 1; //I add one second for processing time
		hexString = "T" + BaseLib::HelperFunctions::getHexString(time, 8) + ',' + BaseLib::HelperFunctions::getHexString(localTime->tm_gmtoff / 1800, 2) + ",00,00000000\r\n";
		packet.insert(packet.end(), hexString.begin(), hexString.end());
		_initCommandQueue.push_back(packet);
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

bool HM_CFG_LAN::aesInit()
{
	aesCleanup();

	if(_settings->lanKey.size() != 32)
	{
		_out.printError("Error: The AES key specified in homematicbidcos.conf for communication with your HM-CFG-LAN has the wrong size.");
		return false;
		//_key.resize(16);
		//MD5((uint8_t*)_settings->lanKey.c_str(), _settings->lanKey.size(), &_key.at(0));
	}
	else
	{
		_key = _bl->hf.getUBinary(_settings->lanKey);
		if(_key.size() != 16)
		{
			_out.printError("Error: The AES key specified in homematicbidcos.conf for communication with your HM-CFG-LAN is not a valid hexadecimal string.");
			return false;
		}
	}

	gcry_error_t result;
	if((result = gcry_cipher_open(&_encryptHandle, GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_CFB, GCRY_CIPHER_SECURE)) != GPG_ERR_NO_ERROR)
	{
		_encryptHandle = nullptr;
		_out.printError("Error initializing cypher handle for encryption: " + BaseLib::Security::Gcrypt::getError(result));
		return false;
	}
	if(!_encryptHandle)
	{
		_out.printError("Error cypher handle for encryption is nullptr.");
		return false;
	}
	if((result = gcry_cipher_setkey(_encryptHandle, &_key.at(0), _key.size())) != GPG_ERR_NO_ERROR)
	{
		aesCleanup();
		_out.printError("Error: Could not set key for encryption: " + BaseLib::Security::Gcrypt::getError(result));
		return false;
	}

	if((result = gcry_cipher_open(&_decryptHandle, GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_CFB, GCRY_CIPHER_SECURE)) != GPG_ERR_NO_ERROR)
	{
		_decryptHandle = nullptr;
		_out.printError("Error initializing cypher handle for decryption: " + BaseLib::Security::Gcrypt::getError(result));
		return false;
	}
	if(!_decryptHandle)
	{
		_out.printError("Error cypher handle for decryption is nullptr.");
		return false;
	}
	if((result = gcry_cipher_setkey(_decryptHandle, &_key.at(0), _key.size())) != GPG_ERR_NO_ERROR)
	{
		aesCleanup();
		_out.printError("Error: Could not set key for decryption: " + BaseLib::Security::Gcrypt::getError(result));
		return false;
	}

	_aesInitialized = true;
	_aesExchangeComplete = false;
	return true;
}

void HM_CFG_LAN::aesCleanup()
{
	if(!_aesInitialized) return;
	_aesInitialized = false;
	if(_decryptHandle) gcry_cipher_close(_decryptHandle);
	if(_encryptHandle) gcry_cipher_close(_encryptHandle);
	_decryptHandle = nullptr;
	_encryptHandle = nullptr;
	_myIV.clear();
	_remoteIV.clear();
	_aesExchangeComplete = false;
}

std::vector<char> HM_CFG_LAN::encrypt(std::vector<char>& data)
{
	std::vector<char> encryptedData(data.size());
	if(!_encryptHandle) return encryptedData;
	gcry_error_t result;
	if((result = gcry_cipher_encrypt(_encryptHandle, &encryptedData.at(0), data.size(), &data.at(0), data.size())) != GPG_ERR_NO_ERROR)
	{
		GD::out.printError("Error encrypting data: " + BaseLib::Security::Gcrypt::getError(result));
		reconnect();
		return std::vector<char>();
	}
	return encryptedData;
}

std::vector<uint8_t> HM_CFG_LAN::decrypt(std::vector<uint8_t>& data)
{
	std::vector<uint8_t> decryptedData(data.size());
	if(!_decryptHandle) return decryptedData;
	gcry_error_t result;
	if((result = gcry_cipher_decrypt(_decryptHandle, &decryptedData.at(0), data.size(), &data.at(0), data.size())) != GPG_ERR_NO_ERROR)
	{
		GD::out.printError("Error decrypting data: " + BaseLib::Security::Gcrypt::getError(result));
		reconnect();
		return std::vector<uint8_t>();
	}
	return decryptedData;
}

void HM_CFG_LAN::sendKeepAlive()
{
	try
    {
		if(!_initComplete) return;
		if(BaseLib::HelperFunctions::getTimeSeconds() - _lastKeepAlive >= 10)
		{
			if(_lastKeepAliveResponse < _lastKeepAlive)
			{
				_lastKeepAliveResponse = _lastKeepAlive;
				_missedKeepAliveResponses++;
				if(_missedKeepAliveResponses >= 3)
				{
					_out.printWarning("Warning: No response to keep alive packet received. Closing connection.");
					_stopped = true;
				}
				else _out.printInfo("Info: No response to keep alive packet received. Closing connection.");
				return;
			}

			_missedKeepAliveResponses = 0;

			_lastKeepAlive = BaseLib::HelperFunctions::getTimeSeconds();
			send(_keepAlivePacket, false);
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

void HM_CFG_LAN::sendTimePacket()
{
	try
    {
		const auto timePoint = std::chrono::system_clock::now();
		time_t t = std::chrono::system_clock::to_time_t(timePoint);
		tm* localTime = std::localtime(&t);
		uint32_t time = (uint32_t)(t - 946684800);
		std::string hexString = "T" + BaseLib::HelperFunctions::getHexString(time, 8) + ',' + BaseLib::HelperFunctions::getHexString(localTime->tm_gmtoff / 1800, 2) + ",00,00000000\r\n";
		send(hexString, false);
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

void HM_CFG_LAN::listen()
{
    try
    {
    	createInitCommandQueue(); //Called here in a seperate thread so that startListening is not blocking Homegear

    	uint32_t receivedBytes = 0;
    	int32_t bufferMax = 2048;
		std::vector<char> buffer(bufferMax);
		_lastKeepAlive = BaseLib::HelperFunctions::getTimeSeconds();
		_lastKeepAliveResponse = _lastKeepAlive;

        while(!_stopCallbackThread)
        {
        	try
        	{
				if(_reconnecting)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
					continue;
				}
				if(_stopped)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(1000));
					if(_stopCallbackThread) return;
					_out.printWarning("Warning: Connection to HM-CFG-LAN closed. Trying to reconnect...");
					reconnect();
					continue;
				}

				{
					std::lock_guard<std::mutex> listenGuard(_listenMutex);
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
									_out.printError("Could not read from HM-CFG-LAN: Too much data.");
									break;
								}
							}
						} while(receivedBytes == (unsigned)bufferMax);
					}
					catch(const BaseLib::SocketTimeOutException& ex)
					{
						if(data.empty()) //When receivedBytes is exactly 2048 bytes long, proofread will be called again, time out and the packet is received with a delay of 5 seconds. It doesn't matter as packets this big are only received at start up.
						{
							if(_socket->connected())
							{
								if(BaseLib::HelperFunctions::getTimeSeconds() - _lastTimePacket > 1800) sendTimePacket();
								sendKeepAlive();
							}
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
					if(data.empty() || data.size() > 1000000) continue;

					if(_bl->debugLevel >= 6)
					{
						_out.printDebug("Debug: Packet received from HM-CFG-LAN. Raw data:");
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

bool HM_CFG_LAN::aesKeyExchange(std::vector<uint8_t>& data)
{
	try
	{
		if(data.at(0) == 'V')
		{
			if(!_useAES)
			{
				reconnect();
				_out.printError("Error: Error communicating with HM-CFG-LAN. Device requires AES, but no AES key was specified in homematicbidcos.conf.");
				return false;
			}
			if(data.size() != 35)
			{
				reconnect();
				_out.printError("Error: Error communicating with HM-CFG-LAN. Received IV has wrong size.");
				return false;
			}
			_remoteIV.clear();
			std::string ivHex((char*)&data.at(1), data.size() - 3);
			_remoteIV = _bl->hf.getUBinary(ivHex);
			if(_remoteIV.size() != 16)
			{
				reconnect();
				_out.printError("Error: Error communicating with HM-CFG-LAN. Received IV is not in hexadecimal format.");
				return false;
			}
			if(_bl->debugLevel >= 5) _out.printDebug("HM-CFG-LAN IV is: " + _bl->hf.getHexString(_remoteIV));

			gcry_error_t result;
			if((result = gcry_cipher_setiv(_encryptHandle, &_remoteIV.at(0), _remoteIV.size())) != GPG_ERR_NO_ERROR)
			{
				_out.printError("Error: Could not set IV for encryption: " + BaseLib::Security::Gcrypt::getError(result));
				reconnect();
				return false;
			}

			std::vector<char> response = { 'V' };
			std::random_device rd;
			std::default_random_engine generator(rd());
			std::uniform_int_distribution<int32_t> distribution(0, 15);
			_myIV.clear();
			for(int32_t i = 0; i < 32; i++)
			{
				int32_t nibble = distribution(generator);
				if((i % 2) == 0)
				{
					_myIV.push_back(nibble << 4);
				}
				else
				{
					_myIV.at(i / 2) |= nibble;
				}
				response.push_back(_bl->hf.getHexChar(nibble));
			}
			response.push_back(0x0D);
			response.push_back(0x0A);

			if(_bl->debugLevel >= 5) _out.printDebug("Homegear IV is: " + _bl->hf.getHexString(_myIV));

			if((result = gcry_cipher_setiv(_decryptHandle, &_myIV.at(0), _myIV.size())) != GPG_ERR_NO_ERROR)
			{
				_out.printError("Error: Could not set IV for decryption: " + BaseLib::Security::Gcrypt::getError(result));
				reconnect();
				return false;
			}

			send(response, true);
			_aesExchangeComplete = true;
			return true;
		}
		else if(_remoteIV.empty())
		{
			_out.printError("Error: Error communicating with HM-CFG-LAN. AES is enabled but no IV was send from HM-CFG-LAN. Please use the HomeMatic LAN Interface Configurator to re-enable AES.");
			reconnect();
			return false;
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
    return false;
}

void HM_CFG_LAN::processData(std::vector<uint8_t>& data)
{
	try
	{
		if(data.empty()) return;
		std::string packets;
		if(_useAES)
		{
			if(!_aesExchangeComplete)
			{
				aesKeyExchange(data);
				return;
			}
			std::vector<uint8_t> decryptedData = decrypt(data);
			if(decryptedData.empty()) return;
			packets.insert(packets.end(), decryptedData.begin(), decryptedData.end());
		}
		else packets.insert(packets.end(), data.begin(), data.end());

		std::istringstream stringStream(packets);
		std::string packet;
		while(std::getline(stringStream, packet))
		{
			if(_initCommandQueue.empty()) parsePacket(packet); else processInit(packet);
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

void HM_CFG_LAN::processInit(std::string& packet)
{
	if(_initCommandQueue.empty() || packet.length() < 10) return;
	if(_initCommandQueue.front().at(0) == 'A') //No init packet has been sent yet
	{
		std::vector<std::string> parts = BaseLib::HelperFunctions::splitAll(packet, ',');
		if(parts.size() < 7 || (parts.at(0) != "HHM-LAN-IF" && parts.at(0) != "HHM-USB-IF"))
		{
			_out.printError("Error: First packet from HM-CFG-LAN does not start with \"HHM-LAN-IF\", \"HHM-USB-IF\" or has wrong structure. Please check your AES key in homematicbidcos.conf. Stopping listening. Packet was: " + packet);
			reconnect();
			return;
		}
		_startUpTime = BaseLib::HelperFunctions::getTime() - (int64_t)BaseLib::Math::getNumber(parts.at(5), true);
		send(_initCommandQueue.front(), false);
		_initCommandQueue.pop_front();
		send(_initCommandQueue.front(), false);
	}
	else if((_initCommandQueue.front().at(0) == 'C' || _initCommandQueue.front().at(0) == 'Y') && packet.at(0) == 'I')
	{
		_initCommandQueue.pop_front();
		send(_initCommandQueue.front(), false);
		if(_initCommandQueue.front().at(0) == 'T')
		{
			_initCommandQueue.pop_front();
			sendPeers();
		}
	}
	else if(GD::bl->hf.getTime() - _initStarted > 30000)
	{
		_out.printWarning("Warning: Init queue did not complete within 30 seconds. Reconnecting...");
		_initCommandQueue.clear();
		reconnect();
	}
}

void HM_CFG_LAN::parsePacket(std::string& packet)
{
	try
	{
		if(packet.empty()) return;
		if(_bl->debugLevel >= 5) _out.printDebug(std::string("Debug: Packet received from HM-CFG-LAN") + (_useAES ? + " (encrypted)" : "") + ": " + packet);
		std::vector<std::string> parts = BaseLib::HelperFunctions::splitAll(packet, ',');
		if(packet.at(0) == 'H' && parts.size() >= 7)
		{
			/*
			Index	Meaning
			0		"HM-LAN-IF"
			1		Firmware version
			2		Serial number
			3		Default address?
			4		Address
			5		Time since boot in milliseconds
			6		Number of registered peers
			*/

			_lastKeepAliveResponse = BaseLib::HelperFunctions::getTimeSeconds();
			_startUpTime = BaseLib::HelperFunctions::getTime() - (int64_t)BaseLib::Math::getNumber(parts.at(5), true);
		}
		else if(packet.at(0) == 'E' || packet.at(0) == 'R')
		{
			if(parts.size() < 6)
			{
				_out.printWarning("Warning: Invalid packet received from HM-CFG-LAN: " + packet);
				return;
			}
			/*
			Index	Meaning
			0		Sender address
			1		Control and status byte
			2		Time received
			3		AES key index?
			4		BidCoS packet
			*/

			int32_t tempNumber = BaseLib::Math::getNumber(parts.at(1), true);

			/*
			00: Not set
			01: Packet received, wait for AES handshake
			02: High load
			04: Overload
			*/
			uint8_t statusByte = tempNumber >> 8;
			if(statusByte & 4) _out.printError("Error: HM-CFG-LAN reached 1% rule.");
			else if(statusByte & 2) _out.printWarning("Warning: HM-CFG-LAN nearly reached 1% rule.");

			/*
			00: Not set
			01: ACK or ACK was sent in response to this packet
			02: Message without BIDI bit was sent
			08: No response after three tries
			21: ?
			2*: ?
			30: AES handshake not successful
			4*: AES handshake successful
			50: AES handshake not successful
			8*: ?
			*/
			uint8_t controlByte = tempNumber & 0xFF;

			if(parts.at(5).size() > 18) //18 is minimal packet length
        	{
				std::vector<uint8_t> binaryPacket({ (uint8_t)(parts.at(5).size() / 2) });
				_bl->hf.getUBinary(parts.at(5), parts.at(5).size() - 1, binaryPacket);
				int32_t rssi = BaseLib::Math::getNumber(parts.at(4), true);
				//Convert to TI CC1101 format
				if(rssi <= -75) rssi = ((rssi + 74) * 2) + 256;
				else rssi = (rssi + 74) * 2;
				binaryPacket.push_back(rssi);

				std::shared_ptr<BidCoSPacket> bidCoSPacket(new BidCoSPacket(binaryPacket, true, BaseLib::HelperFunctions::getTime()));
				if(packet.at(0) == 'E' && (statusByte & 1))
				{
					_out.printDebug("Debug: Waiting for AES handshake.");
					_lastPacketReceived = BaseLib::HelperFunctions::getTime();
					_lastPacketSent = BaseLib::HelperFunctions::getTime();
					return;
				}
				if((controlByte & 0x30) == 0x30 || (controlByte & 0x50) == 0x50)
				{
					_lastPacketReceived = BaseLib::HelperFunctions::getTime();
					_out.printWarning("Warning: AES handshake was not successful: " + bidCoSPacket->hexString());
					return;
				}
				if(packet.at(0) == 'R')
				{
					if(controlByte & 8)
					{
						_out.printWarning("Info: No response to packet after 3 tries: " + bidCoSPacket->hexString());
						return;
					}
					else if(controlByte & 2)
					{
						return;
					}
					else if(!(controlByte & 0x40) && !(controlByte & 0x20) && (controlByte & 1) && (bidCoSPacket->controlByte() & 0x20))
					{
						_lastPacketSent = BaseLib::HelperFunctions::getTime();
						return;
					}
				}

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
        	}
        	else if(!parts.at(5).empty()) _out.printWarning("Warning: Too short packet received: " + parts.at(5));
		}
		else _out.printInfo("Info: Packet received: " + packet);
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
