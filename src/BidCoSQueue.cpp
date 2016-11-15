/* Copyright 2013-2016 Sathya Laufer
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

#include "BidCoSQueue.h"
#include "BidCoSMessage.h"
#include "PendingBidCoSQueues.h"
#include "HomeMaticCentral.h"
#include <homegear-base/BaseLib.h>
#include "GD.h"

#include <memory>

namespace BidCoS
{
BidCoSQueue::BidCoSQueue()
{
	_queueType = BidCoSQueueType::EMPTY;
	_lastPop = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	_physicalInterface = GD::defaultPhysicalInterface;
	_disposing = false;
	_stopPopWaitThread = false;
	_workingOnPendingQueue = false;
	noSending = false;
}

BidCoSQueue::BidCoSQueue(std::shared_ptr<IBidCoSInterface> physicalInterface) : BidCoSQueue()
{
	if(physicalInterface) _physicalInterface = physicalInterface;
}

BidCoSQueue::BidCoSQueue(std::shared_ptr<IBidCoSInterface> physicalInterface, BidCoSQueueType queueType) : BidCoSQueue(physicalInterface)
{
	_queueType = queueType;
}

BidCoSQueue::~BidCoSQueue()
{
	try
	{
		dispose();
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

void BidCoSQueue::serialize(std::vector<uint8_t>& encodedData)
{
	try
	{
		BaseLib::BinaryEncoder encoder(GD::bl);
		_queueMutex.lock();
		if(_queue.size() == 0)
		{
			_queueMutex.unlock();
			return;
		}
		encoder.encodeByte(encodedData, (int32_t)_queueType);
		encoder.encodeInteger(encodedData, _queue.size());
		for(std::list<BidCoSQueueEntry>::iterator i = _queue.begin(); i != _queue.end(); ++i)
		{
			encoder.encodeByte(encodedData, (uint8_t)i->getType());
			encoder.encodeBoolean(encodedData, i->stealthy);
			encoder.encodeBoolean(encodedData, true); //dummy
			if(!i->getPacket()) encoder.encodeBoolean(encodedData, false);
			else
			{
				encoder.encodeBoolean(encodedData, true);
				std::vector<uint8_t> packet = i->getPacket()->byteArray();
				encoder.encodeByte(encodedData, packet.size());
				encodedData.insert(encodedData.end(), packet.begin(), packet.end());
			}
			std::shared_ptr<BidCoSMessage> message = i->getMessage();
			if(!message) encoder.encodeBoolean(encodedData, false);
			else
			{
				encoder.encodeBoolean(encodedData, true);
				uint8_t dummy = 0;
				encoder.encodeByte(encodedData, dummy);
				encoder.encodeByte(encodedData, message->getMessageType());
				encoder.encodeByte(encodedData, 0); //Dummy
			}
			encoder.encodeString(encodedData, parameterName);
			encoder.encodeInteger(encodedData, channel);
			std::string id = _physicalInterface->getID();
			encoder.encodeString(encodedData, id);
		}
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
	_queueMutex.unlock();
}

void BidCoSQueue::unserialize(std::shared_ptr<std::vector<char>> serializedData, uint32_t position)
{
	try
	{
		BaseLib::BinaryDecoder decoder(GD::bl);
		_queueMutex.lock();
		_queueType = (BidCoSQueueType)decoder.decodeByte(*serializedData, position);
		uint32_t queueSize = decoder.decodeInteger(*serializedData, position);
		for(uint32_t i = 0; i < queueSize; i++)
		{
			BidCoSQueueEntry entry;
			entry.setType((QueueEntryType)decoder.decodeByte(*serializedData, position));
			entry.stealthy = decoder.decodeBoolean(*serializedData, position);
			decoder.decodeBoolean(*serializedData, position); //dummy
			int32_t packetExists = decoder.decodeBoolean(*serializedData, position);
			if(packetExists)
			{
				std::vector<uint8_t> packetData;
				uint32_t dataSize = decoder.decodeByte(*serializedData, position);
				if(position + dataSize <= serializedData->size()) packetData.insert(packetData.end(), serializedData->begin() + position, serializedData->begin() + position + dataSize);
				position += dataSize;
				std::shared_ptr<BidCoSPacket> packet(new BidCoSPacket(packetData, false));
				entry.setPacket(packet, false);
			}
			int32_t messageExists = decoder.decodeBoolean(*serializedData, position);
			if(messageExists)
			{
				decoder.decodeByte(*serializedData, position);
				int32_t messageType = decoder.decodeByte(*serializedData, position);
				decoder.decodeByte(*serializedData, position); //Dummy
				std::shared_ptr<HomeMaticCentral> central(std::dynamic_pointer_cast<HomeMaticCentral>(GD::family->getCentral()));
				if(central) entry.setMessage(central->getMessages()->find(messageType), false);
			}
			parameterName = decoder.decodeString(*serializedData, position);
			channel = decoder.decodeInteger(*serializedData, position);
			std::string physicalInterfaceID = decoder.decodeString(*serializedData, position);
			if(GD::physicalInterfaces.find(physicalInterfaceID) != GD::physicalInterfaces.end()) _physicalInterface = GD::physicalInterfaces.at(physicalInterfaceID);
			else _physicalInterface = GD::defaultPhysicalInterface;
			if((entry.getType() == QueueEntryType::PACKET && !entry.getPacket()) || (entry.getType() == QueueEntryType::MESSAGE && !entry.getMessage())) continue;
			_queue.push_back(std::move(entry));
		}
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    	clear();
    	_pendingQueues.reset();
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    	clear();
    	_pendingQueues.reset();
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    	clear();
    	_pendingQueues.reset();
    }
    if(!_physicalInterface) _physicalInterface = GD::defaultPhysicalInterface;
    _queueMutex.unlock();
}

void BidCoSQueue::dispose()
{
	try
	{
		if(_disposing) return;
		_disposing = true;
		_startResendThreadMutex.lock();
		GD::bl->threadManager.join(_startResendThread);
		_startResendThreadMutex.unlock();
		_pushPendingQueueThreadMutex.lock();
		GD::bl->threadManager.join(_pushPendingQueueThread);
		_pushPendingQueueThreadMutex.unlock();
		_sendThreadMutex.lock();
		GD::bl->threadManager.join(_sendThread);
        _sendThreadMutex.unlock();
		stopPopWaitThread();
		_queueMutex.lock();
		_queue.clear();
		_pendingQueues.reset();
	}
	catch(const std::exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    	_sendThreadMutex.unlock();
    	_pushPendingQueueThreadMutex.unlock();
    	_startResendThreadMutex.unlock();
    }
    catch(BaseLib::Exception& ex)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    	_sendThreadMutex.unlock();
    	_pushPendingQueueThreadMutex.unlock();
    	_startResendThreadMutex.unlock();
    }
    catch(...)
    {
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    	_sendThreadMutex.unlock();
    	_pushPendingQueueThreadMutex.unlock();
    	_startResendThreadMutex.unlock();
    }
    _queueMutex.unlock();
}

bool BidCoSQueue::isEmpty()
{
	return _queue.empty() && (!_pendingQueues || _pendingQueues->empty());
}

bool BidCoSQueue::pendingQueuesEmpty()
{
	 return (!_pendingQueues || _pendingQueues->empty());
}

void BidCoSQueue::push(std::shared_ptr<BidCoSPacket> packet, bool stealthy)
{
	try
	{
		if(_disposing) return;
		BidCoSQueueEntry entry;
		entry.setPacket(packet, true);
		entry.stealthy = stealthy;
		_queueMutex.lock();
		if(!noSending && (_queue.size() == 0 || (_queue.size() == 1 && _queue.front().getType() == QueueEntryType::MESSAGE)))
		{
			_queue.push_back(entry);
			_queueMutex.unlock();
			if(!noSending)
			{
				_sendThreadMutex.lock();
				if(_disposing)
				{
					_sendThreadMutex.unlock();
					return;
				}
				GD::bl->threadManager.join(_sendThread);
				GD::bl->threadManager.start(_sendThread, false, GD::bl->settings.packetQueueThreadPriority(), GD::bl->settings.packetQueueThreadPolicy(), &BidCoSQueue::send, this, entry.getPacket(), entry.stealthy);
				_sendThreadMutex.unlock();
			}
		}
		else
		{
			_queue.push_back(entry);
			_queueMutex.unlock();
		}
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
		_sendThreadMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void BidCoSQueue::push(std::shared_ptr<PendingBidCoSQueues>& pendingQueues)
{
	try
	{
		if(_disposing) return;
		_queueMutex.lock();
		_pendingQueues = pendingQueues;
		if(_queue.empty())
		{
			 _queueMutex.unlock();
			pushPendingQueue();
		}
		else  _queueMutex.unlock();
	}
	catch(const std::exception& ex)
    {
		 _queueMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	 _queueMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	 _queueMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }

}

void BidCoSQueue::push(std::shared_ptr<BidCoSQueue> pendingQueue, bool popImmediately, bool clearPendingQueues)
{
	try
	{
		if(_disposing) return;
		if(!pendingQueue) return;
		_queueMutex.lock();
		if(!_pendingQueues) _pendingQueues.reset(new PendingBidCoSQueues());
		if(clearPendingQueues) _pendingQueues->clear();
		_pendingQueues->push(pendingQueue);
		_queueMutex.unlock();
		pushPendingQueue();
		_queueMutex.lock();
		if(popImmediately)
		{
			if(!_pendingQueues->empty()) _pendingQueues->pop(pendingQueueID);
			_workingOnPendingQueue = false;
		}
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
    _queueMutex.unlock();
}

void BidCoSQueue::push(std::shared_ptr<BidCoSMessage> message)
{
	try
	{
		if(_disposing) return;
		if(!message) return;
		BidCoSQueueEntry entry;
		entry.setMessage(message, true);
		_queueMutex.lock();
		_queue.push_back(entry);
		_queueMutex.unlock();
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
		_sendThreadMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void BidCoSQueue::pushFront(std::shared_ptr<BidCoSPacket> packet, bool stealthy, bool popBeforePushing)
{
	try
	{
		if(_disposing) return;
		keepAlive();
		if(popBeforePushing)
		{
			GD::out.printDebug("Popping from BidCoSQueue and pushing packet at the front: " + std::to_string(id));
			if(_popWaitThread.joinable()) _stopPopWaitThread = true;
			_queueMutex.lock();
			_queue.pop_front();
			_queueMutex.unlock();
		}
		BidCoSQueueEntry entry;
		entry.setPacket(packet, true);
		entry.stealthy = stealthy;
		if(!noSending)
		{
			_queueMutex.lock();
			_queue.push_front(entry);
			_queueMutex.unlock();
			if(!noSending)
			{
				_sendThreadMutex.lock();
				if(_disposing)
				{
					_sendThreadMutex.unlock();
					return;
				}
				GD::bl->threadManager.join(_sendThread);
				GD::bl->threadManager.start(_sendThread, false, GD::bl->settings.packetQueueThreadPriority(), GD::bl->settings.packetQueueThreadPolicy(), &BidCoSQueue::send, this, entry.getPacket(), entry.stealthy);
				_sendThreadMutex.unlock();
			}
		}
		else
		{
			_queueMutex.lock();
			_queue.push_front(entry);
			_queueMutex.unlock();
		}
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
		_sendThreadMutex.unlock();
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
        GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void BidCoSQueue::stopPopWaitThread()
{
	try
	{
		_stopPopWaitThread = true;
		GD::bl->threadManager.join(_popWaitThread);
		_stopPopWaitThread = false;
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

void BidCoSQueue::popWait(uint32_t waitingTime)
{
	try
	{
		if(_disposing) return;
		stopPopWaitThread();
		GD::bl->threadManager.start(_popWaitThread, true, GD::bl->settings.packetQueueThreadPriority(), GD::bl->settings.packetQueueThreadPolicy(), &BidCoSQueue::popWaitThread, this, _popWaitThreadId++, waitingTime);
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

void BidCoSQueue::popWaitThread(uint32_t threadId, uint32_t waitingTime)
{
	try
	{
		std::chrono::milliseconds sleepingTime(25);
		uint32_t i = 0;
		while(!_stopPopWaitThread && i < waitingTime)
		{
			std::this_thread::sleep_for(sleepingTime);
			i += 25;
		}
		if(!_stopPopWaitThread)
		{
			pop();
		}
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

void BidCoSQueue::send(std::shared_ptr<BidCoSPacket> packet, bool stealthy)
{
	try
	{
		if(noSending || _disposing || !packet) return;
		if(_setWakeOnRadioBit)
		{
			packet->setControlByte(packet->controlByte() | 0x10);
			_setWakeOnRadioBit = false;
		}
		std::shared_ptr<HomeMaticCentral> central(std::dynamic_pointer_cast<HomeMaticCentral>(GD::family->getCentral()));
		if(central) central->sendPacket(_physicalInterface, packet, stealthy);
		else GD::out.printError("Error: Device pointer of queue " + std::to_string(id) + " is null.");
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

void BidCoSQueue::clear()
{
	try
	{
		_queueMutex.lock();
		if(_pendingQueues) _pendingQueues->clear();
		_queue.clear();
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
    _queueMutex.unlock();
}

void BidCoSQueue::sleepAndPushPendingQueue()
{
	try
	{
		if(_disposing) return;
		std::this_thread::sleep_for(std::chrono::milliseconds(_physicalInterface->responseDelay()));
		pushPendingQueue();
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

void BidCoSQueue::pushPendingQueue()
{
	try
	{
		if(_disposing) return;
		_queueMutex.lock();
		if(_disposing)
		{
			_queueMutex.unlock();
			return;
		}
		if(!_pendingQueues || _pendingQueues->empty())
		{
			_queueMutex.unlock();
			return;
		}
		while(!_pendingQueues->empty() && (!_pendingQueues->front() || _pendingQueues->front()->isEmpty()))
		{
			GD::out.printDebug("Debug: Empty queue was pushed.");
			_pendingQueues->pop();
		}
		if(_pendingQueues->empty())
		{
			_queueMutex.unlock();
			return;
		}
		std::shared_ptr<BidCoSQueue> queue = _pendingQueues->front();
		_queueMutex.unlock();
		if(!queue) return; //Not really necessary, as the mutex is locked, but I had a segmentation fault in this function, so just to make
		_queueType = queue->getQueueType();
		queueEmptyCallback = queue->queueEmptyCallback;
		callbackParameter = queue->callbackParameter;
		pendingQueueID = queue->pendingQueueID;
		for(std::list<BidCoSQueueEntry>::iterator i = queue->getQueue()->begin(); i != queue->getQueue()->end(); ++i)
		{
			if(!noSending && i->getType() == QueueEntryType::PACKET && (_queue.size() == 0 || (_queue.size() == 1 && _queue.front().getType() == QueueEntryType::MESSAGE)))
			{
				_queueMutex.lock();
				_queue.push_back(*i);
				_queueMutex.unlock();
				if(!noSending)
				{
					_sendThreadMutex.lock();
					if(_disposing)
					{
						_sendThreadMutex.unlock();
						return;
					}
					GD::bl->threadManager.join(_sendThread);
					_lastPop = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
					GD::bl->threadManager.start(_sendThread, false, GD::bl->settings.packetQueueThreadPriority(), GD::bl->settings.packetQueueThreadPolicy(), &BidCoSQueue::send, this, i->getPacket(), i->stealthy);
					_sendThreadMutex.unlock();
				}
			}
			else
			{
				_queueMutex.lock();
				_queue.push_back(*i);
				_queueMutex.unlock();
			}
		}
		_workingOnPendingQueue = true;
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
		_sendThreadMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void BidCoSQueue::keepAlive()
{
	if(_disposing) return;
	if(lastAction) *lastAction = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void BidCoSQueue::longKeepAlive()
{
	if(_disposing) return;
	if(lastAction) *lastAction = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() + 5000;
}

void BidCoSQueue::nextQueueEntry()
{
	try
	{
		if(_disposing) return;
		_queueMutex.lock();
		if(_queue.empty()) {
			if(queueEmptyCallback && callbackParameter) queueEmptyCallback(callbackParameter);
			if(_workingOnPendingQueue && !_pendingQueues->empty()) _pendingQueues->pop(pendingQueueID);
			if(!_pendingQueues || (_pendingQueues && _pendingQueues->empty()))
			{
				GD::out.printInfo("Info: Queue " + std::to_string(id) + " is empty and there are no pending queues.");
				_workingOnPendingQueue = false;
				_pendingQueues.reset();
				_queueMutex.unlock();
				return;
			}
			else
			{
				_queueMutex.unlock();
				GD::out.printDebug("Queue " + std::to_string(id) + " is empty. Pushing pending queue...");
				_pushPendingQueueThreadMutex.lock();
				if(_disposing)
				{
					_pushPendingQueueThreadMutex.unlock();
					return;
				}
				GD::bl->threadManager.join(_pushPendingQueueThread);
				GD::bl->threadManager.start(_pushPendingQueueThread, true, GD::bl->settings.packetQueueThreadPriority(), GD::bl->settings.packetQueueThreadPolicy(), &BidCoSQueue::pushPendingQueue, this);
				_pushPendingQueueThreadMutex.unlock();
				return;
			}
		}
		if(_queue.front().getType() == QueueEntryType::PACKET)
		{
			if(!noSending)
			{
				std::shared_ptr<BidCoSPacket> packet = _queue.front().getPacket();
				bool stealthy = _queue.front().stealthy;
				_queueMutex.unlock();
				_sendThreadMutex.lock();
				if(_disposing)
				{
					_sendThreadMutex.unlock();
					return;
				}
				GD::bl->threadManager.join(_sendThread);
				GD::bl->threadManager.start(_sendThread, false, GD::bl->settings.packetQueueThreadPriority(), GD::bl->settings.packetQueueThreadPolicy(), &BidCoSQueue::send, this, packet, stealthy);
				_sendThreadMutex.unlock();
			}
			else _queueMutex.unlock();
		}
		else _queueMutex.unlock();
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
		_sendThreadMutex.unlock();
		_pushPendingQueueThreadMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	_pushPendingQueueThreadMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	_sendThreadMutex.unlock();
    	_pushPendingQueueThreadMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void BidCoSQueue::pop()
{
	try
	{
		if(_disposing) return;
		keepAlive();
		GD::out.printDebug("Popping from BidCoSQueue: " + std::to_string(id));
		if(_popWaitThread.joinable()) _stopPopWaitThread = true;
		_lastPop = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		_queueMutex.lock();
		if(_queue.empty())
		{
			_queueMutex.unlock();
			return;
		}
		_queue.pop_front();
		if(GD::bl->debugLevel >= 5 && !_queue.empty())
		{
			if(_queue.front().getType() == QueueEntryType::PACKET && _queue.front().getPacket()) GD::out.printDebug("Packet now at front of queue: " + _queue.front().getPacket()->hexString());
			else if(_queue.front().getType() == QueueEntryType::MESSAGE && _queue.front().getMessage()) GD::out.printDebug("Message now at front: Message type: 0x" + BaseLib::HelperFunctions::getHexString(_queue.front().getMessage()->getMessageType()));
		}
		_queueMutex.unlock();
		nextQueueEntry();
	}
	catch(const std::exception& ex)
    {
		_queueMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(BaseLib::Exception& ex)
    {
    	_queueMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__, ex.what());
    }
    catch(...)
    {
    	_queueMutex.unlock();
    	GD::out.printEx(__FILE__, __LINE__, __PRETTY_FUNCTION__);
    }
}

void BidCoSQueue::setWakeOnRadioBit()
{
	try
	{
		_setWakeOnRadioBit = true;
		if(_disposing) return;
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
