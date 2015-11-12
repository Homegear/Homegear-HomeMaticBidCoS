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

#ifndef HOMEMATICCENTRAL_H_
#define HOMEMATICCENTRAL_H_

#include "HM-CC-TC.h"
#include "../HomeMaticDevice.h"

#include <memory>
#include <mutex>
#include <string>
#include <cmath>

namespace BidCoS
{
class BidCoSPacket;

class HomeMaticCentral : public HomeMaticDevice, public BaseLib::Systems::Central
{
public:
	HomeMaticCentral(IDeviceEventSink* eventHandler);
	HomeMaticCentral(uint32_t deviceType, std::string, int32_t, IDeviceEventSink* eventHandler);
	virtual ~HomeMaticCentral();
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
	std::string handleCLICommand(std::string command);
	virtual void enqueuePackets(int32_t deviceAddress, std::shared_ptr<BidCoSQueue> packets, bool pushPendingBidCoSQueues = false);

	/**
	 * Enqueues the pending queues of the peer with deviceAddress.
	 *
	 * @param deviceAddress The BidCoS address of the device to enqueue the packets for.
	 * @return Returns the queue managers BidCoS queue for the peer or nullptr when there are no pending queues or on errors.
	 */
	std::shared_ptr<BidCoSQueue> enqueuePendingQueues(int32_t deviceAddress);
	int32_t getUniqueAddress(int32_t seed);
	std::string getUniqueSerialNumber(std::string seedPrefix, uint32_t seedNumber);
	uint64_t getPeerIDFromSerial(std::string serialNumber) { std::shared_ptr<BidCoSPeer> peer = getPeer(serialNumber); if(peer) return peer->getID(); else return 0; }
	void updateFirmwares(std::vector<uint64_t> ids, bool manual);
	void updateFirmware(uint64_t id, bool manual);
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
	void handleReset(int32_t messageCounter, std::shared_ptr<BidCoSPacket>) {}
	void sendRequestConfig(int32_t address, uint8_t localChannel, uint8_t list = 0, int32_t remoteAddress = 0, uint8_t remoteChannel = 0);

	virtual bool knowsDevice(std::string serialNumber);
	virtual bool knowsDevice(uint64_t id);

	virtual PVariable activateLinkParamset(int32_t clientID, std::string serialNumber, int32_t channel, std::string remoteSerialNumber, int32_t remoteChannel, bool longPress);
	virtual PVariable activateLinkParamset(int32_t clientID, uint64_t peerID, int32_t channel, uint64_t remoteID, int32_t remoteChannel, bool longPress);
	virtual PVariable addDevice(int32_t clientID, std::string serialNumber);
	virtual PVariable addLink(int32_t clientID, std::string senderSerialNumber, int32_t senderChannel, std::string receiverSerialNumber, int32_t receiverChannel, std::string name, std::string description);
	virtual PVariable addLink(int32_t clientID, uint64_t senderID, int32_t senderChannel, uint64_t receiverID, int32_t receiverChannel, std::string name, std::string description);
	virtual PVariable deleteDevice(int32_t clientID, std::string serialNumber, int32_t flags);
	virtual PVariable deleteDevice(int32_t clientID, uint64_t peerID, int32_t flags);
	virtual PVariable getDeviceInfo(int32_t clientID, uint64_t id, std::map<std::string, bool> fields);
	virtual PVariable getInstallMode(int32_t clientID);
	virtual PVariable listTeams(int32_t clientID);
	virtual PVariable putParamset(int32_t clientID, std::string serialNumber, int32_t channel, ParameterGroup::Type::Enum type, std::string remoteSerialNumber, int32_t remoteChannel, PVariable paramset);
	virtual PVariable putParamset(int32_t clientID, uint64_t peerID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable paramset);
	virtual PVariable removeLink(int32_t clientID, std::string senderSerialNumber, int32_t senderChannel, std::string receiverSerialNumber, int32_t receiverChannel);
	virtual PVariable removeLink(int32_t clientID, uint64_t senderID, int32_t senderChannel, uint64_t receiverID, int32_t receiverChannel);
	virtual PVariable setTeam(int32_t clientID, std::string serialNumber, int32_t channel, std::string teamSerialNumber, int32_t teamChannel, bool force = false, bool burst = true);
	virtual PVariable setTeam(int32_t clientID, uint64_t peerID, int32_t channel, uint64_t teamID, int32_t teamChannel, bool force = false, bool burst = true);
	virtual PVariable setInstallMode(int32_t clientID, bool on, uint32_t duration = 60, bool debugOutput = true);
	virtual PVariable updateFirmware(int32_t clientID, std::vector<uint64_t> ids, bool manual);
	virtual PVariable setInterface(int32_t clientID, uint64_t peerID, std::string interfaceID);
protected:
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

	void setUpBidCoSMessages();
	void setUpConfig() {}

	std::shared_ptr<BidCoSPeer> createPeer(int32_t address, int32_t firmwareVersion, BaseLib::Systems::LogicalDeviceType deviceType, std::string serialNumber, int32_t remoteChannel, int32_t messageCounter, std::shared_ptr<BidCoSPacket> packet = std::shared_ptr<BidCoSPacket>(), bool save = true);
	virtual void worker();
	virtual void init();
};

}
#endif /* HOMEMATICCENTRAL_H_ */
