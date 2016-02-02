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

#include "Interfaces.h"
#include "GD.h"
#include "PhysicalInterfaces/IBidCoSInterface.h"
#include "PhysicalInterfaces/COC.h"
#include "PhysicalInterfaces/Cul.h"
#include "PhysicalInterfaces/Cunx.h"
#include "PhysicalInterfaces/TICC1100.h"
#include "PhysicalInterfaces/HM-CFG-LAN.h"
#include "PhysicalInterfaces/HM-LGW.h"

namespace BidCoS
{

Interfaces::Interfaces(BaseLib::Obj* bl, std::vector<std::shared_ptr<Systems::PhysicalInterfaceSettings>> physicalInterfaceSettings) : Systems::PhysicalInterfaces(bl, GD::family->getFamily(), physicalInterfaceSettings)
{
	create();
}

Interfaces::~Interfaces()
{
}

void Interfaces::create()
{
	try
	{

		for(std::vector<std::shared_ptr<Systems::PhysicalInterfaceSettings>>::iterator i = _physicalInterfaceSettings.begin(); i != _physicalInterfaceSettings.end(); ++i)
		{
			std::shared_ptr<IBidCoSInterface> device;
			if(!*i) continue;
			GD::out.printDebug("Debug: Creating physical device. Type defined in homematicbidcos.conf is: " + (*i)->type);
			if((*i)->type == "cul") device.reset(new Cul(*i));
			else if((*i)->type == "coc") device.reset(new COC(*i));
			else if((*i)->type == "cunx") device.reset(new Cunx(*i));
	#ifdef SPIINTERFACES
			else if((*i)->type == "cc1100") device.reset(new TICC1100(*i));
	#endif
			else if((*i)->type == "hmcfglan") device.reset(new HM_CFG_LAN(*i));
			else if((*i)->type == "hmlgw") device.reset(new HM_LGW(*i));
			else GD::out.printError("Error: Unsupported physical device type: " + (*i)->type);
			if(device)
			{
				if(_physicalInterfaces.find((*i)->id) != _physicalInterfaces.end()) GD::out.printError("Error: id used for two devices: " + (*i)->id);
				_physicalInterfaces[(*i)->id] = device;
				GD::physicalInterfaces[(*i)->id] = device;
				if((*i)->isDefault || !GD::defaultPhysicalInterface) GD::defaultPhysicalInterface = device;
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

}
