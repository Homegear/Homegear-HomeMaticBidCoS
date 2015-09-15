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

#include "EasyCamPeer.h"

#include "GD.h"
#include "EasyCamPacket.h"
#include "LogicalDevices/EasyCamCentral.h"

namespace EasyCam
{
std::shared_ptr<BaseLib::Systems::Central> EasyCamPeer::getCentral()
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

std::shared_ptr<BaseLib::Systems::LogicalDevice> EasyCamPeer::getDevice(int32_t address)
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

EasyCamPeer::EasyCamPeer(uint32_t parentID, bool centralFeatures, IPeerEventSink* eventHandler) : BaseLib::Systems::Peer(GD::bl, parentID, centralFeatures, eventHandler)
{
	init();
}

EasyCamPeer::EasyCamPeer(int32_t id, std::string serialNumber, uint32_t parentID, bool centralFeatures, IPeerEventSink* eventHandler) : BaseLib::Systems::Peer(GD::bl, id, -1, serialNumber, parentID, centralFeatures, eventHandler)
{
	init();
}

EasyCamPeer::~EasyCamPeer()
{
	try
	{
		dispose();
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

void EasyCamPeer::init()
{
	_binaryEncoder.reset(new BaseLib::RPC::RPCEncoder(_bl));
	_binaryDecoder.reset(new BaseLib::RPC::RPCDecoder(_bl));
	_httpClient.reset(new BaseLib::HTTPClient(_bl, "easycam", 65635, false));
	raiseAddWebserverEventHandler(this);
	std::string httpOkHeader("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n");
	_httpOkHeader.insert(_httpOkHeader.end(), httpOkHeader.begin(), httpOkHeader.end());
}

void EasyCamPeer::dispose()
{
	if(_disposing) return;
	Peer::dispose();
	GD::out.printInfo("Info: Removing Webserver hooks. If Homegear hangs here, Sockets are still open.");
	removeHooks();
}

void EasyCamPeer::removeHooks()
{
	raiseRemoveWebserverEventHandler();
}

void EasyCamPeer::homegearStarted()
{
	try
	{
		Peer::homegearStarted();
		raiseAddWebserverEventHandler(this);
		initHttpClient();
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

void EasyCamPeer::homegearShuttingDown()
{
	try
	{
		_shuttingDown = true;
		Peer::homegearShuttingDown();
		removeHooks();
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

std::string EasyCamPeer::handleCLICommand(std::string command)
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

std::string EasyCamPeer::printConfig()
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


void EasyCamPeer::loadVariables(BaseLib::Systems::LogicalDevice* device, std::shared_ptr<BaseLib::Database::DataTable> rows)
{
	try
	{
		if(!rows) rows = raiseGetPeerVariables();
		Peer::loadVariables(device, rows);
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

bool EasyCamPeer::load(BaseLib::Systems::LogicalDevice* device)
{
	try
	{
		loadVariables((EasyCamDevice*)device);

		_rpcDevice = GD::rpcDevices.find(_deviceType, _firmwareVersion, -1);
		if(!_rpcDevice)
		{
			GD::out.printError("Error loading EasyCam peer " + std::to_string(_peerID) + ": Device type not found: 0x" + BaseLib::HelperFunctions::getHexString((uint32_t)_deviceType.type()) + " Firmware version: " + std::to_string(_firmwareVersion));
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

void EasyCamPeer::saveVariables()
{
	try
	{
		if(_peerID == 0) return;
		Peer::saveVariables();
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

PVariable EasyCamPeer::getValueFromDevice(PParameter& parameter, int32_t channel, bool asynchronous)
{
	try
	{
		if(!parameter) return Variable::createError(-32500, "parameter is nullptr.");
		if(parameter->getPackets.empty()) return Variable::createError(-6, "Parameter can't be requested actively.");
		std::string getRequestPacket = parameter->getPackets.at(0)->id;
		PacketsById::iterator packetIterator = _rpcDevice->packetsById.find(getRequestPacket);
		if(packetIterator == _rpcDevice->packetsById.end()) return Variable::createError(-6, "No frame was found for parameter " + parameter->id);
		PPacket packet = _rpcDevice->packetsById[getRequestPacket];

		if(valuesCentral.find(channel) == valuesCentral.end()) return Variable::createError(-2, "Unknown channel.");
		if(valuesCentral[channel].find(parameter->id) == valuesCentral[channel].end()) return Variable::createError(-5, "Unknown parameter.");

		PParameterGroup parameterGroup = getParameterSet(channel, ParameterGroup::Type::Enum::variables);
		if(!parameterGroup) return Variable::createError(-3, "Unknown parameter set.");

		std::shared_ptr<std::vector<std::pair<std::string, std::string>>> getValues(new std::vector<std::pair<std::string, std::string>>());
		for(HttpPayloads::iterator i = packet->httpPayloads.begin(); i != packet->httpPayloads.end(); ++i)
		{
			if((*i)->constValueInteger > -1)
			{
				if((*i)->key.empty()) continue;
				getValues->push_back(std::pair<std::string, std::string>((*i)->key, std::to_string((*i)->constValueInteger)));
				continue;
			}
			else if(!(*i)->constValueString.empty())
			{
				if((*i)->key.empty()) continue;
				getValues->push_back(std::pair<std::string, std::string>((*i)->key, (*i)->constValueString));
				continue;
			}

			bool paramFound = false;
			for(std::unordered_map<std::string, BaseLib::Systems::RPCConfigurationParameter>::iterator j = valuesCentral[channel].begin(); j != valuesCentral[channel].end(); ++j)
			{
				if(!j->second.rpcParameter) continue;
				if((*i)->parameterId == j->second.rpcParameter->physical->groupId)
				{
					if((*i)->key.empty()) continue;
					getValues->push_back(std::pair<std::string, std::string>((*i)->key, _binaryDecoder->decodeResponse(j->second.data)->toString()));
					paramFound = true;
					break;
				}
			}
			if(!paramFound) GD::out.printError("Error constructing packet. param \"" + (*i)->parameterId + "\" not found. Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + packet->id);
		}

		EasyCamPacket easycamPacket(_baseUrl, packet->function1, packet->function2, _username, _password, getValues);
		std::string httpRequest;
		easycamPacket.getHttpRequest(httpRequest);
		if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: Sending HTTP request:\n" + httpRequest);
		std::map<std::string, std::string> result;
		if(_httpClient)
		{
			std::string response;
			try
			{
				_httpClient->sendRequest(httpRequest, response);
				if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: HTTP response:\n" + response);
				if(parseCgiResult(response, result) != 0)
				{
					return Variable::createError(-100, "Error sending value to EasyCam: Error code received.");
				}
				std::map<std::string, std::string>::iterator resultIterator = result.find("getResult");
				if(resultIterator != result.end())
				{
					if(resultIterator->second == "-1") return Variable::createError(-101, "Error getting value: Unknown value.");
					else if(resultIterator->second != "0") return Variable::createError(-101, "Error adding value: Unknown error.");
					result.erase(resultIterator);
				}
			}
			catch(BaseLib::HTTPClientException& ex)
			{
				GD::out.printWarning("Warning: Error sending HTTP request: " + ex.what());
				GD::out.printMessage("Request was: \n" + httpRequest);
				return Variable::createError(-100, "Error sending value to EasyCam: " + ex.what());
			}

		}

		if(result.empty()) return PVariable(new Variable(VariableType::tVoid));

		PVariable resultVariable;
		if(result.size() == 1) resultVariable = Variable::fromString(result.begin()->second, parameter->physical->type);
		else
		{
			std::string value;
			for(std::map<std::string, std::string>::iterator i = result.begin(); i != result.end(); ++i)
			{
				value += i->first + '=' + i->second + ';';
			}
			resultVariable.reset(new Variable(value));
		}
		BaseLib::Systems::RPCConfigurationParameter* configParameter = &valuesCentral[channel][parameter->id];
		parameter->convertToPacket(resultVariable, configParameter->data);
		if(configParameter->databaseID > 0) saveParameter(configParameter->databaseID, configParameter->data);
		else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, parameter->id, configParameter->data);

		PVariable returnValue = parameter->convertFromPacket(valuesCentral[channel][parameter->id].data, false);
		std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string>{ parameter->id });
		std::shared_ptr<std::vector<PVariable>> values(new std::vector<PVariable> { returnValue });
		std::string address(_serialNumber + ':' + std::to_string(channel));
		raiseEvent(_peerID, channel, valueKeys, values);
		raiseRPCEvent(_peerID, channel, address, valueKeys, values);

		return returnValue;
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

PParameterGroup EasyCamPeer::getParameterSet(int32_t channel, ParameterGroup::Type::Enum type)
{
	try
	{
		PFunction rpcChannel = _rpcDevice->functions.at(channel);
		if(type == ParameterGroup::Type::Enum::variables) return rpcChannel->variables;
		else if(type == ParameterGroup::Type::Enum::config) return rpcChannel->configParameters;
		else if(type == ParameterGroup::Type::Enum::link) return rpcChannel->linkParameters;
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

// {{{ Webserver events
	bool EasyCamPeer::onGet(BaseLib::Rpc::PServerInfo& serverInfo, BaseLib::HTTP& httpRequest, std::shared_ptr<BaseLib::SocketOperations>& socket, std::string& path)
	{
		if(path == "/easycam/" + std::to_string(_peerID) + "/stream.mjpeg")
		{
			if(_ip.empty())
			{
				GD::out.printWarning("Warning: Can't open stream for peer with id " + std::to_string(_peerID) + ": IP address is empty.");
				return false;
			}
			BaseLib::SocketOperations cameraSocket(_bl, _ip, std::to_string(_port), _useSsl, _caFile, _verifyCertificate);
			try
			{
				std::string response;
				try
				{
					std::string request = "GET /cgi-bin/CGIProxy.fcgi?cmd=setSubStreamFormat&format=1&usr=" + HTTP::encodeURL(_username) + "&pwd=" + HTTP::encodeURL(_password) + " HTTP/1.1\r\nUser-Agent: Homegear\r\nHost: " + _ip + ":" + std::to_string(_port) + "\r\nConnection: " + "Close" + "\r\n\r\n";
					_httpClient->sendRequest(request, response, false);
					std::map<std::string, std::string> cgiResult;
					if(parseCgiResult(response, cgiResult) != 0)
					{
						GD::out.printWarning("Warning: Could not set stream format to MJPEG.");
					}
					if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: HTTP response:\n" + response);
					serviceMessages->setUnreach(false, true);
				}
				catch(BaseLib::HTTPClientException& ex)
				{
					GD::out.printWarning("Warning: Error sending value to EasyCam: " + ex.what());
					serviceMessages->setUnreach(true, false);
				}
				cameraSocket.open();
				cameraSocket.setReadTimeout(30000000);
				int32_t bufferMax = 2048;
				char buffer[bufferMax + 1];
				ssize_t receivedBytes;
				std::string getRequest = "GET /cgi-bin/CGIStream.cgi?cmd=GetMJStream&usr=" + HTTP::encodeURL(_username) + "&pwd=" + HTTP::encodeURL(_password) + " HTTP/1.1\r\nUser-Agent: Homegear\r\nHost: " + _ip + ":" + std::to_string(_port) + "\r\nConnection: " + "Close" + "\r\n";
				for(std::map<std::string, std::string>::iterator i = httpRequest.getHeader()->fields.begin(); i != httpRequest.getHeader()->fields.end(); ++i)
				{
					if(i->first == "user-agent" || i->first == "host" || i->first == "connection") continue;
					getRequest += i->first + ": " + i->second + "\r\n";
				}
				getRequest += "\r\n";
				cameraSocket.proofwrite(getRequest);
				while(!_disposing && !deleting && !_shuttingDown)
				{
					receivedBytes = cameraSocket.proofread(buffer, bufferMax);
					socket->proofwrite(buffer, receivedBytes);
				}
				cameraSocket.close();
				socket->close();
			}
			catch(BaseLib::SocketDataLimitException& ex)
			{
				GD::out.printWarning("Warning: " + ex.what());
			}
			catch(BaseLib::SocketClosedException& ex)
			{
				GD::out.printInfo("Info: " + ex.what());
			}
			catch(const BaseLib::SocketOperationException& ex)
			{
				GD::out.printError("Error: " + ex.what());
			}
			return true;
		}
		else if(path == "/easycam/" + std::to_string(_peerID) + "/snapshot.jpg")
		{
			if(_ip.empty())
			{
				GD::out.printWarning("Warning: Can't open stream for peer with id " + std::to_string(_peerID) + ": IP address is empty.");
				return false;
			}
			try
			{
				std::string getRequest = "GET /cgi-bin/CGIProxy.fcgi?cmd=snapPicture2&usr=" + HTTP::encodeURL(_username) + "&pwd=" + HTTP::encodeURL(_password) + " HTTP/1.1\r\nUser-Agent: Homegear\r\nHost: " + _ip + ":" + std::to_string(_port) + "\r\nConnection: " + "Close" + "\r\n\r\n";
				HTTP response;
				_httpClient->sendRequest(getRequest, response, false);
				socket->proofwrite(response.getRawHeader());
				socket->proofwrite(response.getContent());
			}
			catch(BaseLib::HTTPClientException& ex)
			{
				GD::out.printWarning("Warning" + ex.what());
			}
			return true;
		}
		else if(path == "/easycam/" + std::to_string(_peerID) + "/motion")
		{
			try
			{
				socket->proofwrite(_httpOkHeader);
				socket->close();
			}
			catch(BaseLib::SocketDataLimitException& ex)
			{
				GD::out.printWarning("Warning: " + ex.what());
			}
			catch(const BaseLib::SocketOperationException& ex)
			{
				GD::out.printError("Error: " + ex.what());
			}
			BaseLib::Systems::RPCConfigurationParameter* parameter = &valuesCentral[1]["MOTION"];
			if(!parameter->rpcParameter) return true;
			parameter->data.clear();
			parameter->data.push_back(1);
			if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
			else saveParameter(0, ParameterGroup::Type::Enum::variables, 1, "MOTION", parameter->data);
			if(_bl->debugLevel >= 4) GD::out.printInfo("Info: MOTION of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":1 was set.");
			std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string>{ "MOTION" });
			std::shared_ptr<std::vector<PVariable>> values(new std::vector<PVariable> { parameter->rpcParameter->convertFromPacket(parameter->data, true) });
			std::string address(_serialNumber + ":1");
			raiseEvent(_peerID, 1, valueKeys, values);
			raiseRPCEvent(_peerID, 1, address, valueKeys, values);
			return true;
		}
		return false;
	}
// }}}

void EasyCamPeer::initHttpClient()
{
	try
	{
		BaseLib::Systems::RPCConfigurationParameter* parameter = &configCentral[0]["IP_ADDRESS"];
		if(parameter->rpcParameter) _ip = parameter->rpcParameter->convertFromPacket(parameter->data)->stringValue;
		parameter = &configCentral[0]["PORT"];
		if(parameter->rpcParameter) _port = parameter->rpcParameter->convertFromPacket(parameter->data)->integerValue;
		parameter = &configCentral[0]["USE_SSL"];
		if(parameter->rpcParameter) _useSsl = parameter->rpcParameter->convertFromPacket(parameter->data)->booleanValue;
		parameter = &configCentral[0]["CA_FILE"];
		if(parameter->rpcParameter) _caFile = parameter->rpcParameter->convertFromPacket(parameter->data)->stringValue;
		parameter = &configCentral[0]["VERIFY_CERTIFICATE"];
		if(parameter->rpcParameter) _verifyCertificate = parameter->rpcParameter->convertFromPacket(parameter->data)->booleanValue;
		parameter = &configCentral[0]["USER"];
		if(parameter->rpcParameter) _username = parameter->rpcParameter->convertFromPacket(parameter->data)->stringValue;
		parameter = &configCentral[0]["PASSWORD"];
		if(parameter->rpcParameter) _password = parameter->rpcParameter->convertFromPacket(parameter->data)->stringValue;

		if(_ip.empty())
		{
			GD::out.printWarning("Warning: Can't init HTTP client of peer with id " + std::to_string(_peerID) + ": IP address is empty.");
			return;
		}

		_baseUrl = _ip + ':' + std::to_string(_port);
		_httpClient.reset(new BaseLib::HTTPClient(_bl, _ip, _port, false, _useSsl, _caFile, _verifyCertificate));

		parameter = &valuesCentral[1]["STREAM_URL"];
		if(parameter->rpcParameter && _bl->rpcPort != 0)
		{

			BaseLib::PVariable variable = parameter->rpcParameter->convertFromPacket(parameter->data, true);
			std::string newPrefix("http://" + GD::physicalInterface->listenAddress() + (GD::bl->rpcPort != 80 ? ":" + std::to_string(GD::bl->rpcPort) : "") + "/easycam/" + std::to_string(_peerID) + "/");
			std::string newStreamUrl(newPrefix + "stream.mjpeg");
			if(variable->stringValue != newStreamUrl)
			{
				variable->stringValue = newStreamUrl;
				parameter->rpcParameter->convertToPacket(variable, parameter->data);
				if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
				else saveParameter(0, ParameterGroup::Type::Enum::variables, 1, "STREAM_URL", parameter->data);
				std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string>{ "STREAM_URL" });
				std::shared_ptr<std::vector<PVariable>> values(new std::vector<PVariable> { variable });
				std::string address(_serialNumber + ":1");
				if(_bl->debugLevel >= 4) GD::out.printInfo("Info: STREAM_URL of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":1 was set to " + variable->stringValue + ".");

				parameter = &valuesCentral[1]["SNAPSHOT_URL"];
				if(parameter->rpcParameter)
				{
					variable = PVariable(new BaseLib::Variable(newPrefix + "snapshot.jpg"));
					parameter->rpcParameter->convertToPacket(variable, parameter->data);
					if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
					else saveParameter(0, ParameterGroup::Type::Enum::variables, 1, "SNAPSHOT_URL", parameter->data);
					valueKeys->push_back("SNAPSHOT_URL");
					values->push_back(variable);
					std::string address(_serialNumber + ":1");
					if(_bl->debugLevel >= 4) GD::out.printInfo("Info: SNAPSHOT_URL of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":1 was set to " + variable->stringValue + ".");
				}

				raiseEvent(_peerID, 1, valueKeys, values);
				raiseRPCEvent(_peerID, 1, address, valueKeys, values);
			}
		}

		registerMotionCallback();
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

void EasyCamPeer::registerMotionCallback()
{
	try
	{
		if(_bl->rpcPort == 0)
		{
			GD::out.printWarning("Warning: Can't register EasyCam callback server. No RPC server without SSL is available.");
			return;
		}
		std::string response;
		try
		{
			std::string request = "GET /cgi-bin/CGIProxy.fcgi?cmd=setESMotionDetectParam&isEnable=1&httpProtocol=0&httpPort=" + std::to_string(_bl->rpcPort) + "&hostname=" + GD::physicalInterface->listenAddress() + "&requestPage=/easycam/" + std::to_string(_peerID) + "/motion" + "&type=0&isAuthentication=0&optionalParam1=&optionalValue1=&optionalParam2=&optionalValue2=&optionalParam3=&optionalValue3=&optionalParam4=&optionalValue4=&optionalParam5=&optionalValue5=&optionalBody=&usr=" + HTTP::encodeURL(_username) + "&pwd=" + HTTP::encodeURL(_password) + " HTTP/1.1\r\nUser-Agent: Homegear\r\nHost: " + _ip + ":" + std::to_string(_port) + "\r\nConnection: " + "Close" + "\r\n\r\n";
			_httpClient->sendRequest(request, response, false);
			std::map<std::string, std::string> cgiResult;
			if(parseCgiResult(response, cgiResult) != 0)
			{
				GD::out.printWarning("Warning: Could not set stream format to MJPEG.");
			}
			if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: HTTP response:\n" + response);
			serviceMessages->setUnreach(false, true);
		}
		catch(BaseLib::HTTPClientException& ex)
		{
			GD::out.printWarning("Warning: Error sending value to EasyCam: " + ex.what());
			serviceMessages->setUnreach(true, false);
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

int32_t EasyCamPeer::parseCgiResult(std::string& data, std::map<std::string, std::string>& result)
{
	try
	{
		int32_t resultCode = 0;
		result.clear();
		if(data.empty()) return -1;
		xml_document<> doc;
		doc.parse<parse_no_entity_translation | parse_validate_closing_tags>(&data.at(0));
		for(xml_node<>* node = doc.first_node(); node; node = node->next_sibling())
		{
			std::string name(node->name());
			if(name == "CGI_Result")
			{
				for(xml_node<>* subNode = node->first_node(); subNode; subNode = subNode->next_sibling())
				{
					std::string nodeName(subNode->name());
					std::string nodeValue(subNode->value());
					if(nodeName == "result") resultCode = BaseLib::Math::getNumber(nodeValue, false);
					else result[nodeName] = nodeValue;
				}
			}
			else
			{
				GD::out.printWarning("Unknown CGI root element: " + name);
				resultCode = -1;
				break;
			}
		}
		return resultCode;
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

PVariable EasyCamPeer::getDeviceInfo(int32_t clientID, std::map<std::string, bool> fields)
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

PVariable EasyCamPeer::getParamset(int32_t clientID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		if(remoteChannel < 0) remoteChannel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel.");
		if(type == ParameterGroup::Type::none) type = ParameterGroup::Type::link;
		PFunction rpcFunction = functionIterator->second;
		if(!rpcFunction->parameterSetDefined(type)) return Variable::createError(-3, "Unknown parameter set.");
		PParameterGroup parameterGroup = rpcFunction->getParameterGroup(type);
		if(!parameterGroup) return Variable::createError(-3, "Unknown parameter set.");
		PVariable variables(new Variable(VariableType::tStruct));

		for(Parameters::iterator i = parameterGroup->parameters.begin(); i != parameterGroup->parameters.end(); ++i)
		{
			if(!i->second || i->second->id.empty() || !i->second->visible) continue;
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

PVariable EasyCamPeer::getParamsetDescription(int32_t clientID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
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

PVariable EasyCamPeer::getValue(int32_t clientID, uint32_t channel, std::string valueKey, bool requestFromDevice, bool asynchronous)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(!_rpcDevice) return Variable::createError(-32500, "Unknown application error.");
		if(valuesCentral.find(channel) == valuesCentral.end()) return Variable::createError(-2, "Unknown channel.");
		if(valuesCentral[channel].find(valueKey) == valuesCentral[channel].end()) return Variable::createError(-5, "Unknown parameter.");
		if(_rpcDevice->functions.find(channel) == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel.");
		PFunction rpcFunction = _rpcDevice->functions.at(channel);
		PParameterGroup parameterGroup = getParameterSet(channel, ParameterGroup::Type::Enum::variables);
		if(parameterGroup->parameters.empty()) return Variable::createError(-3, "Unknown parameter set.");
		PParameter parameter = parameterGroup->parameters.at(valueKey);
		if(!parameter) return Variable::createError(-5, "Unknown parameter.");
		if(!parameter->readable) return Variable::createError(-6, "Parameter is not readable.");

		if(parameter->getPackets.empty()) return parameter->convertFromPacket(valuesCentral[channel][valueKey].data);
		else return getValueFromDevice(parameter, channel, asynchronous);
	}
	catch(const std::exception& ex)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_bl->out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return Variable::createError(-32500, "Unknown application error.");
}

PVariable EasyCamPeer::putParamset(int32_t clientID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool onlyPushing)
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
		if(!parameterGroup || parameterGroup->parameters.empty()) return Variable::createError(-3, "Unknown parameter set.");
		if(variables->structValue->empty()) return PVariable(new Variable(VariableType::tVoid));

		if(type == ParameterGroup::Type::Enum::config)
		{
			bool reloadHttpClient = false;
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

				if(channel == 0 && (i->first == "IP_ADDRESS" || i->first == "PORT" || i->first == "USE_SSL" || i->first == "CA_FILE" || i->first == "VERIFY_CERTIFICATE" || i->first == "USER" || i->first == "PASSWORD")) reloadHttpClient = true;

				GD::out.printInfo("Info: Parameter " + i->first + " of peer " + std::to_string(_peerID) + " and channel " + std::to_string(channel) + " was set to 0x" + BaseLib::HelperFunctions::getHexString(allParameters[list][intIndex]) + ".");
				//Only send to device when parameter is of type config
				if(parameter->rpcParameter->physical->operationType != IPhysical::OperationType::Enum::config && parameter->rpcParameter->physical->operationType != IPhysical::OperationType::Enum::configString) continue;
				changedParameters[list][intIndex] = allParameters[list][intIndex];
			}

			if(reloadHttpClient) initHttpClient();
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

PVariable EasyCamPeer::setValue(int32_t clientID, uint32_t channel, std::string valueKey, PVariable value)
{
	try
	{
		Peer::setValue(clientID, channel, valueKey, value); //Ignore result, otherwise setHomegerValue might not be executed
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(!_centralFeatures) return Variable::createError(-2, "Not a central peer.");
		if(valueKey.empty()) return Variable::createError(-5, "Value key is empty.");
		if(valuesCentral.find(channel) == valuesCentral.end()) return Variable::createError(-2, "Unknown channel.");
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
		value = _binaryDecoder->decodeResponse(parameter->data);
		if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
		else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, parameter->data);

		valueKeys->push_back(valueKey);
		values->push_back(rpcParameter->convertFromPacket(parameter->data, false));

		if(rpcParameter->physical->operationType == IPhysical::OperationType::Enum::command && !rpcParameter->setPackets.empty())
		{
			if(rpcParameter->setPackets.empty()) return Variable::createError(-6, "parameter is read only");
			std::string setRequest = rpcParameter->setPackets.front()->id;
			if(_rpcDevice->packetsById.find(setRequest) == _rpcDevice->packetsById.end()) return Variable::createError(-6, "No frame was found for parameter " + valueKey);
			PPacket frame = _rpcDevice->packetsById[setRequest];

			std::shared_ptr<std::vector<std::pair<std::string, std::string>>> getValues(new std::vector<std::pair<std::string, std::string>>());
			for(HttpPayloads::iterator i = frame->httpPayloads.begin(); i != frame->httpPayloads.end(); ++i)
			{
				if((*i)->constValueInteger > -1)
				{
					if((*i)->key.empty()) continue;
					getValues->push_back(std::pair<std::string, std::string>((*i)->key, std::to_string((*i)->constValueInteger)));
					continue;
				}
				else if(!(*i)->constValueString.empty())
				{
					if((*i)->key.empty()) continue;
					getValues->push_back(std::pair<std::string, std::string>((*i)->key, (*i)->constValueString));
					continue;
				}
				if((*i)->parameterId == rpcParameter->physical->groupId)
				{
					if((*i)->key.empty()) continue;
					getValues->push_back(std::pair<std::string, std::string>((*i)->key, _binaryDecoder->decodeResponse(parameter->data)->toString()));
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
							getValues->push_back(std::pair<std::string, std::string>((*i)->key, _binaryDecoder->decodeResponse(j->second.data)->toString()));
							paramFound = true;
							break;
						}
					}
					if(!paramFound) GD::out.printError("Error constructing packet. param \"" + (*i)->parameterId + "\" not found. Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frame->id);
				}
			}

			EasyCamPacket packet(_baseUrl, frame->function1, frame->function2, _username, _password, getValues);
			std::string httpRequest;
			packet.getHttpRequest(httpRequest);
			if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: Sending HTTP request:\n" + httpRequest);
			if(_httpClient)
			{
				std::string response;
				try
				{
					_httpClient->sendRequest(httpRequest, response);
					if(GD::bl->debugLevel >= 5) GD::out.printDebug("Debug: HTTP response:\n" + response);
					std::map<std::string, std::string> result;
					if(parseCgiResult(response, result) != 0)
					{
						return Variable::createError(-100, "Error sending value to EasyCam: Error code received.");
					}
					std::map<std::string, std::string>::iterator resultIterator = result.find("addResult");
					if(resultIterator != result.end())
					{
						if(resultIterator->second == "2") return Variable::createError(-101, "Error adding value: Value already exists.");
						else if(resultIterator->second == "1") return Variable::createError(-101, "Error adding value: Value limit reached.");
						else if(resultIterator->second != "0") return Variable::createError(-101, "Error adding value: Unknown error.");
					}
					resultIterator = result.find("setResult");
					if(resultIterator != result.end())
					{
						if(resultIterator->second == "3") return Variable::createError(-101, "Error adding value: Invalid input.");
						else if(resultIterator->second == "2") return Variable::createError(-101, "Error adding value: Value already exists.");
						else if(resultIterator->second == "1") return Variable::createError(-101, "Error adding value: Value limit reached.");
						else if(resultIterator->second != "0") return Variable::createError(-101, "Error adding value: Unknown error.");
					}
					resultIterator = result.find("delResult");
					if(resultIterator != result.end())
					{
						if(resultIterator->second == "4") return Variable::createError(-101, "Error deleting value: Value cannot be deleted.");
						else if(resultIterator->second != "0") return Variable::createError(-101, "Error deleting value: Unknown error.");
					}
				}
				catch(BaseLib::HTTPClientException& ex)
				{
					GD::out.printWarning("Warning: Error sending HTTP request: " + ex.what());
					GD::out.printMessage("Request was: \n" + httpRequest);
					return Variable::createError(-100, "Error sending value to EasyCam: " + ex.what());
				}

			}
		}
		else if(rpcParameter->physical->operationType != IPhysical::OperationType::Enum::store && rpcParameter->physical->operationType != IPhysical::OperationType::Enum::command) return Variable::createError(-6, "Only interface types \"store\" and \"command\" are supported for this device family.");

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

}
