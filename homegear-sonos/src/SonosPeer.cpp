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

#include "SonosPeer.h"
#include "LogicalDevices/SonosCentral.h"
#include "SonosPacket.h"
#include "GD.h"
#include "sys/wait.h"

namespace Sonos
{
std::shared_ptr<BaseLib::Systems::Central> SonosPeer::getCentral()
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

std::shared_ptr<BaseLib::Systems::LogicalDevice> SonosPeer::getDevice(int32_t address)
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

SonosPeer::SonosPeer(uint32_t parentID, bool centralFeatures, IPeerEventSink* eventHandler) : BaseLib::Systems::Peer(GD::bl, parentID, centralFeatures, eventHandler)
{
	init();
}

SonosPeer::SonosPeer(int32_t id, std::string serialNumber, uint32_t parentID, bool centralFeatures, IPeerEventSink* eventHandler) : BaseLib::Systems::Peer(GD::bl, id, -1, serialNumber, parentID, centralFeatures, eventHandler)
{
	init();
}

SonosPeer::~SonosPeer()
{
	try
	{

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

void SonosPeer::init()
{
	_binaryEncoder.reset(new BaseLib::RPC::RPCEncoder(GD::bl));
	_binaryDecoder.reset(new BaseLib::RPC::RPCDecoder(GD::bl));

	_upnpFunctions.insert(UpnpFunctionPair("AddURIToQueue", UpnpFunctionEntry("urn:schemas-upnp-org:service:AVTransport:1", "/MediaRenderer/AVTransport/Control", PSoapValues(new SoapValues()))));
	_upnpFunctions.insert(UpnpFunctionPair("GetCrossfadeMode", UpnpFunctionEntry("urn:schemas-upnp-org:service:AVTransport:1", "/MediaRenderer/AVTransport/Control", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0") }))));
	_upnpFunctions.insert(UpnpFunctionPair("GetMute", UpnpFunctionEntry("urn:schemas-upnp-org:service:RenderingControl:1", "/MediaRenderer/RenderingControl/Control", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0"), SoapValuePair("Channel", "Master") }))));
	_upnpFunctions.insert(UpnpFunctionPair("GetPositionInfo", UpnpFunctionEntry("urn:schemas-upnp-org:service:AVTransport:1", "/MediaRenderer/AVTransport/Control", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0") }))));
	_upnpFunctions.insert(UpnpFunctionPair("GetRemainingSleepTimerDuration", UpnpFunctionEntry("urn:schemas-upnp-org:service:AVTransport:1", "/MediaRenderer/AVTransport/Control", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0") }))));
	_upnpFunctions.insert(UpnpFunctionPair("GetTransportInfo", UpnpFunctionEntry("urn:schemas-upnp-org:service:AVTransport:1", "/MediaRenderer/AVTransport/Control", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0") }))));
	_upnpFunctions.insert(UpnpFunctionPair("GetVolume", UpnpFunctionEntry("urn:schemas-upnp-org:service:RenderingControl:1", "/MediaRenderer/RenderingControl/Control", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0"), SoapValuePair("Channel", "Master") }))));
	_upnpFunctions.insert(UpnpFunctionPair("Next", UpnpFunctionEntry("urn:schemas-upnp-org:service:AVTransport:1", "/MediaRenderer/AVTransport/Control", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0") }))));
	_upnpFunctions.insert(UpnpFunctionPair("Pause", UpnpFunctionEntry("urn:schemas-upnp-org:service:AVTransport:1", "/MediaRenderer/AVTransport/Control", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0") }))));
	_upnpFunctions.insert(UpnpFunctionPair("Play", UpnpFunctionEntry("urn:schemas-upnp-org:service:AVTransport:1", "/MediaRenderer/AVTransport/Control", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0"), SoapValuePair("Speed", "1") }))));
	_upnpFunctions.insert(UpnpFunctionPair("Previous", UpnpFunctionEntry("urn:schemas-upnp-org:service:AVTransport:1", "/MediaRenderer/AVTransport/Control", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0") }))));
	_upnpFunctions.insert(UpnpFunctionPair("RampToVolume", UpnpFunctionEntry("urn:schemas-upnp-org:service:RenderingControl:1", "/MediaRenderer/RenderingControl/Control", PSoapValues(new SoapValues()))));
	_upnpFunctions.insert(UpnpFunctionPair("RemoveTrackFromQueue", UpnpFunctionEntry("urn:schemas-upnp-org:service:AVTransport:1", "/MediaRenderer/AVTransport/Control", PSoapValues(new SoapValues()))));
	_upnpFunctions.insert(UpnpFunctionPair("Seek", UpnpFunctionEntry("urn:schemas-upnp-org:service:AVTransport:1", "/MediaRenderer/AVTransport/Control", PSoapValues(new SoapValues()))));
	_upnpFunctions.insert(UpnpFunctionPair("SetCrossfadeMode", UpnpFunctionEntry("urn:schemas-upnp-org:service:AVTransport:1", "/MediaRenderer/AVTransport/Control", PSoapValues(new SoapValues()))));
	_upnpFunctions.insert(UpnpFunctionPair("SetMute", UpnpFunctionEntry("urn:schemas-upnp-org:service:RenderingControl:1", "/MediaRenderer/RenderingControl/Control", PSoapValues(new SoapValues()))));
	_upnpFunctions.insert(UpnpFunctionPair("SetAVTransportURI", UpnpFunctionEntry("urn:schemas-upnp-org:service:AVTransport:1", "/MediaRenderer/AVTransport/Control", PSoapValues(new SoapValues()))));
	_upnpFunctions.insert(UpnpFunctionPair("SetVolume", UpnpFunctionEntry("urn:schemas-upnp-org:service:RenderingControl:1", "/MediaRenderer/RenderingControl/Control", PSoapValues(new SoapValues()))));
}

void SonosPeer::setIp(std::string value)
{
	try
	{
		Peer::setIp(value);
		_httpClient.reset(new BaseLib::HTTPClient(GD::bl, _ip, 1400, false));
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

void SonosPeer::worker()
{
	try
	{
		if(_shuttingDown || deleting) return;
		std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string>());
		std::shared_ptr<std::vector<PVariable>> rpcValues(new std::vector<PVariable>());
		if( BaseLib::HelperFunctions::getTimeSeconds() - _lastPositionInfo > 5)
		{
			_lastPositionInfo = BaseLib::HelperFunctions::getTimeSeconds();
			//Get position info if TRANSPORT_STATE is PLAYING
			std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>>::iterator channelIterator = valuesCentral.find(2);
			if(channelIterator != valuesCentral.end())
			{
				std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>::iterator parameterIterator = channelIterator->second.find("TRANSPORT_STATE");
				if(parameterIterator != channelIterator->second.end())
				{
					PVariable transportState = _binaryDecoder->decodeResponse(parameterIterator->second.data);
					if(transportState)
					{
						if(transportState->stringValue == "PLAYING" || _getOneMorePositionInfo)
						{
							_getOneMorePositionInfo = (transportState->stringValue == "PLAYING");
							execute("GetPositionInfo");
						}
					}
				}
			}
		}
		if(BaseLib::HelperFunctions::getTimeSeconds() - _lastAvTransportSubscription > 300)
		{
			_lastAvTransportSubscription = BaseLib::HelperFunctions::getTimeSeconds();
			if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: Peer " + std::to_string(_peerID) + " is calling SubscribeMRAVTransport...");
			std::string subscriptionPacket1 = "SUBSCRIBE /ZoneGroupTopology/Event HTTP/1.1\r\nHOST: " + _ip + ":1400\r\nCALLBACK: <http://" + GD::physicalInterface->listenAddress() + ':' + std::to_string(GD::physicalInterface->listenPort()) + ">\r\nNT: upnp:event\r\nTIMEOUT: Second-1800\r\nContent-Length: 0\r\n\r\n";
			std::string subscriptionPacket2 = "SUBSCRIBE /MediaRenderer/RenderingControl/Event HTTP/1.1\r\nHOST: " + _ip + ":1400\r\nCALLBACK: <http://" + GD::physicalInterface->listenAddress() + ':' + std::to_string(GD::physicalInterface->listenPort()) + ">\r\nNT: upnp:event\r\nTIMEOUT: Second-1800\r\nContent-Length: 0\r\n\r\n";
			std::string subscriptionPacket3 = "SUBSCRIBE /MediaRenderer/AVTransport/Event HTTP/1.1\r\nHOST: " + _ip + ":1400\r\nCALLBACK: <http://" + GD::physicalInterface->listenAddress() + ':' + std::to_string(GD::physicalInterface->listenPort()) + ">\r\nNT: upnp:event\r\nTIMEOUT: Second-1800\r\nContent-Length: 0\r\n\r\n";
			std::string subscriptionPacket4 = "SUBSCRIBE /MediaServer/ContentDirectory/Event HTTP/1.1\r\nHOST: " + _ip + ":1400\r\nCALLBACK: <http://" + GD::physicalInterface->listenAddress() + ':' + std::to_string(GD::physicalInterface->listenPort()) + ">\r\nNT: upnp:event\r\nTIMEOUT: Second-1800\r\nContent-Length: 0\r\n\r\n";
			std::string subscriptionPacket5 = "SUBSCRIBE /AlarmClock/Event HTTP/1.1\r\nHOST: " + _ip + ":1400\r\nCALLBACK: <http://" + GD::physicalInterface->listenAddress() + ':' + std::to_string(GD::physicalInterface->listenPort()) + ">\r\nNT: upnp:event\r\nTIMEOUT: Second-1800\r\nContent-Length: 0\r\n\r\n";
			std::string subscriptionPacket6 = "SUBSCRIBE /SystemProperties/Event HTTP/1.1\r\nHOST: " + _ip + ":1400\r\nCALLBACK: <http://" + GD::physicalInterface->listenAddress() + ':' + std::to_string(GD::physicalInterface->listenPort()) + ">\r\nNT: upnp:event\r\nTIMEOUT: Second-1800\r\nContent-Length: 0\r\n\r\n";
			std::string subscriptionPacket7 = "SUBSCRIBE /MusicServices/Event HTTP/1.1\r\nHOST: " + _ip + ":1400\r\nCALLBACK: <http://" + GD::physicalInterface->listenAddress() + ':' + std::to_string(GD::physicalInterface->listenPort()) + ">\r\nNT: upnp:event\r\nTIMEOUT: Second-1800\r\nContent-Length: 0\r\n\r\n";
			if(_httpClient)
			{
				std::string response;
				try
				{
					_httpClient->sendRequest(subscriptionPacket1, response, true);
					_httpClient->sendRequest(subscriptionPacket2, response, true);
					_httpClient->sendRequest(subscriptionPacket3, response, true);
					_httpClient->sendRequest(subscriptionPacket4, response, true);
					_httpClient->sendRequest(subscriptionPacket5, response, true);
					_httpClient->sendRequest(subscriptionPacket6, response, true);
					_httpClient->sendRequest(subscriptionPacket7, response, true);
					if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: SOAP response:\n" + response);
					serviceMessages->setUnreach(false, true);
				}
				catch(BaseLib::HTTPClientException& ex)
				{
					GD::out.printWarning("Warning: Error sending value to Sonos device: " + ex.what());
					serviceMessages->setUnreach(true, false);
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
}

void SonosPeer::homegearShuttingDown()
{
	try
	{
		_shuttingDown = true;
		Peer::homegearShuttingDown();
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

std::string SonosPeer::handleCLICommand(std::string command)
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

std::string SonosPeer::printConfig()
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


void SonosPeer::loadVariables(BaseLib::Systems::LogicalDevice* device, std::shared_ptr<BaseLib::Database::DataTable> rows)
{
	try
	{
		if(!rows) rows = raiseGetPeerVariables();
		Peer::loadVariables(device, rows);
		_databaseMutex.lock();
		for(BaseLib::Database::DataTable::iterator row = rows->begin(); row != rows->end(); ++row)
		{
			_variableDatabaseIDs[row->second.at(2)->intValue] = row->second.at(0)->intValue;
			switch(row->second.at(2)->intValue)
			{
			case 1:
				_ip = row->second.at(4)->textValue;
				_httpClient.reset(new BaseLib::HTTPClient(GD::bl, _ip, 1400, false));
				break;
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

bool SonosPeer::load(BaseLib::Systems::LogicalDevice* device)
{
	try
	{
		loadVariables((SonosDevice*)device);

		_rpcDevice = GD::rpcDevices.find(_deviceType, 0x10, -1);
		if(!_rpcDevice)
		{
			GD::out.printError("Error loading Sonos peer " + std::to_string(_peerID) + ": Device type not found: 0x" + BaseLib::HelperFunctions::getHexString((uint32_t)_deviceType.type()) + " Firmware version: " + std::to_string(_firmwareVersion));
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

void SonosPeer::saveVariables()
{
	try
	{
		if(_peerID == 0) return;
		Peer::saveVariables();
		saveVariable(1, _ip);
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

void SonosPeer::getValuesFromPacket(std::shared_ptr<SonosPacket> packet, std::vector<FrameValues>& frameValues)
{
	try
	{
		if(!_rpcDevice) return;
		//equal_range returns all elements with "0" or an unknown element as argument
		if(_rpcDevice->packetsByFunction2.find(packet->functionName()) == _rpcDevice->packetsByFunction2.end()) return;
		std::pair<PacketsByFunction::iterator, PacketsByFunction::iterator> range = _rpcDevice->packetsByFunction2.equal_range(packet->functionName());
		if(range.first == _rpcDevice->packetsByFunction2.end()) return;
		PacketsByFunction::iterator i = range.first;
		std::shared_ptr<std::unordered_map<std::string, std::string>> soapValues;
		std::string field;
		do
		{
			FrameValues currentFrameValues;
			PPacket frame(i->second);
			if(!frame) continue;
			if(frame->direction != Packet::Direction::Enum::toCentral) continue;
			int32_t channel = -1;
			if(frame->channel > -1) channel = frame->channel;
			currentFrameValues.frameID = frame->id;

			for(JsonPayloads::iterator j = frame->jsonPayloads.begin(); j != frame->jsonPayloads.end(); ++j)
			{
				if(!(*j)->subkey.empty() && ((*j)->key == "CurrentTrackMetaData" || (*j)->key == "TrackMetaData"))
				{
					soapValues = packet->currentTrackMetadata();
					if(!soapValues || soapValues->find((*j)->subkey) == soapValues->end()) continue;
					field = (*j)->subkey;
				}
				else if(!(*j)->subkey.empty() && ((*j)->key == "r:NextTrackMetaData"))
				{
					soapValues = packet->nextTrackMetadata();
					if(!soapValues || soapValues->find((*j)->subkey) == soapValues->end()) continue;
					field = (*j)->subkey;
				}
				else if(!(*j)->subkey.empty() && ((*j)->key == "AVTransportURIMetaData"))
				{
					soapValues = packet->avTransportUriMetaData();
					if(!soapValues || soapValues->find((*j)->subkey) == soapValues->end()) continue;
					field = (*j)->subkey;
				}
				else if(!(*j)->subkey.empty() && ((*j)->key == "NextAVTransportURIMetaData"))
				{
					soapValues = packet->nextAvTransportUriMetaData();
					if(!soapValues || soapValues->find((*j)->subkey) == soapValues->end()) continue;
					field = (*j)->subkey;
				}
				else if(!(*j)->subkey.empty() && ((*j)->key == "r:EnqueuedTransportURIMetaData"))
				{
					soapValues = packet->currentTrackMetadata();
					if(!soapValues || soapValues->find((*j)->subkey) == soapValues->end()) continue;
					field = (*j)->subkey;
				}
				else
				{
					soapValues = packet->values();
					if(!soapValues || soapValues->find((*j)->key) == soapValues->end()) continue;
					field = (*j)->key;
				}
				if(soapValues->find(field) == soapValues->end()) continue;

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
							Parameters::iterator parameterIterator = parameterGroup->parameters.find((*k)->id);
							if(parameterIterator == parameterGroup->parameters.end()) continue;
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
							Parameters::iterator parameterIterator = parameterGroup->parameters.find((*k)->id);
							if(parameterIterator == parameterGroup->parameters.end()) continue;
							currentFrameValues.values[(*k)->id].channels.push_back(*l);
							setValues = true;
						}
					}

					if(setValues)
					{
						//This is a little nasty and costs a lot of resources, but we need to run the data through the packet converter
						std::vector<uint8_t> encodedData;
						_binaryEncoder->encodeResponse(Variable::fromString(soapValues->at(field), (*k)->physical->type), encodedData);
						PVariable data = (*k)->convertFromPacket(encodedData, true);
						(*k)->convertToPacket(data, currentFrameValues.values[(*k)->id].value);
					}
				}
			}
			if(!currentFrameValues.values.empty()) frameValues.push_back(currentFrameValues);
		} while(++i != range.second && i != _rpcDevice->packetsByFunction2.end());
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

void SonosPeer::packetReceived(std::shared_ptr<SonosPacket> packet)
{
	try
	{
		if(!packet) return;
		if(!_centralFeatures || _disposing) return;
		if(!_rpcDevice) return;
		setLastPacketReceived();
		std::vector<FrameValues> frameValues;
		getValuesFromPacket(packet, frameValues);
		std::map<uint32_t, std::shared_ptr<std::vector<std::string>>> valueKeys;
		std::map<uint32_t, std::shared_ptr<std::vector<PVariable>>> rpcValues;

		//Loop through all matching frames
		for(std::vector<FrameValues>::iterator a = frameValues.begin(); a != frameValues.end(); ++a)
		{
			PPacket frame;
			if(!a->frameID.empty()) frame = _rpcDevice->packetsById.at(a->frameID);

			for(std::unordered_map<std::string, FrameValue>::iterator i = a->values.begin(); i != a->values.end(); ++i)
			{
				for(std::list<uint32_t>::const_iterator j = a->paramsetChannels.begin(); j != a->paramsetChannels.end(); ++j)
				{
					if(std::find(i->second.channels.begin(), i->second.channels.end(), *j) == i->second.channels.end()) continue;

					BaseLib::Systems::RPCConfigurationParameter* parameter = &valuesCentral[*j][i->first];
					if(parameter->data == i->second.value) continue;

					if(!valueKeys[*j] || !rpcValues[*j])
					{
						valueKeys[*j].reset(new std::vector<std::string>());
						rpcValues[*j].reset(new std::vector<PVariable>());
					}

					parameter->data = i->second.value;
					if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
					else saveParameter(0, ParameterGroup::Type::Enum::variables, *j, i->first, parameter->data);
					if(_bl->debugLevel >= 4) GD::out.printInfo("Info: " + i->first + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(*j) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(i->second.value) + ".");

					if(parameter->rpcParameter)
					{
						//Process service messages
						if(parameter->rpcParameter->service && !i->second.value.empty())
						{
							if(parameter->rpcParameter->logical->type == ILogical::Type::Enum::tEnum)
							{
								serviceMessages->set(i->first, i->second.value.at(i->second.value.size() - 1), *j);
							}
							else if(parameter->rpcParameter->logical->type == ILogical::Type::Enum::tBoolean)
							{
								serviceMessages->set(i->first, (bool)i->second.value.at(i->second.value.size() - 1));
							}
						}

						valueKeys[*j]->push_back(i->first);
						rpcValues[*j]->push_back(parameter->rpcParameter->convertFromPacket(i->second.value, true));
					}
				}
			}
		}

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

void SonosPeer::sendSoapRequest(std::string& request, bool ignoreErrors)
{
	try
	{
		if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: Sending SOAP request:\n" + request);
		if(_httpClient)
		{
			std::string response;
			try
			{
				_httpClient->sendRequest(request, response);
				if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: SOAP response:\n" + response);
				std::shared_ptr<SonosPacket> responsePacket(new SonosPacket(response));
				packetReceived(responsePacket);
				serviceMessages->setUnreach(false, true);
			}
			catch(BaseLib::HTTPClientException& ex)
			{
				if(ignoreErrors) return;
				GD::out.printWarning("Warning: Error in UPnP request: " + ex.what());
				GD::out.printMessage("Request was: \n" + request);
				serviceMessages->setUnreach(true, false);
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

void SonosPeer::execute(std::string& functionName, std::string& service, std::string& path, std::shared_ptr<std::vector<std::pair<std::string, std::string>>>& soapValues, bool ignoreErrors)
{
	try
	{
		std::string soapRequest;
		std::string headerSoapRequest = service + '#' + functionName;
		SonosPacket packet(_ip, path, headerSoapRequest, service, functionName, soapValues);
		packet.getSoapRequest(soapRequest);
		sendSoapRequest(soapRequest);
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

void SonosPeer::execute(std::string functionName, bool ignoreErrors)
{
	try
	{
		UpnpFunctions::iterator functionEntry = _upnpFunctions.find(functionName);
		if(functionEntry == _upnpFunctions.end())
		{
			GD::out.printError("Error: Tried to execute unknown function: " + functionName);
			return;
		}
		std::string soapRequest;
		std::string headerSoapRequest = functionEntry->second.service() + '#' + functionName;
		SonosPacket packet(_ip, functionEntry->second.path(), headerSoapRequest, functionEntry->second.service(), functionName, functionEntry->second.soapValues());
		packet.getSoapRequest(soapRequest);
		sendSoapRequest(soapRequest, ignoreErrors);
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

void SonosPeer::execute(std::string functionName, PSoapValues soapValues, bool ignoreErrors)
{
	try
	{
		UpnpFunctions::iterator functionEntry = _upnpFunctions.find(functionName);
		if(functionEntry == _upnpFunctions.end())
		{
			GD::out.printError("Error: Tried to execute unknown function: " + functionName);
			return;
		}
		std::string soapRequest;
		std::string headerSoapRequest = functionEntry->second.service() + '#' + functionName;
		SonosPacket packet(_ip, functionEntry->second.path(), headerSoapRequest, functionEntry->second.service(), functionName, soapValues);
		packet.getSoapRequest(soapRequest);
		sendSoapRequest(soapRequest, ignoreErrors);
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

PVariable SonosPeer::getValueFromDevice(PParameter& parameter, int32_t channel, bool asynchronous)
{
	try
	{
		if(!parameter) return Variable::createError(-32500, "parameter is nullptr.");
		if(parameter->getPackets.empty()) return Variable::createError(-6, "Parameter can't be requested actively.");
		std::string getRequestFrame = parameter->getPackets.front()->id;
		std::string getResponseFrame = parameter->getPackets.front()->responseId;
		if(_rpcDevice->packetsById.find(getRequestFrame) == _rpcDevice->packetsById.end()) return Variable::createError(-6, "No frame was found for parameter " + parameter->id);
		PPacket frame = _rpcDevice->packetsById[getRequestFrame];
		PPacket responseFrame;
		if(_rpcDevice->packetsById.find(getResponseFrame) != _rpcDevice->packetsById.end()) responseFrame = _rpcDevice->packetsById[getResponseFrame];

		if(valuesCentral.find(channel) == valuesCentral.end()) return Variable::createError(-2, "Unknown channel.");
		if(valuesCentral[channel].find(parameter->id) == valuesCentral[channel].end()) return Variable::createError(-5, "Unknown parameter.");

		PParameterGroup parameterGroup = getParameterSet(channel, ParameterGroup::Type::Enum::variables);
		if(!parameterGroup) return Variable::createError(-3, "Unknown parameter set.");

		std::shared_ptr<std::vector<std::pair<std::string, std::string>>> soapValues(new std::vector<std::pair<std::string, std::string>>());
		for(JsonPayloads::iterator i = frame->jsonPayloads.begin(); i != frame->jsonPayloads.end(); ++i)
		{
			if((*i)->constValueInteger > -1)
			{
				if((*i)->key.empty()) continue;
				soapValues->push_back(std::pair<std::string, std::string>((*i)->key, std::to_string((*i)->constValueInteger)));
				continue;
			}
			else if(!(*i)->constValueString.empty())
			{
				if((*i)->key.empty()) continue;
				soapValues->push_back(std::pair<std::string, std::string>((*i)->key, (*i)->constValueString));
				continue;
			}

			bool paramFound = false;
			for(Parameters::iterator j = parameterGroup->parameters.begin(); j != parameterGroup->parameters.end(); ++j)
			{
				if((*i)->parameterId == j->second->physical->groupId)
				{
					if((*i)->key.empty()) continue;
					soapValues->push_back(std::pair<std::string, std::string>((*i)->key, _binaryDecoder->decodeResponse((valuesCentral[channel][j->second->id].data))->toString()));
					paramFound = true;
					break;
				}
			}
			if(!paramFound) GD::out.printError("Error constructing packet. param \"" + (*i)->parameterId + "\" not found. Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frame->id);
		}

		std::string soapRequest;
		SonosPacket packet(_ip, frame->metaString1, frame->function1, frame->metaString2, frame->function2, soapValues);
		packet.getSoapRequest(soapRequest);
		if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: Sending SOAP request:\n" + soapRequest);
		if(_httpClient)
		{
			std::string response;
			try
			{
				_httpClient->sendRequest(soapRequest, response);
				if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: SOAP response:\n" + response);
				std::shared_ptr<SonosPacket> responsePacket(new SonosPacket(response));
				packetReceived(responsePacket);
				serviceMessages->setUnreach(false, true);
			}
			catch(BaseLib::HTTPClientException& ex)
			{
				return Variable::createError(-100, "Error sending value to Sonos device: " + ex.what());
				serviceMessages->setUnreach(true, false);
			}
		}

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

PParameterGroup SonosPeer::getParameterSet(int32_t channel, ParameterGroup::Type::Enum type)
{
	try
	{
		PParameterGroup parameterGroup = _rpcDevice->functions.at(channel)->getParameterGroup(type);
		if(parameterGroup->parameters.empty())
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

PVariable SonosPeer::getDeviceInfo(int32_t clientID, std::map<std::string, bool> fields)
{
	try
	{
		PVariable info(Peer::getDeviceInfo(clientID, fields));
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

PVariable SonosPeer::getParamset(int32_t clientID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		if(remoteChannel < 0) remoteChannel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel.");
		if(type == ParameterGroup::Type::none) type = ParameterGroup::Type::link;
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(parameterGroup || parameterGroup->parameters.empty()) return Variable::createError(-3, "Unknown parameter set.");
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
				return Variable::createError(-3, "Parameter set type is not supported.");
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

PVariable SonosPeer::getParamsetDescription(int32_t clientID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel.");
		if(type == ParameterGroup::Type::none) type = ParameterGroup::Type::link;
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(parameterGroup || parameterGroup->parameters.empty()) return Variable::createError(-3, "Unknown parameter set.");
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

PVariable SonosPeer::putParamset(int32_t clientID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool onlyPushing)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(!_centralFeatures) return Variable::createError(-2, "Not a central peer.");
		if(channel < 0) channel = 0;
		if(remoteChannel < 0) remoteChannel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel.");
		if(type == ParameterGroup::Type::none) type = ParameterGroup::Type::link;
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(parameterGroup || parameterGroup->parameters.empty()) return Variable::createError(-3, "Unknown parameter set.");
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

			if(!changedParameters.empty() && !changedParameters.begin()->second.empty()) raiseRPCUpdateDevice(_peerID, channel, _serialNumber + ":" + std::to_string(channel), 0);
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

PVariable SonosPeer::setValue(int32_t clientID, uint32_t channel, std::string valueKey, PVariable value)
{
	try
	{
		Peer::setValue(clientID, channel, valueKey, value); //Ignore result, otherwise setHomegerValue might not be executed
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(!_centralFeatures) return Variable::createError(-2, "Not a central peer.");
		if(valueKey.empty()) return Variable::createError(-5, "Value key is empty.");
		if(valuesCentral.find(channel) == valuesCentral.end()) return Variable::createError(-2, "Unknown channel.");
		if(setHomegearValue(channel, valueKey, value)) return PVariable(new Variable(VariableType::tVoid));
		if(valuesCentral[channel].find(valueKey) == valuesCentral[channel].end()) return Variable::createError(-5, "Unknown parameter.");
		PParameter rpcParameter = valuesCentral[channel][valueKey].rpcParameter;
		if(!rpcParameter) return Variable::createError(-5, "Unknown parameter.");
		if(rpcParameter->service)
		{
			if(channel == 0 && value->type == VariableType::tBoolean)
			{
				if(serviceMessages->set(valueKey, value->booleanValue)) return PVariable(new Variable(VariableType::tVoid));
			}
			else if(value->type == VariableType::tInteger) serviceMessages->set(valueKey, value->integerValue, channel);
		}
		if(rpcParameter->logical->type == ILogical::Type::tAction && !value->booleanValue) return Variable::createError(-5, "Parameter of type action cannot be set to \"false\".");
		if(!rpcParameter->writeable && clientID != -1 && !(rpcParameter->addonWriteable && raiseIsAddonClient(clientID) == 1)) return Variable::createError(-6, "parameter is read only");
		BaseLib::Systems::RPCConfigurationParameter* parameter = &valuesCentral[channel][valueKey];
		std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string>());
		std::shared_ptr<std::vector<PVariable>> values(new std::vector<PVariable>());

		rpcParameter->convertToPacket(value, parameter->data);
		value = rpcParameter->convertFromPacket(parameter->data, false);
		if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
		else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, parameter->data);

		valueKeys->push_back(valueKey);
		values->push_back(value);

		if(rpcParameter->physical->operationType == IPhysical::OperationType::Enum::command)
		{
			if(rpcParameter->setPackets.empty()) return Variable::createError(-6, "parameter is read only");
			std::string setRequest = rpcParameter->setPackets.front()->id;
			if(_rpcDevice->packetsById.find(setRequest) == _rpcDevice->packetsById.end()) return Variable::createError(-6, "No frame was found for parameter " + valueKey);
			PPacket frame = _rpcDevice->packetsById[setRequest];

			std::shared_ptr<std::vector<std::pair<std::string, std::string>>> soapValues(new std::vector<std::pair<std::string, std::string>>());
			for(JsonPayloads::iterator i = frame->jsonPayloads.begin(); i != frame->jsonPayloads.end(); ++i)
			{
				if((*i)->constValueInteger > -1)
				{
					if((*i)->key.empty()) continue;
					soapValues->push_back(std::pair<std::string, std::string>((*i)->key, std::to_string((*i)->constValueInteger)));
					continue;
				}
				else if(!(*i)->constValueString.empty())
				{
					if((*i)->key.empty()) continue;
					soapValues->push_back(std::pair<std::string, std::string>((*i)->key, (*i)->constValueString));
					continue;
				}
				if((*i)->parameterId == rpcParameter->physical->groupId)
				{
					if((*i)->key.empty()) continue;
					soapValues->push_back(std::pair<std::string, std::string>((*i)->key, _binaryDecoder->decodeResponse(parameter->data)->toString()));
				}
				//Search for all other parameters
				else
				{
					bool paramFound = false;
					for(std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>::iterator j = valuesCentral[channel].begin(); j != valuesCentral[channel].end(); ++j)
					{
						if(!j->second.rpcParameter) continue;
						if((*i)->parameterId == j->second.rpcParameter->physical->groupId)
						{
							if((*i)->key.empty()) continue;
							soapValues->push_back(std::pair<std::string, std::string>((*i)->key, _binaryDecoder->decodeResponse(j->second.data)->toString()));
							paramFound = true;
							break;
						}
					}
					if(!paramFound) GD::out.printError("Error constructing packet. param \"" + (*i)->parameterId + "\" not found. Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frame->id);
				}
			}

			std::string soapRequest;
			SonosPacket packet(_ip, frame->metaString1, frame->function1, frame->metaString2, frame->function2, soapValues);
			packet.getSoapRequest(soapRequest);
			if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: Sending SOAP request:\n" + soapRequest);
			if(_httpClient)
			{
				std::string response;
				try
				{
					_httpClient->sendRequest(soapRequest, response);
					if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: SOAP response:\n" + response);
				}
				catch(BaseLib::HTTPClientException& ex)
				{
					GD::out.printWarning("Warning: Error in UPnP request: " + ex.what());
					GD::out.printMessage("Request was: \n" + soapRequest);
					return Variable::createError(-100, "Error sending value to Sonos device: " + ex.what());
				}

			}
		}
		else if(rpcParameter->physical->operationType != IPhysical::OperationType::Enum::store) return Variable::createError(-6, "Only interface types \"store\" and \"command\" are supported for this device family.");

		raiseEvent(_peerID, channel, valueKeys, values);
		raiseRPCEvent(_peerID, channel, _serialNumber + ":" + std::to_string(channel), valueKeys, values);

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

void SonosPeer::playLocalFile(std::string filename, bool now, bool unmute, int32_t volume)
{
	try
	{
		if(filename.size() < 5) return;
		std::string tempPath = GD::bl->settings.tempPath() + "sonos/";

		if(now)
		{
			execute("GetPositionInfo");
			execute("GetVolume");
			execute("GetMute");
			execute("GetTransportInfo");
		}

		std::string playlistFilename = filename.substr(0, filename.size() - 4) + ".m3u";
		BaseLib::HelperFunctions::stringReplace(playlistFilename, "/", "_");

		std::string playlistContent = "#EXTM3U\n#EXTINF:0,<Homegear><TTS><TTS>\nhttp://" + GD::physicalInterface->listenAddress() + ':' + std::to_string(GD::physicalInterface->listenPort()) + '/' + filename + '\n';
		std::string playlistFilepath = tempPath + playlistFilename;
		BaseLib::Io::writeFile(playlistFilepath, playlistContent);
		playlistFilename = BaseLib::HTTP::encodeURL(playlistFilename);

		std::string currentTrackUri;
		std::string currentTrackMetadata;
		std::string currentTransportUri;
		std::string currentTransportMetadata;
		std::string transportState;
		int32_t volumeState = -1;
		bool muteState = false;
		int32_t trackNumberState = -1;
		std::string seekTimeState;

		std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>>::iterator channelTwoIterator = valuesCentral.find(2);
		if(channelTwoIterator == valuesCentral.end())
		{
			GD::out.printError("Error: Channel 2 not found.");
			return;
		}
		if(now)
		{
			std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>::iterator parameterIterator = channelTwoIterator->second.find("CURRENT_TRACK_URI");
			if(parameterIterator != channelTwoIterator->second.end())
			{
				PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
				if(variable) currentTrackUri = variable->stringValue;
			}

			parameterIterator = channelTwoIterator->second.find("CURRENT_TRACK_METADATA");
			if(parameterIterator != channelTwoIterator->second.end())
			{
				PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
				if(variable) currentTrackMetadata = variable->stringValue;
			}

			parameterIterator = channelTwoIterator->second.find("AV_TRANSPORT_URI");
			if(parameterIterator != channelTwoIterator->second.end())
			{
				PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
				if(variable) currentTransportUri = variable->stringValue;
			}

			parameterIterator = channelTwoIterator->second.find("AV_TRANSPORT_METADATA");
			if(parameterIterator != channelTwoIterator->second.end())
			{
				PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
				if(variable) currentTransportMetadata = variable->stringValue;
			}

			parameterIterator = channelTwoIterator->second.find("VOLUME");
			if(parameterIterator != channelTwoIterator->second.end())
			{
				PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
				if(variable) volumeState = variable->integerValue;
			}

			parameterIterator = channelTwoIterator->second.find("MUTE");
			if(parameterIterator != channelTwoIterator->second.end())
			{
				PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
				if(variable) muteState = (variable->stringValue == "1");
			}

			parameterIterator = channelTwoIterator->second.find("CURRENT_TRACK");
			if(parameterIterator != channelTwoIterator->second.end())
			{
				PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
				if(variable) trackNumberState = variable->integerValue;
			}

			parameterIterator = channelTwoIterator->second.find("CURRENT_TRACK_RELATIVE_TIME");
			if(parameterIterator != channelTwoIterator->second.end())
			{
				PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
				if(variable) seekTimeState = variable->stringValue;
			}

			parameterIterator = channelTwoIterator->second.find("TRANSPORT_STATE");
			if(parameterIterator != channelTwoIterator->second.end())
			{
				PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
				if(variable) transportState = variable->stringValue;
			}

			execute("Pause", true);

			if(unmute) execute("SetMute", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0"), SoapValuePair("Channel", "Master"), SoapValuePair("DesiredMute", "0") }));
			if(volume > 0) execute("SetVolume", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0"), SoapValuePair("Channel", "Master"), SoapValuePair("DesiredVolume", std::to_string(volume)) }));
		}

		std::string playlistUri("http://" + GD::physicalInterface->listenAddress() + ':' + std::to_string(GD::physicalInterface->listenPort()) + '/' + playlistFilename);
		execute("AddURIToQueue", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0"), SoapValuePair("EnqueuedURI", playlistUri), SoapValuePair("EnqueuedURIMetaData", ""), SoapValuePair("DesiredFirstTrackNumberEnqueued", "0"), SoapValuePair("EnqueueAsNext", "0") }));

		if(now)
		{
			std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>>::iterator channelOneIterator = valuesCentral.find(1);
			std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>::iterator parameterIterator = channelOneIterator->second.find("FIRST_TRACK_NUMBER_ENQUEUED");
			int32_t trackNumber = 0;
			if(parameterIterator != channelOneIterator->second.end())
			{
				PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
				if(variable) trackNumber = variable->integerValue;
			}

			execute("Seek", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0"), SoapValuePair("Unit", "TRACK_NR"), SoapValuePair("Target", std::to_string(trackNumber)) }));
			execute("Play");

			while(!serviceMessages->getUnreach())
			{
				execute("GetPositionInfo");

				std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>::iterator parameterIterator = channelTwoIterator->second.find("CURRENT_TRACK");
				if(parameterIterator != channelTwoIterator->second.end())
				{
					PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
					if(!variable || trackNumber != variable->integerValue) break;
				}
				else break;

				execute("GetTransportInfo");

				parameterIterator = channelTwoIterator->second.find("TRANSPORT_STATE");
				if(parameterIterator != channelTwoIterator->second.end())
				{
					PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
					if(!variable || (variable->stringValue != "PLAYING" && variable->stringValue != "TRANSITIONING")) break;
				}
				else break;
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
			}

			execute("Pause", true);
			execute("SetMute", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0"), SoapValuePair("Channel", "Master"), SoapValuePair("DesiredMute", std::to_string((int32_t)muteState)) }));
			execute("Seek", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0"), SoapValuePair("Unit", "TRACK_NR"), SoapValuePair("Target", std::to_string(trackNumberState)) }));
			execute("Seek", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0"), SoapValuePair("Unit", "REL_TIME"), SoapValuePair("Target", seekTimeState) }));
			execute("RemoveTrackFromQueue", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0"), SoapValuePair("ObjectID", "Q:0/" + std::to_string(trackNumber)) }));

			if(transportState == "PLAYING")
			{
				execute("SetVolume", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0"), SoapValuePair("Channel", "Master"), SoapValuePair("DesiredVolume", "0") }));
				execute("Play");
				execute("RampToVolume", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0"), SoapValuePair("Channel", "Master"), SoapValuePair("RampType", "AUTOPLAY_RAMP_TYPE"), SoapValuePair("DesiredVolume", std::to_string(volumeState)), SoapValuePair("ResetVolumeAfter", "false"), SoapValuePair("ProgramURI", "") }));
			}
			else execute("SetVolume", PSoapValues(new SoapValues{ SoapValuePair("InstanceID", "0"), SoapValuePair("Channel", "Master"), SoapValuePair("DesiredVolume", std::to_string(volumeState)) }));
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

bool SonosPeer::setHomegearValue(uint32_t channel, std::string valueKey, PVariable value)
{
	try
	{
		if(valueKey == "PLAY_TTS")
		{
			if(value->stringValue.empty()) return true;
			std::string ttsProgram = GD::physicalInterface->ttsProgram();
			if(ttsProgram.empty())
			{
				GD::out.printError("Error: No program to generate TTS audio file specified in physicalinterfaces.conf");
				return true;
			}

			bool unmute = true;
			int32_t volume = -1;
			std::string language;

			std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>>::iterator channelOneIterator = valuesCentral.find(1);
			if(channelOneIterator == valuesCentral.end())
			{
				GD::out.printError("Error: Channel 1 not found.");
				return true;
			}

			std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>::iterator parameterIterator = channelOneIterator->second.find("PLAY_TTS_UNMUTE");
			if(parameterIterator != channelOneIterator->second.end())
			{
				PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
				if(variable) unmute = variable->booleanValue;
			}

			parameterIterator = channelOneIterator->second.find("PLAY_TTS_VOLUME");
			if(parameterIterator != channelOneIterator->second.end())
			{
				PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
				if(variable) volume = variable->integerValue;
			}

			parameterIterator = channelOneIterator->second.find("PLAY_TTS_LANGUAGE");
			if(parameterIterator != channelOneIterator->second.end())
			{
				PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
				if(variable) language = variable->stringValue;
				if(!BaseLib::HelperFunctions::isAlphaNumeric(language))
				{
					GD::out.printError("Error: Language is not alphanumeric.");
					language = "en";
				}
			}

			std::string audioPath = GD::bl->settings.tempPath() + "sonos/";
			std::string filename;
			BaseLib::HelperFunctions::stringReplace(value->stringValue, "\"", "");
			std::string execPath = ttsProgram + ' ' + language + " \"" + value->stringValue + "\"";
			if(BaseLib::HelperFunctions::exec(execPath, filename) != 0)
			{
				GD::out.printError("Error: Error executing program to generate TTS audio file: \"" + ttsProgram + ' ' + language + ' ' + value->stringValue + "\"");
				return true;
			}
			BaseLib::HelperFunctions::trim(filename);
			if(!BaseLib::Io::fileExists(filename))
			{
				GD::out.printError("Error: Error executing program to generate TTS audio file: File not found. Output needs to be the full path to the TTS audio file, but was: \"" + filename + "\"");
				return true;
			}
			if(filename.size() <= audioPath.size() || filename.compare(0, audioPath.size(), audioPath) != 0)
			{
				GD::out.printError("Error: Error executing program to generate TTS audio file. Output needs to be the full path to the TTS audio file and the file needs to be within \"" + audioPath + "\". Returned path was: \"" + filename + "\"");
				return true;
			}
			filename = filename.substr(audioPath.size());

			playLocalFile(filename, true, unmute, volume);

			return true;
		}
		else if(valueKey == "PLAY_AUDIO_FILE")
		{
			if(value->stringValue.empty()) return true;

			bool unmute = true;
			int32_t volume = -1;

			std::unordered_map<uint32_t, std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>>::iterator channelOneIterator = valuesCentral.find(1);
			if(channelOneIterator == valuesCentral.end())
			{
				GD::out.printError("Error: Channel 1 not found.");
				return true;
			}

			std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>::iterator parameterIterator = channelOneIterator->second.find("PLAY_AUDIO_FILE_UNMUTE");
			if(parameterIterator != channelOneIterator->second.end())
			{
				PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
				if(variable) unmute = variable->booleanValue;
			}

			parameterIterator = channelOneIterator->second.find("PLAY_AUDIO_FILE_VOLUME");
			if(parameterIterator != channelOneIterator->second.end())
			{
				PVariable variable = _binaryDecoder->decodeResponse(parameterIterator->second.data);
				if(variable) volume = variable->integerValue;
			}

			std::string audioPath = GD::physicalInterface->dataPath();
			if(!BaseLib::Io::fileExists(audioPath + value->stringValue))
			{
				GD::out.printError("Error: Can't stream audio file \"" + audioPath + value->stringValue + "\". File not found.");
				return true;
			}

			playLocalFile(value->stringValue, true, unmute, volume);

			return true;
		}
		else if(valueKey == "ENQUEUE_AUDIO_FILE")
		{
			if(value->stringValue.empty()) return true;

			std::string audioPath = GD::physicalInterface->dataPath();
			if(!BaseLib::Io::fileExists(audioPath + value->stringValue))
			{
				GD::out.printError("Error: Can't stream audio file \"" + audioPath + value->stringValue + "\". File not found.");
				return true;
			}

			playLocalFile(value->stringValue, false, false, -1);

			return true;
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

}
