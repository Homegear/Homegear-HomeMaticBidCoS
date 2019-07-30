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

#ifndef HM_LGW_H
#define HM_LGW_H

#include "../BidCoSPacket.h"
#include "IBidCoSInterface.h"
#include "Crc16.h"

#include <thread>
#include <iostream>
#include <fstream>
#include <string>
#include <list>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <ctime>
#include <iomanip>

#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>

#include <gcrypt.h>

namespace BidCoS
{

class HM_LGW  : public IBidCoSInterface
{
    public:
        HM_LGW(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings);
        virtual ~HM_LGW();
        void startListening();
        void stopListening();
        virtual void sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet);
        virtual bool isOpen() { return _initComplete && _socket->connected(); }

        virtual void addPeer(PeerInfo peerInfo);
        virtual void addPeers(std::vector<PeerInfo>& peerInfos);
        virtual void setWakeUp(PeerInfo peerInfo);
        virtual void setAES(PeerInfo peerInfo, int32_t channel);
        virtual void removePeer(int32_t address);

        void enableUpdateMode();
        void disableUpdateMode();
    protected:
        enum class AddPeerQueueEntryType
		{
			add,
			remove,
			aes,
			wakeUp
		};

        class AddPeerQueueEntry : public BaseLib::ITimedQueueEntry
		{
		public:
			AddPeerQueueEntry() {}
			AddPeerQueueEntry(PeerInfo& peerInfo, AddPeerQueueEntryType type, int64_t sendingTime) : ITimedQueueEntry(sendingTime) { this->peerInfo = peerInfo; this->type = type; }
			AddPeerQueueEntry(PeerInfo& peerInfo, int32_t channel, AddPeerQueueEntryType type, int64_t sendingTime) : ITimedQueueEntry(sendingTime) { this->peerInfo = peerInfo; this->channel = channel; this->type = type; }
			AddPeerQueueEntry(int32_t address, AddPeerQueueEntryType type, int64_t sendingTime) : ITimedQueueEntry(sendingTime) { this->address = address; this->type = type; }
			virtual ~AddPeerQueueEntry() {}

			AddPeerQueueEntryType type = AddPeerQueueEntryType::add;
			int32_t address = 0;
			int32_t channel = 0;
			PeerInfo peerInfo;
		};

        class Request
        {
        public:
        	std::mutex mutex;
        	std::condition_variable conditionVariable;
        	bool mutexReady = false;
        	std::vector<uint8_t> response;
        	uint8_t getResponseControlByte() { return _responseControlByte; }
        	uint8_t getResponseType() { return _responseType; }

        	Request(uint8_t responseControlByte, uint8_t responseType) { _responseControlByte = responseControlByte; _responseType = responseType; };
        	virtual ~Request() {};
        private:
        	uint8_t _responseControlByte;
        	uint8_t _responseType;
        };

        std::thread _listenThreadKeepAlive;
        std::thread _initThread;
        std::string _port;
        std::unique_ptr<BaseLib::TcpSocket> _socket;
        std::unique_ptr<BaseLib::TcpSocket> _socketKeepAlive;
        std::mutex _getResponseMutex;
        std::mutex _requestsMutex;
        std::map<uint8_t, std::shared_ptr<Request>> _requests;
        std::mutex _sendMutex;
        std::mutex _sendMutexKeepAlive;
        bool _initStarted = false;
        bool _firstPacket = true;
        std::atomic_bool _initCompleteKeepAlive;
        int32_t _lastKeepAlive1 = 0;
        int32_t _lastKeepAliveResponse1 = 0;
        int32_t _missedKeepAliveResponses1 = 0;
        int32_t _lastKeepAlive2 = 0;
        int32_t _lastKeepAliveResponse2 = 0;
        int32_t _missedKeepAliveResponses2 = 0;
        int32_t _lastTimePacket = 0;
        int64_t _startUpTime = 0;
        bool _escapeByte = false;
        std::vector<uint8_t> _packetBuffer;
        uint8_t _packetIndex = 0;
        uint8_t _packetIndexKeepAlive = 0;
        CRC16 _crc;

        //AES stuff
        bool _aesInitialized = false;
        bool _aesExchangeComplete = false;
        bool _aesExchangeKeepAliveComplete = false;
        std::vector<uint8_t> _key;
		std::vector<uint8_t> _remoteIV;
		std::vector<uint8_t> _myIV;
        gcry_cipher_hd_t _encryptHandle = nullptr;
        gcry_cipher_hd_t _decryptHandle = nullptr;
        std::vector<uint8_t> _remoteIVKeepAlive;
		std::vector<uint8_t> _myIVKeepAlive;
        gcry_cipher_hd_t _encryptHandleKeepAlive = nullptr;
        gcry_cipher_hd_t _decryptHandleKeepAlive = nullptr;

        std::vector<char> encrypt(const std::vector<char>& data);
        std::vector<uint8_t> decrypt(std::vector<uint8_t>& data);
        std::vector<char> encryptKeepAlive(std::vector<char>& data);
        std::vector<uint8_t> decryptKeepAlive(std::vector<uint8_t>& data);
        bool aesKeyExchange(std::vector<uint8_t>& data);
        bool aesKeyExchangeKeepAlive(std::vector<uint8_t>& data);
        bool aesInit();
        void aesCleanup();
        //End AES stuff

        void reconnect();
        void doInit();
        void sendPeers();
        void sendPeer(PeerInfo& peerInfo);
        void processData(std::vector<uint8_t>& data);
        void processDataKeepAlive(std::vector<uint8_t>& data);
        void processPacket(std::vector<uint8_t>& packet);
        void processInitKeepAlive(std::string& packet);
        void parsePacket(std::vector<uint8_t>& packet);
        void parsePacketKeepAlive(std::string& packet);
        void buildPacket(std::vector<char>& packet, const std::vector<char>& payload);
        void escapePacket(const std::vector<char>& unescapedPacket, std::vector<char>& escapedPacket);
        void getResponse(const std::vector<char>& packet, std::vector<uint8_t>& response, uint8_t messageCounter, uint8_t responseControlByte, uint8_t responseType);
        void send(std::string hexString, bool raw = false);
        void send(const std::vector<char>& data, bool raw);
        void sendKeepAlive(std::vector<char>& data, bool raw);
        void sendKeepAlivePacket1();
        void sendKeepAlivePacket2();
        void sendTimePacket();
        void dutyCycleTest(int32_t destinationAddress);
        void listen();
        void listenKeepAlive();
        void getFileDescriptor(bool& timedout);
        std::shared_ptr<BaseLib::FileDescriptor> getConnection(std::string& hostname, const std::string& port, std::string& ipAddress);
        virtual void processQueueEntry(int32_t index, int64_t id, std::shared_ptr<BaseLib::ITimedQueueEntry>& entry);
    private:
};

}
#endif

