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

#ifndef INTERFACES_H_
#define INTERFACES_H_

#include "PhysicalInterfaces/IBidCoSInterface.h"

#include <homegear-base/BaseLib.h>

namespace BidCoS
{

using namespace BaseLib;

class Interfaces : public BaseLib::Systems::PhysicalInterfaces
{
public:
	Interfaces(BaseLib::SharedObjects* bl, std::map<std::string, Systems::PPhysicalInterfaceSettings> physicalInterfaceSettings);
	virtual ~Interfaces();

    void addEventHandlers(BaseLib::Systems::IPhysicalInterface::IPhysicalInterfaceEventSink* central);
    void removeEventHandlers();
    void startListening() override;
    void stopListening() override;
    std::shared_ptr<IBidCoSInterface> getDefaultInterface();
    bool hasInterface(const std::string& name);
    std::shared_ptr<IBidCoSInterface> getInterface(const std::string& name);
    std::vector<std::shared_ptr<IBidCoSInterface>> getInterfaces();
    void worker();
protected:
    BaseLib::PVariable _updatedHgdcModules;

    std::atomic_bool _stopped{true};
    std::atomic_bool _hgdcReconnected{false};
    int32_t _hgdcModuleUpdateEventHandlerId = -1;
    int32_t _hgdcReconnectedEventHandlerId = -1;
    BaseLib::Systems::IPhysicalInterface::IPhysicalInterfaceEventSink* _central = nullptr;
    std::shared_ptr<IBidCoSInterface> _defaultPhysicalInterface;
    std::map<std::string, PEventHandler> _physicalInterfaceEventhandlers;

    void create() override;
    void hgdcReconnected();
    void createHgdcInterfaces(bool reconnected);
    void hgdcModuleUpdate(const BaseLib::PVariable& modules);
    void hgdcReconnectedThread();
    void hgdcModuleUpdateThread();
};

}

#endif
