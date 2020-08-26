/* Copyright 2013-2019 Homegear GmbH
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
#include "HomeMaticCentral.h"
#include "PhysicalInterfaces/IBidCoSInterface.h"
#include "PhysicalInterfaces/COC.h"
#include "PhysicalInterfaces/Cul.h"
#include "PhysicalInterfaces/Cunx.h"
#include "PhysicalInterfaces/TICC1100.h"
#include "PhysicalInterfaces/HM-CFG-LAN.h"
#include "PhysicalInterfaces/HM-LGW.h"
#include "PhysicalInterfaces/Hm-Mod-Rpi-Pcb.h"
#include "PhysicalInterfaces/HomegearGateway.h"
#include "PhysicalInterfaces/Hgdc.h"

#include "../config.h"

namespace BidCoS
{

Interfaces::Interfaces(BaseLib::SharedObjects* bl, std::map<std::string, Systems::PPhysicalInterfaceSettings> physicalInterfaceSettings) : Systems::PhysicalInterfaces(bl, GD::family->getFamily(), physicalInterfaceSettings)
{
	create();
}

Interfaces::~Interfaces()
{
    _physicalInterfaces.clear();
    _defaultPhysicalInterface.reset();
    _physicalInterfaceEventhandlers.clear();
}

void Interfaces::addEventHandlers(BaseLib::Systems::IPhysicalInterface::IPhysicalInterfaceEventSink* central)
{
    try
    {
        std::lock_guard<std::mutex> interfaceGuard(_physicalInterfacesMutex);
        _central = central;
        for(const auto& interface : _physicalInterfaces)
        {
            if(_physicalInterfaceEventhandlers.find(interface.first) != _physicalInterfaceEventhandlers.end()) continue;
            _physicalInterfaceEventhandlers[interface.first] = interface.second->addEventHandler(central);
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void Interfaces::removeEventHandlers()
{
    try
    {
        std::lock_guard<std::mutex> interfaceGuard(_physicalInterfacesMutex);
        for(const auto& interface : _physicalInterfaces)
        {
            auto physicalInterfaceEventhandler = _physicalInterfaceEventhandlers.find(interface.first);
            if(physicalInterfaceEventhandler == _physicalInterfaceEventhandlers.end()) continue;
            interface.second->removeEventHandler(physicalInterfaceEventhandler->second);
            _physicalInterfaceEventhandlers.erase(physicalInterfaceEventhandler);
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void Interfaces::create()
{
	try
	{
		for(std::map<std::string, Systems::PPhysicalInterfaceSettings>::iterator i = _physicalInterfaceSettings.begin(); i != _physicalInterfaceSettings.end(); ++i)
		{
			std::shared_ptr<IBidCoSInterface> device;
			if(!i->second) continue;
			GD::out.printDebug("Debug: Creating physical device. Type defined in homematicbidcos.conf is: " + i->second->type);
			if(i->second->type == "cul") device.reset(new Cul(i->second));
			else if(i->second->type == "coc") device.reset(new COC(i->second));
			else if(i->second->type == "cunx") device.reset(new Cunx(i->second));
	#ifdef SPIINTERFACES
			else if(i->second->type == "cc1100") device.reset(new TICC1100(i->second));
	#endif
			else if(i->second->type == "hmcfglan") device.reset(new HM_CFG_LAN(i->second));
			else if(i->second->type == "hmlgw") device.reset(new HM_LGW(i->second));
			else if(i->second->type == "hm-mod-rpi-pcb") device.reset(new Hm_Mod_Rpi_Pcb(i->second));
			else if(i->second->type == "homegeargateway") device.reset(new HomegearGateway(i->second));
			else GD::out.printError("Error: Unsupported physical device type: " + i->second->type);
			if(device)
			{
				if(_physicalInterfaces.find(i->second->id) != _physicalInterfaces.end()) GD::out.printError("Error: id used for two devices: " + i->second->id);
				_physicalInterfaces[i->second->id] = device;
				if(i->second->isDefault || !_defaultPhysicalInterface) _defaultPhysicalInterface = device;
			}
		}
        if(!_defaultPhysicalInterface) _defaultPhysicalInterface = std::make_shared<IBidCoSInterface>(std::make_shared<BaseLib::Systems::PhysicalInterfaceSettings>());
	}
	catch(const std::exception& ex)
	{
		GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
	}
}

void Interfaces::startListening()
{
    try
    {
        _stopped = false;

        if(GD::bl->hgdc)
        {
            _hgdcModuleUpdateEventHandlerId = GD::bl->hgdc->registerModuleUpdateEventHandler(std::function<void(const BaseLib::PVariable&)>(std::bind(&Interfaces::hgdcModuleUpdate, this, std::placeholders::_1)));
            _hgdcReconnectedEventHandlerId = GD::bl->hgdc->registerReconnectedEventHandler(std::function<void()>(std::bind(&Interfaces::hgdcReconnected, this)));

            createHgdcInterfaces(false);
        }

        PhysicalInterfaces::startListening();
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void Interfaces::stopListening()
{
    try
    {
        _stopped = true;

        if(GD::bl->hgdc)
        {
            GD::bl->hgdc->unregisterModuleUpdateEventHandler(_hgdcModuleUpdateEventHandlerId);
            GD::bl->hgdc->unregisterModuleUpdateEventHandler(_hgdcReconnectedEventHandlerId);
        }

        PhysicalInterfaces::stopListening();
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

std::vector<std::shared_ptr<IBidCoSInterface>> Interfaces::getInterfaces()
{
    std::vector<std::shared_ptr<IBidCoSInterface>> interfaces;
    try
    {
        std::lock_guard<std::mutex> interfaceGuard(_physicalInterfacesMutex);
        interfaces.reserve(_physicalInterfaces.size());
        for(const auto& interfaceBase : _physicalInterfaces)
        {
            std::shared_ptr<IBidCoSInterface> interface(std::dynamic_pointer_cast<IBidCoSInterface>(interfaceBase.second));
            if(!interface) continue;
            if(interface->isOpen()) interfaces.push_back(interface);
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    return interfaces;
}

std::shared_ptr<IBidCoSInterface> Interfaces::getDefaultInterface()
{
    std::lock_guard<std::mutex> interfaceGuard(_physicalInterfacesMutex);
    return _defaultPhysicalInterface;
}

bool Interfaces::hasInterface(const std::string& name)
{
    std::lock_guard<std::mutex> interfaceGuard(_physicalInterfacesMutex);
    auto interfaceBase = _physicalInterfaces.find(name);
    return interfaceBase != _physicalInterfaces.end();
}

std::shared_ptr<IBidCoSInterface> Interfaces::getInterface(const std::string& name)
{
    std::lock_guard<std::mutex> interfaceGuard(_physicalInterfacesMutex);
    auto interfaceBase = _physicalInterfaces.find(name);
    if(interfaceBase == _physicalInterfaces.end()) return _defaultPhysicalInterface;
    std::shared_ptr<IBidCoSInterface> interface(std::dynamic_pointer_cast<IBidCoSInterface>(interfaceBase->second));
    return interface;
}

void Interfaces::hgdcReconnected()
{
    try
    {
        int32_t cycles = BaseLib::HelperFunctions::getRandomNumber(40, 100);
        for(int32_t i = 0; i < cycles; i++)
        {
            if(_stopped) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        _hgdcReconnected = true;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void Interfaces::createHgdcInterfaces(bool reconnected)
{
    try
    {
        if(GD::bl->hgdc)
        {
            std::lock_guard<std::mutex> interfacesGuard(_physicalInterfacesMutex);
            auto modules = GD::bl->hgdc->getModules(BIDCOS_FAMILY_ID);
            if(modules->errorStruct)
            {
                GD::out.printError("Error getting HGDC modules: " + modules->structValue->at("faultString")->stringValue);
            }
            for(auto& module : *modules->arrayValue)
            {
                auto deviceId = module->structValue->at("serialNumber")->stringValue;;

                if(_physicalInterfaces.find(deviceId) == _physicalInterfaces.end())
                {
                    std::shared_ptr<IBidCoSInterface> device;
                    GD::out.printDebug("Debug: Creating HGDC device.");
                    auto settings = std::make_shared<Systems::PhysicalInterfaceSettings>();
                    settings->type = "hgdc";
                    settings->id = deviceId;
                    settings->serialNumber = settings->id;
                    settings->responseDelay = 89;
                    device = std::make_shared<Hgdc>(settings);
                    _physicalInterfaces[settings->id] = device;
                    if(settings->isDefault || !_defaultPhysicalInterface || _defaultPhysicalInterface->getID().empty())
                    {
                        _defaultPhysicalInterface = device;
                        auto central = GD::family->getCentral();
                        std::shared_ptr<HomeMaticCentral> homematicCentral = std::dynamic_pointer_cast<HomeMaticCentral>(central);
                        if(homematicCentral) homematicCentral->changeDefaultInterface();
                    }

                    if(_central)
                    {
                        if(_physicalInterfaceEventhandlers.find(settings->id) != _physicalInterfaceEventhandlers.end()) continue;
                        _physicalInterfaceEventhandlers[settings->id] = device->addEventHandler(_central);
                    }

                    if(reconnected) device->startListening();
                }
                else if(reconnected)
                {
                    //Do nothing
                }
            }
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void Interfaces::hgdcModuleUpdate(const BaseLib::PVariable& modules)
{
    try
    {
        std::lock_guard<std::mutex> interfaceGuard(_physicalInterfacesMutex);
        _updatedHgdcModules = modules;
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void Interfaces::hgdcReconnectedThread()
{
    try
    {
        if(!_hgdcReconnected) return;
        _hgdcReconnected = false;
        createHgdcInterfaces(true);
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void Interfaces::hgdcModuleUpdateThread()
{
    try
    {
        BaseLib::PVariable modules;

        {
            std::lock_guard<std::mutex> interfaceGuard(_physicalInterfacesMutex);
            modules = _updatedHgdcModules;
        }

        if(!modules) return;

        auto addedModules = std::make_shared<std::list<std::shared_ptr<BaseLib::Systems::IPhysicalInterface>>>();

        for(auto& module : *modules->structValue)
        {
            auto familyIdIterator = module.second->structValue->find("familyId");
            if(familyIdIterator == module.second->structValue->end() || familyIdIterator->second->integerValue64 != BIDCOS_FAMILY_ID) continue;

            auto removedIterator = module.second->structValue->find("removed");
            if(removedIterator != module.second->structValue->end())
            {
                std::unique_lock<std::mutex> interfaceGuard(_physicalInterfacesMutex);
                auto interfaceIterator = _physicalInterfaces.find(module.first);
                if(interfaceIterator != _physicalInterfaces.end())
                {
                    auto interface = interfaceIterator->second;
                    interfaceGuard.unlock();
                    interface->stopListening();
                    continue;
                }
            }

            auto addedIterator = module.second->structValue->find("added");
            if(addedIterator != module.second->structValue->end())
            {
                std::unique_lock<std::mutex> interfaceGuard(_physicalInterfacesMutex);
                auto interfaceIterator = _physicalInterfaces.find(module.first);
                if(interfaceIterator == _physicalInterfaces.end())
                {
                    interfaceGuard.unlock();
                    std::shared_ptr<IBidCoSInterface> device;
                    GD::out.printDebug("Debug: Creating HGDC device.");
                    auto settings = std::make_shared<Systems::PhysicalInterfaceSettings>();
                    settings->type = "hgdc";
                    settings->id = module.first;
                    settings->serialNumber = settings->id;
                    device = std::make_shared<Hgdc>(settings);

                    if(_physicalInterfaces.find(settings->id) != _physicalInterfaces.end()) GD::out.printError("Error: id used for two devices: " + settings->id);
                    _physicalInterfaces[settings->id] = device;
                    if(settings->isDefault || !_defaultPhysicalInterface || _defaultPhysicalInterface->getID().empty())
                    {
                        _defaultPhysicalInterface = device;
                        auto central = GD::family->getCentral();
                        std::shared_ptr<HomeMaticCentral> homematicCentral = std::dynamic_pointer_cast<HomeMaticCentral>(central);
                        if(homematicCentral) homematicCentral->changeDefaultInterface();
                    }

                    addedModules->push_back(device);
                }
                else
                {
                    auto interface = interfaceIterator->second;
                    interfaceGuard.unlock();
                    if(interface->getType() == "hgdc" && !interface->isOpen())
                    {
                        interface->startListening();
                    }
                }
            }
        }

        for(auto& module : *addedModules)
        {
            if(_central)
            {
                if(_physicalInterfaceEventhandlers.find(module->getID()) != _physicalInterfaceEventhandlers.end()) continue;
                _physicalInterfaceEventhandlers[module->getID()] = module->addEventHandler(_central);
            }

            module->startListening();
        }
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }

    try
    {
        std::lock_guard<std::mutex> interfaceGuard(_physicalInterfacesMutex);
        _updatedHgdcModules.reset();
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

void Interfaces::worker()
{
    try
    {
        hgdcModuleUpdateThread();
        hgdcReconnectedThread();
    }
    catch(const std::exception& ex)
    {
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
}

}
