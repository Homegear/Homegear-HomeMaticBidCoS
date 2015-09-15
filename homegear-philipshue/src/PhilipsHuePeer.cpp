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

#include "PhilipsHuePeer.h"
#include "LogicalDevices/PhilipsHueCentral.h"
#include "GD.h"

namespace PhilipsHue
{
std::shared_ptr<BaseLib::Systems::Central> PhilipsHuePeer::getCentral()
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

std::shared_ptr<BaseLib::Systems::LogicalDevice> PhilipsHuePeer::getDevice(int32_t address)
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

PhilipsHuePeer::PhilipsHuePeer(uint32_t parentID, bool centralFeatures, IPeerEventSink* eventHandler) : Peer(GD::bl, parentID, centralFeatures, eventHandler)
{
	_binaryEncoder.reset(new BaseLib::RPC::RPCEncoder(GD::bl));
	_binaryDecoder.reset(new BaseLib::RPC::RPCDecoder(GD::bl));
}

PhilipsHuePeer::PhilipsHuePeer(int32_t id, int32_t address, std::string serialNumber, uint32_t parentID, bool centralFeatures, IPeerEventSink* eventHandler) : Peer(GD::bl, id, address, serialNumber, parentID, centralFeatures, eventHandler)
{
	_binaryEncoder.reset(new BaseLib::RPC::RPCEncoder(GD::bl));
	_binaryDecoder.reset(new BaseLib::RPC::RPCDecoder(GD::bl));
}

PhilipsHuePeer::~PhilipsHuePeer()
{
	dispose();
}

std::string PhilipsHuePeer::handleCLICommand(std::string command)
{
	try
	{
		std::ostringstream stringStream;

		if(command == "help")
		{
			stringStream << "List of commands:" << std::endl << std::endl;
			stringStream << "For more information about the individual command type: COMMAND help" << std::endl << std::endl;
			stringStream << "unselect\t\tUnselect this peer" << std::endl;
			return stringStream.str();
		}
		return "Unknown command.\n";
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

void PhilipsHuePeer::save(bool savePeer, bool variables, bool centralConfig)
{
	try
	{
		Peer::save(savePeer, variables, centralConfig);
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

void PhilipsHuePeer::loadVariables(BaseLib::Systems::LogicalDevice* device, std::shared_ptr<BaseLib::Database::DataTable> rows)
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

bool PhilipsHuePeer::load(BaseLib::Systems::LogicalDevice* device)
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

void PhilipsHuePeer::saveVariables()
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

PParameterGroup PhilipsHuePeer::getParameterSet(int32_t channel, ParameterGroup::Type::Enum type)
{
	try
	{
		PParameterGroup parameterGroup = _rpcDevice->functions.at(channel)->getParameterGroup(type);
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

void PhilipsHuePeer::getValuesFromPacket(std::shared_ptr<PhilipsHuePacket> packet, std::vector<FrameValues>& frameValues)
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
			int32_t channel = -1;
			if(frame->channel > -1) channel = frame->channel;
			currentFrameValues.frameID = frame->id;

			for(JsonPayloads::iterator j = frame->jsonPayloads.begin(); j != frame->jsonPayloads.end(); ++j)
			{
				PVariable json = packet->getJson();
				if(!json) continue;
				if(json->structValue->find((*j)->key) == json->structValue->end()) continue;
				json = json->structValue->operator []((*j)->key);
				if(!(*j)->subkey.empty())
				{
					if(json->structValue->find((*j)->subkey) == json->structValue->end()) continue;
					json = json->structValue->operator[]((*j)->subkey);
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

					if(setValues)
					{
						//This is a little nasty and costs a lot of resources, but we need to run the data through the packet converter
						std::vector<uint8_t> encodedData;
						_binaryEncoder->encodeResponse(json, encodedData);
						PVariable data = (*k)->convertFromPacket(encodedData, true);
						(*k)->convertToPacket(data, currentFrameValues.values[(*k)->id].value);
					}
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

void PhilipsHuePeer::packetReceived(std::shared_ptr<PhilipsHuePacket> packet)
{
	try
	{
		if(!packet) return;
		if(!_centralFeatures || _disposing) return;
		if(packet->senderAddress() != _address) return;
		if(!_rpcDevice) return;
		std::shared_ptr<PhilipsHueCentral> central = std::dynamic_pointer_cast<PhilipsHueCentral>(getCentral());
		if(!central) return;
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

			for(std::map<std::string, FrameValue>::iterator i = a->values.begin(); i != a->values.end(); ++i)
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
								if(parameter->rpcParameter->id == "REACHABLE")
								{
									bool value = !((bool)i->second.value.at(i->second.value.size() - 1));
									serviceMessages->setUnreach(value, false);
								}
								else serviceMessages->set(i->first, (bool)i->second.value.at(i->second.value.size() - 1));
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

				for(std::vector<std::string>::iterator i = j->second->begin(); i != j->second->end(); ++i)
				{
					if((*i == "HUE" || *i == "SATURATION" || *i == "BRIGHTNESS") //Calculate RGB
						&& valuesCentral.at(j->first).find("HUE") != valuesCentral.at(j->first).end()) //Does this peer support colors?
					{
						uint8_t brightness = _binaryDecoder->decodeResponse(valuesCentral.at(j->first).at("BRIGHTNESS").data)->integerValue;
						uint8_t saturation = _binaryDecoder->decodeResponse(valuesCentral.at(j->first).at("SATURATION").data)->integerValue;
						int32_t hue = _binaryDecoder->decodeResponse(valuesCentral.at(j->first).at("HUE").data)->integerValue;

						BaseLib::Color::HSV hsv((double)hue / getHueFactor(hue), (double)saturation / 255.0, (double)brightness / 255.0);

						PVariable rpcRGB(new Variable(hsv.toRGB().toString()));
						RPCConfigurationParameter* rgbParameter = &valuesCentral.at(j->first).at("RGB");
						_binaryEncoder->encodeResponse(rpcRGB, rgbParameter->data);
						if(rgbParameter->databaseID > 0) saveParameter(rgbParameter->databaseID, rgbParameter->data);
						else saveParameter(0, ParameterGroup::Type::Enum::variables, j->first, "RGB", rgbParameter->data);

						j->second->push_back("RGB");
						rpcValues[j->first]->push_back(rgbParameter->rpcParameter->convertFromPacket(rgbParameter->data, true));
						break;
					}
				}

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

std::string PhilipsHuePeer::getFirmwareVersionString(int32_t firmwareVersion)
{
	try
	{
		return std::to_string(firmwareVersion);
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

void PhilipsHuePeer::initializeConversionMatrix()
{
	try
	{
		if(_rgbGamut.getA().x == 0)
		{
			if(_deviceType.type() == (uint32_t)DeviceType::LCT001)
			{
				_rgbGamut.setA(BaseLib::Math::Point2D(0.675, 0.322));
				_rgbGamut.setB(BaseLib::Math::Point2D(0.4091, 0.518));
				_rgbGamut.setC(BaseLib::Math::Point2D(0.167, 0.04));
			}
			else
			{
				_rgbGamut.setA(BaseLib::Math::Point2D(0.704, 0.296));
				_rgbGamut.setB(BaseLib::Math::Point2D(0.2151, 0.7106));
				_rgbGamut.setC(BaseLib::Math::Point2D(0.138, 0.08));
			}

			BaseLib::Color::getConversionMatrix(_rgbGamut, _rgbXyzConversionMatrix, _xyzRgbConversionMatrix);
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

void PhilipsHuePeer::getXY(const std::string& rgb, BaseLib::Math::Point2D& xy, uint8_t& brightness)
{
	try
	{
		initializeConversionMatrix();

		BaseLib::Color::RGB cRGB(rgb);
		BaseLib::Color::NormalizedRGB nRGB(cRGB);

		double nBrightness = 0;
		BaseLib::Color::rgbToCie1931Xy(nRGB, _rgbXyzConversionMatrix, _gamma, xy, nBrightness);
		brightness = (cRGB.opacityDefined()) ? cRGB.getOpacity() : std::lround(nBrightness * 100) + 155;

		BaseLib::Math::Point2D closestPoint;
		_rgbGamut.distance(xy, &closestPoint);
		xy.x = closestPoint.x;
		xy.y = closestPoint.y;
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

void PhilipsHuePeer::getRGB(const BaseLib::Math::Point2D& xy, const uint8_t& brightness, std::string& rgb)
{
	try
	{
		initializeConversionMatrix();

		BaseLib::Math::Point2D closestPoint;
		_rgbGamut.distance(xy, &closestPoint);
		BaseLib::Math::Point2D xy2;
		xy2.x = closestPoint.x;
		xy2.y = closestPoint.y;

		double nBrightness = (double)brightness / 255;

		BaseLib::Color::NormalizedRGB nRGB;
		BaseLib::Color::cie1931XyToRgb(xy2, nBrightness, _xyzRgbConversionMatrix, _gamma, nRGB);

		BaseLib::Color::RGB cRGB(nRGB);
		rgb = cRGB.toString();
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

double PhilipsHuePeer::getHueFactor(const int32_t& hue)
{
	//       Color   +30°
	if(hue < 18000 + 9000) //Red to yellow
	{
		return 300;
	}
	else if(hue < 25500 + 6375) //Yellow to green
	{
		return 212.5;
	}
	else if(hue < 36207 + 6035) //Green to cyan
	{
		return 201.15;
	}
	else if(hue < 46920 + 5865) //Cyan to blue
	{
		return 195.5;
	}
	else if(hue < 56100 + 5.610) //Blue to pink
	{
		return 187;
	}
	else
	{
		return 182.04;
	}
}

double PhilipsHuePeer::getHueFactor(const double& hue)
{
	if(hue < 90) //Red to yellow
	{
		return 300;
	}
	else if(hue < 150) //Yellow to green
	{
		return 212.5;
	}
	else if(hue < 210) //Green to cyan
	{
		return 201.15;
	}
	else if(hue < 270) //Cyan to blue
	{
		return 195.5;
	}
	else if(hue < 330) //Blue to pink
	{
		return 187;
	}
	else //Pink to red
	{
		return 182.04;
	}
}

//RPC Methods
PVariable PhilipsHuePeer::getDeviceInfo(int32_t clientID, std::map<std::string, bool> fields)
{
	try
	{
		PVariable info(Peer::getDeviceInfo(clientID, fields));
		if(info->errorStruct) return info;

		if(fields.empty() || fields.find("INTERFACE") != fields.end()) info->structValue->insert(StructElement("INTERFACE", PVariable(new Variable(GD::physicalInterface->getID()))));

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

PVariable PhilipsHuePeer::getParamsetDescription(int32_t clientID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel");
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(!parameterGroup || parameterGroup->parameters.empty()) return Variable::createError(-3, "Unknown parameter set");

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

PVariable PhilipsHuePeer::putParamset(int32_t clientID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool onlyPushing)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(!_centralFeatures) return Variable::createError(-2, "Not a central peer.");
		if(channel < 0) channel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel");
		PParameterGroup parameterGroup = functionIterator->second->getParameterGroup(type);
		if(!parameterGroup || parameterGroup->parameters.empty()) return Variable::createError(-3, "Unknown parameter set");
		if(variables->structValue->empty()) return PVariable(new Variable(VariableType::tVoid));

		if(type == ParameterGroup::Type::Enum::variables)
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

PVariable PhilipsHuePeer::getParamset(int32_t clientID, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel)
{
	try
	{
		if(_disposing) return Variable::createError(-32500, "Peer is disposing.");
		if(channel < 0) channel = 0;
		if(remoteChannel < 0) remoteChannel = 0;
		Functions::iterator functionIterator = _rpcDevice->functions.find(channel);
		if(functionIterator == _rpcDevice->functions.end()) return Variable::createError(-2, "Unknown channel");
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

PVariable PhilipsHuePeer::setValue(int32_t clientID, uint32_t channel, std::string valueKey, PVariable value)
{
	return setValue(clientID, channel, valueKey, value, false);
}

PVariable PhilipsHuePeer::setValue(int32_t clientID, uint32_t channel, std::string valueKey, PVariable value, bool noSending)
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
		BaseLib::Systems::RPCConfigurationParameter* parameter = &parameterIterator->second;
		std::shared_ptr<std::vector<std::string>> valueKeys(new std::vector<std::string>());
		std::shared_ptr<std::vector<PVariable>> values(new std::vector<PVariable>());

		if(valueKey == "RGB") //Special case, because it sets two parameters (XY and BRIGHTNESS)
		{
			BaseLib::Color::RGB cRGB(value->stringValue);
			BaseLib::Color::NormalizedRGB nRGB(cRGB);
			BaseLib::Color::HSV hsv = nRGB.toHSV();

			PVariable result;
			uint8_t brightness = std::lround(hsv.getBrightness() * 255.0);
			if(brightness < 10) result = setValue(clientID, channel, "STATE", PVariable(new Variable(false)), true);
			else result = setValue(clientID, channel, "STATE", PVariable(new Variable(true)), true);
			if(result->errorStruct) return result;
			result = setValue(clientID, channel, "BRIGHTNESS", PVariable(new Variable((int32_t)brightness)), true);
			if(result->errorStruct) return result;
			int32_t hue = std::lround(hsv.getHue() * getHueFactor(hsv.getHue()));
			result = setValue(clientID, channel, "HUE", PVariable(new Variable(hue)), true);
			if(result->errorStruct) return result;
			uint8_t saturation = std::lround(hsv.getSaturation() * 255.0);
			result = setValue(clientID, channel, "SATURATION", PVariable(new Variable((int32_t)saturation)), false);
			if(result->errorStruct) return result;

			//Convert back, because the value might be different than the passed one.
			value->stringValue = hsv.toRGB().toString();
			_binaryEncoder->encodeResponse(value, parameter->data);
			if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
			else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, parameter->data);
			valueKeys->push_back(valueKey);
			values->push_back(value);
			if(!valueKeys->empty())
			{
				raiseEvent(_peerID, channel, valueKeys, values);
				raiseRPCEvent(_peerID, channel, _serialNumber + ":" + std::to_string(channel), valueKeys, values);
			}
			return PVariable(new Variable(VariableType::tVoid));
		}

		if(rpcParameter->physical->operationType == IPhysical::OperationType::Enum::store)
		{
			rpcParameter->convertToPacket(value, parameter->data);
			if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
			else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, parameter->data);
			value = rpcParameter->convertFromPacket(parameter->data, false);
			if(rpcParameter->readable)
			{
				valueKeys->push_back(valueKey);
				values->push_back(value);
			}
			if(!valueKeys->empty()) raiseRPCEvent(_peerID, channel, _serialNumber + ":" + std::to_string(channel), valueKeys, values);
			return PVariable(new Variable(VariableType::tVoid));
		}
		else if(rpcParameter->physical->operationType != IPhysical::OperationType::Enum::command) return Variable::createError(-6, "Parameter is not settable.");
		if(rpcParameter->setPackets.empty()) return Variable::createError(-6, "parameter is read only");
		std::string setRequest = rpcParameter->setPackets.front()->id;
		PacketsById::iterator packetIterator = _rpcDevice->packetsById.find(setRequest);
		if(packetIterator == _rpcDevice->packetsById.end()) return Variable::createError(-6, "No frame was found for parameter " + valueKey);
		PPacket frame = packetIterator->second;
		rpcParameter->convertToPacket(value, parameter->data);
		if(parameter->databaseID > 0) saveParameter(parameter->databaseID, parameter->data);
		else saveParameter(0, ParameterGroup::Type::Enum::variables, channel, valueKey, parameter->data);
		if(_bl->debugLevel > 4) GD::out.printDebug("Debug: " + valueKey + " of peer " + std::to_string(_peerID) + " with serial number " + _serialNumber + ":" + std::to_string(channel) + " was set to " + BaseLib::HelperFunctions::getHexString(parameter->data) + ".");

		value = rpcParameter->convertFromPacket(parameter->data, false);
		if(rpcParameter->readable)
		{
			valueKeys->push_back(valueKey);
			values->push_back(value);
		}

		if(!noSending)
		{
			PVariable json(new Variable(VariableType::tStruct));
			for(JsonPayloads::iterator i = frame->jsonPayloads.begin(); i != frame->jsonPayloads.end(); ++i)
			{
				if((*i)->constValueIntegerSet)
				{
					if((*i)->key.empty()) continue;
					PVariable fieldElement;
					if((*i)->subkey.empty()) json->structValue->operator[]((*i)->key) = PVariable(new Variable((*i)->constValueInteger));
					else  json->structValue->operator[]((*i)->key)->structValue->operator[]((*i)->subkey) = PVariable(new Variable((*i)->constValueInteger));
					continue;
				}
				if((*i)->constValueBooleanSet)
				{
					if((*i)->key.empty()) continue;
					if((*i)->subkey.empty()) json->structValue->operator[]((*i)->key) = PVariable(new Variable((bool)(*i)->constValueInteger));
					else  json->structValue->operator[]((*i)->key)->structValue->operator[]((*i)->subkey) = PVariable(new Variable((bool)(*i)->constValueInteger));
					continue;
				}
				//We can't just search for param, because it is ambiguous (see for example LEVEL for HM-CC-TC).
				if((*i)->parameterId == rpcParameter->physical->groupId)
				{
					if((*i)->key.empty()) continue;
					if((*i)->subkey.empty()) json->structValue->operator[]((*i)->key) = _binaryDecoder->decodeResponse(parameter->data);
					else  json->structValue->operator[]((*i)->key)->structValue->operator[]((*i)->subkey) = _binaryDecoder->decodeResponse(parameter->data);
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
							if((*i)->subkey.empty()) json->structValue->operator[]((*i)->key) = _binaryDecoder->decodeResponse(j->second.data);
							else  json->structValue->operator[]((*i)->key)->structValue->operator[]((*i)->subkey) = _binaryDecoder->decodeResponse(j->second.data);
							paramFound = true;
							break;
						}
					}
					if(!paramFound) GD::out.printError("Error constructing packet. param \"" + (*i)->parameterId + "\" not found. Peer: " + std::to_string(_peerID) + " Serial number: " + _serialNumber + " Frame: " + frame->id);
				}
			}

			std::shared_ptr<PhilipsHueCentral> central = std::dynamic_pointer_cast<PhilipsHueCentral>(getCentral());
			std::shared_ptr<PhilipsHuePacket> packet(new PhilipsHuePacket(central->getAddress(), _address, frame->type, json));
			if(central) central->sendPacket(packet);
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
