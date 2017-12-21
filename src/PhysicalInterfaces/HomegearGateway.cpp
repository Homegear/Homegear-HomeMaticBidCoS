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

#include "HomegearGateway.h"
#include "../BidCoSPacket.h"
#include "../GD.h"

namespace BidCoS
{

HomegearGateway::HomegearGateway(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IBidCoSInterface(settings)
{
    _settings = settings;
    _out.init(GD::bl);
    _out.setPrefix(GD::out.getPrefix() + "HomeMatic Homegear Gateway \"" + settings->id + "\": ");

    signal(SIGPIPE, SIG_IGN);

    _stopped = true;
    _waitForResponse = false;

    _binaryRpc.reset(new BaseLib::Rpc::BinaryRpc(_bl));
    _rpcEncoder.reset(new BaseLib::Rpc::RpcEncoder(_bl, true, true));
    _rpcDecoder.reset(new BaseLib::Rpc::RpcDecoder(_bl, false, false));
}

HomegearGateway::~HomegearGateway()
{
    stopListening();
}

void HomegearGateway::startListening()
{
    try
    {
        stopListening();

        if(_settings->host.empty() || _settings->port.empty() || _settings->caFile.empty() || _settings->certFile.empty() || _settings->keyFile.empty())
        {
            _out.printError("Error: Configuration of Homegear Gateway is incomplete. Please correct it in \"homematic.conf\".");
            return;
        }

        _tcpSocket.reset(new BaseLib::TcpSocket(_bl, _settings->host, _settings->port, true, _settings->caFile, true, _settings->certFile, _settings->keyFile));
        _tcpSocket->setConnectionRetries(1);
        _tcpSocket->setReadTimeout(5000000);
        _tcpSocket->setWriteTimeout(5000000);
        _stopCallbackThread = false;
        if(_settings->listenThreadPriority > -1) _bl->threadManager.start(_listenThread, true, _settings->listenThreadPriority, _settings->listenThreadPolicy, &HomegearGateway::listen, this);
        else _bl->threadManager.start(_listenThread, true, &HomegearGateway::listen, this);
        IPhysicalInterface::startListening();
    }
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void HomegearGateway::stopListening()
{
    try
    {
        _stopCallbackThread = true;
        if(_tcpSocket) _tcpSocket->close();
        _bl->threadManager.join(_listenThread);
        _stopped = true;
        _tcpSocket.reset();
        IPhysicalInterface::stopListening();
    }
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void HomegearGateway::enableUpdateMode()
{
    try
    {
        if(!_tcpSocket->connected())
        {
            _out.printError("Error: Could not enable update mode. Not connected to gateway.");
            return;
        }

        BaseLib::PArray parameters = std::make_shared<BaseLib::Array>();
        parameters->push_back(std::make_shared<BaseLib::Variable>(BIDCOS_FAMILY_ID));

        auto result = invoke("enableUpdateMode", parameters);
        if(result->errorStruct)
        {
            _out.printError(result->structValue->at("faultString")->stringValue);
        }
        else _out.printInfo("Info: Update mode enabled.");
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void HomegearGateway::disableUpdateMode()
{
    try
    {
        if(!_tcpSocket->connected())
        {
            _out.printError("Error: Could not disable update mode. Not connected to gateway.");
            return;
        }

        BaseLib::PArray parameters = std::make_shared<BaseLib::Array>();
        parameters->push_back(std::make_shared<BaseLib::Variable>(BIDCOS_FAMILY_ID));

        auto result = invoke("disableUpdateMode", parameters);
        if(result->errorStruct)
        {
            _out.printError(result->structValue->at("faultString")->stringValue);
        }
        else _out.printInfo("Info: Update mode disabled.");
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void HomegearGateway::listen()
{
    try
    {
        try
        {
            _tcpSocket->open();
            if(_tcpSocket->connected())
            {
                _out.printInfo("Info: Successfully connected.");
                _stopped = false;
            }
        }
        catch(BaseLib::Exception& ex)
        {
            _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
        }
        catch(const std::exception& ex)
        {
            _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
        }
        catch(...)
        {
            _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
        }

        std::vector<char> buffer(1024);
        while(!_stopCallbackThread)
        {
            try
            {
                if(_stopped || !_tcpSocket->connected())
                {
                    if(_stopCallbackThread) return;
                    if(_stopped) _out.printWarning("Warning: Connection to device closed. Trying to reconnect...");
                    _tcpSocket->close();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    _tcpSocket->open();
                    if(_tcpSocket->connected())
                    {
                        _out.printInfo("Info: Successfully connected.");
                        _stopped = false;
                    }
                    continue;
                }

                int32_t bytesRead = 0;
                try
                {
                    bytesRead = _tcpSocket->proofread(buffer.data(), buffer.size());
                }
                catch(BaseLib::SocketTimeOutException& ex)
                {
                    continue;
                }
                if(bytesRead <= 0) continue;
                if(bytesRead > 1024) bytesRead = 1024;

                if(GD::bl->debugLevel >= 5) _out.printDebug("Debug: TCP packet received: " + BaseLib::HelperFunctions::getHexString(buffer.data(), bytesRead));

                _binaryRpc->process(buffer.data(), bytesRead);
                if(_binaryRpc->isFinished())
                {
                    if(_binaryRpc->getType() == BaseLib::Rpc::BinaryRpc::Type::request)
                    {
                        std::string method;
                        BaseLib::PArray parameters = _rpcDecoder->decodeRequest(_binaryRpc->getData(), method);

                        if(method == "packetReceived" && parameters && parameters->size() == 2 && parameters->at(0)->integerValue64 == BIDCOS_FAMILY_ID && !parameters->at(1)->stringValue.empty())
                        {
                            processPacket(parameters->at(1)->stringValue);
                        }

                        BaseLib::PVariable response = std::make_shared<BaseLib::Variable>();
                        std::vector<char> data;
                        _rpcEncoder->encodeResponse(response, data);
                        _tcpSocket->proofwrite(data);
                    }
                    else if(_binaryRpc->getType() == BaseLib::Rpc::BinaryRpc::Type::response && _waitForResponse)
                    {
                        std::unique_lock<std::mutex> requestLock(_requestMutex);
                        _rpcResponse = _rpcDecoder->decodeResponse(_binaryRpc->getData());
                        requestLock.unlock();
                        _requestConditionVariable.notify_all();
                    }
                    _binaryRpc->reset();
                }
            }
            catch(BaseLib::Exception& ex)
            {
                _stopped = true;
                _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
            }
            catch(const std::exception& ex)
            {
                _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
            }
            catch(...)
            {
                _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
            }
        }
    }
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void HomegearGateway::forceSendPacket(std::shared_ptr<BidCoSPacket> packet)
{
    try
    {
        if(!_tcpSocket || !_tcpSocket->connected()) return;

        BaseLib::PArray parameters = std::make_shared<BaseLib::Array>();
        parameters->reserve(2);
        parameters->push_back(std::make_shared<BaseLib::Variable>(BIDCOS_FAMILY_ID));
        parameters->push_back(std::make_shared<BaseLib::Variable>(packet->hexString()));

        auto result = invoke("sendPacket", parameters);
        if(result->errorStruct)
        {
            _out.printError("Error sending packet " + packet->hexString() + ": " + result->structValue->at("faultString")->stringValue);
        }
    }
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

PVariable HomegearGateway::invoke(std::string methodName, PArray& parameters)
{
    try
    {
        std::lock_guard<std::mutex> invokeGuard(_invokeMutex);

        std::unique_lock<std::mutex> requestLock(_requestMutex);
        _rpcResponse.reset();
        _waitForResponse = true;

        std::vector<char> encodedPacket;
        _rpcEncoder->encodeRequest(methodName, parameters, encodedPacket);

        int32_t i = 0;
        for(i = 0; i < 5; i++)
        {
            try
            {
                _tcpSocket->proofwrite(encodedPacket);
                break;
            }
            catch(BaseLib::SocketOperationException& ex)
            {
                _out.printError("Error: " + ex.what());
                if(i == 5) return BaseLib::Variable::createError(-32500, ex.what());
                _tcpSocket->open();
            }
        }

        i = 0;
        while(!_requestConditionVariable.wait_for(requestLock, std::chrono::milliseconds(1000), [&]
        {
            i++;
            return _rpcResponse || _stopped || i == 10;
        }));
        _waitForResponse = false;
        if(i == 10 || !_rpcResponse) return BaseLib::Variable::createError(-32500, "No RPC response received.");

        return _rpcResponse;
    }
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
    return BaseLib::Variable::createError(-32500, "Unknown application error. See log for more details.");
}

void HomegearGateway::processPacket(std::string& data)
{
    try
    {
        std::shared_ptr<BidCoSPacket> packet = std::make_shared<BidCoSPacket>(data, BaseLib::HelperFunctions::getTime());
        processReceivedPacket(packet);
    }
    catch(const std::exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
        _out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

}