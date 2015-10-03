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

#include "AesHandshake.h"
#include "../BidCoSPacket.h"

namespace BidCoS
{
AesHandshake::AesHandshake(BaseLib::Obj* baseLib, BaseLib::Output& out, int32_t address, std::vector<uint8_t> rfKey, std::vector<uint8_t> oldRfKey)
{
	_bl = baseLib;
	_out.init(_bl);
	_out.setPrefix(out.getPrefix());
	_myAddress = address;
	_rfKey = rfKey;
	_oldRfKey = oldRfKey;

	gcry_error_t result;
	if((result = gcry_cipher_open(&_encryptHandle, GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_ECB, GCRY_CIPHER_SECURE)) != GPG_ERR_NO_ERROR)
	{
		_encryptHandle = nullptr;
		_out.printError("Error initializing cypher handle for encryption: " + _bl->hf.getGCRYPTError(result));
		return;
	}
	if(!_encryptHandle)
	{
		_out.printError("Error cypher handle for encryption is nullptr.");
		return;
	}
	if((result = gcry_cipher_open(&_decryptHandle, GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_ECB, GCRY_CIPHER_SECURE)) != GPG_ERR_NO_ERROR)
	{
		_decryptHandle = nullptr;
		_out.printError("Error initializing cypher handle for decryption: " + _bl->hf.getGCRYPTError(result));
		return;
	}
	if(!_decryptHandle)
	{
		_out.printError("Error cypher handle for decryption is nullptr.");
		return;
	}
}

AesHandshake::~AesHandshake()
{
	if(_decryptHandle) gcry_cipher_close(_decryptHandle);
	if(_encryptHandle) gcry_cipher_close(_encryptHandle);
	_decryptHandle = nullptr;
	_encryptHandle = nullptr;
}

void AesHandshake::collectGarbage()
{
	_handshakeInfoMutex.lock();
	try
	{
		std::vector<int32_t> toDelete;
		int64_t time = _bl->hf.getTime();
		for(std::map<int32_t, HandshakeInfo>::iterator i = _handshakeInfoRequest.begin(); i != _handshakeInfoRequest.end(); ++i)
		{
			if(!i->second.mFrame || time - i->second.mFrame->timeReceived() > 5000) toDelete.push_back(i->first);
		}
		for(std::vector<int32_t>::iterator i = toDelete.begin(); i != toDelete.end(); ++i)
		{
			_handshakeInfoRequest.erase(*i);
		}

		toDelete.clear();
		for(std::map<int32_t, HandshakeInfo>::iterator i = _handshakeInfoResponse.begin(); i != _handshakeInfoResponse.end(); ++i)
		{
			if(!i->second.mFrame || time - i->second.mFrame->timeSending() > 5000) toDelete.push_back(i->first);
		}
		for(std::vector<int32_t>::iterator i = toDelete.begin(); i != toDelete.end(); ++i)
		{
			_handshakeInfoResponse.erase(*i);
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
    _handshakeInfoMutex.unlock();
}


// {{{ Request
std::shared_ptr<BidCoSPacket> AesHandshake::getCFrame(std::shared_ptr<BidCoSPacket> mFrame)
{
	std::shared_ptr<BidCoSPacket> cFrame;
	try
	{
		std::vector<uint8_t> cPayload;
		cPayload.reserve(8);
		cPayload.push_back(0x04);
		cPayload.push_back(BaseLib::HelperFunctions::getRandomNumber(0, 255));
		cPayload.push_back(BaseLib::HelperFunctions::getRandomNumber(0, 255));
		cPayload.push_back(BaseLib::HelperFunctions::getRandomNumber(0, 255));
		cPayload.push_back(BaseLib::HelperFunctions::getRandomNumber(0, 255));
		cPayload.push_back(BaseLib::HelperFunctions::getRandomNumber(0, 255));
		cPayload.push_back(BaseLib::HelperFunctions::getRandomNumber(0, 255));
		cPayload.push_back(0);
		cFrame.reset(new BidCoSPacket(mFrame->messageCounter(), 0xA0, 0x02, _myAddress, mFrame->senderAddress(), cPayload));
		cFrame->setTimeReceived(mFrame->timeReceived());
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

    _handshakeInfoMutex.lock();
	try
	{
		HandshakeInfo* handshakeInfo = &_handshakeInfoRequest[mFrame->senderAddress()];
		*handshakeInfo = HandshakeInfo();
		handshakeInfo->mFrame = mFrame;
		handshakeInfo->cFrame = cFrame;
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
    _handshakeInfoMutex.unlock();
    return cFrame;
}

std::shared_ptr<BidCoSPacket> AesHandshake::getAFrame(std::shared_ptr<BidCoSPacket> rFrame, std::shared_ptr<BidCoSPacket>& mFrame)
{
	std::shared_ptr<BidCoSPacket> cFrame;
	std::shared_ptr<BidCoSPacket> aFrame;
    _handshakeInfoMutex.lock();
	try
	{
		HandshakeInfo* handshakeInfo = &_handshakeInfoRequest[rFrame->senderAddress()];
		int64_t time = BaseLib::HelperFunctions::getTime();
		if(!handshakeInfo->mFrame || !handshakeInfo->cFrame || time - handshakeInfo->mFrame->timeReceived() > 1000)
		{
			_handshakeInfoMutex.unlock();
			return aFrame;
		}
		mFrame = handshakeInfo->mFrame;
		cFrame = handshakeInfo->cFrame;
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
    _handshakeInfoMutex.unlock();

    try
	{
    	if(_bl->debugLevel >= 4)_out.printInfo("Info: r-Frame is: " + rFrame->hexString());
    	std::vector<uint8_t> tempKey;
		tempKey.reserve(16);
		uint32_t j = 0;
		for(std::vector<uint8_t>::iterator i = _rfKey.begin(); i != _rfKey.end(); ++i, ++j)
		{
			if(j < 6) tempKey.push_back(*i ^ cFrame->payload()->at(j + 1));
			else tempKey.push_back(*i);
		}

		gcry_error_t result;
		if((result = gcry_cipher_setkey(_decryptHandle, &tempKey.at(0), tempKey.size())) != GPG_ERR_NO_ERROR)
		{
			_out.printError("Error: Could not set key for decryption: " + _bl->hf.getGCRYPTError(result));
			return false;
		}

		std::vector<uint8_t> pd(tempKey.size());
		if(!_decryptHandle) return false;
		if((result = gcry_cipher_decrypt(_decryptHandle, &pd.at(0), pd.size(), &rFrame->payload()->at(0), rFrame->payload()->size())) != GPG_ERR_NO_ERROR)
		{
			_out.printError("Error decrypting data: " + _bl->hf.getGCRYPTError(result));
			return false;
		}

		j = 1;
		for(std::vector<uint8_t>::iterator i = pd.begin(); i != pd.end(); ++i, ++j)
		{
			if(j < mFrame->payload()->size()) *i = *i ^ mFrame->payload()->at(j);
			else break;
		}


		std::vector<uint8_t> pdd(tempKey.size());
		if((result = gcry_cipher_decrypt(_decryptHandle, &pdd.at(0), pdd.size(), &pd.at(0), pd.size())) != GPG_ERR_NO_ERROR)
		{
			_out.printError("Error decrypting data: " + _bl->hf.getGCRYPTError(result));
			return false;
		}

		for(uint32_t i = 6; i < pdd.size(); i++)
		{
			if(i == 6)
			{
				if(pdd[i] != mFrame->messageCounter()) return aFrame;
			}
			else if(i == 7)
			{
				if((pdd[i] & 0xBF) != (mFrame->controlByte() & 0xBF)) return aFrame;
			}
			else if(i == 8)
			{
				if(pdd[i] != mFrame->messageType()) return aFrame;
			}
			else if(i == 9)
			{
				if(pdd[i] != (mFrame->senderAddress() >> 16)) return aFrame;
			}
			else if(i == 19)
			{
				if(pdd[i] != ((mFrame->senderAddress() >> 8) & 0xFF)) return aFrame;
			}
			else if(i == 11)
			{
				if(pdd[i] != (mFrame->senderAddress() & 0xFF)) return aFrame;
			}
			else if(i == 12)
			{
				if(pdd[i] != (mFrame->destinationAddress() >> 16)) return aFrame;
			}
			else if(i == 13)
			{
				if(pdd[i] != ((mFrame->destinationAddress() >> 8) & 0xFF)) return aFrame;
			}
			else if(i == 14)
			{
				if(pdd[i] != (mFrame->destinationAddress() & 0xFF)) return aFrame;
			}
			else if(i == 15)
			{
				if(!mFrame->payload()->empty() && pdd[i] != mFrame->payload()->at(0)) return aFrame;
			}
		}

		std::vector<uint8_t> aPayload;
		aPayload.reserve(5);
		aPayload.push_back(0);
		aPayload.push_back(pd.at(0));
		aPayload.push_back(pd.at(1));
		aPayload.push_back(pd.at(2));
		aPayload.push_back(pd.at(3));
		aFrame.reset(new BidCoSPacket(mFrame->messageCounter(), 0x80, 0x02, _myAddress, mFrame->senderAddress(), aPayload));
		aFrame->setTimeReceived(rFrame->timeReceived());
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
    return aFrame;
}
// }}}

// {{{ Response
void AesHandshake::setMFrame(std::shared_ptr<BidCoSPacket> mFrame)
{
	_handshakeInfoMutex.lock();
	try
	{
		HandshakeInfo* handshakeInfo = &_handshakeInfoResponse[mFrame->destinationAddress()];
		*handshakeInfo = HandshakeInfo();
		handshakeInfo->mFrame = mFrame;
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
    _handshakeInfoMutex.unlock();
}

std::shared_ptr<BidCoSPacket> AesHandshake::getRFrame(std::shared_ptr<BidCoSPacket> cFrame, std::shared_ptr<BidCoSPacket>& mFrame)
{
	if(_bl->debugLevel >= 4) _out.printInfo("Info: c-Frame is: " + cFrame->hexString());

	std::shared_ptr<BidCoSPacket> rFrame;
	_handshakeInfoMutex.lock();
	try
	{
		HandshakeInfo* handshakeInfo = &_handshakeInfoResponse[cFrame->senderAddress()];
		int64_t time = BaseLib::HelperFunctions::getTime();
		if(!handshakeInfo->mFrame || time - handshakeInfo->mFrame->timeSending() > 1000)
		{
			_handshakeInfoMutex.unlock();
			return rFrame;
		}
		mFrame = handshakeInfo->mFrame;
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
    _handshakeInfoMutex.unlock();

    try
    {
    	std::vector<uint8_t> tempKey;
		tempKey.reserve(16);
		uint32_t j = 0;
		for(std::vector<uint8_t>::iterator i = _rfKey.begin(); i != _rfKey.end(); ++i, ++j)
		{
			if(j < 6) tempKey.push_back(*i ^ cFrame->payload()->at(j + 1));
			else tempKey.push_back(*i);
		}

		gcry_error_t result;
		if(!_encryptHandle) return rFrame;
		if((result = gcry_cipher_setkey(_encryptHandle, &tempKey.at(0), tempKey.size())) != GPG_ERR_NO_ERROR)
		{
			_out.printError("Error: Could not set key for encryption: " + _bl->hf.getGCRYPTError(result));
			return rFrame;
		}

		std::vector<uint8_t> pdd(tempKey.size());
		for(int32_t i = 0; i < 6; i++)
		{
			pdd[i] = BaseLib::HelperFunctions::getRandomNumber(0, 255);
		}
		pdd[6] = mFrame->messageCounter();
		pdd[7] = mFrame->controlByte() & 0xBF;
		pdd[8] = mFrame->messageType();
		pdd[9] = mFrame->senderAddress() >> 16;
		pdd[10] = (mFrame->senderAddress() >> 8) & 0xFF;
		pdd[11] = mFrame->senderAddress() & 0xFF;
		pdd[12] = mFrame->destinationAddress() >> 16;
		pdd[13] = (mFrame->destinationAddress() >> 8) & 0xFF;
		pdd[14] = mFrame->destinationAddress() & 0xFF;
		pdd[15] = mFrame->payload()->empty() ? 0 : mFrame->payload()->at(0);

		std::vector<uint8_t> pd(tempKey.size());
		if((result = gcry_cipher_encrypt(_encryptHandle, &pd.at(0), pd.size(), &pdd.at(0), pdd.size())) != GPG_ERR_NO_ERROR)
		{
			_out.printError("Error encrypting data: " + _bl->hf.getGCRYPTError(result));
			return rFrame;
		}

		_handshakeInfoMutex.lock();
		try
		{
			HandshakeInfo* handshakeInfo = &_handshakeInfoResponse[cFrame->senderAddress()];
			handshakeInfo->pd.reset(new std::vector<uint8_t>());
			handshakeInfo->pd->insert(handshakeInfo->pd->begin(), pd.begin(), pd.end());
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
		_handshakeInfoMutex.unlock();

		j = 1;
		for(std::vector<uint8_t>::iterator i = pd.begin(); i != pd.end(); ++i, ++j)
		{
			if(j < mFrame->payload()->size()) *i = *i ^ mFrame->payload()->at(j);
			else break;
		}

		std::vector<uint8_t> rPayload(tempKey.size());
		if((result = gcry_cipher_encrypt(_encryptHandle, &rPayload.at(0), tempKey.size(), &pd.at(0), pd.size())) != GPG_ERR_NO_ERROR)
		{
			_out.printError("Error decrypting data: " + _bl->hf.getGCRYPTError(result));
			return rFrame;
		}

		rFrame.reset(new BidCoSPacket(mFrame->messageCounter(), 0xA0, 0x03, _myAddress, mFrame->destinationAddress(), rPayload));
		rFrame->setTimeReceived(cFrame->timeReceived());
		return rFrame;
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
    return rFrame;
}

bool AesHandshake::checkAFrame(std::shared_ptr<BidCoSPacket> aFrame)
{
	try
	{
		std::shared_ptr<std::vector<uint8_t>> pd;
		_handshakeInfoMutex.lock();
		try
		{
			HandshakeInfo* handshakeInfo = &_handshakeInfoResponse[aFrame->senderAddress()];
			int64_t time = BaseLib::HelperFunctions::getTime();
			if(!handshakeInfo->mFrame || time - handshakeInfo->mFrame->timeSending() > 1000)
			{
				_handshakeInfoMutex.unlock();
				return false;
			}
			if(!handshakeInfo->pd) //No AES handshake was performed
			{
				_handshakeInfoMutex.unlock();
				return true;
			}
			pd = handshakeInfo->pd;
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
		_handshakeInfoMutex.unlock();

		if(!pd) return false;
		if(aFrame->payload()->size() >= 5 && aFrame->payload()->at(aFrame->payload()->size() - 4) == pd->at(0) && aFrame->payload()->at(aFrame->payload()->size() - 3) == pd->at(1) && aFrame->payload()->at(aFrame->payload()->size() - 2) == pd->at(2) && aFrame->payload()->at(aFrame->payload()->size() - 1) == pd->at(3))
		{
			aFrame->setValidAesAck(true);
			if(_bl->debugLevel >= 5) _out.printDebug("Debug: ACK AES signature is valid.");
			return true;
		}
		else if(_bl->debugLevel >= 3) _out.printInfo("Warning: ACK AES signature is invalid.");
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
    return false;
}
// }}}
}
