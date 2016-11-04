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

#ifndef IBIDCOSINTERFACE_H_
#define IBIDCOSINTERFACE_H_

#include "AesHandshake.h"
#include <homegear-base/BaseLib.h>

#include <random>

namespace BidCoS {

class IBidCoSInterface : public BaseLib::Systems::IPhysicalInterface, public BaseLib::ITimedQueue
{
public:
	class PeerInfo
	{
	public:
		PeerInfo() {}
		virtual ~PeerInfo() {}
		std::vector<char> getAESChannelMap();

		bool aesEnabled = false;
		bool wakeUp = false;
		int32_t address = 0;
		int32_t keyIndex = 0;
		std::map<int32_t, bool> aesChannels;
	};

	IBidCoSInterface(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings);
	virtual ~IBidCoSInterface();

	virtual void startListening();
	virtual void stopListening();

	virtual void addPeer(PeerInfo peerInfo);
	virtual void addPeers(std::vector<PeerInfo>& peerInfos);
	virtual void removePeer(int32_t address);
	virtual void setWakeUp(PeerInfo peerInfo);
	virtual void setAES(PeerInfo peerInfo, int32_t channel);

	virtual bool firmwareUpdatesSupported() { return true; }
	virtual uint32_t currentRFKeyIndex() { return _currentRfKeyIndex; }
	virtual std::string rfKey() { return _rfKeyHex; }

	virtual uint32_t getCurrentRFKeyIndex() { return _currentRfKeyIndex; }

	virtual void sendPacket(std::shared_ptr<BaseLib::Systems::Packet> packet);
	virtual void sendTest() {}
protected:
	class QueueEntry : public BaseLib::ITimedQueueEntry
	{
	public:
		QueueEntry() {}
		QueueEntry(int64_t sendingTime, std::shared_ptr<BidCoSPacket> packet) : ITimedQueueEntry(sendingTime) { this->packet = packet; }
		virtual ~QueueEntry() {}

		std::shared_ptr<BidCoSPacket> packet;
	};

	BaseLib::SharedObjects* _bl = nullptr;
	int64_t _lastAesHandshakeGc = 0;
	std::shared_ptr<AesHandshake> _aesHandshake;
	std::mutex _queueIdsMutex;
	std::map<int32_t, std::set<int64_t>> _queueIds;
	std::mutex _peersMutex;
	std::map<int32_t, PeerInfo> _peers;
	int32_t _myAddress = 0x1C6940;

	BaseLib::Output _out;
	bool _initComplete = false;
	int32_t _currentRfKeyIndex = 0;
	std::string _rfKeyHex;
	std::string _oldRfKeyHex;
	std::vector<uint8_t> _rfKey;
	std::vector<uint8_t> _oldRfKey;

	virtual void forceSendPacket(std::shared_ptr<BidCoSPacket> packet) {};
	virtual void processQueueEntry(int32_t index, int64_t id, std::shared_ptr<BaseLib::ITimedQueueEntry>& entry);
	void queuePacket(std::shared_ptr<BidCoSPacket> packet, int64_t sendingTime = 0);
	void processReceivedPacket(std::shared_ptr<BidCoSPacket> packet);
};

}

#endif
