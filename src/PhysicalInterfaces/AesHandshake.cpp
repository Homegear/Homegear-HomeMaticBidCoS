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

#include "AesHandshake.h"
#include "../BidCoSPacket.h"

namespace BidCoS
{
AesHandshake::AesHandshake(BaseLib::SharedObjects* baseLib, BaseLib::Output& out, int32_t address, std::vector<uint8_t> rfKey, std::vector<uint8_t> oldRfKey, uint32_t currentRfKeyIndex)
{
	_bl = baseLib;
	_out.init(_bl);
	_out.setPrefix(out.getPrefix());
	_myAddress = address;
	_rfKey = rfKey;
	_oldRfKey = oldRfKey;
	_currentRfKeyIndex = currentRfKeyIndex;

	gcry_error_t result;
	if((result = gcry_cipher_open(&_encryptHandle, GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_ECB, GCRY_CIPHER_SECURE)) != GPG_ERR_NO_ERROR)
	{
		_encryptHandle = nullptr;
		_out.printError("Error initializing cypher handle for encryption: " + BaseLib::Security::Gcrypt::getError(result));
		return;
	}
	if(!_encryptHandle)
	{
		_out.printError("Error cypher handle for encryption is nullptr.");
		return;
	}
	if((result = gcry_cipher_open(&_encryptHandleKeyChange, GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_ECB, GCRY_CIPHER_SECURE)) != GPG_ERR_NO_ERROR)
	{
		_encryptHandleKeyChange = nullptr;
		_out.printError("Error initializing cypher handle for encryption: " + BaseLib::Security::Gcrypt::getError(result));
		return;
	}
	if(!_encryptHandleKeyChange)
	{
		_out.printError("Error cypher handle for encryption is nullptr.");
		return;
	}
	if((result = gcry_cipher_open(&_decryptHandle, GCRY_CIPHER_AES128, GCRY_CIPHER_MODE_ECB, GCRY_CIPHER_SECURE)) != GPG_ERR_NO_ERROR)
	{
		_decryptHandle = nullptr;
		_out.printError("Error initializing cypher handle for decryption: " + BaseLib::Security::Gcrypt::getError(result));
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
	if(_encryptHandleKeyChange) gcry_cipher_close(_encryptHandleKeyChange);
	_decryptHandle = nullptr;
	_encryptHandle = nullptr;
	_encryptHandleKeyChange = nullptr;
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
		if(_myAddress == -1) _out.printWarning("Warning: address is unset in AesHandshake.");

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
		handshakeInfo->handshakeStarted = true;
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

std::shared_ptr<BidCoSPacket> AesHandshake::getAFrame(std::shared_ptr<BidCoSPacket> rFrame, std::shared_ptr<BidCoSPacket>& mFrame, uint32_t keyIndex, bool wakeUp)
{
	std::shared_ptr<BidCoSPacket> cFrame;
	std::shared_ptr<BidCoSPacket> aFrame;
	{
		std::lock_guard<std::mutex> hanshakeInfoGuard(_handshakeInfoMutex);
		HandshakeInfo* handshakeInfo = &_handshakeInfoRequest[rFrame->senderAddress()];
		int64_t time = BaseLib::HelperFunctions::getTime();
		if(!handshakeInfo->mFrame || !handshakeInfo->cFrame || time - handshakeInfo->mFrame->timeReceived() > 1000) return aFrame;
		handshakeInfo->handshakeStarted = true;
		mFrame = handshakeInfo->mFrame;
		cFrame = handshakeInfo->cFrame;
    }

    try
	{
		if(_myAddress == -1) _out.printWarning("Warning: address is unset in AesHandshake.");

    	std::lock_guard<std::mutex> decryptGuard(_decryptMutex);
    	if(_bl->debugLevel >= 4)_out.printInfo("Info: r-Frame is: " + rFrame->hexString());
    	std::vector<uint8_t> rfKey;
    	getKey(rfKey, keyIndex);
    	if(rfKey.empty())
    	{
    		return aFrame;
    	}
    	std::vector<uint8_t> tempKey;
		tempKey.reserve(16);
		uint32_t j = 0;
		for(std::vector<uint8_t>::iterator i = rfKey.begin(); i != rfKey.end(); ++i, ++j)
		{
			if(j < 6) tempKey.push_back(*i ^ cFrame->payload()->at(j + 1));
			else tempKey.push_back(*i);
		}

		gcry_error_t result;
		if((result = gcry_cipher_setkey(_decryptHandle, &tempKey.at(0), tempKey.size())) != GPG_ERR_NO_ERROR)
		{
			_out.printError("Error: Could not set key for decryption: " + BaseLib::Security::Gcrypt::getError(result));
			return aFrame;
		}

		std::vector<uint8_t> pd(tempKey.size());
		if(!_decryptHandle) return aFrame;
		if((result = gcry_cipher_decrypt(_decryptHandle, &pd.at(0), pd.size(), &rFrame->payload()->at(0), rFrame->payload()->size())) != GPG_ERR_NO_ERROR)
		{
			_out.printError("Error decrypting data: " + BaseLib::Security::Gcrypt::getError(result));
			return aFrame;
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
			_out.printError("Error decrypting data: " + BaseLib::Security::Gcrypt::getError(result));
			return aFrame;
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
		aFrame.reset(new BidCoSPacket(mFrame->messageCounter(), ((mFrame->controlByte() & 2) && wakeUp && mFrame->messageType() != 0) ? 0x81 : 0x80, 0x02, _myAddress, mFrame->senderAddress(), aPayload));
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
	if(mFrame->messageType() == 0x03) return;
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

std::shared_ptr<BidCoSPacket> AesHandshake::getRFrame(std::shared_ptr<BidCoSPacket> cFrame, std::shared_ptr<BidCoSPacket>& mFrame, uint32_t keyIndex)
{
	if(_myAddress == -1) _out.printWarning("Warning: address is unset in AesHandshake.");
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
		handshakeInfo->handshakeStarted = true;
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

    _encryptMutex.lock();
    try
    {
    	std::vector<uint8_t> rfKey;
    	getKey(rfKey, keyIndex);
    	if(rfKey.empty())
    	{
    		_encryptMutex.unlock();
			return rFrame;
    	}
    	std::vector<uint8_t> tempKey;
		tempKey.reserve(16);
		uint32_t j = 0;
		for(std::vector<uint8_t>::iterator i = rfKey.begin(); i != rfKey.end(); ++i, ++j)
		{
			if(j < 6) tempKey.push_back(*i ^ cFrame->payload()->at(j + 1));
			else tempKey.push_back(*i);
		}

		gcry_error_t result;
		if(!_encryptHandle)
		{
			_encryptMutex.unlock();
			return rFrame;
		}
		if((result = gcry_cipher_setkey(_encryptHandle, &tempKey.at(0), tempKey.size())) != GPG_ERR_NO_ERROR)
		{
			_out.printError("Error: Could not set key for encryption: " + BaseLib::Security::Gcrypt::getError(result));
			_encryptMutex.unlock();
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
			_out.printError("Error encrypting data: " + BaseLib::Security::Gcrypt::getError(result));
			_encryptMutex.unlock();
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
			_out.printError("Error decrypting data: " + BaseLib::Security::Gcrypt::getError(result));
			_encryptMutex.unlock();
			return rFrame;
		}

		rFrame.reset(new BidCoSPacket(mFrame->messageCounter(), 0xA0, 0x03, _myAddress, mFrame->destinationAddress(), rPayload));
		rFrame->setTimeReceived(cFrame->timeReceived());
		_encryptMutex.unlock();
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
    _encryptMutex.unlock();
    return rFrame;
}

bool AesHandshake::handshakeStarted(int32_t address)
{
	try
	{
		std::lock_guard<std::mutex> handshakeInfoGuard(_handshakeInfoMutex);
		HandshakeInfo* handshakeInfo = &_handshakeInfoResponse[address];
		if(!handshakeInfo->handshakeStarted || !handshakeInfo->mFrame || BaseLib::HelperFunctions::getTime() - handshakeInfo->mFrame->timeSending() > 1000)
		{
			return false;
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
    return true;
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

bool AesHandshake::generateKeyChangePacket(std::shared_ptr<BidCoSPacket> keyChangeTemplate)
{
	std::vector<uint8_t>* payload = keyChangeTemplate->payload();
	std::vector<uint8_t> oldRfKey;
	try
	{
		payload->at(1) += 2;
		uint32_t index = (payload->at(1) / 2);
		uint32_t subindex = payload->at(1) % 2;
		std::vector<uint8_t> key;
		if(_currentRfKeyIndex != index)
		{
			_out.printError("Error: No AES key is defined for the key index to set. You probably changed rfKey before the last key was sent to the device or you forgot to set oldRfKey. Please set oldRfKey in homematicbidcos.conf to the current AES key of the peer or reset the peer and pair it again.");
			return false;
		}
		if(_currentRfKeyIndex == 1) oldRfKey = std::vector<uint8_t> { 0xA4, 0xE3, 0x75, 0xC6, 0xB0, 0x9F, 0xD1, 0x85, 0xF2, 0x7C, 0x4E, 0x96, 0xFC, 0x27, 0x3A, 0xE4 };
		else oldRfKey = _oldRfKey;
		getKey(key, index);
		if(key.empty() || oldRfKey.empty())
		{
			_out.printError("Error: rfKey or oldRfKey are empty.");
			return false;
		}

		if(subindex == 0) payload->insert(payload->end(), key.begin(), key.begin() + 8);
		else payload->insert(payload->end(), key.begin() + 8, key.end());
		payload->push_back((uint8_t)BaseLib::HelperFunctions::getRandomNumber(0, 255));
		payload->push_back((uint8_t)BaseLib::HelperFunctions::getRandomNumber(0, 255));
		payload->push_back(0x7E);
		payload->push_back(0x29);
		payload->push_back(0x6F);
		payload->push_back(0xA5);
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

    try
    {
		gcry_error_t result;
		_keyChangeMutex.lock();
		if(!_encryptHandleKeyChange)
		{
			_keyChangeMutex.unlock();
			return false;
		}
		if((result = gcry_cipher_setkey(_encryptHandleKeyChange, &oldRfKey.at(0), oldRfKey.size())) != GPG_ERR_NO_ERROR)
		{
			_out.printError("Error: Could not set key for encryption: " + BaseLib::Security::Gcrypt::getError(result));
			_keyChangeMutex.unlock();
			return false;
		}

		std::vector<uint8_t> encryptedPayload(oldRfKey.size());
		if((result = gcry_cipher_encrypt(_encryptHandleKeyChange, &encryptedPayload.at(0), encryptedPayload.size(), &payload->at(0), payload->size())) != GPG_ERR_NO_ERROR)
		{
			_out.printError("Error encrypting data: " + BaseLib::Security::Gcrypt::getError(result));
			_keyChangeMutex.unlock();
			return false;
		}

		*keyChangeTemplate->payload() = encryptedPayload;

		_keyChangeMutex.unlock();
		return true;
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
    _keyChangeMutex.unlock();
    return false;
}

void AesHandshake::getKey(std::vector<uint8_t>& key, uint32_t keyIndex)
{
	try
	{
		if(keyIndex == 0) key = std::vector<uint8_t> { 0xA4, 0xE3, 0x75, 0xC6, 0xB0, 0x9F, 0xD1, 0x85, 0xF2, 0x7C, 0x4E, 0x96, 0xFC, 0x27, 0x3A, 0xE4 };
		else if(keyIndex == _currentRfKeyIndex) key = _rfKey;
		else if(keyIndex == _currentRfKeyIndex - 1) key = _oldRfKey;
		else key.clear();
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
// }}}

// {{{ Signature
void AesHandshake::appendSignature(std::shared_ptr<BidCoSPacket> packet)
{
	try
	{
		if(packet->payload()->size() < 6) return;

		std::vector<uint8_t> iv(16, 0);
		iv.at(0) = 0x49;
		int32_t senderAddress = packet->senderAddress();
		iv.at(1) = senderAddress >> 16;
		iv.at(2) = (senderAddress >> 8) & 0xFF;
		iv.at(3) = senderAddress & 0xFF;
		int32_t destinationAddress = packet->destinationAddress();
		iv.at(4) = destinationAddress >> 16;
		iv.at(5) = (destinationAddress >> 8) & 0xFF;
		iv.at(6) = destinationAddress & 0xFF;
		iv.at(7) = packet->payload()->at(4);
		iv.at(8) = packet->payload()->at(5);
		iv.at(9) = packet->messageCounter();
		iv.at(15) = 5;

		gcry_error_t result = 0;
		std::vector<uint8_t> eIv(iv.size());

		std::lock_guard<std::mutex> encryptGuard(_encryptMutex);
		if(!_encryptHandle) return;

		if((result = gcry_cipher_setkey(_encryptHandle, _rfKey.data(), _rfKey.size())) != GPG_ERR_NO_ERROR)
		{
			_out.printError("Error: Could not set key for encryption: " + BaseLib::Security::Gcrypt::getError(result));
			return;
		}

		if((result = gcry_cipher_encrypt(_encryptHandle, eIv.data(), eIv.size(), iv.data(), iv.size())) != GPG_ERR_NO_ERROR)
		{
			_out.printError("Error encrypting data: " + BaseLib::Security::Gcrypt::getError(result));
			return;
		}

		std::vector<uint8_t> plain(2, 0);
		plain.reserve(16);
		plain.at(0) = packet->messageCounter();
		plain.at(1) = packet->controlByte();
		plain.insert(plain.end(), packet->payload()->begin(), packet->payload()->end() - 2);
		plain.resize(16, 0);

		for(int32_t i = 0; i < 16; i++)
		{
			eIv.at(i) = eIv.at(i) ^ plain.at(i);
		}

		std::vector<uint8_t> signature(16);

		if((result = gcry_cipher_encrypt(_encryptHandle, signature.data(), signature.size(), eIv.data(), eIv.size())) != GPG_ERR_NO_ERROR)
		{
			_out.printError("Error encrypting data: " + BaseLib::Security::Gcrypt::getError(result));
			return;
		}

		packet->payload()->reserve(packet->payload()->size() + 4);
		packet->payload()->push_back(signature.at(12));
		packet->payload()->push_back(signature.at(13));
		packet->payload()->push_back(signature.at(14));
		packet->payload()->push_back(signature.at(15));
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
// }}}
}
