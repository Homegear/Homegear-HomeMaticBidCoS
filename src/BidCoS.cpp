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

#include "BidCoS.h"
#include "HomeMaticCentral.h"
#include "Interfaces.h"
#include "BidCoSDeviceTypes.h"
#include "homegear-base/BaseLib.h"
#include "GD.h"

namespace BidCoS
{
BidCoS::BidCoS(BaseLib::Obj* bl, BaseLib::Systems::DeviceFamily::IFamilyEventSink* eventHandler) : BaseLib::Systems::DeviceFamily(bl, eventHandler, BIDCOS_FAMILY_ID, "HomeMatic BidCoS")
{
	GD::bl = bl;
	GD::family = this;
	GD::out.init(bl);
	GD::out.setPrefix("Module HomeMatic BidCoS: ");
	GD::out.printDebug("Debug: Loading module...");
	GD::rpcDevices.init(_bl, this);
	_physicalInterfaces.reset(new Interfaces(bl, _settings->getPhysicalInterfaceSettings()));
}

BidCoS::~BidCoS()
{

}

bool BidCoS::init()
{
	GD::out.printInfo("Loading XML RPC devices...");
	GD::rpcDevices.load();
	if(GD::rpcDevices.empty()) return false;
	return true;
}

void BidCoS::dispose()
{
	if(_disposed) return;
	DeviceFamily::dispose();

	GD::physicalInterfaces.clear();
	GD::defaultPhysicalInterface.reset();
	_central.reset();
	GD::rpcDevices.clear();
}

std::shared_ptr<BaseLib::Systems::ICentral> BidCoS::getCentral() { return _central; }

void BidCoS::createCentral()
{
	try
	{
		if(_central) return;

		int32_t address = (0xfd << 16) + BaseLib::HelperFunctions::getRandomNumber(0, 0xFFFF);
		int32_t seedNumber = BaseLib::HelperFunctions::getRandomNumber(1, 9999999);
		std::ostringstream stringstream;
		stringstream << "VBC" << std::setw(7) << std::setfill('0') << std::dec << seedNumber;
		std::string serialNumber(stringstream.str());

		_central.reset(new HomeMaticCentral(0, serialNumber, address, this));
		GD::out.printMessage("Created HomeMatic BidCoS central with id " + std::to_string(_central->getId()) + ", address 0x" + BaseLib::HelperFunctions::getHexString(address, 6) + " and serial number " + serialNumber);
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

void BidCoS::load()
{
	try
	{
		std::shared_ptr<BaseLib::Database::DataTable> rows = _bl->db->getDevices((uint32_t)getFamily());
		for(BaseLib::Database::DataTable::iterator row = rows->begin(); row != rows->end(); ++row)
		{
			uint32_t deviceID = row->second.at(0)->intValue;
			GD::out.printMessage("Loading HomeMatic BidCoS device " + std::to_string(deviceID));
			int32_t address = row->second.at(1)->intValue;
			std::string serialNumber = row->second.at(2)->textValue;
			uint32_t deviceType = row->second.at(3)->intValue;

			if(deviceType == 0xFFFFFFFD)
			{
				_central = std::shared_ptr<HomeMaticCentral>(new HomeMaticCentral(deviceID, serialNumber, address, this));
				_central->load();
				_central->loadPeers();
			}
		}
		if(!GD::physicalInterfaces.empty())
		{
			if(!_central) createCentral();
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

std::string BidCoS::handleCliCommand(std::string& command)
{
	try
	{
		std::ostringstream stringStream;
		if(!_central) return "Error: No central exists.\n";
		return _central->handleCliCommand(command);
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

PVariable BidCoS::getPairingMethods()
{
	try
	{
		if(!_central) return PVariable(new Variable(VariableType::tArray));
		PVariable array(new Variable(VariableType::tArray));
		array->arrayValue->push_back(PVariable(new Variable(std::string("addDevice"))));
		array->arrayValue->push_back(PVariable(new Variable(std::string("setInstallMode"))));
		return array;
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
}
