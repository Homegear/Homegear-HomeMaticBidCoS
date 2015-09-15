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

#ifndef EASYCAMPEER_H_
#define EASYCAMPEER_H_

#include "homegear-base/BaseLib.h"

#include <list>

using namespace BaseLib;
using namespace BaseLib::DeviceDescription;

namespace EasyCam
{
class EasyCamCentral;
class EasyCamDevice;

class EasyCamPeer : public BaseLib::Systems::Peer, public BaseLib::Rpc::IWebserverEventSink
{
public:
	EasyCamPeer(uint32_t parentID, bool centralFeatures, IPeerEventSink* eventHandler);
	EasyCamPeer(int32_t id, std::string serialNumber, uint32_t parentID, bool centralFeatures, IPeerEventSink* eventHandler);
	virtual ~EasyCamPeer();
	void removeHooks();
	void init();
	void dispose();

	//Features
	virtual bool wireless() { return false; }
	//End features

	virtual std::string handleCLICommand(std::string command);

	virtual bool load(BaseLib::Systems::LogicalDevice* device);
    virtual void loadVariables(BaseLib::Systems::LogicalDevice* device = nullptr, std::shared_ptr<BaseLib::Database::DataTable> rows = std::shared_ptr<BaseLib::Database::DataTable>());
    virtual void saveVariables();
    virtual void savePeers() {}

	virtual int32_t getChannelGroupedWith(int32_t channel) { return -1; }
	virtual int32_t getNewFirmwareVersion() { return 0; }
	virtual std::string getFirmwareVersionString(int32_t firmwareVersion) { return "1.0"; }
    virtual bool firmwareUpdateAvailable() { return false; }

    std::string printConfig();

    /**
	 * {@inheritDoc}
	 */
    virtual void homegearStarted();

    /**
	 * {@inheritDoc}
	 */
    virtual void homegearShuttingDown();

    // {{{ Webserver events
		bool onGet(BaseLib::Rpc::PServerInfo& serverInfo, BaseLib::HTTP& httpRequest, std::shared_ptr<BaseLib::SocketOperations>& socket, std::string& path);
	// }}}

	//RPC methods
	virtual PVariable getDeviceInfo(int32_t clientID, std::map<std::string, bool> fields);
	virtual PVariable getParamsetDescription(int32_t clientID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel);
	virtual PVariable getParamset(int32_t clientID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel);
	virtual PVariable getValue(int32_t clientID, uint32_t channel, std::string valueKey, bool requestFromDevice, bool asynchronous);
	virtual PVariable putParamset(int32_t clientID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool onlyPushing = false);
	virtual PVariable setValue(int32_t clientID, uint32_t channel, std::string valueKey, PVariable value);
	//End RPC methods
protected:
	bool _shuttingDown = false;
	std::shared_ptr<BaseLib::RPC::RPCEncoder> _binaryEncoder;
	std::shared_ptr<BaseLib::RPC::RPCDecoder> _binaryDecoder;
	std::shared_ptr<BaseLib::HTTPClient> _httpClient;
	std::string _baseUrl;
	int32_t _port = 88;
	bool _useSsl = false;
	std::string _caFile;
	bool _verifyCertificate = false;
	std::vector<char> _httpOkHeader;

	std::string _username;
	std::string _password;

	virtual std::shared_ptr<BaseLib::Systems::Central> getCentral();
	virtual std::shared_ptr<BaseLib::Systems::LogicalDevice> getDevice(int32_t address);

	/**
	 * {@inheritDoc}
	 */
	virtual PVariable getValueFromDevice(PParameter& parameter, int32_t channel, bool asynchronous);

	virtual PParameterGroup getParameterSet(int32_t channel, ParameterGroup::Type::Enum type);
	void initHttpClient();
	int32_t parseCgiResult(std::string& data, std::map<std::string, std::string>& result);
	void registerMotionCallback();
};

}

#endif
