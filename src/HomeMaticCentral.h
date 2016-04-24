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

#ifndef HOMEMATICCENTRAL_H_
#define HOMEMATICCENTRAL_H_

#include "../../config.h"

#include "homegear-base/BaseLib.h"
#include "BidCoSQueue.h"
#include "BidCoSPeer.h"
#include "BidCoSMessage.h"
#include "BidCoSMessages.h"
#include "BidCoSQueueManager.h"
#include "BidCoSPacketManager.h"

#include <memory>
#include <mutex>
#include <string>
#include <cmath>

namespace BidCoS
{
class BidCoSPacket;

class HomeMaticCentral : public BaseLib::Systems::ICentral
{
public:
	HomeMaticCentral(ICentralEventSink* eventHandler);
	HomeMaticCentral(uint32_t deviceType, std::string serialNumber, int32_t address, ICentralEventSink* eventHandler);
	virtual ~HomeMaticCentral();
	virtual void stopThreads();
    virtual void dispose(bool wait = true);

	std::shared_ptr<BidCoSPeer> getPeer(int32_t address);
	std::shared_ptr<BidCoSPeer> getPeer(uint64_t id);
	std::shared_ptr<BidCoSPeer> getPeer(std::string serialNumber);
	std::shared_ptr<BidCoSQueue> getQueue(int32_t address);
	virtual void saveMessageCounters();
	virtual void serializeMessageCounters(std::vector<uint8_t>& encodedData);
    virtual void unserializeMessageCounters(std::shared_ptr<std::vector<char>> serializedData);

	std::unordered_map<int32_t, uint8_t>* messageCounter() { return &_messageCounter; }
	virtual std::shared_ptr<BidCoSMessages> getMessages() { return _messages; }
	virtual bool isInPairingMode() { return _pairing; }
	static bool isDimmer(BaseLib::Systems::LogicalDeviceType type);
    static bool isSwitch(BaseLib::Systems::LogicalDeviceType type);

	virtual bool onPacketReceived(std::string& senderID, std::shared_ptr<BaseLib::Systems::Packet> packet);
	void enablePairingMode() { _pairing = true; }
	void disablePairingMode() { _pairing = false; }
	void unpair(uint64_t id, bool defer);
	void reset(uint64_t id, bool defer);
	void deletePeer(uint64_t id);
	void addPeerToTeam(std::shared_ptr<BidCoSPeer> peer, int32_t channel, uint32_t teamChannel, std::string teamSerialNumber);
	void addPeerToTeam(std::shared_ptr<BidCoSPeer> peer, int32_t channel, int32_t address, uint32_t teamChannel);
	void removePeerFromTeam(std::shared_ptr<BidCoSPeer> peer);
	void resetTeam(std::shared_ptr<BidCoSPeer> peer, uint32_t channel);
	std::string handleCliCommand(std::string command);
	virtual void sendPacket(std::shared_ptr<IBidCoSInterface> physicalInterface, std::shared_ptr<BidCoSPacket> packet, bool stealthy = false);
    virtual void sendPacketMultipleTimes(std::shared_ptr<IBidCoSInterface> physicalInterface, std::shared_ptr<BidCoSPacket> packet, int32_t peerAddress, int32_t count, int32_t delay, bool useCentralMessageCounter = false, bool isThread = false);
	virtual void enqueuePackets(int32_t deviceAddress, std::shared_ptr<BidCoSQueue> packets, bool pushPendingBidCoSQueues = false);
	std::shared_ptr<BidCoSPacket> getReceivedPacket(int32_t address) { return _receivedPackets.get(address); }
    std::shared_ptr<BidCoSPacket> getSentPacket(int32_t address) { return _sentPackets.get(address); }

	/**
	 * Enqueues the pending queues of the peer with deviceAddress.
	 *
	 * @param deviceAddress The BidCoS address of the device to enqueue the packets for.
	 * @return Returns the queue managers BidCoS queue for the peer or nullptr when there are no pending queues or on errors.
	 */
	std::shared_ptr<BidCoSQueue> enqueuePendingQueues(int32_t deviceAddress, bool wait = false, bool* result = nullptr);
	int32_t getUniqueAddress(int32_t seed);
	std::string getUniqueSerialNumber(std::string seedPrefix, uint32_t seedNumber);
	uint64_t getPeerIdFromSerial(std::string serialNumber) { std::shared_ptr<BidCoSPeer> peer = getPeer(serialNumber); if(peer) return peer->getID(); else return 0; }
	void updateFirmwares(std::vector<uint64_t> ids);
	void updateFirmware(uint64_t id);
	void addPeersToVirtualDevices();
	void addHomegearFeatures(std::shared_ptr<BidCoSPeer> peer, int32_t channel, bool pushPendingBidCoSQueues);
	void addHomegearFeaturesHMCCVD(std::shared_ptr<BidCoSPeer> peer, int32_t channel, bool pushPendingBidCoSQueues);
	void addHomegearFeaturesRemote(std::shared_ptr<BidCoSPeer> peer, int32_t channel, bool pushPendingBidCoSQueues);
	void addHomegearFeaturesSwitch(std::shared_ptr<BidCoSPeer> peer, int32_t channel, bool pushPendingBidCoSQueues);
	void addHomegearFeaturesMotionDetector(std::shared_ptr<BidCoSPeer> peer, int32_t channel, bool pushPendingBidCoSQueues);
	void addHomegearFeaturesHMCCRTDN(std::shared_ptr<BidCoSPeer> peer, int32_t channel, bool pushPendingBidCoSQueues);

	void handlePairingRequest(int32_t messageCounter, std::shared_ptr<BidCoSPacket>);
	void handleAck(int32_t messageCounter, std::shared_ptr<BidCoSPacket>);
	void handleConfigParamResponse(int32_t messageCounter, std::shared_ptr<BidCoSPacket>);
	void handleTimeRequest(int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet);
	void sendRequestConfig(int32_t address, uint8_t localChannel, uint8_t list = 0, int32_t remoteAddress = 0, uint8_t remoteChannel = 0);
	void sendOK(int32_t messageCounter, int32_t destinationAddress, std::vector<uint8_t> payload = std::vector<uint8_t>());

	virtual BaseLib::PVariable activateLinkParamset(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t channel, std::string remoteSerialNumber, int32_t remoteChannel, bool longPress);
	virtual BaseLib::PVariable activateLinkParamset(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, int32_t channel, uint64_t remoteID, int32_t remoteChannel, bool longPress);
	virtual BaseLib::PVariable addDevice(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber);
	virtual BaseLib::PVariable addLink(BaseLib::PRpcClientInfo clientInfo, std::string senderSerialNumber, int32_t senderChannel, std::string receiverSerialNumber, int32_t receiverChannel, std::string name, std::string description);
	virtual BaseLib::PVariable addLink(BaseLib::PRpcClientInfo clientInfo, uint64_t senderID, int32_t senderChannel, uint64_t receiverID, int32_t receiverChannel, std::string name, std::string description);
	virtual BaseLib::PVariable deleteDevice(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t flags);
	virtual BaseLib::PVariable deleteDevice(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, int32_t flags);
	virtual BaseLib::PVariable getDeviceInfo(BaseLib::PRpcClientInfo clientInfo, uint64_t id, std::map<std::string, bool> fields);
	virtual BaseLib::PVariable getInstallMode(BaseLib::PRpcClientInfo clientInfo);
	virtual BaseLib::PVariable listTeams(BaseLib::PRpcClientInfo clientInfo);
	virtual BaseLib::PVariable putParamset(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t channel, ParameterGroup::Type::Enum type, std::string remoteSerialNumber, int32_t remoteChannel, BaseLib::PVariable paramset);
	virtual BaseLib::PVariable putParamset(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, BaseLib::PVariable paramset);
	virtual BaseLib::PVariable removeLink(BaseLib::PRpcClientInfo clientInfo, std::string senderSerialNumber, int32_t senderChannel, std::string receiverSerialNumber, int32_t receiverChannel);
	virtual BaseLib::PVariable removeLink(BaseLib::PRpcClientInfo clientInfo, uint64_t senderID, int32_t senderChannel, uint64_t receiverID, int32_t receiverChannel);
	virtual BaseLib::PVariable setTeam(BaseLib::PRpcClientInfo clientInfo, std::string serialNumber, int32_t channel, std::string teamSerialNumber, int32_t teamChannel, bool force = false, bool burst = true);
	virtual BaseLib::PVariable setTeam(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, int32_t channel, uint64_t teamID, int32_t teamChannel, bool force = false, bool burst = true);
	virtual BaseLib::PVariable setInstallMode(BaseLib::PRpcClientInfo clientInfo, bool on, uint32_t duration = 60, bool debugOutput = true);
	virtual BaseLib::PVariable updateFirmware(BaseLib::PRpcClientInfo clientInfo, std::vector<uint64_t> ids, bool manual);
	virtual BaseLib::PVariable setInterface(BaseLib::PRpcClientInfo clientInfo, uint64_t peerID, std::string interfaceID);
protected:
	// {{{ In table variables
        std::unordered_map<int32_t, uint8_t> _messageCounter;
    // }}}

    BidCoSQueueManager _bidCoSQueueManager;
	BidCoSPacketManager _receivedPackets;
	BidCoSPacketManager _sentPackets;
	std::shared_ptr<BidCoSMessages> _messages;

    bool _stopWorkerThread = false;
    std::thread _workerThread;

    std::mutex _sendMultiplePacketsThreadMutex;
    std::thread _sendMultiplePacketsThread;
    std::mutex _sendPacketThreadMutex;
    std::thread _sendPacketThread;
    std::thread _resetThread;

    bool _pairing = false;
	uint32_t _timeLeftInPairingMode = 0;
	void pairingModeTimer(int32_t duration, bool debugOutput = true);
	bool _stopPairingModeThread = false;
	std::mutex _pairingModeThreadMutex;
	std::thread _pairingModeThread;
	std::mutex _enqueuePendingQueuesMutex;

	//Updates:
	bool _updateMode = false;
	std::mutex _updateFirmwareThreadMutex;
	std::mutex _updateMutex;
	std::thread _updateFirmwareThread;
	//End

	virtual void loadPeers();
	virtual void savePeers(bool full);
	virtual void loadVariables();
	virtual void saveVariables();
	void setUpBidCoSMessages();
	uint8_t getMessageCounter();

	std::shared_ptr<BidCoSPeer> createPeer(int32_t address, int32_t firmwareVersion, BaseLib::Systems::LogicalDeviceType deviceType, std::string serialNumber, int32_t remoteChannel, int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet = std::shared_ptr<BidCoSPacket>(), bool save = true);
    std::shared_ptr<BidCoSPeer> createTeam(int32_t address, BaseLib::Systems::LogicalDeviceType deviceType, std::string serialNumber);
	virtual void worker();
	virtual void init();
	virtual std::shared_ptr<IBidCoSInterface> getPhysicalInterface(int32_t peerAddress);
};

}
#endif /* HOMEMATICCENTRAL_H_ */
