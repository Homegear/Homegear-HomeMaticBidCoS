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

#ifndef EASYCAMPACKET_H_
#define EASYCAMPACKET_H_

#include "homegear-base/BaseLib.h"
#include "homegear-base/Encoding/RapidXml/rapidxml.hpp"

#include <unordered_map>

namespace EasyCam
{

class EasyCamPacket : public BaseLib::Systems::Packet
{
    public:
        EasyCamPacket();
        EasyCamPacket(std::string& baseUrl, std::string& path, std::string& function, std::string& username, std::string& password, std::shared_ptr<std::vector<std::pair<std::string, std::string>>> valuesToSet);
        virtual ~EasyCamPacket();

        std::string baseUrl() { return _baseUrl; }
        std::string path() { return _path; }
        std::string function() { return _function; }
        std::shared_ptr<std::unordered_map<std::string, std::string>> values() { return _values; }

        std::shared_ptr<std::vector<std::pair<std::string, std::string>>> valuesToSet() { return _valuesToSet; }

        void getHttpRequest(std::string& request);
    protected:
        //To device
        std::shared_ptr<std::vector<std::pair<std::string, std::string>>> _valuesToSet;

        //From device
        std::string _baseUrl;
        std::string _path;
        std::string _function;
        std::string _username;
        std::string _password;
        std::shared_ptr<std::unordered_map<std::string, std::string>> _values;
};

}
#endif
