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

#ifndef HMCCTC_H
#define HMCCTC_H

#include "../BidCoSPeer.h"

namespace BidCoS
{

class HmCcTc : public BidCoSPeer
{
    public:
		HmCcTc(uint32_t parentID, IPeerEventSink* eventHandler);
		HmCcTc(int32_t id, int32_t address, std::string serialNumber, uint32_t parentID, IPeerEventSink* eventHandler);
		virtual ~HmCcTc();
		virtual void dispose();

		virtual bool isVirtual() { return true; }

		void setValveState(int32_t valveState);
        int32_t getNewValueState() { return _newValveState; }
    protected:
		//In table variables
        int32_t _currentDutyCycleDeviceAddress = -1;
        int32_t _valveState = 0;
        int32_t _newValveState = 0;
        int64_t _lastDutyCycleEvent = 0;
        //End

        std::unordered_map<int32_t, bool> _decalcification;

        const int32_t _dutyCycleTimeOffset = 3000;
        std::atomic_bool _stopDutyCycleThread;
        std::thread _dutyCycleThread;
        int32_t _dutyCycleCounter  = 0;
        std::thread _sendDutyCyclePacketThread;
        uint8_t _dutyCycleMessageCounter = 0;

        void init();
        void worker();
        virtual bool load(BaseLib::Systems::ICentral* device);
        virtual void loadVariables(BaseLib::Systems::ICentral* device, std::shared_ptr<BaseLib::Database::DataTable>& rows);
        virtual void saveVariables();

        int32_t calculateCycleLength(uint8_t messageCounter);
        int32_t getNextDutyCycleDeviceAddress();
        int64_t calculateLastDutyCycleEvent();
        int32_t getAdjustmentCommand(int32_t peerAddress);

        void sendDutyCycleBroadcast();
        void sendDutyCyclePacket(uint8_t messageCounter, int64_t sendingTime);
        void startDutyCycle(int64_t lastDutyCycleEvent);
        void dutyCycleThread(int64_t lastDutyCycleEvent);
        void setDecalcification();

        /**
         * {@inheritDoc}
         */
        virtual PVariable putParamset(BaseLib::PRpcClientInfo clientInfo, int32_t channel, ParameterGroup::Type::Enum type, uint64_t remoteID, int32_t remoteChannel, PVariable variables, bool onlyPushing = false) { return Variable::createError(-32601, "Method not implemented by this virtual device."); }

        /**
         * {@inheritDoc}
         */
        virtual PVariable setInterface(BaseLib::PRpcClientInfo clientInfo, std::string interfaceID) { return Variable::createError(-32601, "Method not implemented by this virtual device."); }

        /**
         * {@inheritDoc}
         */
        virtual PVariable setValue(BaseLib::PRpcClientInfo clientInfo, uint32_t channel, std::string valueKey, PVariable value, bool wait) { return Variable::createError(-32601, "Method not implemented by this virtual device."); }
};
}
#endif
