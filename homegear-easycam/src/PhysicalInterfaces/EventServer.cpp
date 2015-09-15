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

#include "EventServer.h"

#include "homegear-base/Encoding/RapidXml/rapidxml.hpp"
#include <ifaddrs.h>

#include "../GD.h"

namespace EasyCam
{
EventServer::EventServer(std::shared_ptr<BaseLib::Systems::PhysicalInterfaceSettings> settings) : IEasyCamInterface(settings)
{
	_out.init(GD::bl);
	_out.setPrefix(GD::out.getPrefix() + "Event server \"" + settings->id + "\": ");

	signal(SIGPIPE, SIG_IGN);

	if(!settings)
	{
		_out.printCritical("Critical: Error initializing. Settings pointer is empty.");
		return;
	}

	getAddress();
}

EventServer::~EventServer()
{
	try
	{
		_stopCallbackThread = true;
		if(_listenThread.joinable()) _listenThread.join();
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

void EventServer::getAddress()
{
	try
	{
		if(_settings->host.empty())
		{
			_listenAddress = BaseLib::Net::getMyIpAddress();
			if(_listenAddress.empty()) _bl->out.printError("Error: No IP address could be found to bind the server to. Please specify the IP address manually in main.conf.");
		}
		else _listenAddress = _settings->host;
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
}

void EventServer::startListening()
{
}

void EventServer::stopListening()
{
}
}
