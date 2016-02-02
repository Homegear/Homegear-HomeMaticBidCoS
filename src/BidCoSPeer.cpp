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

class CallbackFunctionParameter;

#include "BidCoSPeer.h"
#include "BidCoSQueue.h"
#include "PendingBidCoSQueues.h"
#include "HomeMaticCentral.h"
#include "homegear-base/BaseLib.h"
#include "GD.h"
#include "VirtualPeers/HmCcTc.h"

namespace BidCoS
{
std::shared_ptr<BaseLib::Systems::ICentral> BidCoSPeer::getCentral()
{
	try
	{
		if(_central) return _central;
		_central = GD::family->getCentral();
		return _central;
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
	return std::shared_ptr<HomeMaticCentral>();
}

void BidCoSPeer::setDefaultValue(BaseLib::Systems::RPCConfigurationParameter* parameter)
{
	try
	{
		parameter->rpcParameter->convertToPacket(parameter->rpcParameter->logical->getDefaultValue(), parameter->data);
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

void BidCoSPeer::initializeLinkConfig(int32_t channel, int32_t remoteAddress, int32_t remoteChannel, bool useConfigFunction)
{
	std::string savePointname("bidCoSPeerLinkConfig" + std::to_string(_peerID));
	try
	{
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end())	return;
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(ParameterGroup::Type::link);
		if(!parameterGroup || parameterGroup->parameters.empty()) return;
		_bl->db->createSavepointAsynchronous(savePointname);
		//This line creates an empty link config. This is essential as the link config must exist, even if it is empty.
		std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>* linkConfig = &linksCentral[channel][remoteAddress][remoteChannel];
		BaseLib::Systems::RPCConfigurationParameter parameter;
		for(Parameters::iterator j = parameterGroup->parameters.begin(); j != parameterGroup->parameters.end(); ++j)
		{
			if(!j->second) continue;
			if(!j->second->id.empty() && linkConfig->find(j->second->id) == linkConfig->end())
			{
				parameter = BaseLib::Systems::RPCConfigurationParameter();
				parameter.rpcParameter = j->second;
				j->second->convertToPacket(j->second->logical->getDefaultValue(), parameter.data);
				linkConfig->insert(std::pair<std::string, BaseLib::Systems::RPCConfigurationParameter>(j->second->id, parameter));
				saveParameter(0, ParameterGroup::Type::link, channel, j->second->id, parameter.data, remoteAddress, remoteChannel);
			}
		}
		if(useConfigFunction) applyConfigFunction(channel, remoteAddress, remoteChannel);
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
    _bl->db->releaseSavepointAsynchronous(savePointname);
}

void BidCoSPeer::applyConfigFunction(int32_t channel, int32_t peerAddress, int32_t remoteChannel)
{
	try
	{
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return;
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(ParameterGroup::Type::link);
		if(!parameterGroup || parameterGroup->parameters.empty()) return;
		std::shared_ptr<HomeMaticCentral> central = std::dynamic_pointer_cast<HomeMaticCentral>(getCentral());
		if(!central) return; //Shouldn't happen
		std::shared_ptr<BidCoSPeer> remotePeer(central->getPeer(peerAddress));
		if(!remotePeer) return; //Shouldn't happen
		PHomegearDevice remoteRpcDevice = remotePeer->getRpcDevice();
		if(!remoteRpcDevice) return;
		Functions::iterator remoteFunctionIterator = remoteRpcDevice->functions.find(remoteChannel);
		if(remoteFunctionIterator == remoteRpcDevice->functions.end()) return;
		PFunction remoteRpcFunction = remoteFunctionIterator->second;
		int32_t groupedWith = remotePeer->getChannelGroupedWith(remoteChannel);
		std::string scenario;
		if(groupedWith == -1 && !remoteRpcFunction->defaultLinkScenarioElementId.empty()) scenario = remoteRpcFunction->defaultLinkScenarioElementId;
		if(groupedWith > -1)
		{
			if(groupedWith > remoteChannel && !remoteRpcFunction->defaultGroupedLinkScenarioElementId1.empty()) scenario = remoteRpcFunction->defaultGroupedLinkScenarioElementId1;
			else if(groupedWith < remoteChannel && !remoteRpcFunction->defaultGroupedLinkScenarioElementId2.empty()) scenario = remoteRpcFunction->defaultGroupedLinkScenarioElementId2;
		}
		if(scenario.empty()) return;
		Scenarios::iterator scenarioIterator = parameterGroup->scenarios.find(scenario);
		if(scenarioIterator == parameterGroup->scenarios.end()) return;
		GD::out.printInfo("Info: Peer " + std::to_string(_peerID) + ": Applying scenario " + scenario + ".");
		for(ScenarioEntries::iterator j = scenarioIterator->second->scenarioEntries.begin(); j != scenarioIterator->second->scenarioEntries.end(); ++j)
		{
			BaseLib::Systems::RPCConfigurationParameter* parameter = &linksCentral[channel][peerAddress][remoteChannel][j->first];
			if(!parameter->rpcParameter) continue;
			parameter->rpcParameter->convertToPacket(j->second, parameter->data);
			if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
			else saveParameter(0, ParameterGroup::Type::Enum::link, channel, parameter->rpcParameter->id, parameter->data, peerAddress, remoteChannel);
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

BidCoSPeer::~BidCoSPeer()
{
	try
	{
		dispose();
		_pingThreadMutex.lock();
		if(_pingThread.joinable()) _pingThread.join();
		_pingThreadMutex.unlock();
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

uint64_t BidCoSPeer::getTeamRemoteID()
{
	try
	{
		if(_team.serialNumber.length() > 0 && _team.id == 0)
		{
			std::shared_ptr<BaseLib::Systems::Peer> peer = getCentral()->getPeer(_team.serialNumber);
			if(peer) setTeamRemoteID(peer->getID());
		}
		return _team.id;
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
    return 0;
}

void BidCoSPeer::setAESKeyIndex(int32_t value)
{
	try
	{
		_aesKeyIndex = value;
		saveVariable(17, value);
		if(valuesCentral.find(0) != valuesCentral.end() && valuesCentral.at(0).find("AES_KEY") != valuesCentral.at(0).end())
		{
			BaseLib::Systems::RPCConfigurationParameter* parameter = &valuesCentral[0]["AES_KEY"];
			parameter->data.clear();
			parameter->data.push_back(_aesKeyIndex);
			if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
			else saveParameter(0, ParameterGroup::Type::Enum::variables, 0, "AES_KEY", parameter->data);
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

void BidCoSPeer::setPhysicalInterfaceID(std::string id)
{
	if(id.empty() || (GD::physicalInterfaces.find(id) != GD::physicalInterfaces.end() && GD::physicalInterfaces.at(id)))
	{
		_physicalInterfaceID = id;
		if(peerInfoPacketsEnabled) _physicalInterface->removePeer(_address);
		setPhysicalInterface(id.empty() ? GD::defaultPhysicalInterface : GD::physicalInterfaces.at(_physicalInterfaceID));
		uint64_t virtualPeerId = getVirtualPeerId();
		if(virtualPeerId > 0)
		{
			std::shared_ptr<HomeMaticCentral> central = std::dynamic_pointer_cast<HomeMaticCentral>(getCentral());
			if(central)
			{
				std::shared_ptr<BidCoSPeer> virtualPeer = central->getPeer(virtualPeerId);
				if(virtualPeer) virtualPeer->setPhysicalInterfaceID(id);
			}
		}
		saveVariable(19, _physicalInterfaceID);
		if(peerInfoPacketsEnabled) _physicalInterface->addPeer(getPeerInfo());
	}
}

void BidCoSPeer::setPhysicalInterface(std::shared_ptr<IBidCoSInterface> interface)
{
	try
	{
		if(!interface) return;
		_physicalInterface = interface;
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

BidCoSPeer::BidCoSPeer(uint32_t parentID, IPeerEventSink* eventHandler) : Peer(GD::bl, parentID, eventHandler)
{
	try
	{
		_team.address = 0;
		pendingBidCoSQueues.reset(new PendingBidCoSQueues());
		setPhysicalInterface(GD::defaultPhysicalInterface);
		_lastPing = BaseLib::HelperFunctions::getTime() - (BaseLib::HelperFunctions::getRandomNumber(1, 60) * 10000);
		_bestInterfaceCurrent = std::tuple<int32_t, int32_t, std::string>(-1, 0, "");
		_bestInterfaceLast = std::tuple<int32_t, int32_t, std::string>(-1, 0, "");
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

BidCoSPeer::BidCoSPeer(int32_t id, int32_t address, std::string serialNumber, uint32_t parentID, IPeerEventSink* eventHandler) : Peer(GD::bl, id, address, serialNumber, parentID, eventHandler)
{
	setPhysicalInterface(GD::defaultPhysicalInterface);
	_lastPing = BaseLib::HelperFunctions::getTime() - (BaseLib::HelperFunctions::getRandomNumber(1, 60) * 10000);
	_bestInterfaceCurrent = std::tuple<int32_t, int32_t, std::string>(-1, 0, "");
	_bestInterfaceLast = std::tuple<int32_t, int32_t, std::string>(-1, 0, "");
}

void BidCoSPeer::worker()
{
	if(_disposing) return;
	std::vector<uint32_t> positionsToDelete;
	std::vector<std::shared_ptr<VariableToReset>> variablesToReset;
	int32_t index;
	int64_t time;
	try
	{
		time = BaseLib::HelperFunctions::getTime();
		if(!_variablesToReset.empty())
		{
			index = 0;
			_variablesToResetMutex.lock();
			for(std::vector<std::shared_ptr<VariableToReset>>::iterator i = _variablesToReset.begin(); i != _variablesToReset.end(); ++i)
			{
				if((*i)->resetTime + 3000 <= time)
				{
					variablesToReset.push_back(*i);
					positionsToDelete.push_back(index);
				}
				index++;
			}
			_variablesToResetMutex.unlock();
			for(std::vector<std::shared_ptr<VariableToReset>>::iterator i = variablesToReset.begin(); i != variablesToReset.end(); ++i)
			{
				if((*i)->isDominoEvent)
				{
					BaseLib::Systems::RPCConfigurationParameter* parameter = &valuesCentral.at((*i)->channel).at((*i)->key);
					parameter->data = (*i)->data;
					if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
					else saveParameter(0, ParameterGroup::Type::Enum::variables, (*i)->channel, (*i)->key, parameter->data);
					std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string> {(*i)->key});
					std::shared_ptr<std::vector<PVariable>> rpcValues(new std::vector<PVariable> { valuesCentral.at((*i)->channel).at((*i)->key).rpcParameter->convertFromPacket((*i)->data) });
					GD::out.printInfo("Info: Domino event: " + (*i)->key + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string((*i)->channel) + " was reset.");
					raiseRPCEvent(_peerID, (*i)->channel, _serialNumber + ":" + std::to_string((*i)->channel), valueKeys, rpcValues);
				}
				else
				{
					PVariable rpcValue = valuesCentral.at((*i)->channel).at((*i)->key).rpcParameter->convertFromPacket((*i)->data);
					setValue(nullptr, (*i)->channel, (*i)->key, rpcValue, false);
				}
			}
			_variablesToResetMutex.lock();
			for(std::vector<uint32_t>::reverse_iterator i = positionsToDelete.rbegin(); i != positionsToDelete.rend(); ++i)
			{
				std::vector<std::shared_ptr<VariableToReset>>::iterator element = _variablesToReset.begin() + *i;
				if(element < _variablesToReset.end()) _variablesToReset.erase(element);
			}
			_variablesToResetMutex.unlock();
			positionsToDelete.clear();
			variablesToReset.clear();
		}
		if(_rpcDevice)
		{
			serviceMessages->checkUnreach(_rpcDevice->timeout, getLastPacketReceived());
			if(serviceMessages->getUnreach())
			{
				if(time - _lastPing > 600000 && ((getRXModes() & HomegearDevice::ReceiveModes::Enum::always) || (getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeOnRadio)))
				{
					_pingThreadMutex.lock();
					if(!_disposing && !deleting && _lastPing < time) //Check that _lastPing wasn't set in putParamset after locking the mutex
					{
						_lastPing = time; //Set here to avoid race condition between worker thread and ping thread
						_bl->threadManager.join(_pingThread);
						_bl->threadManager.start(_pingThread, false, &BidCoSPeer::pingThread, this);
					}
					_pingThreadMutex.unlock();
				}
			}
			else
			{
				if(configCentral[0].find("POLLING") != configCentral[0].end() && configCentral[0].at("POLLING").data.size() > 0 && configCentral[0].at("POLLING").data.at(0) > 0 && configCentral[0].find("POLLING_INTERVAL") != configCentral[0].end())
				{
					//Polling is enabled
					BaseLib::Systems::RPCConfigurationParameter* parameter = &configCentral[0]["POLLING_INTERVAL"];
					int32_t data = 0;
					_bl->hf.memcpyBigEndian(data, parameter->data); //Shortcut to save resources. The normal way would be to call "convertFromPacket".
					int64_t pollingInterval = data * 60000;
					if(pollingInterval < 600000) pollingInterval = 600000;
					if(time - _lastPing >= pollingInterval && ((getRXModes() & HomegearDevice::ReceiveModes::Enum::always) || (getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeOnRadio)))
					{
						int64_t timeSinceLastPacket = time - ((int64_t)_lastPacketReceived * 1000);
						if(timeSinceLastPacket > 0 && timeSinceLastPacket >= pollingInterval)
						{
							_pingThreadMutex.lock();
							if(!_disposing && !deleting && _lastPing < time) //Check that _lastPing wasn't set in putParamset after locking the mutex
							{
								_lastPing = time; //Set here to avoid race condition between worker thread and ping thread
								_bl->threadManager.join(_pingThread);
								_bl->threadManager.start(_pingThread, false, &BidCoSPeer::pingThread, this);
							}
							_pingThreadMutex.unlock();
						}
					}
				}
				else _lastPing = time; //Set _lastPing, so there is a delay of 10 minutes after the device is unreachable before the first ping.
			}
		}
		if(serviceMessages->getConfigPending())
		{
			if(!pendingBidCoSQueues || pendingBidCoSQueues->empty()) serviceMessages->setConfigPending(false);
			else if(_bl->settings.devLog() && (getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeUp) && (_bl->hf.getTime() - serviceMessages->getConfigPendingSetTime()) > 360000)
			{
				GD::out.printWarning("Devlog warning: Configuration for peer with id " + std::to_string(_peerID) + " supporting wake up is pending since more than 6 minutes.");
				serviceMessages->resetConfigPendingSetTime();
			}
		}
		if(_valuePending)
		{
			if(!pendingBidCoSQueues || pendingBidCoSQueues->empty()) setValuePending(false);
		}
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		_variablesToResetMutex.unlock();
	}
	catch(BaseLib::Exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
		_variablesToResetMutex.unlock();
	}
	catch(...)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
		_variablesToResetMutex.unlock();
	}
}

std::string BidCoSPeer::handleCliCommand(std::string command)
{
	try
	{
		std::ostringstream stringStream;

		if(command == "help")
		{
			stringStream << "List of commands:" << std::endl << std::endl;
			stringStream << "For more information about the individual command type: COMMAND help" << std::endl << std::endl;
			stringStream << "unselect\t\tUnselect this peer" << std::endl;
			stringStream << "channel count\t\tPrint the number of channels of this peer" << std::endl;
			stringStream << "config print\t\tPrints all configuration parameters and their values" << std::endl;
			stringStream << "queues info\t\tPrints information about the pending BidCoS packet queues" << std::endl;
			stringStream << "queues clear\t\tClears pending BidCoS packet queues" << std::endl;
			stringStream << "team info\t\tPrints information about this peers team" << std::endl;
			stringStream << "peers list\t\tLists all peers paired to this peer" << std::endl;
			return stringStream.str();
		}
		if(command.compare(0, 13, "channel count") == 0)
		{
			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help")
					{
						stringStream << "Description: This command prints this peer's number of channels." << std::endl;
						stringStream << "Usage: channel count" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  There are no parameters." << std::endl;
						return stringStream.str();
					}
				}
				index++;
			}

			stringStream << "Peer has " << _rpcDevice->functions.size() << " channels." << std::endl;
			return stringStream.str();
		}
		else if(command.compare(0, 12, "config print") == 0)
		{
			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help")
					{
						stringStream << "Description: This command prints all configuration parameters of this peer. The values are in BidCoS packet format." << std::endl;
						stringStream << "Usage: config print" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  There are no parameters." << std::endl;
						return stringStream.str();
					}
				}
				index++;
			}

			return printConfig();
		}
		else if(command.compare(0, 11, "queues info") == 0)
		{
			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help")
					{
						stringStream << "Description: This command prints information about the pending BidCoS queues." << std::endl;
						stringStream << "Usage: queues info" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  There are no parameters." << std::endl;
						return stringStream.str();
					}
				}
				index++;
			}

			pendingBidCoSQueues->getInfoString(stringStream);
			return stringStream.str();
		}
		else if(command.compare(0, 12, "queues clear") == 0)
		{
			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help")
					{
						stringStream << "Description: This command clears all pending BidCoS queues." << std::endl;
						stringStream << "Usage: queues clear" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  There are no parameters." << std::endl;
						return stringStream.str();
					}
				}
				index++;
			}

			pendingBidCoSQueues->clear();
			stringStream << "All pending BidCoSQueues were deleted." << std::endl;
			return stringStream.str();
		}
		else if(command.compare(0, 9, "team info") == 0)
		{
			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help")
					{
						stringStream << "Description: This command prints information about this peers team." << std::endl;
						stringStream << "Usage: team info" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  There are no parameters." << std::endl;
						return stringStream.str();
					}
				}
				index++;
			}

			if(_team.serialNumber.empty()) stringStream << "This peer doesn't support teams." << std::endl;
			else stringStream << "Team address: 0x" << std::hex << _team.address << std::dec << " Team serial number: " << _team.serialNumber << std::endl;
			return stringStream.str();
		}
		else if(command.compare(0, 10, "peers list") == 0)
		{
			std::stringstream stream(command);
			std::string element;
			int32_t index = 0;
			while(std::getline(stream, element, ' '))
			{
				if(index < 2)
				{
					index++;
					continue;
				}
				else if(index == 2)
				{
					if(element == "help")
					{
						stringStream << "Description: This command lists all peers paired to this peer." << std::endl;
						stringStream << "Usage: peers list" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  There are no parameters." << std::endl;
						return stringStream.str();
					}
				}
				index++;
			}

			_peersMutex.lock();
			if(_peers.empty())
			{
				stringStream << "No peers are paired to this peer." << std::endl;
				return stringStream.str();
			}
			for(std::unordered_map<int32_t, std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>>::iterator i = _peers.begin(); i != _peers.end(); ++i)
			{
				for(std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>::iterator j = i->second.begin(); j != i->second.end(); ++j)
				{
					stringStream << "Channel: " << i->first << "\tAddress: 0x" << std::hex << (*j)->address << "\tRemote channel: " << std::dec << (*j)->channel << "\tSerial number: " << (*j)->serialNumber << std::endl << std::dec;
				}
			}
			_peersMutex.unlock();
			return stringStream.str();
		}
		else return "Unknown command.\n";
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
    return "Error executing command. See log file for more details.\n";
}

bool BidCoSPeer::ping(int32_t packetCount, bool waitForResponse)
{
	try
	{
		std::shared_ptr<HomeMaticCentral> central = std::dynamic_pointer_cast<HomeMaticCentral>(getCentral());
		if(!central) return false;
		uint32_t time = BaseLib::HelperFunctions::getTimeSeconds();
		_lastPing = (int64_t)time * 1000;
		if(_rpcDevice && !_rpcDevice->valueRequestPackets.empty())
		{
			for(ValueRequestPackets::iterator i = _rpcDevice->valueRequestPackets.begin(); i != _rpcDevice->valueRequestPackets.end(); ++i)
			{
				for(std::map<std::string, PPacket>::iterator j = i->second.begin(); j != i->second.end(); ++j)
				{
					if(j->second->associatedVariables.empty()) continue;
					if(valuesCentral.find(i->first) == valuesCentral.end()) continue;
					int32_t associatedVariablesIndex = -1;
					for(uint32_t k = 0; k < j->second->associatedVariables.size(); k++)
					{
						if(valuesCentral[i->first].find(j->second->associatedVariables.at(k)->id) != valuesCentral[i->first].end())
						{
							associatedVariablesIndex = k;
							break;
						}
					}
					if(associatedVariablesIndex == -1) continue;
					PVariable result = getValueFromDevice(j->second->associatedVariables.at(associatedVariablesIndex), i->first, !waitForResponse);
					if(result && result->errorStruct) GD::out.printError("Error: getValueFromDevice in ping returned RPC error: " + result->structValue->at("faultString")->stringValue);
					if(!result || result->errorStruct || result->type == VariableType::tVoid) return false;
				}
			}
			return true;
		}

		//No get value frames
		std::vector<uint8_t> payload;
		payload.push_back(0x00);
		payload.push_back(0x06);
		std::shared_ptr<BidCoSPacket> ping(new BidCoSPacket(_messageCounter++, 0xA0, 0x01, central->getAddress(), _address, payload));
		if(getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeOnRadio) ping->setControlByte(0xB0);
		for(int32_t i = 0; i < packetCount; i++)
		{
			central->sendPacket(getPhysicalInterface(), ping);
			int32_t waitIndex = 0;
			std::shared_ptr<BidCoSPacket> receivedPacket;
			bool responseReceived = false;
			while(waitIndex < 5)
			{
				if(_lastPacketReceived >= time)
				{
					responseReceived = true;
					break;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
				waitIndex++;
			}
			if(responseReceived) return true;
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
    return false;
}

void BidCoSPeer::pingThread()
{
	if(ping(3, true)) serviceMessages->endUnreach();
	else serviceMessages->setUnreach(true, false);
}

void BidCoSPeer::addPeer(int32_t channel, std::shared_ptr<BaseLib::Systems::BasicPeer> peer)
{
	try
	{
		if(_rpcDevice->functions.find(channel) == _rpcDevice->functions.end()) return;
		_peersMutex.lock();
		try
		{
			for(std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>::iterator i = _peers[channel].begin(); i != _peers[channel].end(); ++i)
			{
				if((*i)->address == peer->address && (*i)->channel == peer->channel)
				{
					_peers[channel].erase(i);
					break;
				}
			}
			_peers[channel].push_back(peer);
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
		_peersMutex.unlock();
		initializeLinkConfig(channel, peer->address, peer->channel, true);
		savePeers();
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

std::shared_ptr<BaseLib::Systems::BasicPeer> BidCoSPeer::getVirtualPeer(int32_t channel)
{
	_peersMutex.lock();
	try
	{
		for(std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>::iterator i = _peers[channel].begin(); i != _peers[channel].end(); ++i)
		{
			if((*i)->isVirtual) return *i;
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
    _peersMutex.unlock();
	return std::shared_ptr<BaseLib::Systems::BasicPeer>();
}

void BidCoSPeer::removePeer(int32_t channel, int32_t address, int32_t remoteChannel)
{
	_peersMutex.lock();
	try
	{
		for(std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>::iterator i = _peers[channel].begin(); i != _peers[channel].end(); ++i)
		{
			if((*i)->address == address && (*i)->channel == remoteChannel)
			{
				_peers[channel].erase(i);
				_peersMutex.unlock();
				if(linksCentral[channel].find(address) != linksCentral[channel].end() && linksCentral[channel][address].find(remoteChannel) != linksCentral[channel][address].end()) linksCentral[channel][address].erase(linksCentral[channel][address].find(remoteChannel));
				_databaseMutex.lock();
				BaseLib::Database::DataRow data;
				data.push_back(std::shared_ptr<BaseLib::Database::DataColumn>(new BaseLib::Database::DataColumn(_peerID)));
				data.push_back(std::shared_ptr<BaseLib::Database::DataColumn>(new BaseLib::Database::DataColumn((int32_t)ParameterGroup::Type::Enum::link)));
				data.push_back(std::shared_ptr<BaseLib::Database::DataColumn>(new BaseLib::Database::DataColumn(channel)));
				data.push_back(std::shared_ptr<BaseLib::Database::DataColumn>(new BaseLib::Database::DataColumn(address)));
				data.push_back(std::shared_ptr<BaseLib::Database::DataColumn>(new BaseLib::Database::DataColumn(remoteChannel)));
				_bl->db->deletePeerParameter(_peerID, data);
				_databaseMutex.unlock();
				savePeers();
				return;
			}
		}
		_peersMutex.unlock();
		return;
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
    _peersMutex.unlock();
    _databaseMutex.unlock();
}

void BidCoSPeer::save(bool savePeer, bool variables, bool centralConfig)
{
	try
	{
		Peer::save(savePeer, variables, centralConfig);
		if(!variables)
		{
			saveNonCentralConfig();
			saveVariablesToReset();
			savePendingQueues();
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

void BidCoSPeer::serializePeers(std::vector<uint8_t>& encodedData)
{
	_peersMutex.lock();
	try
	{
		BaseLib::BinaryEncoder encoder(_bl);
		encoder.encodeInteger(encodedData, 0);
		encoder.encodeInteger(encodedData, _peers.size());
		for(std::unordered_map<int32_t, std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>>::const_iterator i = _peers.begin(); i != _peers.end(); ++i)
		{
			encoder.encodeInteger(encodedData, i->first);
			encoder.encodeInteger(encodedData, i->second.size());
			for(std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>::const_iterator j = i->second.begin(); j != i->second.end(); ++j)
			{
				if(!*j) continue;
				encoder.encodeBoolean(encodedData, (*j)->isSender);
				encoder.encodeInteger(encodedData, (*j)->id);
				encoder.encodeInteger(encodedData, (*j)->address);
				encoder.encodeInteger(encodedData, (*j)->channel);
				encoder.encodeString(encodedData, (*j)->serialNumber);
				encoder.encodeBoolean(encodedData, (*j)->isVirtual);
				encoder.encodeString(encodedData, (*j)->linkName);
				encoder.encodeString(encodedData, (*j)->linkDescription);
				encoder.encodeInteger(encodedData, (*j)->data.size());
				encodedData.insert(encodedData.end(), (*j)->data.begin(), (*j)->data.end());
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
    _peersMutex.unlock();
}

void BidCoSPeer::unserializePeers(std::shared_ptr<std::vector<char>> serializedData)
{
	_peersMutex.lock();
	try
	{
		BaseLib::BinaryDecoder decoder(_bl);
		uint32_t position = 0;
		uint32_t version = decoder.decodeInteger(*serializedData, position);
		uint32_t peersSize = 0;
		bool oldFormat = false;
		if(version != 0)
		{
			oldFormat = true;
			peersSize = version;
		}
		else peersSize = decoder.decodeInteger(*serializedData, position);
		for(uint32_t i = 0; i < peersSize; i++)
		{
			uint32_t channel = decoder.decodeInteger(*serializedData, position);
			uint32_t peerCount = decoder.decodeInteger(*serializedData, position);
			for(uint32_t j = 0; j < peerCount; j++)
			{
				std::shared_ptr<BaseLib::Systems::BasicPeer> basicPeer(new BaseLib::Systems::BasicPeer());
				if(!oldFormat)
				{
					basicPeer->isSender = decoder.decodeBoolean(*serializedData, position);
					basicPeer->id = decoder.decodeInteger(*serializedData, position);
				}
				basicPeer->address = decoder.decodeInteger(*serializedData, position);
				basicPeer->channel = decoder.decodeInteger(*serializedData, position);
				basicPeer->serialNumber = decoder.decodeString(*serializedData, position);
				basicPeer->isVirtual = decoder.decodeBoolean(*serializedData, position);
				_peers[channel].push_back(basicPeer);
				basicPeer->linkName = decoder.decodeString(*serializedData, position);
				basicPeer->linkDescription = decoder.decodeString(*serializedData, position);
				uint32_t dataSize = decoder.decodeInteger(*serializedData, position);
				if(position + dataSize <= serializedData->size()) basicPeer->data.insert(basicPeer->data.end(), serializedData->begin() + position, serializedData->begin() + position + dataSize);
				position += dataSize;
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
    _peersMutex.unlock();
}

void BidCoSPeer::serializeNonCentralConfig(std::vector<uint8_t>& encodedData)
{
	try
	{
		BaseLib::BinaryEncoder encoder(_bl);
		encoder.encodeInteger(encodedData, config.size());
		for(std::unordered_map<int32_t, int32_t>::const_iterator i = config.begin(); i != config.end(); ++i)
		{
			encoder.encodeInteger(encodedData, i->first);
			encoder.encodeInteger(encodedData, i->second);
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

void BidCoSPeer::unserializeNonCentralConfig(std::shared_ptr<std::vector<char>> serializedData)
{
	try
	{
		config.clear();
		BaseLib::BinaryDecoder decoder(_bl);
		uint32_t position = 0;
		uint32_t configSize = decoder.decodeInteger(*serializedData, position);
		for(uint32_t i = 0; i < configSize; i++)
		{
			int32_t index = decoder.decodeInteger(*serializedData, position);
			config[index] = decoder.decodeInteger(*serializedData, position);
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

void BidCoSPeer::serializeVariablesToReset(std::vector<uint8_t>& encodedData)
{
	try
	{
		BaseLib::BinaryEncoder encoder(_bl);
		_variablesToResetMutex.lock();
		encoder.encodeInteger(encodedData, _variablesToReset.size());
		for(std::vector<std::shared_ptr<VariableToReset>>::iterator i = _variablesToReset.begin(); i != _variablesToReset.end(); ++i)
		{
			encoder.encodeInteger(encodedData, (*i)->channel);
			encoder.encodeString(encodedData, (*i)->key);
			encoder.encodeInteger(encodedData, (*i)->data.size());
			encodedData.insert(encodedData.end(), (*i)->data.begin(), (*i)->data.end());
			encoder.encodeInteger(encodedData, (*i)->resetTime / 1000);
			encoder.encodeBoolean(encodedData, (*i)->isDominoEvent);
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
    _variablesToResetMutex.unlock();
}

void BidCoSPeer::unserializeVariablesToReset(std::shared_ptr<std::vector<char>> serializedData)
{
	try
	{
		_variablesToResetMutex.lock();
		_variablesToReset.clear();
		_variablesToResetMutex.unlock();
		BaseLib::BinaryDecoder decoder(_bl);
		uint32_t position = 0;
		uint32_t variablesToResetSize = decoder.decodeInteger(*serializedData, position);
		for(uint32_t i = 0; i < variablesToResetSize; i++)
		{
			std::shared_ptr<VariableToReset> variable(new VariableToReset());
			variable->channel = decoder.decodeInteger(*serializedData, position);
			variable->key = decoder.decodeString(*serializedData, position);
			uint32_t dataSize = decoder.decodeInteger(*serializedData, position);
			if(position + dataSize <= serializedData->size()) variable->data.insert(variable->data.end(), serializedData->begin() + position, serializedData->begin() + position + dataSize);
			position += dataSize;
			variable->resetTime = ((int64_t)decoder.decodeInteger(*serializedData, position)) * 1000;
			variable->isDominoEvent = decoder.decodeBoolean(*serializedData, position);
			try
			{
				_variablesToResetMutex.lock();
				_variablesToReset.push_back(variable);
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
			_variablesToResetMutex.unlock();
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

void BidCoSPeer::saveVariables()
{
	try
	{
		if(_peerID == 0 || isTeam()) return;
		Peer::saveVariables();
		saveVariable(1, _remoteChannel);
		saveVariable(2, _localChannel);
		saveVariable(4, _countFromSysinfo);
		saveVariable(5, _messageCounter);
		saveVariable(6, _pairingComplete);
		saveVariable(7, _teamChannel);
		saveVariable(8, _team.address);
		saveVariable(9, _team.channel);
		saveVariable(10, _team.serialNumber);
		saveVariable(11, _team.data);
		savePeers(); //12
		saveNonCentralConfig(); //13
		saveVariablesToReset(); //14
		savePendingQueues(); //16
		if(_aesKeyIndex > 0)
		{
			saveVariable(17, _aesKeyIndex);
		}
		saveVariable(19, _physicalInterfaceID);
		saveVariable(20, (int32_t)_valuePending);
		saveVariable(21, (int32_t)_team.id);
		saveVariable(22, _generalCounter);
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

void BidCoSPeer::savePeers()
{
	try
	{
		std::vector<uint8_t> serializedData;
		serializePeers(serializedData);
		saveVariable(12, serializedData);
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

void BidCoSPeer::saveNonCentralConfig()
{
	try
	{
		std::vector<uint8_t> serializedData;
		serializeNonCentralConfig(serializedData);
		saveVariable(13, serializedData);
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

void BidCoSPeer::saveVariablesToReset()
{
	try
	{
		std::vector<uint8_t> serializedData;
		serializeVariablesToReset(serializedData);
		saveVariable(14, serializedData);
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

void BidCoSPeer::savePendingQueues()
{
	try
	{
		if(!pendingBidCoSQueues) return;
		std::vector<uint8_t> serializedData;
		pendingBidCoSQueues->serialize(serializedData);
		saveVariable(16, serializedData);
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

bool BidCoSPeer::pendingQueuesEmpty()
{
	if(!pendingBidCoSQueues) return true;
	return pendingBidCoSQueues->empty();
}

void BidCoSPeer::enqueuePendingQueues()
{
	try
	{
		std::shared_ptr<HomeMaticCentral> central = std::dynamic_pointer_cast<HomeMaticCentral>(getCentral());
		if(central)
		{
			GD::out.printInfo("Info: Queue is not finished (peer: " + std::to_string(_peerID) + "). Retrying...");
			central->enqueuePendingQueues(_address);
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

void BidCoSPeer::loadVariables(BaseLib::Systems::ICentral* device, std::shared_ptr<BaseLib::Database::DataTable>& rows)
{
	try
	{
		if(!rows) rows = _bl->db->getPeerVariables(_peerID);
		Peer::loadVariables(device, rows);
		_databaseMutex.lock();
		for(BaseLib::Database::DataTable::iterator row = rows->begin(); row != rows->end(); ++row)
		{
			_variableDatabaseIDs[row->second.at(2)->intValue] = row->second.at(0)->intValue;
			switch(row->second.at(2)->intValue)
			{
			case 1:
				_remoteChannel = row->second.at(3)->intValue;
				break;
			case 2:
				_localChannel = row->second.at(3)->intValue;
				break;
			case 4:
				_countFromSysinfo = row->second.at(3)->intValue;
				break;
			case 5:
				_messageCounter = row->second.at(3)->intValue;
				break;
			case 6:
				_pairingComplete = (bool)row->second.at(3)->intValue;
				break;
			case 7:
				_teamChannel = row->second.at(3)->intValue;
				break;
			case 8:
				_team.address = row->second.at(3)->intValue;
				break;
			case 9:
				_team.channel = row->second.at(3)->intValue;
				break;
			case 10:
				_team.serialNumber = row->second.at(4)->textValue;
				break;
			case 11:
				_team.data.insert(_team.data.begin(), row->second.at(5)->binaryValue->begin(), row->second.at(5)->binaryValue->end());
				break;
			case 12:
				unserializePeers(row->second.at(5)->binaryValue);
				break;
			case 13:
				unserializeNonCentralConfig(row->second.at(5)->binaryValue);
				break;
			case 14:
				unserializeVariablesToReset(row->second.at(5)->binaryValue);
				break;
			case 16:
				if(device)
				{
					pendingBidCoSQueues.reset(new PendingBidCoSQueues());
					pendingBidCoSQueues->unserialize(row->second.at(5)->binaryValue, this);
				}
				break;
			case 17:
				_aesKeyIndex = row->second.at(3)->intValue;
				break;
			case 19:
				_physicalInterfaceID = row->second.at(4)->textValue;
				if(!_physicalInterfaceID.empty() && GD::physicalInterfaces.find(_physicalInterfaceID) != GD::physicalInterfaces.end()) setPhysicalInterface(GD::physicalInterfaces.at(_physicalInterfaceID));
				_bestInterfaceLast = std::tuple<int32_t, int32_t, std::string>(-1, 0, _physicalInterfaceID);
				break;
			case 20:
				_valuePending = row->second.at(3)->intValue;
				break;
			case 21:
				_team.id = row->second.at(3)->intValue;
				break;
			case 22:
				_generalCounter = row->second.at(3)->intValue;
				break;
			}
		}
		if(!pendingBidCoSQueues) pendingBidCoSQueues.reset(new PendingBidCoSQueues());
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
	_databaseMutex.unlock();
}

bool BidCoSPeer::load(BaseLib::Systems::ICentral* device)
{
	try
	{
		std::shared_ptr<BaseLib::Database::DataTable> rows;
		loadVariables(device, rows);

		_rpcDevice = GD::family->getRpcDevices()->find(_deviceType, _firmwareVersion, _countFromSysinfo);
		if(!_rpcDevice)
		{
			GD::out.printError("Error loading HomeMatic BidCoS peer " + std::to_string(_peerID) + ": Device type not found: 0x" + BaseLib::HelperFunctions::getHexString((uint32_t)_deviceType.type()) + " Firmware version: " + std::to_string(_firmwareVersion));
			return false;
		}
		initializeTypeString();
		std::string entry;
		loadConfig();
		initializeCentralConfig();

		for(std::unordered_map<int32_t, std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>>::iterator i = _peers.begin(); i != _peers.end(); ++i)
		{
			for(std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>::iterator j = i->second.begin(); j != i->second.end(); ++j)
			{
				initializeLinkConfig(i->first, (*j)->address, (*j)->channel, false);
			}
		}

		serviceMessages.reset(new BaseLib::Systems::ServiceMessages(_bl, _peerID, _serialNumber, this));
		serviceMessages->load();

		if(aesEnabled()) checkAESKey();

		return true;
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
    return false;
}

bool BidCoSPeer::aesEnabled()
{
	try
	{
		for(std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>>::const_iterator i = configCentral.begin(); i != configCentral.end(); ++i)
		{
			std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>::const_iterator parameterIterator = i->second.find("AES_ACTIVE");
			if(parameterIterator != i->second.end() && !parameterIterator->second.data.empty() && (bool)parameterIterator->second.data.at(0))
			{
				return true;
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
    return false;
}

bool BidCoSPeer::aesEnabled(int32_t channel)
{
	try
	{
		std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>>::const_iterator channelIterator = configCentral.find(channel);
		if(channelIterator != configCentral.end())
		{
			std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>::const_iterator parameterIterator = channelIterator->second.find("AES_ACTIVE");
			if(parameterIterator != channelIterator->second.end() && !parameterIterator->second.data.empty() && (bool)parameterIterator->second.data.at(0))
			{
				return true;
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
    return false;
}

void BidCoSPeer::checkAESKey(bool onlyPushing)
{
	try
	{
		if(!_rpcDevice || !_rpcDevice->encryption) return;
		if(!aesEnabled()) return;
		if(_aesKeyIndex == (signed)_physicalInterface->getCurrentRFKeyIndex())
		{
			GD::out.printDebug("Debug: AES key of peer " + std::to_string(_peerID) + " is current.");
			return;
		}
		GD::out.printInfo("Info: Updating AES key of peer " + std::to_string(_peerID) + ".");
		if(_aesKeyIndex > (signed)_physicalInterface->getCurrentRFKeyIndex())
		{
			GD::out.printError("Error: Can't update AES key of peer " + std::to_string(_peerID) + ". Peer's AES key index is larger than the key index defined in homematicbidcos.conf.");
			return;
		}
		if(_aesKeyIndex > 0 && _physicalInterface->getCurrentRFKeyIndex() - _aesKeyIndex > 1)
		{
			GD::out.printError("Error: Can't update AES key of peer " + std::to_string(_peerID) + ". AES key seems to be updated more than once since this peer's config was last updated (key indexes differ by more than 1).");
			return;
		}
		if(pendingBidCoSQueues->find(BidCoSQueueType::SETAESKEY)) return;

		std::shared_ptr<BidCoSQueue> queue(new BidCoSQueue(_physicalInterface, BidCoSQueueType::SETAESKEY));
		queue->noSending = true;
		std::vector<uint8_t> payload;
		std::shared_ptr<HomeMaticCentral> central = std::dynamic_pointer_cast<HomeMaticCentral>(getCentral());

		payload.push_back(1);
		payload.push_back(_aesKeyIndex * 2);
		std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(_messageCounter, 0xA0, 0x04, central->getAddress(), _address, payload));
		queue->push(configPacket);
		queue->push(central->getMessages()->find(0x02, std::vector<std::pair<uint32_t, int32_t>>()));
		payload.clear();
		setMessageCounter(_messageCounter + 1);

		payload.push_back(1);
		payload.push_back((_aesKeyIndex * 2) + 1);
		configPacket = std::shared_ptr<BidCoSPacket>(new BidCoSPacket(_messageCounter, 0xA0, 0x04, central->getAddress(), _address, payload));
		queue->push(configPacket);
		queue->push(central->getMessages()->find(0x02, std::vector<std::pair<uint32_t, int32_t>>()));
		setMessageCounter(_messageCounter + 1);

		pendingBidCoSQueues->push(queue);
		if(serviceMessages) serviceMessages->setConfigPending(true);
		if((getRXModes() & HomegearDevice::ReceiveModes::Enum::always) || (getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeOnRadio))
		{
			if(!onlyPushing) central->enqueuePendingQueues(_address);
		}
		else
		{
			GD::out.printDebug("Debug: Packet was queued and will be sent with next wake me up packet.");
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

uint64_t BidCoSPeer::getVirtualPeerId()
{
	_peersMutex.lock();
	try
	{
		//This is pretty dirty, but as all virtual peers should be the same peer, this should always work
		for(std::unordered_map<int32_t, std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>>::iterator j = _peers.begin(); j != _peers.end(); ++j)
		{
			for(std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>::iterator i = j->second.begin(); i != j->second.end(); ++i)
			{
				if((*i)->isVirtual)
				{
					_peersMutex.unlock();
					return (*i)->id;
				}
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
    _peersMutex.unlock();
	return 0;
}

std::string BidCoSPeer::printConfig()
{
	try
	{
		std::ostringstream stringStream;
		stringStream << "MASTER" << std::endl;
		stringStream << "{" << std::endl;
		for(std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>>::const_iterator i = configCentral.begin(); i != configCentral.end(); ++i)
		{
			stringStream << "\t" << "Channel: " << std::dec << i->first << std::endl;
			stringStream << "\t{" << std::endl;
			for(std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>::const_iterator j = i->second.begin(); j != i->second.end(); ++j)
			{
				stringStream << "\t\t[" << j->first << "]: ";
				if(!j->second.rpcParameter) stringStream << "(No RPC parameter) ";
				for(std::vector<uint8_t>::const_iterator k = j->second.data.begin(); k != j->second.data.end(); ++k)
				{
					stringStream << std::hex << std::setfill('0') << std::setw(2) << (int32_t)*k << " ";
				}
				stringStream << std::endl;
			}
			stringStream << "\t}" << std::endl;
		}
		stringStream << "}" << std::endl << std::endl;

		stringStream << "VALUES" << std::endl;
		stringStream << "{" << std::endl;
		for(std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>>::const_iterator i = valuesCentral.begin(); i != valuesCentral.end(); ++i)
		{
			stringStream << "\t" << "Channel: " << std::dec << i->first << std::endl;
			stringStream << "\t{" << std::endl;
			for(std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>::const_iterator j = i->second.begin(); j != i->second.end(); ++j)
			{
				stringStream << "\t\t[" << j->first << "]: ";
				if(!j->second.rpcParameter) stringStream << "(No RPC parameter) ";
				for(std::vector<uint8_t>::const_iterator k = j->second.data.begin(); k != j->second.data.end(); ++k)
				{
					stringStream << std::hex << std::setfill('0') << std::setw(2) << (int32_t)*k << " ";
				}
				stringStream << std::endl;
			}
			stringStream << "\t}" << std::endl;
		}
		stringStream << "}" << std::endl << std::endl;

		stringStream << "LINK" << std::endl;
		stringStream << "{" << std::endl;
		for(std::unordered_map<uint32_t, std::unordered_map<int32_t, std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>>>>::const_iterator i = linksCentral.begin(); i != linksCentral.end(); ++i)
		{
			stringStream << "\t" << "Channel: " << std::dec << i->first << std::endl;
			stringStream << "\t{" << std::endl;
			for(std::unordered_map<int32_t, std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>>>::const_iterator j = i->second.begin(); j != i->second.end(); ++j)
			{
				stringStream << "\t\t" << "Address: " << std::hex << "0x" << j->first << std::endl;
				stringStream << "\t\t{" << std::endl;
				for(std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>>::const_iterator k = j->second.begin(); k != j->second.end(); ++k)
				{
					stringStream << "\t\t\t" << "Remote channel: " << std::dec << k->first << std::endl;
					stringStream << "\t\t\t{" << std::endl;
					for(std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>::const_iterator l = k->second.begin(); l != k->second.end(); ++l)
					{
						stringStream << "\t\t\t\t[" << l->first << "]: ";
						if(!l->second.rpcParameter) stringStream << "(No RPC parameter) ";
						for(std::vector<uint8_t>::const_iterator m = l->second.data.begin(); m != l->second.data.end(); ++m)
						{
							stringStream << std::hex << std::setfill('0') << std::setw(2) << (int32_t)*m << " ";
						}
						stringStream << std::endl;
					}
					stringStream << "\t\t\t}" << std::endl;
				}
				stringStream << "\t\t}" << std::endl;
			}
			stringStream << "\t}" << std::endl;
		}
		stringStream << "}" << std::endl << std::endl;
		return stringStream.str();
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
    return "";
}

bool BidCoSPeer::needsWakeup()
{
	try
	{
		HomegearDevice::ReceiveModes::Enum rxModes = getRXModes();
		return (serviceMessages->getConfigPending() || _valuePending) && ((rxModes & HomegearDevice::ReceiveModes::Enum::wakeUp) || (rxModes & HomegearDevice::ReceiveModes::Enum::lazyConfig));
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
    return false;
}

IBidCoSInterface::PeerInfo BidCoSPeer::getPeerInfo()
{
	try
	{
		IBidCoSInterface::PeerInfo peerInfo;
		peerInfo.address = _address;
		if(!_rpcDevice) return peerInfo;
		peerInfo.wakeUp = needsWakeup();
		peerInfo.aesEnabled = (pendingBidCoSQueues->find(BidCoSQueueType::SETAESKEY) && _aesKeyIndex == 0) ? false : aesEnabled();
		peerInfo.keyIndex = _aesKeyIndex;
		for(Functions::iterator i = _rpcDevice->functions.begin(); i != _rpcDevice->functions.end(); ++i)
		{
			if(i->first == 0) continue;
			if(!i->second || !peerInfo.aesEnabled)
			{
				peerInfo.aesChannels[i->first] = false;
			}
			else if(configCentral.find(i->first) == configCentral.end() || configCentral.at(i->first).find("AES_ACTIVE") == configCentral.at(i->first).end() || configCentral.at(i->first).at("AES_ACTIVE").data.empty())
			{
				peerInfo.aesChannels[i->first] = i->second->encryptionEnabledByDefault;
			}
			else
			{
				peerInfo.aesChannels[i->first] = (bool)configCentral.at(i->first).at("AES_ACTIVE").data.at(0);
			}
		}
		return peerInfo;
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
    return IBidCoSInterface::PeerInfo();
}

void BidCoSPeer::onConfigPending(bool configPending)
{
	try
	{
		Peer::onConfigPending(configPending);

		HomegearDevice::ReceiveModes::Enum rxModes = getRXModes();
		if(configPending)
		{
			if((rxModes & HomegearDevice::ReceiveModes::Enum::wakeUp) || (rxModes & HomegearDevice::ReceiveModes::Enum::lazyConfig))
			{
				GD::out.printDebug("Debug: Setting physical device's wake up flag.");
				if(peerInfoPacketsEnabled) _physicalInterface->setWakeUp(getPeerInfo());
			}
		}
		else
		{
			if((rxModes & HomegearDevice::ReceiveModes::Enum::wakeUp) || (rxModes & HomegearDevice::ReceiveModes::Enum::lazyConfig))
			{
				GD::out.printDebug("Debug: Removing physical device's wake up flag.");
				if(peerInfoPacketsEnabled) _physicalInterface->setWakeUp(getPeerInfo());
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

void BidCoSPeer::setValuePending(bool value)
{
	try
	{
		_valuePending = value;
		saveVariable(20, value);

		HomegearDevice::ReceiveModes::Enum rxModes = getRXModes();
		if(value)
		{
			if((rxModes & HomegearDevice::ReceiveModes::Enum::wakeUp) || (rxModes & HomegearDevice::ReceiveModes::Enum::lazyConfig))
			{
				GD::out.printDebug("Debug: Setting physical device's wake up flag.");
				if(peerInfoPacketsEnabled) _physicalInterface->setWakeUp(getPeerInfo());
			}
		}
		else
		{
			if((rxModes & HomegearDevice::ReceiveModes::Enum::wakeUp) || (rxModes & HomegearDevice::ReceiveModes::Enum::lazyConfig))
			{
				GD::out.printDebug("Debug: Removing physical device's wake up flag.");
				if(peerInfoPacketsEnabled) _physicalInterface->setWakeUp(getPeerInfo());
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

int32_t BidCoSPeer::getChannelGroupedWith(int32_t channel)
{
	try
	{
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return -1;
		if(functionIterator->second->grouped)
		{
			uint32_t firstGroupChannel = 0;
			//Find first channel of group
			for(Functions::iterator i = _rpcDevice->functions.begin(); i != _rpcDevice->functions.end(); ++i)
			{
				if(i->second->grouped)
				{
					firstGroupChannel = i->first;
					break;
				}
			}
			uint32_t groupedWith = 0;
			if((channel - firstGroupChannel) % 2 == 0) groupedWith = channel + 1; //Grouped with next channel
			else groupedWith = channel - 1; //Grouped with last channel
			if(_rpcDevice->functions.find(groupedWith) != _rpcDevice->functions.end())
			{
				return groupedWith;
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
	return -1;
}

int32_t BidCoSPeer::getNewFirmwareVersion()
{
	try
	{
		std::string filenamePrefix = BaseLib::HelperFunctions::getHexString(0, 4) + "." + BaseLib::HelperFunctions::getHexString(_deviceType.type(), 8);
		std::string versionFile(_bl->settings.firmwarePath() + filenamePrefix + ".version");
		if(!BaseLib::Io::fileExists(versionFile)) return 0;
		std::string versionHex = BaseLib::Io::getFileContent(versionFile);
		return BaseLib::Math::getNumber(versionHex, true);
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
	return 0;
}

std::string BidCoSPeer::getFirmwareVersionString(int32_t firmwareVersion)
{
	try
	{
		return GD::bl->hf.getHexString(firmwareVersion >> 4) + "." + GD::bl->hf.getHexString(firmwareVersion & 0x0F);
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
	return "";
}

bool BidCoSPeer::firmwareUpdateAvailable()
{
	try
	{
		return _firmwareVersion > 0 && _firmwareVersion < getNewFirmwareVersion();
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
	return false;
}

void BidCoSPeer::getValuesFromPacket(std::shared_ptr<BidCoSPacket> packet, std::vector<FrameValues>& frameValues)
{
	try
	{
		if(!_rpcDevice) return;
		//equal_range returns all elements with "0" or an unknown element as argument
		if(packet->messageType() == 0 || _rpcDevice->packetsByMessageType.find(packet->messageType()) == _rpcDevice->packetsByMessageType.end()) return;
		std::pair<PacketsByMessageType::iterator, PacketsByMessageType::iterator> range = _rpcDevice->packetsByMessageType.equal_range((uint32_t)packet->messageType());
		if(range.first == _rpcDevice->packetsByMessageType.end()) return;
		PacketsByMessageType::iterator i = range.first;
		do
		{
			FrameValues currentFrameValues;
			PPacket frame(i->second);
			if(!frame) continue;
			if(frame->direction == Packet::Direction::Enum::toCentral && packet->senderAddress() != _address && (!hasTeam() || packet->senderAddress() != _team.address)) continue;
			if(frame->direction == Packet::Direction::Enum::fromCentral && packet->destinationAddress() != _address) continue;
			if(packet->payload()->empty()) break;
			if(frame->subtype > -1 && frame->subtypeIndex >= 9 && (signed)packet->payload()->size() > (frame->subtypeIndex - 9) && packet->payload()->at(frame->subtypeIndex - 9) != (unsigned)frame->subtype) continue;
			int32_t channelIndex = frame->channelIndex;
			int32_t channel = -1;
			if(channelIndex >= 9 && (signed)packet->payload()->size() > (channelIndex - 9)) channel = packet->payload()->at(channelIndex - 9);
			if(channel > -1 && frame->channelSize < 1.0) channel &= (0xFF >> (8 - std::lround(frame->channelSize * 10) % 10));
			if(frame->channel > -1) channel = frame->channel;
			if(frame->length > 0 && packet->length() != frame->length) continue;
			currentFrameValues.frameID = frame->id;

			for(BinaryPayloads::iterator j = frame->binaryPayloads.begin(); j != frame->binaryPayloads.end(); ++j)
			{
				std::vector<uint8_t> data;
				if((*j)->size > 0 && (*j)->index > 0)
				{
					if(((int32_t)(*j)->index) - 9 >= (signed)packet->payload()->size()) continue;
					data = packet->getPosition((*j)->index, (*j)->size, -1);

					if((*j)->constValueInteger > -1)
					{
						int32_t intValue = 0;
						_bl->hf.memcpyBigEndian(intValue, data);
						if(intValue != (*j)->constValueInteger) break; else continue;
					}
				}
				else if((*j)->constValueInteger > -1)
				{
					_bl->hf.memcpyBigEndian(data, (*j)->constValueInteger);
				}
				else continue;
				for(std::vector<PParameter>::iterator k = frame->associatedVariables.begin(); k != frame->associatedVariables.end(); ++k)
				{
					if((*k)->physical->groupId != (*j)->parameterId) continue;
					currentFrameValues.parameterSetType = (*k)->parent()->type();
					bool setValues = false;
					if(currentFrameValues.paramsetChannels.empty()) //Fill paramsetChannels
					{
						int32_t startChannel = (channel < 0) ? 0 : channel;
						int32_t endChannel;
						//When fixedChannel is -2 (means '*') cycle through all channels
						if(frame->channel == -2)
						{
							startChannel = 0;
							endChannel = (_rpcDevice->functions.end()--)->first;
						}
						else endChannel = startChannel;
						for(int32_t l = startChannel; l <= endChannel; l++)
						{
							Functions::iterator functionIterator = _rpcDevice->functions.find(l);
							if(functionIterator == _rpcDevice->functions.end()) continue;
							PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(currentFrameValues.parameterSetType);
							if(!parameterGroup || parameterGroup->parameters.find((*k)->id) == parameterGroup->parameters.end()) continue;
							currentFrameValues.paramsetChannels.push_back(l);
							currentFrameValues.values[(*k)->id].channels.push_back(l);
							setValues = true;
						}
					}
					else //Use paramsetChannels
					{
						for(std::list<uint32_t>::const_iterator l = currentFrameValues.paramsetChannels.begin(); l != currentFrameValues.paramsetChannels.end(); ++l)
						{
							Functions::iterator functionIterator = _rpcDevice->functions.find(*l);
							if(functionIterator == _rpcDevice->functions.end()) continue;
							PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(currentFrameValues.parameterSetType);
							if(!parameterGroup || parameterGroup->parameters.find((*k)->id) == parameterGroup->parameters.end()) continue;
							currentFrameValues.values[(*k)->id].channels.push_back(*l);
							setValues = true;
						}
					}
					if(setValues) currentFrameValues.values[(*k)->id].value = data;
				}
			}
			if(!currentFrameValues.values.empty()) frameValues.push_back(currentFrameValues);
		} while(++i != range.second && i != _rpcDevice->packetsByMessageType.end());
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

void BidCoSPeer::handleDominoEvent(PParameter parameter, std::string& frameID, uint32_t channel)
{
	try
	{
		if(!parameter || !parameter->hasDelayedAutoResetParameters) return;
		for(std::vector<std::shared_ptr<Parameter::Packet>>::iterator j = parameter->eventPackets.begin(); j != parameter->eventPackets.end(); ++j)
		{
			if((*j)->id != frameID) continue;
			if((*j)->delayedAutoReset.first.empty()) continue;
			if(!_variablesToReset.empty())
			{
				_variablesToResetMutex.lock();
				for(std::vector<std::shared_ptr<VariableToReset>>::iterator k = _variablesToReset.begin(); k != _variablesToReset.end(); ++k)
				{
					if((*k)->channel == channel && (*k)->key == parameter->id)
					{
						GD::out.printDebug("Debug: Deleting element " + parameter->id + " from _variablesToReset. Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frameID);
						_variablesToReset.erase(k);
						break; //The key should only be once in the vector, so breaking is ok and we can't continue as the iterator is invalidated.
					}
				}
				_variablesToResetMutex.unlock();
			}
			PParameter delayParameter = _rpcDevice->functions.at(channel)->getParameterGroup(ParameterGroup::Type::Enum::variables)->parameters.at((*j)->delayedAutoReset.first);
			if(!delayParameter) continue;
			int64_t delay = delayParameter->convertFromPacket(valuesCentral[channel][(*j)->delayedAutoReset.first].data)->integerValue;
			if(delay < 0) continue; //0 is allowed. When 0 the parameter will be reset immediately
			int64_t time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
			std::shared_ptr<VariableToReset> variable(new VariableToReset);
			variable->channel = channel;
			_bl->hf.memcpyBigEndian(variable->data, (*j)->delayedAutoReset.second);
			variable->resetTime = time + (delay * 1000);
			variable->key = parameter->id;
			variable->isDominoEvent = true;
			_variablesToResetMutex.lock();
			_variablesToReset.push_back(variable);
			_variablesToResetMutex.unlock();
			GD::out.printDebug("Debug: " + parameter->id + " will be reset in " + std::to_string((variable->resetTime - time) / 1000) + "s.", 5);
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

void BidCoSPeer::checkForBestInterface(std::string interfaceID, int32_t rssi, uint8_t messageCounter)
{
	try
	{
		if(configCentral.find(0) == configCentral.end() || configCentral.at(0).find("ROAMING") == configCentral.at(0).end() || configCentral.at(0).at("ROAMING").data.size() == 0 || configCentral.at(0).at("ROAMING").data.at(0) == 0) return;
		if(interfaceID.empty() || GD::physicalInterfaces.find(interfaceID) == GD::physicalInterfaces.end()) return;

		if(std::get<0>(_bestInterfaceCurrent) != messageCounter && !std::get<2>(_bestInterfaceCurrent).empty())
		{
			if(_lastPacketMessageCounterFromAnyInterface != messageCounter) _lastPacketMessageCounterFromAnyInterface = _currentPacketMessageCounterFromAnyInterface;
			_currentPacketMessageCounterFromAnyInterface = messageCounter;
			int32_t rssiDifference = std::get<1>(_bestInterfaceLast) - std::get<1>(_bestInterfaceCurrent);
			if((rssiDifference > 10 || std::get<0>(_bestInterfaceLast) != _lastPacketMessageCounterFromAnyInterface) && std::get<2>(_bestInterfaceCurrent) != _physicalInterfaceID)
			{
				_bestInterfaceLast = _bestInterfaceCurrent;
				GD::bl->out.printInfo("Info: Changing interface of peer " + std::to_string(_peerID) + " to " + std::get<2>(_bestInterfaceLast) + ", because the reception is better.");
				if(_bl->settings.devLog()) GD::bl->out.printMessage("Devlog: Changing physical interface from " + _physicalInterfaceID + " to " + std::get<2>(_bestInterfaceLast) + " start.");
				setPhysicalInterfaceID(std::get<2>(_bestInterfaceLast));
				if(_bl->settings.devLog()) GD::bl->out.printMessage("Devlog: Changing physical interface end.");
			}
			_bestInterfaceCurrent = std::tuple<int64_t, int32_t, std::string>(messageCounter, 0, "");
		}
		if(std::get<2>(_bestInterfaceCurrent).empty() || std::get<1>(_bestInterfaceCurrent) == 0 || std::get<1>(_bestInterfaceCurrent) > rssi)
		{
			std::map<std::string, std::shared_ptr<IBidCoSInterface>>::iterator interfaceIterator = GD::physicalInterfaces.find(interfaceID);
			if(interfaceIterator != GD::physicalInterfaces.end() && interfaceIterator->second->isOpen()) _bestInterfaceCurrent = std::tuple<int32_t, int32_t, std::string>(messageCounter, rssi, interfaceID);
		}
		if(std::get<2>(_bestInterfaceLast) == interfaceID) _bestInterfaceLast = std::tuple<int32_t, int32_t, std::string>(messageCounter, rssi, interfaceID); //Update message counter and rssi
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

void BidCoSPeer::setRSSIDevice(uint8_t rssi)
{
	try
	{
		if(_disposing || rssi == 0) return;
		uint32_t time = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		if(valuesCentral.find(0) != valuesCentral.end() && valuesCentral.at(0).find("RSSI_DEVICE") != valuesCentral.at(0).end() && (time - _lastRSSIDevice) > 10)
		{
			_lastRSSIDevice = time;
			BaseLib::Systems::RPCConfigurationParameter* parameter = &valuesCentral.at(0).at("RSSI_DEVICE");
			parameter->data.at(0) = rssi;

			std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string>({std::string("RSSI_DEVICE")}));
			std::shared_ptr<std::vector<PVariable>> rpcValues(new std::vector<PVariable>());
			rpcValues->push_back(parameter->rpcParameter->convertFromPacket(parameter->data));

			raiseRPCEvent(_peerID, 0, _serialNumber + ":0", valueKeys, rpcValues);
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

bool BidCoSPeer::hasLowbatBit(PPacket frame)
{
	try
	{
		//Three things to check to see if position 9.7 is used: channelField, subtypeIndex and parameter indices
		if(frame->channelIndex == 9 && frame->channelSize >= 0.8) return false;
		else if(frame->subtypeIndex == 9 && frame->subtypeSize >= 0.8) return false;
		for(BinaryPayloads::iterator j = frame->binaryPayloads.begin(); j != frame->binaryPayloads.end(); ++j)
		{
			if((*j)->index >= 9 && (*j)->index < 10)
			{
				//fmod is needed for sizes > 1 (e. g. for frame WEATHER_EVENT)
				//9.8 is not working, result is 9.7999999
				if(((*j)->index + std::fmod((*j)->size, 1)) >= 9.79 && (*j)->parameterId != "LOWBAT") return false;
			}
		}
		return true;
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
    return false;
}

PVariable BidCoSPeer::getValueFromDevice(PParameter& parameter, int32_t channel, bool asynchronous)
{
	try
	{
		if(!parameter) return Variable::createError(-32500, "parameter is nullptr.");
		if(!(getRXModes() & HomegearDevice::ReceiveModes::Enum::always) && !(getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeOnRadio)) return Variable::createError(-6, "Parameter can't be requested actively, because the device isn't reachable all the time.");
		if(parameter->getPackets.empty()) return Variable::createError(-6, "Parameter can't be requested actively.");
		std::string getRequest = parameter->getPackets.front()->id;
		std::string getResponse = parameter->getPackets.front()->responseId;
		PacketsById::iterator packetIterator = _rpcDevice->packetsById.find(getRequest);
		if(packetIterator == _rpcDevice->packetsById.end()) return Variable::createError(-6, "No frame was found for parameter " + parameter->id);
		PPacket frame = packetIterator->second;
		packetIterator = _rpcDevice->packetsById.find(getResponse);
		PPacket responseFrame;
		if(packetIterator != _rpcDevice->packetsById.end()) responseFrame = packetIterator->second;

		if(valuesCentral.find(channel) == valuesCentral.end()) return Variable::createError(-2, "Unknown channel.");
		if(valuesCentral[channel].find(parameter->id) == valuesCentral[channel].end()) return Variable::createError(-5, "Unknown parameter.");

		std::shared_ptr<BidCoSQueue> queue(new BidCoSQueue(_physicalInterface, BidCoSQueueType::GETVALUE));
		queue->noSending = true;

		std::vector<uint8_t> payload;
		if(frame->subtype > -1 && frame->subtypeIndex >= 9)
		{
			while((signed)payload.size() - 1 < frame->subtypeIndex - 9) payload.push_back(0);
			payload.at(frame->subtypeIndex - 9) = (uint8_t)frame->subtype;
		}
		if(frame->channelIndex >= 9)
		{
			while((signed)payload.size() - 1 < frame->channelIndex - 9) payload.push_back(0);
			payload.at(frame->channelIndex - 9) = (uint8_t)channel;
		}
		uint8_t controlByte = 0xA0;
		if(getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeOnRadio) controlByte |= 0x10;
		std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(_messageCounter, controlByte, (uint8_t)frame->type, getCentral()->getAddress(), _address, payload));

		for(BinaryPayloads::iterator i = frame->binaryPayloads.begin(); i != frame->binaryPayloads.end(); ++i)
		{
			if((*i)->constValueInteger > -1)
			{
				std::vector<uint8_t> data;
				_bl->hf.memcpyBigEndian(data, (*i)->constValueInteger);
				packet->setPosition((*i)->index, (*i)->size, data);
				continue;
			}

			bool paramFound = false;
			for(std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>::iterator j = valuesCentral[channel].begin(); j != valuesCentral[channel].end(); ++j)
			{
				//Only compare id. Till now looking for value_id was not necessary.
				if((*i)->parameterId == j->second.rpcParameter->physical->groupId)
				{
					packet->setPosition((*i)->index, (*i)->size, j->second.data);
					paramFound = true;
					break;
				}
			}
			if(!paramFound) GD::out.printError("Error constructing packet. param \"" + (*i)->parameterId + "\" not found. Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frame->id);
		}

		setMessageCounter(_messageCounter + 1);
		queue->parameterName = parameter->id;
		queue->channel = channel;
		queue->push(packet);
		std::shared_ptr<HomeMaticCentral> central = std::dynamic_pointer_cast<HomeMaticCentral>(getCentral());
		if(responseFrame) queue->push(std::shared_ptr<BidCoSMessage>(new BidCoSMessage(responseFrame->type, 0, nullptr)));
		else queue->push(std::shared_ptr<BidCoSMessage>(new BidCoSMessage(-1, 0, nullptr)));
		pendingBidCoSQueues->remove(BidCoSQueueType::GETVALUE, parameter->id, channel);
		pendingBidCoSQueues->push(queue);

		//Assign the queue managers queue to "queue".
		queue = central->enqueuePendingQueues(_address);

		if(asynchronous) return PVariable(new Variable(VariableType::tVoid));

		for(int32_t i = 0; i < 120; i++)
		{
			if(queue && !queue->isEmpty()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
			else break;
			if(i == 119)
			{
				pendingBidCoSQueues->remove(BidCoSQueueType::GETVALUE, parameter->id, channel);
				return PVariable(new Variable(VariableType::tVoid));
			}
		}
		queue.reset();
		//Make sure queue gets deleted. This is a little nasty as it is a race condition
		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		return parameter->convertFromPacket(valuesCentral[channel][parameter->id].data, true);
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
	return Variable::createError(-32500, "Unknown application error.");
}

PParameterGroup BidCoSPeer::getParameterSet(int32_t channel, ParameterGroup::Type::Enum type)
{
	try
	{
		PFunction rpcFunction = _rpcDevice->functions.at(channel);
		PParameterGroup parameterGroup = rpcFunction->getParameterGroup(type);
		if(!parameterGroup || parameterGroup->parameters.empty())
		{
			GD::out.printDebug("Debug: Parameter set of type " + std::to_string(type) + " not found for channel " + std::to_string(channel));
			return PParameterGroup();
		}
		return parameterGroup;
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
	return PParameterGroup();
}

void BidCoSPeer::packetReceived(std::shared_ptr<BidCoSPacket> packet)
{
	try
	{
		if(!packet) return;
		if(_disposing) return;
		if(packet->senderAddress() != _address && (!hasTeam() || packet->senderAddress() != _team.address || packet->destinationAddress() == getCentral()->getAddress())) return;
		if(packet->destinationAddress() != getCentral()->getAddress() && aesEnabled())
		{
			if(packet->destinationAddress() == 0) _bl->out.printInfo("Info: Ignoring broadcast packet from peer " + std::to_string(_peerID) + ", because AES handshakes are enabled for this peer and AES handshakes are not possible for broadcast packets.");
			else _bl->out.printInfo("Info: Ignoring broadcast packet from peer " + std::to_string(_peerID) + " to other peer, because AES handshakes are enabled for this peer.");
			return;
		}
		if(!_rpcDevice) return;
		std::shared_ptr<HomeMaticCentral> central = std::dynamic_pointer_cast<HomeMaticCentral>(getCentral());
		if(!central) return;
		setLastPacketReceived();
		setRSSIDevice(packet->rssiDevice());
		serviceMessages->endUnreach();
		std::vector<FrameValues> frameValues;
		getValuesFromPacket(packet, frameValues);
		std::map<uint32_t, std::shared_ptr<std::vector<std::string>>> valueKeys;
		std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>> rpcValues;
		//Loop through all matching frames
		for(std::vector<FrameValues>::iterator a = frameValues.begin(); a != frameValues.end(); ++a)
		{
			PPacket frame;
			if(!a->frameID.empty()) frame = _rpcDevice->packetsById.at(a->frameID);

			//Check for low battery
			//If values is not empty, packet is valid
			if(_rpcDevice->hasBattery && !a->values.empty() && !packet->payload()->empty() && frame && hasLowbatBit(frame))
			{
				if(packet->payload()->at(0) & 0x80) serviceMessages->set("LOWBAT", true);
				else serviceMessages->set("LOWBAT", false);
			}

			for(std::map<std::string, FrameValue>::iterator i = a->values.begin(); i != a->values.end(); ++i)
			{
				for(std::list<uint32_t>::const_iterator j = a->paramsetChannels.begin(); j != a->paramsetChannels.end(); ++j)
				{
					if(packet->messageType() == 0x02 && aesEnabled(*j) && !packet->validAesAck()) continue;
					if(std::find(i->second.channels.begin(), i->second.channels.end(), *j) == i->second.channels.end()) continue;
					if(pendingBidCoSQueues->exists(BidCoSQueueType::PEER, i->first, *j)) continue; //Don't set queued values
					if(!valueKeys[*j] || !rpcValues[*j])
					{
						valueKeys[*j].reset(new std::vector<std::string>());
						rpcValues[*j].reset(new std::vector<PVariable>());
					}

					BaseLib::Systems::RPCConfigurationParameter* parameter = &valuesCentral[*j][i->first];
					parameter->data = i->second.value;
					if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
					else saveParameter(0, ParameterGroup::Type::Enum::variables, *j, i->first, parameter->data);
					if(_bl->debugLevel >= 4) GD::out.printInfo("Info: " + i->first + " on channel " + std::to_string(*j) + " of HomeMatic BidCoS peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + " was set to 0x" + BaseLib::HelperFunctions::getHexString(i->second.value) + ".");

					if(parameter->rpcParameter)
					{
						 //Process service messages
						if(parameter->rpcParameter->service && !i->second.value.empty())
						{
							if(parameter->rpcParameter->logical->type == ILogical::Type::Enum::tEnum)
							{
								serviceMessages->set(i->first, i->second.value.at(0), *j);
							}
							else if(parameter->rpcParameter->logical->type == ILogical::Type::Enum::tBoolean)
							{
								serviceMessages->set(i->first, (bool)i->second.value.at(0));
							}
						}

						valueKeys[*j]->push_back(i->first);
						rpcValues[*j]->push_back(parameter->rpcParameter->convertFromPacket(i->second.value, true));
					}
				}
			}

			if(isTeam() && !valueKeys.empty())
			{
				//Set SENDERADDRESS so that the we can identify the sending peer in our home automation software
				std::shared_ptr<BidCoSPeer> senderPeer(central->getPeer(packet->destinationAddress()));
				if(senderPeer)
				{
					//Check for low battery
					//If values is not empty, packet is valid
					if(senderPeer->_rpcDevice->hasBattery && !a->values.empty() && !packet->payload()->empty() && frame && hasLowbatBit(frame))
					{
						if(packet->payload()->at(0) & 0x80) senderPeer->serviceMessages->set("LOWBAT", true);
						else senderPeer->serviceMessages->set("LOWBAT", false);
					}
					for(std::list<uint32_t>::const_iterator i = a->paramsetChannels.begin(); i != a->paramsetChannels.end(); ++i)
					{
						PParameter rpcParameter(_rpcDevice->functions.at(*i)->getParameterGroup(a->parameterSetType)->parameters.at("SENDERADDRESS"));
						if(rpcParameter)
						{
							BaseLib::Systems::RPCConfigurationParameter* parameter = &valuesCentral[*i]["SENDERADDRESS"];
							rpcParameter->convertToPacket(senderPeer->getSerialNumber() + ":" + std::to_string(*i), parameter->data);
							if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
							else saveParameter(0, ParameterGroup::Type::Enum::variables, *i, "SENDERADDRESS", parameter->data);
							valueKeys[*i]->push_back("SENDERADDRESS");
							rpcValues[*i]->push_back(rpcParameter->convertFromPacket(parameter->data, true));
						}
					}
				}
			}

			//We have to do this in a seperate loop, because all parameters need to be set first
			for(std::map<std::string, FrameValue>::iterator i = a->values.begin(); i != a->values.end(); ++i)
			{
				for(std::list<uint32_t>::const_iterator j = a->paramsetChannels.begin(); j != a->paramsetChannels.end(); ++j)
				{
					if(packet->messageType() == 0x02 && aesEnabled(*j) && !packet->validAesAck()) continue;
					if(std::find(i->second.channels.begin(), i->second.channels.end(), *j) == i->second.channels.end()) continue;
					handleDominoEvent(_rpcDevice->functions.at(*j)->getParameterGroup(a->parameterSetType)->parameters.at(i->first), a->frameID, *j);
				}
			}
		}
		std::shared_ptr<BidCoSQueue> queue = central->getQueue(_address);
		if(queue && !queue->isEmpty() && queue->getQueueType() == BidCoSQueueType::GETVALUE)
		{
			//Handle get value response
			//Popping is not possible at this point, because otherwise another packet might be queued before the response is sent.
			std::shared_ptr<BidCoSPacket> queuePacket;
			if(queue->front()->getType() == QueueEntryType::PACKET)
			{
				queuePacket = queue->front()->getPacket();
				queue->pop();
			}
			if(!queue->isEmpty() && queue->front()->getType() == QueueEntryType::MESSAGE)
			{
				//Requeue getValue packet if message type doesn't match received packet
				if(!queue->front()->getMessage()->typeIsEqual(packet) && queuePacket) queue->pushFront(queuePacket);
			}
		}
		if(pendingBidCoSQueues && !pendingBidCoSQueues->empty() && packet->senderAddress() == _address)
		{
			if((getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeUp) && (packet->controlByte() & 2)) //Packet is wake me up packet
			{
				if(packet->controlByte() & 0x20) //Bidirectional?
				{
					std::vector<uint8_t> payload;
					payload.push_back(0x00);
					std::shared_ptr<BidCoSPacket> ok(new BidCoSPacket(packet->messageCounter(), 0x81, 0x02, central->getAddress(), _address, payload));
					central->sendPacket(_physicalInterface, ok);
					central->enqueuePendingQueues(_address);
				}
				else
				{
					std::shared_ptr<BidCoSQueue> queue(new BidCoSQueue(_physicalInterface, BidCoSQueueType::DEFAULT));
					queue->noSending = true;
					std::vector<uint8_t> payload;
					std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(packet->messageCounter(), 0xA1, 0x12, central->getAddress(), _address, payload));
					queue->push(configPacket);
					queue->push(central->getMessages()->find(0x02, std::vector<std::pair<uint32_t, int32_t>>()));

					central->enqueuePackets(_address, queue, true);
				}
			}
		}
		else if((packet->controlByte() & 0x20) && packet->destinationAddress() == central->getAddress())
		{
			central->sendOK(packet->messageCounter(), packet->senderAddress());
		}
		if(queue && !queue->isEmpty() && queue->getQueueType() == BidCoSQueueType::GETVALUE)
		{
			//Handle get value response
			//Do this after sendOK, otherwise ACK might be sent after another packet, if other queues are waiting
			if(!queue->isEmpty() && queue->front()->getType() == QueueEntryType::MESSAGE && queue->front()->getMessage()->typeIsEqual(packet)) queue->pop();
		}

		//if(!rpcValues.empty() && !resendPacket)
		if(!rpcValues.empty())
		{
			for(std::map<uint32_t, std::shared_ptr<std::vector<std::string>>>::const_iterator j = valueKeys.begin(); j != valueKeys.end(); ++j)
			{
				if(j->second->empty()) continue;
				std::string address(_serialNumber + ":" + std::to_string(j->first));
				raiseEvent(_peerID, j->first, j->second, rpcValues.at(j->first));
				raiseRPCEvent(_peerID, j->first, address, j->second, rpcValues.at(j->first));
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

PVariable BidCoSPeer::activateLinkParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, uint64_t remoteID, int32_t remoteChannel, bool longPress)
{
	try
	{
		if(remoteID == 0) remoteID = 0xFFFFFFFFFFFFFFFF; //Remote peer is central
		std::shared_ptr<BaseLib::Systems::BasicPeer> remotePeer = getPeer(channel, remoteID, remoteChannel);
		if(!remotePeer) return Variable::createError(-3, "Not paired to this peer.");
		if(remotePeer->isSender) return Variable::createError(-3, "Remote peer needs to be sender.");
		if(!HomeMaticCentral::isSwitch(getDeviceType()) && !HomeMaticCentral::isDimmer(getDeviceType())) return Variable::createError(-32400, "Method currently is only supported for dim and switch actuators.");

		std::vector<uint8_t> payload;
		payload.push_back(remotePeer->address >> 16);
		payload.push_back((remotePeer->address >> 8) & 0xFF);
		payload.push_back(remotePeer->address & 0xFF);
		payload.push_back(0x40);
		payload.push_back(longPress ? (0x40 | remoteChannel) : remoteChannel);
		payload.push_back(_generalCounter);
		setGeneralCounter(_generalCounter + 1);

		uint8_t controlByte = 0xA0;
		if(getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeOnRadio) controlByte |= 0x10;
		std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(_messageCounter, controlByte, 0x3E, getCentral()->getAddress(), _address, payload));
		setMessageCounter(_messageCounter + 1);

		std::shared_ptr<BidCoSQueue> queue(new BidCoSQueue(_physicalInterface, BidCoSQueueType::PEER));
		queue->noSending = true;
		queue->push(packet);
		std::shared_ptr<HomeMaticCentral> central = std::dynamic_pointer_cast<HomeMaticCentral>(getCentral());
		queue->push(central->getMessages()->find(0x02, std::vector<std::pair<uint32_t, int32_t>>()));
		pendingBidCoSQueues->push(queue);
		if((getRXModes() & HomegearDevice::ReceiveModes::Enum::always) || (getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeOnRadio))
		{
			central->enqueuePendingQueues(_address);
		}
		else
		{
			setValuePending(true);
			GD::out.printDebug("Debug: Packet was queued and will be sent with next wake me up packet.");
		}

		return PVariable(new Variable(VariableType::tVoid));
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
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable BidCoSPeer::getDeviceDescription(BaseLib::PRpcClientInfo clientInfo, int32_t channel, std::map<std::string, bool> fields)
{
	try
	{
		PVariable description(Peer::getDeviceDescription(clientInfo, channel, fields));
		if(description->errorStruct || description->structValue->empty()) return description;

		if(channel > -1)
		{
			PFunction rpcFunction = _rpcDevice->functions.at(channel);
			if(fields.empty() || fields.find("TEAM_CHANNELS") != fields.end())
			{
				if(!teamChannels.empty() && !rpcFunction->groupId.empty())
				{
					PVariable array(new Variable(VariableType::tArray));
					for(std::vector<std::pair<std::string, uint32_t>>::iterator j = teamChannels.begin(); j != teamChannels.end(); ++j)
					{
						array->arrayValue->push_back(PVariable(new Variable(j->first + ":" + std::to_string(j->second))));
					}
					description->structValue->insert(StructElement("TEAM_CHANNELS", array));
				}
			}

			if(!_team.serialNumber.empty() && rpcFunction->hasGroup)
			{
				if(fields.empty() || fields.find("TEAM") != fields.end()) description->structValue->insert(StructElement("TEAM", PVariable(new Variable(_team.serialNumber))));
				if(fields.empty() || fields.find("TEAM_ID") != fields.end()) description->structValue->insert(StructElement("TEAM_ID", PVariable(new Variable((int32_t)getTeamRemoteID()))));
				if(fields.empty() || fields.find("TEAM_CHANNEL") != fields.end()) description->structValue->insert(StructElement("TEAM_CHANNEL", PVariable(new Variable((int32_t)_team.channel))));

				if(fields.empty() || fields.find("TEAM_TAG") != fields.end()) description->structValue->insert(StructElement("TEAM_TAG", PVariable(new Variable(rpcFunction->groupId))));
			}
			else if(!_serialNumber.empty() && _serialNumber[0] == '*' && !rpcFunction->groupId.empty())
			{
				if(fields.empty() || fields.find("TEAM_TAG") != fields.end()) description->structValue->insert(StructElement("TEAM_TAG", PVariable(new Variable(rpcFunction->groupId))));
			}
		}
		return description;
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
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable BidCoSPeer::getDeviceInfo(BaseLib::PRpcClientInfo clientInfo, std::map<std::string, bool> fields)
{
	try
	{
		PVariable info(Peer::getDeviceInfo(clientInfo, fields));
		if(info->errorStruct) return info;

		if(fields.empty() || fields.find("INTERFACE") != fields.end()) info->structValue->insert(StructElement("INTERFACE", PVariable(new Variable(_physicalInterface->getID()))));

		return info;
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
    return PVariable();
}

PVariable BidCoSPeer::getParamsetDescription(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel");
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(!parameterGroup) return Variable::createError(-3, "Unknown parameter set");
		if(type == ParameterGroup::Type::link && remoteID > 0)
		{
			std::shared_ptr<BaseLib::Systems::BasicPeer> remotePeer = getPeer(channel, remoteID, remoteChannel);
			if(!remotePeer) return Variable::createError(-2, "Unknown remote peer.");
		}

		return Peer::getParamsetDescription(clientInfo, parameterGroup);
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
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable BidCoSPeer::putParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool onlyPushing)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		if(remoteChannel < 0) remoteChannel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel");
		if(type == ParameterGroup::Type::none) type = ParameterGroup::Type::link;
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(!parameterGroup) return Variable::createError(-3, "Unknown parameter set");
		if(variables->structValue->empty()) return PVariable(new Variable(VariableType::tVoid));

		_pingThreadMutex.lock();
		_lastPing = BaseLib::HelperFunctions::getTime(); //No ping now
		if(_pingThread.joinable()) _pingThread.join();
		_pingThreadMutex.unlock();

		if(type == ParameterGroup::Type::Enum::config)
		{
			std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>>::iterator configIterator = configCentral.find(channel);
			if(configIterator == configCentral.end()) return Variable::createError(-3, "Unknown parameter set");

			bool aesActivated = false;
			std::map<int32_t, std::map<int32_t, std::vector<uint8_t>>> changedParameters;
			//allParameters is necessary to temporarily store all values. It is used to set changedParameters.
			//This is necessary when there are multiple variables per index and not all of them are changed.
			std::map<int32_t, std::map<int32_t, std::vector<uint8_t>>> allParameters;

			// {{{ Add missing parameters at the same index
				std::map<std::string, PParameter> parametersToAdd;
				for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
				{
					if(configIterator->second.find(i->first) == configIterator->second.end()) continue;
					BaseLib::Systems::RPCConfigurationParameter* parameter = &configIterator->second[i->first];
					if(!parameter->rpcParameter) continue;
					std::vector<PParameter> parameters;
					int32_t size = (parameter->rpcParameter->physical->size) < 1 ? 1 : (int32_t)parameter->rpcParameter->physical->size;
					if(parameter->rpcParameter->physical->size > 1.0 && std::fmod(parameter->rpcParameter->physical->size, 1) != 0) size += 1;
					parameterGroup->getIndices((uint32_t)parameter->rpcParameter->physical->index, (uint32_t)parameter->rpcParameter->physical->index + (size - 1), parameter->rpcParameter->physical->list, parameters);
					for(std::vector<PParameter>::iterator j = parameters.begin(); j != parameters.end(); ++j)
					{
						if(variables->structValue->find((*j)->id) == variables->structValue->end() && parametersToAdd.find((*j)->id) == parametersToAdd.end()) parametersToAdd[(*j)->id] = *j;
					}
				}
				for(std::map<std::string, PParameter>::iterator i = parametersToAdd.begin(); i != parametersToAdd.end(); ++i)
				{
					if(configIterator->second.find(i->first) == configIterator->second.end()) continue;
					BaseLib::Systems::RPCConfigurationParameter* parameter = &configIterator->second[i->first];
					variables->structValue->insert(std::pair<std::string, PVariable>(i->first, i->second->convertFromPacket(parameter->data)));
				}
			// }}}

			for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
			{
				if(i->first.empty() || !i->second) continue;
				std::vector<uint8_t> value;
				if(configIterator->second.find(i->first) == configIterator->second.end()) continue;
				BaseLib::Systems::RPCConfigurationParameter* parameter = &configIterator->second[i->first];
				if(!parameter->rpcParameter) continue;
				if(i->first == "AES_ACTIVE")
				{
					if(i->second->booleanValue && !aesEnabled())
					{
						GD::out.printDebug("Debug: AES is enabled now for peer " + std::to_string(_peerID) + ".");
						aesActivated = true;
					}
				}
				parameter->rpcParameter->convertToPacket(i->second, value);
				std::vector<uint8_t> shiftedValue = value;
				parameter->rpcParameter->adjustBitPosition(shiftedValue);
				int32_t intIndex = (int32_t)parameter->rpcParameter->physical->index;
				int32_t list = parameter->rpcParameter->physical->list;
				if(list == -1) list = 0;
				if(allParameters[list].find(intIndex) == allParameters[list].end()) allParameters[list][intIndex] = shiftedValue;
				else
				{
					uint32_t index = 0;
					for(std::vector<uint8_t>::iterator j = shiftedValue.begin(); j != shiftedValue.end(); ++j)
					{
						if(index >= allParameters[list][intIndex].size()) allParameters[list][intIndex].push_back(0);
						allParameters[list][intIndex].at(index) |= *j;
						index++;
					}
				}
				parameter->data = value;
				if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
				else saveParameter(0, ParameterGroup::Type::Enum::config, channel, i->first, parameter->data);
				if(peerInfoPacketsEnabled && i->first == "AES_ACTIVE" && !aesActivated) _physicalInterface->setAES(getPeerInfo(), channel);
				GD::out.printInfo("Info: Parameter " + i->first + " of peer " + std::to_string(_peerID) + " and channel " + std::to_string(channel) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(parameter->data) + ".");
				//Only send to device when parameter is of type config;
				if(parameter->rpcParameter->physical->operationType != IPhysical::OperationType::Enum::config && parameter->rpcParameter->physical->operationType != IPhysical::OperationType::Enum::configString) continue;
				//Don't active AES, when aesAlways is true as it is activated already
				if(i->first == "AES_ACTIVE" && functionIterator->second->forceEncryption) continue;
				changedParameters[list][intIndex] = allParameters[list][intIndex];
			}

			if(changedParameters.empty() || changedParameters.begin()->second.empty())
			{
				if(aesActivated) checkAESKey(onlyPushing); //If "aesAlways" is true, changedParameters might be empty
				return PVariable(new Variable(VariableType::tVoid));
			}

			std::shared_ptr<BidCoSQueue> queue(new BidCoSQueue(_physicalInterface, BidCoSQueueType::CONFIG));
			queue->noSending = true;
			std::vector<uint8_t> payload;
			std::shared_ptr<HomeMaticCentral> central = std::dynamic_pointer_cast<HomeMaticCentral>(getCentral());
			bool firstPacket = true;

			for(std::map<int32_t, std::map<int32_t, std::vector<uint8_t>>>::iterator i = changedParameters.begin(); i != changedParameters.end(); ++i)
			{
				//CONFIG_START
				payload.push_back(channel);
				payload.push_back(0x05);
				payload.push_back(0);
				payload.push_back(0);
				payload.push_back(0);
				payload.push_back(0);
				payload.push_back(i->first);
				uint8_t controlByte = 0xA0;
				//Only send first packet as burst packet
				if(firstPacket && (getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeOnRadio)) controlByte |= 0x10;
				firstPacket = false;
				std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(_messageCounter, controlByte, 0x01, central->getAddress(), _address, payload));
				queue->push(configPacket);
				queue->push(central->getMessages()->find(0x02, std::vector<std::pair<uint32_t, int32_t>>()));
				payload.clear();
				setMessageCounter(_messageCounter + 1);

				//CONFIG_WRITE_INDEX
				payload.push_back(channel);
				payload.push_back(0x08);
				for(std::map<int32_t, std::vector<uint8_t>>::iterator j = i->second.begin(); j != i->second.end(); ++j)
				{
					int32_t index = j->first;
					for(std::vector<uint8_t>::iterator k = j->second.begin(); k != j->second.end(); ++k)
					{
						payload.push_back(index);
						payload.push_back(*k);
						index++;
						if(payload.size() == 16)
						{
							configPacket = std::shared_ptr<BidCoSPacket>(new BidCoSPacket(_messageCounter, 0xA0, 0x01, central->getAddress(), _address, payload));
							queue->push(configPacket);
							queue->push(central->getMessages()->find(0x02, std::vector<std::pair<uint32_t, int32_t>>()));
							payload.clear();
							setMessageCounter(_messageCounter + 1);
							payload.push_back(channel);
							payload.push_back(0x08);
						}
					}
				}
				if(payload.size() > 2)
				{
					configPacket = std::shared_ptr<BidCoSPacket>(new BidCoSPacket(_messageCounter, 0xA0, 0x01, central->getAddress(), _address, payload));
					queue->push(configPacket);
					queue->push(central->getMessages()->find(0x02, std::vector<std::pair<uint32_t, int32_t>>()));
					payload.clear();
					setMessageCounter(_messageCounter + 1);
				}
				else payload.clear();

				//END_CONFIG
				payload.push_back(channel);
				payload.push_back(0x06);
				configPacket = std::shared_ptr<BidCoSPacket>(new BidCoSPacket(_messageCounter, 0xA0, 0x01, central->getAddress(), _address, payload));
				queue->push(configPacket);
				queue->push(central->getMessages()->find(0x02, std::vector<std::pair<uint32_t, int32_t>>()));
				payload.clear();
				setMessageCounter(_messageCounter + 1);
			}

			pendingBidCoSQueues->push(queue);
			if(aesActivated) checkAESKey(onlyPushing);
			serviceMessages->setConfigPending(true);
			//if((getRXModes() & HomegearDevice::ReceiveModes::Enum::always) || (getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeOnRadio))
			//{
				if(!onlyPushing) central->enqueuePendingQueues(_address);
			//}
			//else
			//{
				//GD::out.printDebug("Debug: Packet was queued and will be sent with next wake me up packet.");
			//}
			raiseRPCUpdateDevice(_peerID, channel, _serialNumber + ":" + std::to_string(channel), 0);
		}
		else if(type == ParameterGroup::Type::Enum::variables)
		{
			for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
			{
				if(i->first.empty() || !i->second) continue;
				setValue(clientInfo, channel, i->first, i->second, false);
			}
		}
		else if(type == ParameterGroup::Type::Enum::link)
		{
			std::unordered_map<uint32_t, std::unordered_map<int32_t, std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>>>>::iterator configIterator = linksCentral.find(channel);
			if(configIterator == linksCentral.end()) return Variable::createError(-3, "Unknown parameter set");

			std::shared_ptr<BaseLib::Systems::BasicPeer> remotePeer;
			if(remoteID == 0) remoteID = 0xFFFFFFFFFFFFFFFF; //Remote peer is central
			remotePeer = getPeer(channel, remoteID, remoteChannel);
			if(!remotePeer) return Variable::createError(-3, "Not paired to this peer.");
			if(configIterator->second.find(remotePeer->address) == configIterator->second.end()) Variable::createError(-3, "Unknown parameter set.");
			if(configIterator->second[_address].find(remotePeer->channel) == configIterator->second[_address].end()) Variable::createError(-3, "Unknown parameter set.");

			std::map<int32_t, std::map<int32_t, std::vector<uint8_t>>> changedParameters;
			//allParameters is necessary to temporarily store all values. It is used to set changedParameters.
			//This is necessary when there are multiple variables per index and not all of them are changed.
			std::map<int32_t, std::map<int32_t, std::vector<uint8_t>>> allParameters;

			// {{{ Add missing parameters at the same index
				std::map<std::string, PParameter> parametersToAdd;
				for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
				{
					if(configIterator->second[remotePeer->address][remotePeer->channel].find(i->first) == configIterator->second[remotePeer->address][remotePeer->channel].end()) continue;
					BaseLib::Systems::RPCConfigurationParameter* parameter = &configIterator->second[remotePeer->address][remotePeer->channel][i->first];
					if(!parameter->rpcParameter) continue;
					std::vector<PParameter> parameters;
					int32_t size = (parameter->rpcParameter->physical->size) < 1 ? 1 : (int32_t)parameter->rpcParameter->physical->size;
					if(parameter->rpcParameter->physical->size > 1.0 && std::fmod(parameter->rpcParameter->physical->size, 1) != 0) size += 1;
					parameterGroup->getIndices((uint32_t)parameter->rpcParameter->physical->index, (uint32_t)parameter->rpcParameter->physical->index + (size - 1), parameter->rpcParameter->physical->list, parameters);
					for(std::vector<PParameter>::iterator j = parameters.begin(); j != parameters.end(); ++j)
					{
						if(variables->structValue->find((*j)->id) == variables->structValue->end() && parametersToAdd.find((*j)->id) == parametersToAdd.end()) parametersToAdd[(*j)->id] = *j;
					}
				}
				for(std::map<std::string, PParameter>::iterator i = parametersToAdd.begin(); i != parametersToAdd.end(); ++i)
				{
					if(configIterator->second[remotePeer->address][remotePeer->channel].find(i->first) == configIterator->second[remotePeer->address][remotePeer->channel].end()) continue;
					BaseLib::Systems::RPCConfigurationParameter* parameter = &configIterator->second[remotePeer->address][remotePeer->channel][i->first];
					variables->structValue->insert(std::pair<std::string, PVariable>(i->first, i->second->convertFromPacket(parameter->data)));
				}
			// }}}

			for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
			{
				if(i->first.empty() || !i->second) continue;
				std::vector<uint8_t> value;
				if(configIterator->second[remotePeer->address][remotePeer->channel].find(i->first) == configIterator->second[remotePeer->address][remotePeer->channel].end()) continue;
				BaseLib::Systems::RPCConfigurationParameter* parameter = &configIterator->second[remotePeer->address][remotePeer->channel][i->first];
				if(!parameter->rpcParameter) continue;
				parameter->rpcParameter->convertToPacket(i->second, value);
				std::vector<uint8_t> shiftedValue = value;
				parameter->rpcParameter->adjustBitPosition(shiftedValue);
				int32_t intIndex = (int32_t)parameter->rpcParameter->physical->index;
				int32_t list = parameter->rpcParameter->physical->list;
				if(list == -1) list = 0;
				if(allParameters[list].find(intIndex) == allParameters[list].end()) allParameters[list][intIndex] = shiftedValue;
				else
				{
					uint32_t index = 0;
					for(std::vector<uint8_t>::iterator j = shiftedValue.begin(); j != shiftedValue.end(); ++j)
					{
						if(index >= allParameters[list][intIndex].size()) allParameters[list][intIndex].push_back(0);
						allParameters[list][intIndex].at(index) |= *j;
						index++;
					}
				}
				parameter->data = value;
				if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
				else saveParameter(0, ParameterGroup::Type::Enum::link, channel, i->first, parameter->data, remotePeer->address, remotePeer->channel);
				GD::out.printInfo("Info: Parameter " + i->first + " of peer " + std::to_string(_peerID) + " and channel " + std::to_string(channel) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(allParameters[list][intIndex]) + ".");
				//Only send to device when parameter is of type config
				if(parameter->rpcParameter->physical->operationType != IPhysical::OperationType::Enum::config && parameter->rpcParameter->physical->operationType != IPhysical::OperationType::Enum::configString) continue;
				changedParameters[list][intIndex] = allParameters[list][intIndex];
			}

			if(changedParameters.empty() || changedParameters.begin()->second.empty()) return PVariable(new Variable(VariableType::tVoid));

			std::shared_ptr<BidCoSQueue> queue(new BidCoSQueue(_physicalInterface, BidCoSQueueType::CONFIG));
			queue->noSending = true;
			std::vector<uint8_t> payload;
			std::shared_ptr<HomeMaticCentral> central = std::dynamic_pointer_cast<HomeMaticCentral>(getCentral());
			bool firstPacket = true;

			for(std::map<int32_t, std::map<int32_t, std::vector<uint8_t>>>::iterator i = changedParameters.begin(); i != changedParameters.end(); ++i)
			{
				//CONFIG_START
				payload.push_back(channel);
				payload.push_back(0x05);
				payload.push_back(remotePeer->address >> 16);
				payload.push_back((remotePeer->address >> 8) & 0xFF);
				payload.push_back(remotePeer->address & 0xFF);
				payload.push_back(remotePeer->channel);
				payload.push_back(i->first);
				uint8_t controlByte = 0xA0;
				//Only send first packet as burst packet
				if(firstPacket && (getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeOnRadio)) controlByte |= 0x10;
				firstPacket = false;
				std::shared_ptr<BidCoSPacket> configPacket(new BidCoSPacket(_messageCounter, controlByte, 0x01, central->getAddress(), _address, payload));
				queue->push(configPacket);
				queue->push(central->getMessages()->find(0x02, std::vector<std::pair<uint32_t, int32_t>>()));
				payload.clear();
				setMessageCounter(_messageCounter + 1);

				//CONFIG_WRITE_INDEX
				payload.push_back(channel);
				payload.push_back(0x08);
				for(std::map<int32_t, std::vector<uint8_t>>::iterator j = i->second.begin(); j != i->second.end(); ++j)
				{
					int32_t index = j->first;
					for(std::vector<uint8_t>::iterator k = j->second.begin(); k != j->second.end(); ++k)
					{
						payload.push_back(index);
						payload.push_back(*k);
						index++;
						if(payload.size() == 16)
						{
							configPacket = std::shared_ptr<BidCoSPacket>(new BidCoSPacket(_messageCounter, 0xA0, 0x01, central->getAddress(), _address, payload));
							queue->push(configPacket);
							queue->push(central->getMessages()->find(0x02, std::vector<std::pair<uint32_t, int32_t>>()));
							payload.clear();
							setMessageCounter(_messageCounter + 1);
							payload.push_back(channel);
							payload.push_back(0x08);
						}
					}
				}
				if(payload.size() > 2)
				{
					configPacket = std::shared_ptr<BidCoSPacket>(new BidCoSPacket(_messageCounter, 0xA0, 0x01, central->getAddress(), _address, payload));
					queue->push(configPacket);
					queue->push(central->getMessages()->find(0x02, std::vector<std::pair<uint32_t, int32_t>>()));
					payload.clear();
					setMessageCounter(_messageCounter + 1);
				}
				else payload.clear();

				//END_CONFIG
				payload.push_back(channel);
				payload.push_back(0x06);
				configPacket = std::shared_ptr<BidCoSPacket>(new BidCoSPacket(_messageCounter, 0xA0, 0x01, central->getAddress(), _address, payload));
				queue->push(configPacket);
				queue->push(central->getMessages()->find(0x02, std::vector<std::pair<uint32_t, int32_t>>()));
				payload.clear();
				setMessageCounter(_messageCounter + 1);
			}

			pendingBidCoSQueues->push(queue);
			serviceMessages->setConfigPending(true);
			//if((getRXModes() & HomegearDevice::ReceiveModes::Enum::always) || (getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeOnRadio))
			//{
				if(!onlyPushing) central->enqueuePendingQueues(_address);
			//}
			//else
			//{
				//GD::out.printDebug("Debug: Packet was queued and will be sent with next wake me up packet.");
			//}
			raiseRPCUpdateDevice(_peerID, channel, _serialNumber + ":" + std::to_string(channel), 0);
		}
		return PVariable(new Variable(VariableType::tVoid));
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
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable BidCoSPeer::getParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		if(remoteChannel < 0) remoteChannel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel");
		if(type == ParameterGroup::Type::none) type = ParameterGroup::Type::link;
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(!parameterGroup) return Variable::createError(-3, "Unknown parameter set");
		PVariable variables(new Variable(VariableType::tStruct));

		for(Parameters::iterator i = parameterGroup->parameters.begin(); i != parameterGroup->parameters.end(); ++i)
		{
			if(i->second->id.empty() || !i->second->visible) continue;
			if(!i->second->visible && !i->second->service && !i->second->internal && !i->second->transform)
			{
				GD::out.printDebug("Debug: Omitting parameter " + i->second->id + " because of it's ui flag.");
				continue;
			}
			PVariable element;
			if(type == ParameterGroup::Type::Enum::variables)
			{
				if(!i->second->readable) continue;
				if(valuesCentral.find(channel) == valuesCentral.end()) continue;
				if(valuesCentral[channel].find(i->second->id) == valuesCentral[channel].end()) continue;
				element = i->second->convertFromPacket(valuesCentral[channel][i->second->id].data);
			}
			else if(type == ParameterGroup::Type::Enum::config)
			{
				if(configCentral.find(channel) == configCentral.end()) continue;
				if(configCentral[channel].find(i->second->id) == configCentral[channel].end()) continue;
				element = i->second->convertFromPacket(configCentral[channel][i->second->id].data);
			}
			else if(type == ParameterGroup::Type::Enum::link)
			{
				std::shared_ptr<BaseLib::Systems::BasicPeer> remotePeer;
				if(remoteID == 0) remoteID = 0xFFFFFFFFFFFFFFFF; //Remote peer is central
				remotePeer = getPeer(channel, remoteID, remoteChannel);
				if(!remotePeer) return Variable::createError(-3, "Not paired to this peer.");
				if(linksCentral.find(channel) == linksCentral.end()) continue;
				if(linksCentral[channel][remotePeer->address][remotePeer->channel].find(i->second->id) == linksCentral[channel][remotePeer->address][remotePeer->channel].end()) continue;
				if(remotePeer->channel != remoteChannel) continue;
				element = i->second->convertFromPacket(linksCentral[channel][remotePeer->address][remotePeer->channel][i->second->id].data);
			}

			if(!element) continue;
			if(element->type == VariableType::tVoid) continue;
			variables->structValue->insert(StructElement(i->second->id, element));
		}
		return variables;
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
    return Variable::createError(-32500, "Unknown application error.");
}

bool BidCoSPeer::setHomegearValue(uint32_t channel, std::string valueKey, PVariable value)
{
	try
	{
		if(_deviceType.type() == (uint32_t)DeviceType::HMCCVD && valueKey == "VALVE_STATE")
		{
			_peersMutex.lock();
			std::unordered_map<int32_t, std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>>::iterator peersIterator = _peers.find(1);
			if(peersIterator == _peers.end() || peersIterator->second.empty() || !peersIterator->second.at(0)->isVirtual)
			{
				_peersMutex.unlock();
				return false;
			}
			std::shared_ptr<BaseLib::Systems::BasicPeer> remotePeer = peersIterator->second.at(0);
			_peersMutex.unlock();
			if(!remotePeer->peer)
			{
				remotePeer->peer = getCentral()->getPeer(remotePeer->id);
				if(!remotePeer->peer || remotePeer->peer->getDeviceType().type() != (uint32_t)DeviceType::HMCCTC) return false;
			}
			if(remotePeer->peer)
			{
				if(remotePeer->peer->getDeviceType().type() != (uint32_t)DeviceType::HMCCTC) return false;
				std::shared_ptr<HmCcTc> tc(std::dynamic_pointer_cast<HmCcTc>(remotePeer->peer));
				if(!tc) return false;
				tc->setValveState(value->integerValue);
				PParameter rpcParameter = valuesCentral[channel][valueKey].rpcParameter;
				if(!rpcParameter) return false;
				BaseLib::Systems::RPCConfigurationParameter* parameter = &valuesCentral[channel][valueKey];
				rpcParameter->convertToPacket(value, parameter->data);
				if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
				else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, parameter->data);
				GD::out.printInfo("Info: Setting valve state of HM-CC-VD with id " + std::to_string(_peerID) + " to " + std::to_string(value->integerValue / 2) + "%.");
				return true;
			}
		}
		else if(_deviceType.type() == (uint32_t)DeviceType::HMSECSD)
		{
			if(valueKey == "STATE")
			{
				PParameter rpcParameter = valuesCentral[channel][valueKey].rpcParameter;
				if(!rpcParameter) return false;
				BaseLib::Systems::RPCConfigurationParameter* parameter = &valuesCentral[channel][valueKey];
				rpcParameter->convertToPacket(value, parameter->data);
				if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
				else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, parameter->data);

				std::shared_ptr<HomeMaticCentral> central = std::dynamic_pointer_cast<HomeMaticCentral>(getCentral());
				std::shared_ptr<BidCoSPeer> associatedPeer = central->getPeer(_address);
				if(!associatedPeer)
				{
					GD::out.printError("Error: Could not handle \"STATE\", because the main team peer is not paired to this central.");
					return false;
				}
				if(associatedPeer->getTeamData().empty()) associatedPeer->getTeamData().push_back(0);
				std::vector<uint8_t> payload;
				payload.push_back(0x01);
				payload.push_back(associatedPeer->getTeamData().at(0));
				associatedPeer->getTeamData().at(0)++;
				associatedPeer->saveVariable(11, associatedPeer->getTeamData());
				if(value->booleanValue) payload.push_back(0xC8);
				else payload.push_back(0x01);
				std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(central->messageCounter()->at(0), 0x94, 0x41, _address, central->getAddress(), payload));
				central->messageCounter()->at(0)++;
				central->sendPacketMultipleTimes(_physicalInterface, packet, _address, 6, 1000, true);
				return true;
			}
			else if(valueKey == "INSTALL_TEST")
			{
				PParameter rpcParameter = valuesCentral[channel][valueKey].rpcParameter;
				if(!rpcParameter) return false;
				BaseLib::Systems::RPCConfigurationParameter* parameter = &valuesCentral[channel][valueKey];
				rpcParameter->convertToPacket(value, parameter->data);
				if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
				else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, parameter->data);

				std::shared_ptr<HomeMaticCentral> central = std::dynamic_pointer_cast<HomeMaticCentral>(getCentral());
				std::shared_ptr<BidCoSPeer> associatedPeer = central->getPeer(_address);
				if(!associatedPeer)
				{
					GD::out.printError("Error: Could not handle \"INSTALL_TEST\", because the main team peer is not paired to this central.");
					return false;
				}
				if(associatedPeer->getTeamData().empty()) associatedPeer->getTeamData().push_back(0);
				std::vector<uint8_t> payload;
				payload.push_back(0x00);
				payload.push_back(associatedPeer->getTeamData().at(0));
				associatedPeer->getTeamData().at(0)++;
				associatedPeer->saveVariable(11, associatedPeer->getTeamData());
				std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(central->messageCounter()->at(0), 0x94, 0x40, _address, central->getAddress(), payload));
				central->messageCounter()->at(0)++;
				central->sendPacketMultipleTimes(_physicalInterface, packet, _address, 6, 600, true);
				return true;
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
	return false;
}

void BidCoSPeer::addVariableToResetCallback(std::shared_ptr<CallbackFunctionParameter> parameters)
{
	try
	{
		if(parameters->integers.size() != 3) return;
		if(parameters->strings.size() != 1) return;
		GD::out.printMessage("addVariableToResetCallback invoked for parameter " + parameters->strings.at(0) + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ".", 5);
		GD::out.printInfo("Parameter " + parameters->strings.at(0) + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + " will be reset at " + BaseLib::HelperFunctions::getTimeString(parameters->integers.at(2)) + ".");
		std::shared_ptr<VariableToReset> variable(new VariableToReset);
		variable->channel = parameters->integers.at(0);
		int32_t integerValue = parameters->integers.at(1);
		_bl->hf.memcpyBigEndian(variable->data, integerValue);
		variable->resetTime = parameters->integers.at(2);
		variable->key = parameters->strings.at(0);
		_variablesToResetMutex.lock();
		_variablesToReset.push_back(variable);
		_variablesToResetMutex.unlock();
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

PVariable BidCoSPeer::setInterface(BaseLib::PRpcClientInfo clientInfo, std::string interfaceID)
{
	try
	{
		if(!interfaceID.empty() && GD::physicalInterfaces.find(interfaceID) == GD::physicalInterfaces.end())
		{
			return Variable::createError(-5, "Unknown physical interface.");
		}
		std::shared_ptr<IBidCoSInterface> interface = interfaceID.empty() ? GD::defaultPhysicalInterface : GD::physicalInterfaces.at(interfaceID);
		if(configCentral.find(0) != configCentral.end() && configCentral.at(0).find("ROAMING") != configCentral.at(0).end() && configCentral.at(0).at("ROAMING").data.size() > 0 && configCentral.at(0).at("ROAMING").data.at(0) == 1) return Variable::createError(-104, "Can't set physical interface, because ROAMING is enabled. Please disable ROAMING to manually select the interface.");
		setPhysicalInterfaceID(interfaceID);
		return PVariable(new Variable(VariableType::tVoid));
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
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable BidCoSPeer::setValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, PVariable value, bool wait)
{
	try
	{
		Peer::setValue(clientInfo, channel, valueKey, value, wait); //Ignore result, otherwise setHomegerValue might not be executed
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(valueKey.empty()) return Variable::createError(-5, "Value key is empty.");
		if(channel == 0 && serviceMessages->set(valueKey, value->booleanValue)) return PVariable(new Variable(VariableType::tVoid));
		if(valuesCentral.find(channel) == valuesCentral.end()) return Variable::createError(-2, "Unknown channel.");
		if(setHomegearValue(channel, valueKey, value)) return PVariable(new Variable(VariableType::tVoid));
		if(valuesCentral[channel].find(valueKey) == valuesCentral[channel].end()) return Variable::createError(-5, "Unknown parameter.");
		PParameter rpcParameter = valuesCentral[channel][valueKey].rpcParameter;
		if(!rpcParameter) return Variable::createError(-5, "Unknown parameter.");
		if(rpcParameter->logical->type == ILogical::Type::tAction && !value->booleanValue) return Variable::createError(-5, "Parameter of type action cannot be set to \"false\".");
		BaseLib::Systems::RPCConfigurationParameter* parameter = &valuesCentral[channel][valueKey];
		std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string>());
		std::shared_ptr<std::vector<PVariable>> values(new std::vector<PVariable>());
		if(rpcParameter->readable)
		{
			valueKeys->push_back(valueKey);
			values->push_back(value);
		}
		if(rpcParameter->physical->operationType == IPhysical::OperationType::Enum::store)
		{
			rpcParameter->convertToPacket(value, parameter->data);
			if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
			else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, parameter->data);
			if(!valueKeys->empty())
			{
				raiseEvent(_peerID, channel, valueKeys, values);
				raiseRPCEvent(_peerID, channel, _serialNumber + ":" + std::to_string(channel), valueKeys, values);
			}
			return PVariable(new Variable(VariableType::tVoid));
		}
		else if(rpcParameter->physical->operationType != IPhysical::OperationType::Enum::command) return Variable::createError(-6, "Parameter is not settable.");
		//_resendCounter[valueKey] = 0;
		PToggle toggleCast;
		if(!rpcParameter->casts.empty()) toggleCast = std::dynamic_pointer_cast<Toggle>(rpcParameter->casts.at(0));
		if(toggleCast)
		{
			//Handle toggle parameter
			if(toggleCast->parameter.empty()) return Variable::createError(-6, "No toggle parameter specified (parameter attribute value is empty).");
			if(valuesCentral[channel].find(toggleCast->parameter) == valuesCentral[channel].end()) return Variable::createError(-5, "Toggle parameter not found.");
			BaseLib::Systems::RPCConfigurationParameter* toggleParam = &valuesCentral[channel][toggleCast->parameter];
			PVariable toggleValue;
			if(toggleParam->rpcParameter->logical->type == ILogical::Type::Enum::tBoolean)
			{
				toggleValue = toggleParam->rpcParameter->convertFromPacket(toggleParam->data);
				toggleValue->booleanValue = !toggleValue->booleanValue;
			}
			else if(toggleParam->rpcParameter->logical->type == ILogical::Type::Enum::tInteger ||
					toggleParam->rpcParameter->logical->type == ILogical::Type::Enum::tFloat)
			{
				int32_t currentToggleValue = (int32_t)toggleParam->data.at(0);
				std::vector<uint8_t> temp({0});
				if(currentToggleValue != toggleCast->on) temp.at(0) = toggleCast->on;
				else temp.at(0) = toggleCast->off;
				toggleValue = toggleParam->rpcParameter->convertFromPacket(temp);
			}
			else return Variable::createError(-6, "Toggle parameter has to be of type boolean, float or integer.");
			return setValue(clientInfo, channel, toggleCast->parameter, toggleValue, wait);
		}
		if(rpcParameter->setPackets.empty()) return Variable::createError(-6, "parameter is read only");
		std::string setRequest = rpcParameter->setPackets.front()->id;
		PacketsById::iterator packetIterator = _rpcDevice->packetsById.find(setRequest);
		if(packetIterator == _rpcDevice->packetsById.end()) return Variable::createError(-6, "No frame was found for parameter " + valueKey);
		PPacket frame = packetIterator->second;
		rpcParameter->convertToPacket(value, parameter->data);
		if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
		else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, parameter->data);
		if(_bl->debugLevel > 4) GD::out.printDebug("Debug: " + valueKey + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(channel) + " was set to " + BaseLib::HelperFunctions::getHexString(parameter->data) + ".");

		pendingBidCoSQueues->remove(BidCoSQueueType::PEER, valueKey, channel);
		std::shared_ptr<BidCoSQueue> queue(new BidCoSQueue(_physicalInterface, BidCoSQueueType::PEER));
		queue->noSending = true;
		queue->parameterName = valueKey;
		queue->channel = channel;

		std::vector<uint8_t> payload;
		if(frame->subtype > -1 && frame->subtypeIndex >= 9)
		{
			while((signed)payload.size() - 1 < frame->subtypeIndex - 9) payload.push_back(0);
			payload.at(frame->subtypeIndex - 9) = (uint8_t)frame->subtype;
		}
		if(frame->channelIndex >= 9)
		{
			while((signed)payload.size() - 1 < frame->channelIndex - 9) payload.push_back(0);
			payload.at(frame->channelIndex - 9) = (uint8_t)channel;
		}
		std::shared_ptr<HomeMaticCentral> central = std::dynamic_pointer_cast<HomeMaticCentral>(getCentral());
		uint8_t controlByte = 0xA0;
		if(getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeOnRadio) controlByte |= 0x10;
		std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(_messageCounter, controlByte, (uint8_t)frame->type, getCentral()->getAddress(), _address, payload));

		for(BinaryPayloads::iterator i = frame->binaryPayloads.begin(); i != frame->binaryPayloads.end(); ++i)
		{
			if((*i)->constValueInteger > -1)
			{
				std::vector<uint8_t> data;
				_bl->hf.memcpyBigEndian(data, (*i)->constValueInteger);
				packet->setPosition((*i)->index, (*i)->size, data);
				continue;
			}
			else if(!(*i)->constValueString.empty())
			{
				std::vector<uint8_t> data;
				data.insert(data.begin(), (*i)->constValueString.begin(), (*i)->constValueString.end());
				packet->setPosition((*i)->index, (*i)->size, data);
				continue;
			}
			BaseLib::Systems::RPCConfigurationParameter* additionalParameter = nullptr;
			//We can't just search for param, because it is ambiguous (see for example LEVEL for HM-CC-TC.
			if((*i)->parameterId == "ON_TIME" && valuesCentral[channel].find((*i)->parameterId) != valuesCentral[channel].end())
			{
				additionalParameter = &valuesCentral[channel][(*i)->parameterId];
				int32_t intValue = 0;
				_bl->hf.memcpyBigEndian(intValue, additionalParameter->data);
				if(!(*i)->omitIfSet || intValue != (*i)->omitIf)
				{
					//Don't set ON_TIME when value is false
					if((rpcParameter->physical->groupId == "STATE" && value->booleanValue) || (rpcParameter->physical->groupId == "LEVEL" && value->floatValue > 0)) packet->setPosition((*i)->index, (*i)->size, additionalParameter->data);
				}
			}
			//param sometimes is ambiguous (e. g. LEVEL of HM-CC-TC), so don't search and use the given parameter when possible
			else if((*i)->parameterId == rpcParameter->physical->groupId)
			{
				if(frame->splitAfter > -1)
				{
					//For HM-Dis-WM55
					if(frame->binaryPayloads.size() > 1) GD::out.printError("Error constructing packet: Split after requires that there is only one parameter.");
					int32_t blockSize = frame->splitAfter - payload.size();
					std::vector<uint8_t>* data = &valuesCentral[channel][valueKey].data;
					int32_t blocks = data->size() / blockSize;
					if(data->size() % blockSize) blocks++;
					if(blocks > frame->maxPackets) blocks = frame->maxPackets;
					for(int32_t j = 0; j < blocks; j++)
					{
						int32_t startPosition = j * blockSize;
						int32_t endPosition = startPosition + blockSize;
						if((unsigned)endPosition >= data->size()) endPosition = data->size();
						std::vector<uint8_t> dataBlock(data->begin() + startPosition, data->begin() + endPosition);
						packet->setPosition((*i)->index, (double)(endPosition - startPosition), dataBlock);
						if(j < blocks - 1)
						{
							queue->push(packet);
							queue->push(central->getMessages()->find(0x02, std::vector<std::pair<uint32_t, int32_t>>()));
							setMessageCounter(_messageCounter + 1);
							packet = std::shared_ptr<BidCoSPacket>(new BidCoSPacket(_messageCounter, controlByte, (uint8_t)frame->type, getCentral()->getAddress(), _address, payload));
						}
					}
				}
				else packet->setPosition((*i)->index, (*i)->size, valuesCentral[channel][valueKey].data);
			}
			//Search for all other parameters
			else
			{
				bool paramFound = false;
				for(std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>::iterator j = valuesCentral[channel].begin(); j != valuesCentral[channel].end(); ++j)
				{
					if((*i)->parameterId == j->second.rpcParameter->physical->groupId)
					{
						packet->setPosition((*i)->index, (*i)->size, j->second.data);
						paramFound = true;
						break;
					}
				}
				if(!paramFound) GD::out.printError("Error constructing packet. param \"" + (*i)->parameterId + "\" not found. Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frame->id);
			}
			if((*i)->parameterId == "ON_TIME" && additionalParameter)
			{
				if((rpcParameter->physical->groupId != "STATE" && rpcParameter->physical->groupId != "LEVEL") || (rpcParameter->physical->groupId == "STATE" && rpcParameter->logical->type != ILogical::Type::Enum::tBoolean) || (rpcParameter->physical->groupId == "LEVEL" && rpcParameter->logical->type != ILogical::Type::Enum::tFloat))
				{
					GD::out.printError("Error: Can't set \"ON_TIME\" for " + rpcParameter->physical->groupId + ". Currently \"ON_TIME\" is only supported for \"STATE\" of type \"boolean\" or \"LEVEL\" of type \"float\". Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frame->id);
					continue;
				}
				if(!_variablesToReset.empty())
				{
					_variablesToResetMutex.lock();
					for(std::vector<std::shared_ptr<VariableToReset>>::iterator i = _variablesToReset.begin(); i != _variablesToReset.end(); ++i)
					{
						if((*i)->channel == channel && (*i)->key == rpcParameter->physical->groupId)
						{
							GD::out.printMessage("Debug: Deleting element from _variablesToReset. Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frame->id, 5);
							_variablesToReset.erase(i);
							break; //The key should only be once in the vector, so breaking is ok and we can't continue as the iterator is invalidated.
						}
					}
					_variablesToResetMutex.unlock();
				}
				if((rpcParameter->physical->groupId == "STATE" && !value->booleanValue) || (rpcParameter->physical->groupId == "LEVEL" && value->floatValue == 0) || additionalParameter->data.empty() || additionalParameter->data.at(0) == 0) continue;
				std::shared_ptr<CallbackFunctionParameter> parameters(new CallbackFunctionParameter());
				parameters->integers.push_back(channel);
				parameters->integers.push_back(0); //false = off
				int64_t time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
				parameters->integers.push_back(time + std::lround(additionalParameter->rpcParameter->convertFromPacket(additionalParameter->data)->floatValue * 1000));
				parameters->strings.push_back(rpcParameter->physical->groupId);
				queue->callbackParameter = parameters;
				queue->queueEmptyCallback = delegate<void (std::shared_ptr<CallbackFunctionParameter>)>::from_method<BidCoSPeer, &BidCoSPeer::addVariableToResetCallback>(this);
			}
		}
		if(!rpcParameter->setPackets.front()->autoReset.empty())
		{
			for(std::vector<std::string>::iterator j = rpcParameter->setPackets.front()->autoReset.begin(); j != rpcParameter->setPackets.front()->autoReset.end(); ++j)
			{
				if(valuesCentral.at(channel).find(*j) == valuesCentral.at(channel).end()) continue;
				PVariable logicalDefaultValue = valuesCentral.at(channel).at(*j).rpcParameter->logical->getDefaultValue();
				std::vector<uint8_t> defaultValue;
				valuesCentral.at(channel).at(*j).rpcParameter->convertToPacket(logicalDefaultValue, defaultValue);
				if(defaultValue != valuesCentral.at(channel).at(*j).data)
				{
					BaseLib::Systems::RPCConfigurationParameter* tempParam = &valuesCentral.at(channel).at(*j);
					tempParam->data = defaultValue;
					if(tempParam->databaseID > 0) saveParameter(tempParam->databaseID, tempParam->data);
					else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, *j, tempParam->data);
					GD::out.printInfo( "Info: Parameter \"" + *j + "\" was reset to " + BaseLib::HelperFunctions::getHexString(defaultValue) + ". Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frame->id);
					if(rpcParameter->readable)
					{
						valueKeys->push_back(*j);
						values->push_back(logicalDefaultValue);
					}
				}
			}
		}
		setMessageCounter(_messageCounter + 1);
		queue->push(packet);
		queue->push(central->getMessages()->find(0x02, std::vector<std::pair<uint32_t, int32_t>>()));
		pendingBidCoSQueues->push(queue);
		if((getRXModes() & HomegearDevice::ReceiveModes::Enum::always) || (getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeOnRadio))
		{
			bool result = false;
			central->enqueuePendingQueues(_address, wait, &result);
			if(!result)
			{
				if((_deviceType.type() == 0x19 || _deviceType.type() == 0x26 || _deviceType.type() == 0x27 || _deviceType.type() == 0x28) && valueKey == "STATE" && value->booleanValue) pendingBidCoSQueues->remove(BidCoSQueueType::PEER, valueKey, channel); //Clear queue of KeyMatic and WinMatic when STATE was set to true;
				return Variable::createError(-100, "No answer from device.");
			}
		}
		else if((getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeUp2))
		{
			std::shared_ptr<BidCoSPacket> lastPacket = central->getReceivedPacket(_address);
			if(lastPacket && BaseLib::HelperFunctions::getTime() - lastPacket->timeReceived() < 150)
			{
				bool result = false;
				central->enqueuePendingQueues(_address, wait, &result);
				if(!result) return Variable::createError(-100, "No answer from device.");
			}
			else
			{
				setValuePending(true);
				GD::out.printDebug("Debug: Packet was queued and will be sent with next wake me up packet.");
			}
		}
		else
		{
			setValuePending(true);
			GD::out.printDebug("Debug: Packet was queued and will be sent with next wake me up packet.");
		}

		if(!valueKeys->empty())
		{
			raiseEvent(_peerID, channel, valueKeys, values);
			raiseRPCEvent(_peerID, channel, _serialNumber + ":" + std::to_string(channel), valueKeys, values);
		}

		return PVariable(new Variable(VariableType::tVoid));
	}
	catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
        _variablesToResetMutex.unlock();
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
        _variablesToResetMutex.unlock();
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
        _variablesToResetMutex.unlock();
    }
    return Variable::createError(-32500, "Unknown application error. See error log for more details.");
}
}
