/* Copyright 2013-2017 Sathya Laufer
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

#ifndef HOMEGEAR_HOMEMATICBIDCOS_HOMEGEARGATEWAY_H
#define HOMEGEAR_HOMEMATICBIDCOS_HOMEGEARGATEWAY_H

#include "IBidCoSInterface.h"

namespace BidCoS
{

class HomegearGateway : public IBidCoSInterface
{
public:
    HomegearGateway(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings);
    virtual ~HomegearGateway();

    virtual void startListening();
    virtual void stopListening();

    virtual void enableUpdateMode();
    virtual void disableUpdateMode();

    virtual bool isOpen() { return !_stopped; }
protected:
    std::unique_ptr<BaseLib::TcpSocket> _tcpSocket;
    std::unique_ptr<BaseLib::Rpc::BinaryRpc> _binaryRpc;
    std::unique_ptr<BaseLib::Rpc::RpcEncoder> _rpcEncoder;
    std::unique_ptr<BaseLib::Rpc::RpcDecoder> _rpcDecoder;

    std::mutex _invokeMutex;
    std::mutex _requestMutex;
    std::atomic_bool _waitForResponse;
    std::condition_variable _requestConditionVariable;
    BaseLib::PVariable _rpcResponse;

    void listen();
    virtual void forceSendPacket(std::shared_ptr<BidCoSPacket> packet);
    BaseLib::PVariable invoke(std::string methodName, BaseLib::PArray& parameters);
    void processPacket(std::string& data);
};

}

#endif
