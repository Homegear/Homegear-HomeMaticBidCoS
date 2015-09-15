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

#include "EasyCamPacket.h"

#include "GD.h"

namespace EasyCam
{
EasyCamPacket::EasyCamPacket()
{
	_values.reset(new std::unordered_map<std::string, std::string>());
	_valuesToSet.reset(new std::vector<std::pair<std::string, std::string>>());
}

EasyCamPacket::EasyCamPacket(std::string& baseUrl, std::string& path, std::string& function, std::string& username, std::string& password, std::shared_ptr<std::vector<std::pair<std::string, std::string>>> valuesToSet)
{
	_baseUrl = baseUrl;
	_path = path;
	_function = function;
	_username = username;
	_password = password;
	_valuesToSet = valuesToSet;
	if(!_valuesToSet) _valuesToSet.reset(new std::vector<std::pair<std::string, std::string>>());
	_values.reset(new std::unordered_map<std::string, std::string>());
}

EasyCamPacket::~EasyCamPacket()
{
}

void EasyCamPacket::getHttpRequest(std::string& request)
{
	try
	{
		request.clear();
		request.reserve(1024);
		request += "GET " + _path + "?cmd=" + _function + "&usr=" + HTTP::encodeURL(_username) + "&pwd=" + HTTP::encodeURL(_password);
		if(_valuesToSet->size() == 1 && _valuesToSet->at(0).first == "null")
		{
			request += '&' + _valuesToSet->at(0).second;
		}
		else
		{
			for(std::vector<std::pair<std::string, std::string>>::iterator i = _valuesToSet->begin(); i != _valuesToSet->end(); ++i)
			{
				request += '&' + i->first + '=' + HTTP::encodeURL(i->second);
			}
		}
		request += " HTTP/1.1\r\nUser-Agent: Homegear\r\nHost: " + _baseUrl + "\r\nConnection: " + "Close" + "\r\n\r\n";
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
