#pragma once
/**
 * @file utilities.h
 * @author Daniel Barrientos (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-01-16
 * 
 * @copyright Copyright (c) 2022
 *  Utilities on task, because lack of a better place...Some funcions are intended to be put in a custom file/class..
 * 
 */
#include <core/Future.h>
#include <memory>
#include <tasking/Event_mthread.h>
#include <parallelism/Barrier.h>
#include <utility>
namespace mel
{
    namespace tasking
    {
    /**
        * @brief Waits for future completion, returning a wapper around the internal vale
        * @throws mel::core::WaitException if some error occured while waiting of the internal future exception if it has any error
        */
    template<class T> ::mel::core::WaitResult<T> waitForFutureMThread(  const mel::core::Future<T>& f,unsigned int msecs = EVENTMT_WAIT_INFINITE)
        {
            using ::mel::tasking::Event_mthread;
            struct _Receiver
            {		
                _Receiver():mEvent(false,false){}
                using futT = mel::core::Future<T>;
                ::mel::core::EWaitError wait( const futT& f,unsigned int msecs)
                {
                    EEventMTWaitCode eventResult;
                    int evId;
                    eventResult = mEvent.waitAndDo([this,f,&evId]()
                    {
                        evId = f.subscribeCallback(
                        std::function<::core::ECallbackResult( typename futT::ValueType&)>([this](typename futT::ValueType& ) 
                        {
                            mEvent.set();
                            return ::mel::core::ECallbackResult::UNSUBSCRIBE; 
                        }));
                    },msecs); 
                    f.unsubscribeCallback(evId); //maybe timeout, so callback won't be unsubscribed automatically       
                    switch( eventResult )
                    {
                    case EEventMTWaitCode::EVENTMT_WAIT_OK:                    
                        return ::mel::core::EWaitError::FUTURE_WAIT_OK;
                    case EEventMTWaitCode::EVENTMT_WAIT_KILL:
                        //event was triggered because a kill signal                    
                        return ::mel::core::EWaitError::FUTURE_RECEIVED_KILL_SIGNAL;
                        break;
                    case EEventMTWaitCode::EVENTMT_WAIT_TIMEOUT:
                        return ::mel::core::EWaitError::FUTURE_WAIT_TIMEOUT;
                        break;
                    default:
                        return ::mel::core::EWaitError::FUTURE_WAIT_OK; //silent warning
                    }			                        
                }
                private:
                ::mel::tasking::Event_mthread<> mEvent;

            };
            auto receiver = std::make_unique<_Receiver>();
            ::mel::core::EWaitError waitRes = receiver->wait(f,msecs);
            switch (waitRes)
            {
            case ::mel::core::EWaitError::FUTURE_WAIT_OK:
                if ( f.getValue().isValid())
                    return ::mel::core::WaitResult(f);
                else
                    std::rethrow_exception(f.getValue().error());
                break;
            case ::mel::core::EWaitError::FUTURE_RECEIVED_KILL_SIGNAL:
                throw mel::core::WaitException(::core::EWaitError::FUTURE_RECEIVED_KILL_SIGNAL,"Kill signal received");
                break;
            case ::mel::core::EWaitError::FUTURE_WAIT_TIMEOUT:
                throw mel::core::WaitException(::core::EWaitError::FUTURE_RECEIVED_KILL_SIGNAL,"Time out exceeded");
                break;
            case ::mel::core::EWaitError::FUTURE_UNKNOWN_ERROR:
                throw mel::core::WaitException(::core::EWaitError::FUTURE_UNKNOWN_ERROR,"Unknown error");
                break;
            default:
                throw mel::core::WaitException(::core::EWaitError::FUTURE_UNKNOWN_ERROR,"Unknown error");
                break;
            }
        }  
        
        MEL_API ::mel::tasking::EEventMTWaitCode waitForBarrierMThread(const ::mel::parallelism::Barrier& b,unsigned int msecs = ::mel::tasking::EVENTMT_WAIT_INFINITE );

    }
}