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

#include "MAXPeer.h"
#include "LogicalDevices/MAXCentral.h"
#include "GD.h"

namespace MAX
{
std::shared_ptr<BaseLib::Systems::Central> MAXPeer::getCentral()
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
	return std::shared_ptr<BaseLib::Systems::Central>();
}

std::shared_ptr<BaseLib::Systems::LogicalDevice> MAXPeer::getDevice(int32_t address)
{
	try
	{
		return GD::family->get(address);
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
	return std::shared_ptr<BaseLib::Systems::LogicalDevice>();
}

void MAXPeer::setPhysicalInterfaceID(std::string id)
{
	if(id.empty() || (GD::physicalInterfaces.find(id) != GD::physicalInterfaces.end() && GD::physicalInterfaces.at(id)))
	{
		_physicalInterfaceID = id;
		setPhysicalInterface(id.empty() ? GD::defaultPhysicalInterface : GD::physicalInterfaces.at(_physicalInterfaceID));
		saveVariable(19, _physicalInterfaceID);
	}
}

void MAXPeer::setPhysicalInterface(std::shared_ptr<IPhysicalInterface> interface)
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

MAXPeer::MAXPeer(uint32_t parentID, bool centralFeatures, IPeerEventSink* eventHandler) : Peer(GD::bl, parentID, centralFeatures, eventHandler)
{
	if(centralFeatures)
	{
		pendingQueues.reset(new PendingQueues());
	}
	setPhysicalInterface(GD::defaultPhysicalInterface);
	_lastTimePacket = BaseLib::HelperFunctions::getTime() + (BaseLib::HelperFunctions::getRandomNumber(1, 1000) * 10000);
}

MAXPeer::MAXPeer(int32_t id, int32_t address, std::string serialNumber, uint32_t parentID, bool centralFeatures, IPeerEventSink* eventHandler) : Peer(GD::bl, id, address, serialNumber, parentID, centralFeatures, eventHandler)
{
	setPhysicalInterface(GD::defaultPhysicalInterface);
	_lastTimePacket = BaseLib::HelperFunctions::getTime() + (BaseLib::HelperFunctions::getRandomNumber(1, 1000) * 10000);
}

MAXPeer::~MAXPeer()
{
	dispose();
}

void MAXPeer::worker()
{
	if(!_centralFeatures || _disposing) return;
	std::vector<uint32_t> positionsToDelete;
	int64_t time;
	try
	{
		time = BaseLib::HelperFunctions::getTime();
		if(_rpcDevice)
		{
			serviceMessages->checkUnreach(_rpcDevice->timeout, getLastPacketReceived());
			if(_rpcDevice->needsTime && (time - _lastTimePacket) > 43200000)
			{
				_lastTimePacket = time;
				std::shared_ptr<MAXCentral> central = std::dynamic_pointer_cast<MAXCentral>(getCentral());
				std::shared_ptr<PacketQueue> queue(new PacketQueue(_physicalInterface, PacketQueueType::PEER));
				queue->peer = central->getPeer(_peerID);
				queue->noSending = true;

				queue->push(central->getTimePacket(central->messageCounter()->at(0)++, _address, getRXModes() & HomegearDevice::ReceiveModes::wakeOnRadio));
				queue->push(central->getMessages()->find(DIRECTIONIN, 0x02, 0x02, std::vector<std::pair<uint32_t, int32_t>>()));
				queue->parameterName = "CURRENT_TIME";
				queue->channel = 0;
				pendingQueues->remove("CURRENT_TIME", 0);
				pendingQueues->push(queue);
				if((getRXModes() & HomegearDevice::ReceiveModes::always) || (getRXModes() & HomegearDevice::ReceiveModes::wakeOnRadio)) central->enqueuePendingQueues(_address);
			}
		}
		if(serviceMessages->getConfigPending())
		{
			if(!pendingQueues || pendingQueues->empty()) serviceMessages->setConfigPending(false);
			else if(_bl->settings.devLog() && (getRXModes() & HomegearDevice::ReceiveModes::wakeUp) && (_bl->hf.getTime() - serviceMessages->getConfigPendingSetTime()) > 360000)
			{
				GD::out.printWarning("Devlog warning: Configuration for peer with id " + std::to_string(_peerID) + " supporting wake up is pending since more than 6 minutes.");
				serviceMessages->resetConfigPendingSetTime();
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

std::string MAXPeer::handleCLICommand(std::string command)
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
			stringStream << "queues info\t\tPrints information about the pending MAX! packet queues" << std::endl;
			stringStream << "queues clear\t\tClears pending MAX! packet queues" << std::endl;
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
						stringStream << "Description: This command prints all configuration parameters of this peer. The values are in MAX! packet format." << std::endl;
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
						stringStream << "Description: This command prints information about the pending MAX! queues." << std::endl;
						stringStream << "Usage: queues info" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  There are no parameters." << std::endl;
						return stringStream.str();
					}
				}
				index++;
			}

			pendingQueues->getInfoString(stringStream);
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
						stringStream << "Description: This command clears all pending MAX! queues." << std::endl;
						stringStream << "Usage: queues clear" << std::endl << std::endl;
						stringStream << "Parameters:" << std::endl;
						stringStream << "  There are no parameters." << std::endl;
						return stringStream.str();
					}
				}
				index++;
			}

			pendingQueues->clear();
			stringStream << "All pending MAX! queues were deleted." << std::endl;
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

void MAXPeer::addPeer(int32_t channel, std::shared_ptr<BaseLib::Systems::BasicPeer> peer)
{
	try
	{

		if(_rpcDevice->functions.find(channel) == _rpcDevice->functions.end()) return;
		for(std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>::iterator i = _peers[channel].begin(); i != _peers[channel].end(); ++i)
		{
			if((*i)->address == peer->address && (*i)->channel == peer->channel)
			{
				_peers[channel].erase(i);
				break;
			}
		}
		_peers[channel].push_back(peer);
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

void MAXPeer::removePeer(int32_t channel, uint64_t id, int32_t remoteChannel)
{
	try
	{
		if(_peers.find(channel) == _peers.end()) return;
		std::shared_ptr<MAXCentral> central(std::dynamic_pointer_cast<MAXCentral>(getCentral()));

		for(std::vector<std::shared_ptr<BaseLib::Systems::BasicPeer>>::iterator i = _peers[channel].begin(); i != _peers[channel].end(); ++i)
		{
			if((*i)->id == id && (*i)->channel == remoteChannel)
			{
				_peers[channel].erase(i);
				savePeers();
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
    _databaseMutex.unlock();
}

void MAXPeer::save(bool savePeer, bool variables, bool centralConfig)
{
	try
	{
		Peer::save(savePeer, variables, centralConfig);
		if(!variables) savePendingQueues();
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

void MAXPeer::loadVariables(BaseLib::Systems::LogicalDevice* device, std::shared_ptr<BaseLib::Database::DataTable> rows)
{
	try
	{
		if(!rows) rows = raiseGetPeerVariables();
		Peer::loadVariables(device, rows);
		_databaseMutex.lock();
		for(BaseLib::Database::DataTable::iterator row = rows->begin(); row != rows->end(); ++row)
		{
			switch(row->second.at(2)->intValue)
			{
			case 5:
				_messageCounter = row->second.at(3)->intValue;
				break;
			case 12:
				unserializePeers(row->second.at(5)->binaryValue);
				break;
			case 16:
				if(_centralFeatures && device)
				{
					pendingQueues.reset(new PendingQueues());
					pendingQueues->unserialize(row->second.at(5)->binaryValue, this, (MAXDevice*)device);
				}
				break;
			case 19:
				_physicalInterfaceID = row->second.at(4)->textValue;
				if(!_physicalInterfaceID.empty() && GD::physicalInterfaces.find(_physicalInterfaceID) != GD::physicalInterfaces.end()) setPhysicalInterface(GD::physicalInterfaces.at(_physicalInterfaceID));
				break;
			}
		}
		if(_centralFeatures && !pendingQueues) pendingQueues.reset(new PendingQueues());
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

bool MAXPeer::load(BaseLib::Systems::LogicalDevice* device)
{
	try
	{
		loadVariables(device);

		_rpcDevice = GD::rpcDevices.find(_deviceType, _firmwareVersion, -1);
		if(!_rpcDevice)
		{
			GD::out.printError("Error loading peer " + std::to_string(_peerID) + ": Device type not found: 0x" + BaseLib::HelperFunctions::getHexString((uint32_t)_deviceType.type()) + " Firmware version: " + std::to_string(_firmwareVersion));
			return false;
		}
		initializeTypeString();
		std::string entry;
		loadConfig();
		initializeCentralConfig();

		serviceMessages.reset(new BaseLib::Systems::ServiceMessages(_bl, _peerID, _serialNumber, this));
		serviceMessages->load();

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

void MAXPeer::saveVariables()
{
	try
	{
		if(_peerID == 0) return;
		Peer::saveVariables();
		saveVariable(5, (int32_t)_messageCounter);
		savePeers(); //12
		savePendingQueues(); //16
		saveVariable(19, _physicalInterfaceID);
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

void MAXPeer::savePeers()
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

void MAXPeer::savePendingQueues()
{
	try
	{
		if(!_centralFeatures || !pendingQueues) return;
		std::vector<uint8_t> serializedData;
		pendingQueues->serialize(serializedData);
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

void MAXPeer::serializePeers(std::vector<uint8_t>& encodedData)
{
	try
	{
		BaseLib::BinaryEncoder encoder(_bl);
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
				encoder.encodeBoolean(encodedData, (*j)->hidden);
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
}

void MAXPeer::unserializePeers(std::shared_ptr<std::vector<char>> serializedData)
{
	try
	{
		BaseLib::BinaryDecoder decoder(_bl);
		uint32_t position = 0;
		uint32_t peersSize = decoder.decodeInteger(*serializedData, position);
		for(uint32_t i = 0; i < peersSize; i++)
		{
			uint32_t channel = decoder.decodeInteger(*serializedData, position);
			uint32_t peerCount = decoder.decodeInteger(*serializedData, position);
			for(uint32_t j = 0; j < peerCount; j++)
			{
				std::shared_ptr<BaseLib::Systems::BasicPeer> basicPeer(new BaseLib::Systems::BasicPeer());
				basicPeer->isSender = decoder.decodeBoolean(*serializedData, position);
				basicPeer->id = decoder.decodeInteger(*serializedData, position);
				basicPeer->address = decoder.decodeInteger(*serializedData, position);
				basicPeer->channel = decoder.decodeInteger(*serializedData, position);
				basicPeer->serialNumber = decoder.decodeString(*serializedData, position);
				basicPeer->hidden = decoder.decodeBoolean(*serializedData, position);
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
}

std::string MAXPeer::printConfig()
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

void MAXPeer::getValuesFromPacket(std::shared_ptr<MAXPacket> packet, std::vector<FrameValues>& frameValues)
{
	try
	{
		if(!_rpcDevice) return;
		//equal_range returns all elements with "0" or an unknown element as argument
		if(_rpcDevice->packetsByMessageType.find(packet->messageType()) == _rpcDevice->packetsByMessageType.end()) return;
		std::pair<PacketsByMessageType::iterator, PacketsByMessageType::iterator> range = _rpcDevice->packetsByMessageType.equal_range((uint32_t)packet->messageType());
		if(range.first == _rpcDevice->packetsByMessageType.end()) return;
		PacketsByMessageType::iterator i = range.first;
		do
		{
			FrameValues currentFrameValues;
			PPacket frame(i->second);
			if(!frame) continue;
			if(frame->direction == BaseLib::DeviceDescription::Packet::Direction::Enum::toCentral && packet->senderAddress() != _address) continue;
			if(frame->direction == BaseLib::DeviceDescription::Packet::Direction::Enum::fromCentral && packet->destinationAddress() != _address) continue;
			if(packet->payload()->empty()) break;
			if(frame->subtype > -1 && packet->messageSubtype() != frame->subtype) continue;
			int32_t channelIndex = frame->channelIndex;
			int32_t channel = -1;
			if(channelIndex >= 9 && (signed)packet->payload()->size() > (channelIndex - 9)) channel = packet->payload()->at(channelIndex - 9) - frame->channelIndexOffset;
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

					//Process split data
					if((*j)->size2 > 0 && (*j)->index2 > 0 && (*j)->index2Offset > 0) //Only
					{
						if((*j)->size2 > 1.0) GD::out.printWarning("Warning: size2 of frame parameter is larger than 1 byte. That is not supported.");
						else if(((int32_t)(*j)->index2) - 9 < (signed)packet->payload()->size())
						{
							std::vector<uint8_t> data2 = packet->getPosition((*j)->index2, (*j)->size2, -1);
							int32_t byteIndex = (*j)->index2Offset / 8;
							int32_t bitIndex = (*j)->index2Offset % 8;
							if(data2.size() == 1)
							{
								if(byteIndex < (signed)data.size())
								{
									data.at(byteIndex) |= (data2.at(0) << bitIndex);
								}
								else
								{
									data2.insert(data2.end(), data.begin(), data.end());
									data = data2;
								}
							}
						}
					}
				}
				else if((*j)->constValueInteger > -1)
				{
					_bl->hf.memcpyBigEndian(data, (*j)->constValueInteger);
				}
				else continue;

				//Check for low battery
				if((*j)->parameterId == "LOWBAT")
				{
					if(data.size() > 0 && data.at(0))
					{
						serviceMessages->set("LOWBAT", true);
						if(_bl->debugLevel >= 4) GD::out.printInfo("Info: LOWBAT of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + " was set to \"true\".");
					}
					else serviceMessages->set("LOWBAT", false);
				}

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

PParameterGroup MAXPeer::getParameterSet(int32_t channel, ParameterGroup::Type::Enum type)
{
	try
	{
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return PParameterGroup();
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
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

void MAXPeer::packetReceived(std::shared_ptr<MAXPacket> packet)
{
	try
	{
		if(!packet) return;
		if(!_centralFeatures || _disposing) return;
		if(packet->senderAddress() != _address) return;
		if(!_rpcDevice) return;
		std::shared_ptr<MAXCentral> central = std::dynamic_pointer_cast<MAXCentral>(getCentral());
		if(!central) return;
		if(packet->messageType() == 0) packet->setMessageType(0xFF);
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

			for(std::map<std::string, FrameValue>::iterator i = a->values.begin(); i != a->values.end(); ++i)
			{
				for(std::list<uint32_t>::const_iterator j = a->paramsetChannels.begin(); j != a->paramsetChannels.end(); ++j)
				{
					if(std::find(i->second.channels.begin(), i->second.channels.end(), *j) == i->second.channels.end()) continue;
					if(pendingQueues->exists(i->first, *j)) continue; //Don't set queued values
					if(!valueKeys[*j] || !rpcValues[*j])
					{
						valueKeys[*j].reset(new std::vector<std::string>());
						rpcValues[*j].reset(new std::vector<PVariable>());
					}

					BaseLib::Systems::RPCConfigurationParameter* parameter = &valuesCentral[*j][i->first];
					parameter->data = i->second.value;
					if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
					else saveParameter(0, ParameterGroup::Type::Enum::variables, *j, i->first, parameter->data);
					if(_bl->debugLevel >= 4) GD::out.printInfo("Info: " + i->first + " on channel " + std::to_string(*j) + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber  + " was set to 0x" + BaseLib::HelperFunctions::getHexString(i->second.value) + ".");

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
		}

		if(packet->senderAddress() == _address && pendingQueues && !pendingQueues->empty())
		{
			if(packet->destinationAddress() == central->getAddress())
			{
				pendingQueues->front()->setWakeOnRadio(false);

				std::vector<uint8_t> payload;
				payload.push_back(0);
				payload.push_back(0);
				std::shared_ptr<MAXPacket> wakeUpPacket(new MAXPacket(packet->messageCounter(), 0x02, 0x00, central->getAddress(), _address, payload, false));
				central->sendPacket(_physicalInterface, wakeUpPacket, false);

				if(packet->messageSubtype() & 2) central->enqueuePendingQueues(_address);
			}
			else if(packet->messageSubtype() & 2) //Wait for the last packet and then enqueue
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(60)); //Wait for the response from the receiving device. 80ms is maximum, 55ms is minimum => With 60ms buffer of 20ms
				central->enqueuePendingQueues(_address);
			}
		}
		else if(packet->messageType() != 0x02 && packet->messageType() != 0xFF && packet->destinationAddress() == central->getAddress()) central->sendOK(packet->messageCounter(), packet->senderAddress());

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

std::string MAXPeer::getFirmwareVersionString(int32_t firmwareVersion)
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

void MAXPeer::setRSSIDevice(uint8_t rssi)
{
	try
	{
		if(!_centralFeatures || _disposing || rssi == 0) return;
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

//RPC Methods
PVariable MAXPeer::getDeviceInfo(int32_t clientID, std::map<std::string, bool> fields)
{
	try
	{
		PVariable info(Peer::getDeviceInfo(clientID, fields));
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

PVariable MAXPeer::getParamsetDescription(int32_t clientID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel");
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(!parameterGroup || parameterGroup->parameters.empty()) return Variable::createError(-3, "Unknown parameter set");
		if(type == ParameterGroup::Type::link && remoteID > 0)
		{
			std::shared_ptr<BaseLib::Systems::BasicPeer> remotePeer = getPeer(channel, remoteID, remoteChannel);
			if(!remotePeer) return Variable::createError(-2, "Unknown remote peer.");
		}
		return Peer::getParamsetDescription(clientID, parameterGroup);
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

PVariable MAXPeer::putParamset(int32_t clientID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool onlyPushing)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(!_centralFeatures) return Variable::createError(-2, "Not a central peer.");
		if(channel < 0) channel = 0;
		if(remoteChannel < 0) remoteChannel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel");
		if(type == ParameterGroup::Type::none) type = ParameterGroup::Type::link;
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(!parameterGroup || parameterGroup->parameters.empty()) return Variable::createError(-3, "Unknown parameter set");
		if(variables->structValue->empty()) return PVariable(new Variable(VariableType::tVoid));

		if(type == ParameterGroup::Type::Enum::config)
		{
			std::map<int32_t, std::map<int32_t, std::vector<uint8_t>>> changedParameters;
			//allParameters is necessary to temporarily store all values. It is used to set changedParameters.
			//This is necessary when there are multiple variables per index and not all of them are changed.
			std::map<int32_t, std::map<int32_t, std::vector<uint8_t>>> allParameters;
			for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
			{
				if(i->first.empty() || !i->second) continue;
				std::vector<uint8_t> value;
				if(configCentral[channel].find(i->first) == configCentral[channel].end()) continue;
				BaseLib::Systems::RPCConfigurationParameter* parameter = &configCentral[channel][i->first];
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
				else saveParameter(0, ParameterGroup::Type::Enum::config, channel, i->first, parameter->data);
				GD::out.printInfo("Info: Parameter " + i->first + " of peer " + std::to_string(_peerID) + " and channel " + std::to_string(channel) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(allParameters[list][intIndex]) + ".");
				//Only send to device when parameter is of type config
				if(parameter->rpcParameter->physical->operationType != IPhysical::OperationType::Enum::config && parameter->rpcParameter->physical->operationType != IPhysical::OperationType::Enum::configString) continue;
				changedParameters[list][intIndex] = allParameters[list][intIndex];
			}

			if(changedParameters.empty() || changedParameters.begin()->second.empty()) return PVariable(new Variable(VariableType::tVoid));

			std::shared_ptr<MAXCentral> central = std::dynamic_pointer_cast<MAXCentral>(getCentral());

			for(std::map<int32_t, std::map<int32_t, std::vector<uint8_t>>>::iterator i = changedParameters.begin(); i != changedParameters.end(); ++i)
			{
				std::vector<uint8_t> payload;
				payload.push_back(0);
				payload.push_back(i->first);
				std::shared_ptr<MAXPacket> configPacket = std::shared_ptr<MAXPacket>(new MAXPacket(_messageCounter, 0x10, 0x00, central->getAddress(), _address, payload, getRXModes() & HomegearDevice::ReceiveModes::wakeOnRadio));

				for(std::map<int32_t, std::vector<uint8_t>>::iterator j = i->second.begin(); j != i->second.end(); ++j)
				{
					configPacket->setPosition(j->first - (j->second.size() - 1), j->second.size(), j->second);
				}

				//Fill in all missing parameters
				Lists::iterator listIterator = parameterGroup->lists.find(i->first);
				std::vector<std::shared_ptr<Parameter>> allListParameters;
				if(listIterator != parameterGroup->lists.end()) allListParameters = listIterator->second;
				for(std::vector<std::shared_ptr<Parameter>>::iterator j = allListParameters.begin(); j!= allListParameters.end(); ++j)
				{
					if(i->second.find((int32_t)(*j)->physical->index) != i->second.end()) continue;
					if(configCentral[channel].find((*j)->id) == configCentral[channel].end()) continue;
					RPCConfigurationParameter* parameter = &configCentral[channel][(*j)->id];
					if(parameter->rpcParameter->physical->operationType != IPhysical::OperationType::Enum::config && parameter->rpcParameter->physical->operationType != IPhysical::OperationType::Enum::configString) continue;
					configPacket->setPosition((*j)->physical->index - (std::lround(std::ceil(((*j)->physical->size))) - 1), (*j)->physical->size, parameter->data);
				}

				if(configPacket->payload()->size() > 2)
				{
					std::shared_ptr<PacketQueue> queue(new PacketQueue(_physicalInterface, PacketQueueType::CONFIG));
					queue->noSending = true;
					queue->peer = central->getPeer(_peerID);
					queue->push(configPacket);
					queue->push(central->getMessages()->find(DIRECTIONIN, 0x02, 0x02, std::vector<std::pair<uint32_t, int32_t>>()));
					payload.clear();
					setMessageCounter(_messageCounter + 1);
					pendingQueues->push(queue);
				}
			}

			serviceMessages->setConfigPending(true);
			if(!onlyPushing) central->enqueuePendingQueues(_address);
			raiseRPCUpdateDevice(_peerID, channel, _serialNumber + ":" + std::to_string(channel), 0);
		}
		else if(type == ParameterGroup::Type::Enum::variables)
		{
			for(Struct::iterator i = variables->structValue->begin(); i != variables->structValue->end(); ++i)
			{
				if(i->first.empty() || !i->second) continue;
				setValue(clientID, channel, i->first, i->second);
			}
		}
		else
		{
			return Variable::createError(-3, "Parameter set type is not supported.");
		}
		return PVariable(new Variable(BaseLib::VariableType::tVoid));
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

PVariable MAXPeer::getParamset(int32_t clientID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
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
		if(!parameterGroup || parameterGroup->parameters.empty()) return Variable::createError(-3, "Unknown parameter set");
		PVariable variables(new Variable(VariableType::tStruct));

		for(Parameters::iterator i = parameterGroup->parameters.begin(); i != parameterGroup->parameters.end(); ++i)
		{
			if(i->second->id.empty()) continue;
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

PVariable MAXPeer::setInterface(int32_t clientID, std::string interfaceID)
{
	try
	{
		if(!interfaceID.empty() && GD::physicalInterfaces.find(interfaceID) == GD::physicalInterfaces.end())
		{
			return Variable::createError(-5, "Unknown physical interface.");
		}
		std::shared_ptr<IPhysicalInterface> interface(GD::physicalInterfaces.at(interfaceID));
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

PVariable MAXPeer::setValue(int32_t clientID, uint32_t channel, std::string valueKey, PVariable value)
{
	try
	{
		Peer::setValue(clientID, channel, valueKey, value); //Ignore result, otherwise setHomegerValue might not be executed
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(!_centralFeatures) return Variable::createError(-2, "Not a central peer.");
		if(valueKey.empty()) return Variable::createError(-5, "Value key is empty.");
		if(channel == 0 && serviceMessages->set(valueKey, value->booleanValue)) return PVariable(new Variable(VariableType::tVoid));
		std::unordered_map<uint32_t, std::unordered_map<std::string, RPCConfigurationParameter>>::iterator channelIterator = valuesCentral.find(channel);
		if(channelIterator == valuesCentral.end()) return Variable::createError(-2, "Unknown channel.");
		std::unordered_map<std::string, RPCConfigurationParameter>::iterator parameterIterator = channelIterator->second.find(valueKey);
		if(parameterIterator == valuesCentral[channel].end()) return Variable::createError(-5, "Unknown parameter.");
		PParameter rpcParameter = parameterIterator->second.rpcParameter;
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
		PToggle toggleCast;
		if(rpcParameter->casts.empty()) toggleCast = std::dynamic_pointer_cast<Toggle>(rpcParameter->casts.at(0));
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
			return setValue(clientID, channel, toggleCast->parameter, toggleValue);
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

		std::shared_ptr<MAXCentral> central = std::dynamic_pointer_cast<MAXCentral>(getCentral());
		std::shared_ptr<PacketQueue> queue(new PacketQueue(_physicalInterface, PacketQueueType::PEER));
		queue->peer = central->getPeer(_peerID);
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
		std::shared_ptr<MAXPacket> packet(new MAXPacket(_messageCounter, (uint8_t)frame->type, frame->subtype, getCentral()->physicalAddress(), _address, payload, getRXModes() & HomegearDevice::ReceiveModes::Enum::wakeOnRadio));

		for(BinaryPayloads::iterator i = frame->binaryPayloads.begin(); i != frame->binaryPayloads.end(); ++i)
		{
			if((*i)->constValueInteger > -1)
			{
				std::vector<uint8_t> data;
				_bl->hf.memcpyBigEndian(data, (*i)->constValueInteger);
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
				std::vector<uint8_t> data = valuesCentral[channel][valueKey].data;
				packet->setPosition((*i)->index, (*i)->size, data);
			}
			//Search for all other parameters
			else
			{
				bool paramFound = false;
				for(std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>::iterator j = valuesCentral[channel].begin(); j != valuesCentral[channel].end(); ++j)
				{
					//Only compare id. Till now looking for value_id was not necessary.
					if((*i)->parameterId == j->second.rpcParameter->physical->groupId)
					{
						std::vector<uint8_t> data = j->second.data;
						packet->setPosition((*i)->index, (*i)->size, data);
						paramFound = true;
						break;
					}
				}
				if(!paramFound) GD::out.printError("Error constructing packet. param \"" + (*i)->parameterId + "\" not found. Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frame->id);
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
					else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, tempParam->data);
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
		queue->parameterName = valueKey;
		queue->channel = channel;
		queue->push(packet);
		queue->push(central->getMessages()->find(DIRECTIONIN, 0x02, 0x02, std::vector<std::pair<uint32_t, int32_t>>()));
		pendingQueues->remove(valueKey, channel);
		pendingQueues->push(queue);
		if(MAXDevice::isSwitch(_deviceType)) queue->retries = 12;
		central->enqueuePendingQueues(_address);

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
    }
    catch(BaseLib::Exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error. See error log for more details.");
}
//End RPC methods
}
