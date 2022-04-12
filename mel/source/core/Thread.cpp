#include <assert.h>
#include <iostream>  //for debug purposes
#include <core/Thread.h>
#include <mpl/MemberEncapsulate.h>

using mel::mpl::makeMemberEncapsulate;

#include <text/logger.h>


/*#if defined(MEL_MACOSX) || defined(MEL_IOS)
#import <Foundation/Foundation.h>
#endif*/
#if defined(MEL_MACOSX) || defined(MEL_IOS)
//#import <sys/sysctl.h>
#include <sys/sysctl.h>
#include <pthread.h>
//#import <mach/thread_act.h>
#include <mach/thread_act.h>
#elif defined(MEL_LINUX) || defined(MEL_ANDROID) || defined(MEL_EMSCRIPTEN)
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#endif

namespace mel
{
	namespace core {

	unsigned int getNumProcessors()
	{
	#ifdef _WINDOWS
		unsigned int result = 0;
		//Processor count
		HANDLE hProcess = GetCurrentProcess();
		DWORD_PTR processAffinity;
		DWORD_PTR systemAffinity;
		DWORD_PTR initialAffinity = 1;
		BOOL ok = GetProcessAffinityMask(hProcess, &processAffinity, &systemAffinity);
		while (initialAffinity) {
			if (initialAffinity & systemAffinity)
				++result;
			initialAffinity <<= 1;
		}
		return result;
	#elif defined(MEL_MACOSX) || defined(MEL_IOS)
		unsigned int result = 0;
		size_t size = sizeof(result);
		if (sysctlbyname("hw.ncpu", &result, &size, NULL, 0))
			result = 1;
		return result;
	#elif defined(MEL_LINUX) || defined(MEL_ANDROID) 
		unsigned int count = 1;
		count = sysconf(_SC_NPROCESSORS_ONLN);
		return count;
	#elif defined(MEL_EMSCRIPTEN)
		return 1;  //@todo 
	#endif
	}
	uint64_t getProcessAffinity()
	{
		uint64_t result;
	#ifdef _WINDOWS
		//Processor count
		HANDLE hProcess = GetCurrentProcess();
		DWORD_PTR processAffinity;
		DWORD_PTR systemAffinity;
		DWORD_PTR initialAffinity = 1;
		BOOL ok = GetProcessAffinityMask(hProcess, &processAffinity, &systemAffinity);
		result = processAffinity;
		return result;
	#elif defined(MEL_MACOSX) || defined(_IOS)
		//macos and ios doesnt' allow affinity manipulation
		result = 0;
	#elif defined(MEL_ANDROID)
		cpu_set_t set;
		CPU_ZERO(&set);
		int ok = sched_getaffinity(0, sizeof(set), &set);
		if (ok == 0)
		{
			uint64_t aff = 0x1;
			result = 0;
			for (size_t i = 0; i < sizeof(result) * 8; ++i)
			{
				if (CPU_ISSET(i,&set))
				{
					result = result | aff;
				}
				aff = aff << 1;
			}
		}
		else
			result = 0;
	#endif
		return result;
	}

	#ifdef _WINDOWS
	#define HANDLETYPE HANDLE
	#elif defined(MEL_MACOSX) || defined(MEL_IOS)
	#define HANDLETYPE mel::core::ThreadId
	#elif defined(MEL_LINUX) || defined(MEL_ANDROID) || defined(MEL_EMSCRIPTEN)
	#define HANDLETYPE pid_t
	#endif
	//!helper
	static bool _setAffinity(uint64_t affinity, HANDLETYPE h )
	{
		bool result = false;
	#ifdef _WINDOWS
		DWORD_PTR aff = (DWORD_PTR)affinity;
		DWORD_PTR oldAff = SetThreadAffinityMask(h, aff);
		result = oldAff != 0;
	#elif defined(MEL_MACOSX) || defined(MEL_IOS)
		//@todo macos X (and assume same for iOS) doesn't allow to set the core for each thread. instead, it uses "groups" indetifying which threads belong to
		//same gruop and then share L2 cache. So, this is not the concept intended with current function, and so leave ti unimplemented
		/*
		pthread_t handle = pthread_self();
		mach_port_t mach_thread1 = pthread_mach_thread_np(handle);
		
		thread_affinity_policy_data_t policyData = { (integer_t)affinity };
		auto err = thread_policy_set(mach_thread1, THREAD_AFFINITY_POLICY, (thread_policy_t)&policyData, 1);
		switch(err)
		{
			case KERN_SUCCESS:
				result = true;
				break;
			case KERN_NOT_SUPPORTED:
				mel::text::error("Error setting thread affinity. Operation not supported");
				break;
			default:
				mel::text::errorf("Error setting thread affinity: %d",err);
		}
		*/
		return true;
	#elif defined(MEL_LINUX) || defined(MEL_ANDROID) 
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		for (size_t i = 0; i < sizeof(affinity) * 8; ++i)
		{
			if (affinity & 1)
				CPU_SET(i, &cpuset);
			affinity = affinity >> 1;
		}
		//pthread_setaffinity_np(mHandle, sizeof(cpuset), &cpuset); ->no existe en android
		//int ok = syscall(__NR_sched_setaffinity, mHandle, sizeof(affinity), &affinity);
		int ok = sched_setaffinity(h, sizeof(cpuset), &cpuset);
		result = (ok == 0);
		if (!result)
		{
			int err = errno;
			text::error("Error setting thread affinity. {}", err);
		}
	#elif defined(MEL_EMSCRIPTEN)
		//@todo no pilla el sched, pero se supone que está soportado
	#endif
		return result;
	}
	//Set affinity for current thread
	bool setAffinity(uint64_t affinity)
	{
	#ifdef _WINDOWS
		HANDLE h = GetCurrentThread();
		return _setAffinity(affinity,h);
	#elif defined(MEL_MACOSX) || defined(MEL_IOS)
		return _setAffinity(affinity,0);  //i don't know how to get current thread id on macos, but it doesn't matter because affinity in this sense can't be modified
	#elif defined(MEL_EMSCRIPTEN)
		return _setAffinity(affinity,0);  //seems to no exist gettid 
	#else
		return _setAffinity(affinity,gettid());
	#endif
	}

	#ifdef _WINDOWS
		DWORD WINAPI _threadProc(void* lpParameter)
		{
			Thread* t = (Thread*)lpParameter;
			assert(t && "NULL Thread!");
			DWORD result = 0;
			//result = t->runInternal();
			t->mFunction();
			return result;
		}
	#endif
	#if defined(MEL_LINUX) || defined(MEL_ANDROID) || defined(MEL_IOS) || defined (MEL_MACOSX) || defined(MEL_EMSCRIPTEN)
		void* _threadProc(void* param) {
			Thread* t=(Thread*)param;
			assert(t && "NULL Thread!");
			if ( t->mAffinity != 0 )
				t->setAffinity(t->mAffinity);
	/*
	esto tengo que estudiarlo a ver si realmente es necesario. no me fío
	#if defined(MEL_MACOSX) || defined(MEL_IOS)
			//Create the auto-release pool so anyone can call Objective-C code safely
			t->mARP=[[NSAutoreleasePool alloc] init];
	#endif		
	*/
			
			t->mFunction();
			//t->mResult = t->runInternal();
			/*
			lo mismo que el comentario anterior
	#if defined(MEL_MACOSX) || defined(MEL_IOS)
			//Destroy auto-release pool
			[(NSAutoreleasePool*)t->mARP release];
	#endif
	*/
			return NULL;
	}
	#endif	

	Thread::Thread()
	{	
		_initialize();
	}
	void Thread::_initialize()
	{
		mJoinResult = EJoinResult::JOINED_NONE;
	#ifdef _WINDOWS
		mID = 0;
	#endif
	/*
	@todo
	#if defined (MEL_MACOSX) || defined(MEL_IOS)
		mARP = nil;
	#endif
	*/
		mHandle = 0;
		mPriority = TP_NORMAL;
		

	/*
	I think I should create it on start
	#ifdef MEL_WINDOWS
		mHandle=CreateThread(
			NULL,
			0,
			_threadProc,
			this,
			CREATE_SUSPENDED,
			&mID);	
	#endif
	*/
	#if defined (MEL_MACOSX) || defined(MEL_IOS)
	/* @todoesto no me gusta nada,. revisar
		if ([NSThread isMultiThreaded]!=YES) {
			NSThread* t=[[NSThread alloc] init];
			[t start];
			assert([NSThread isMultiThreaded]==YES && "Cocoa is not running in multi-threaded mode!");
			[t release];
		}
		*/
	#endif
	#if defined (MEL_MACOSX) || defined(MEL_IOS) 
		mPriorityMin = sched_get_priority_min(SCHED_RR);
		mPriorityMax = sched_get_priority_max(SCHED_RR);
	#elif defined(MEL_LINUX) || defined(MEL_ANDROID)
		mPriorityMin=sched_get_priority_min(SCHED_OTHER);
		mPriorityMax=sched_get_priority_max(SCHED_OTHER);
	#endif
	}

	Thread::~Thread()
	{
	#ifdef _WINDOWS
		//maybe the thread was not started
		if (mHandle )
		{
			join();
			CloseHandle(mHandle);
			mHandle=0;
		}
		mID=0;
	#else
		if (mHandle && mJoinResult == EJoinResult::JOINED_NONE) 
		{
			join();
		}
	#endif
	}

	void Thread::sleep(const unsigned int millis) {
	#ifdef _WINDOWS
		Sleep(millis);
	#else
		struct timespec req={0},rem={0};
		time_t sec=(int)(millis/1000);
		long millisec=millis-(sec*1000);
		req.tv_sec=sec;
		req.tv_nsec=millisec*1000000L;
		nanosleep(&req,&rem);
	#endif
	}

	#if defined(MEL_MACOSX) || defined(MEL_LINUX) ||defined(MEL_IOS) || defined(MEL_ANDROID) || defined(MEL_EMSCRIPTEN)
	int priority2pthread(ThreadPriority tp,int pMin,int pMax) {
		switch (tp) {
			case TP_HIGHEST:
				return pMax;
			case TP_HIGH:
				return pMin+((pMax-pMin)*3/4);
			case TP_NORMAL:
				return (pMin+pMax)>>1;
			case TP_LOW:
				return pMin+((pMax-pMin)*1/4);
			case TP_LOWEST:
				return pMin;
			default:
				return (pMin+pMax)>>1;
		}
	}
	#endif

	void Thread::_start() {

		
	#ifdef MEL_WINDOWS
		mHandle=CreateThread(
			NULL,
			0,
			_threadProc,
			this,
			0,
			&mID);	
	#else
	using namespace ::std::string_literals;
		pthread_attr_t  attr;
		int err;
		//Then we setup thread attributes as "joinable"
		if ((err=pthread_attr_init(&attr))) {
			throw std::runtime_error( "Unable to initialize thread attributes: err="s+ std::to_string(err));
		}
		if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE)) { //PTHREAD_CREATE_DETACHED
			pthread_attr_destroy(&attr);
			throw std::runtime_error("Unable to set detached state!");
		}
		sched_param sp;
		sp.sched_priority=priority2pthread(mPriority,mPriorityMin,mPriorityMax);
		if (pthread_attr_setschedparam(&attr,&sp)) {
			pthread_attr_destroy(&attr);
			throw std::runtime_error("Unable to set thread priority!");
		}
		err = pthread_create(&mHandle, &attr, _threadProc, this);
		if (err != 0) {
			throw std::runtime_error("Unable to spawn new thread: err="s+ std::to_string(err));
		}
		if ((err=pthread_attr_destroy(&attr))) {
			#ifndef MEL_ANDROID
			pthread_cancel(mHandle);
			#endif
			throw std::runtime_error("Unable to destroy thread attribute!");
		}	
	#endif	
	}

	bool Thread::join(unsigned int millis)
	{
		if ( mJoinResult == EJoinResult::JOINED_NONE)
		{
	#ifdef MEL_WINDOWS
		DWORD status;
		mJoinResult = EJoinResult::JOINED_ERROR;
		if ( getCurrentThreadId() == getThreadId())
		{
			text::warn("Thread join done from same thread. deadLock!!. Ignoring join");	
		}
		else
		{
			status=WaitForSingleObject(mHandle, millis);
			switch(status)
			{
				case WAIT_OBJECT_0:
					mJoinResult = EJoinResult::JOINED_OK;
					break;
				case WAIT_FAILED:
				{
					DWORD err = GetLastError();
					text::error("Error joining thread. Err = 0x{:x}",err);
				}
					break;
					//@todo log en deadlock
				case WAIT_TIMEOUT:
					text::error("Error joining thread.Time out exceeded");
				default:			
					break;
				
			}
		}
	#else
		int err = pthread_join(mHandle, NULL/*result*/);
		
		if ( err != 0)
		{
			mJoinResult = EJoinResult::JOINED_ERROR;
			switch(err)
			{
				case EDEADLK:
					text::error("Error joining thread, deadloack. Maybe thread is destroyed from this thread context");	
					break;
				case EINVAL:
					text::error("Error joining thread. Thread not joinable");	
					break;
				case ESRCH:
					text::error("Error joining thread. Thread doesn't exist");
					break;
			}
		}else
			mJoinResult = EJoinResult::JOINED_OK;
		
	#endif
		}
		return mJoinResult == EJoinResult::JOINED_OK;	
	}

	void Thread::yield(YieldPolicy yp) {
	#ifdef _WINDOWS
		if (yp==YP_ANY_THREAD_ANY_PROCESSOR) {
			Sleep(0);
		}
		else {
			SwitchToThread();
		}
	#elif defined(MEL_LINUX) || defined(MEL_ANDROID) || defined(MEL_IOS) || defined (MEL_MACOSX) || defined(MEL_EMSCRIPTEN)
		if (yp==YP_ANY_THREAD_ANY_PROCESSOR) {
			Thread::sleep(0);
		}
		else {
			sched_yield();
		}
	#endif
	}

	#ifdef _WINDOWS
	void Thread::setPriority(ThreadPriority tp) {
		if (mPriority==tp)
			return;

		BOOL r;
		mPriority=tp;
		switch (tp) {
			case TP_HIGHEST:
				r=SetThreadPriority(mHandle,THREAD_PRIORITY_HIGHEST);
				break;
			case TP_HIGH:
				r=SetThreadPriority(mHandle,THREAD_PRIORITY_ABOVE_NORMAL);
				break;
			case TP_NORMAL:
				r=SetThreadPriority(mHandle,THREAD_PRIORITY_NORMAL);
				break;
			case TP_LOW:
				r=SetThreadPriority(mHandle,THREAD_PRIORITY_BELOW_NORMAL);
				break;
			case TP_LOWEST:
				r=SetThreadPriority(mHandle,THREAD_PRIORITY_LOWEST);
				break;
		}
		assert(r==TRUE);
	}
	#endif
	#ifndef MEL_WINDOWS

	void Thread::setPriority(ThreadPriority tp) {
		if (mPriority==tp)
			return;
		mPriority=tp;

		if (mHandle) {
			sched_param sp;
			sp.sched_priority = priority2pthread(mPriority,mPriorityMin,mPriorityMax);
	#ifdef MEL_ANDROID
			#define __PLATFORM_POLICY SCHED_OTHER
	#else
			//SCHED_RR vs SCHED_FIFO?
			#define __PLATFORM_POLICY SCHED_RR

	#endif
			int ret = pthread_setschedparam(mHandle, __PLATFORM_POLICY, &sp);
			#pragma unused(ret)
			assert(!ret);
		}

	}
	#endif


	uint64_t Thread::getAffinity() const
	{
		//@todo  uff, en Windows es mortal esto
		return 0;
	}
	//@todo incompleto
	bool Thread::setAffinity(uint64_t affinity)
	{
		bool result = false;
	#ifdef MEL_WINDOWS
		DWORD_PTR aff = (DWORD_PTR)affinity;
		DWORD_PTR oldAff = SetThreadAffinityMask(mHandle, aff);
		result = oldAff != 0;
	#elif defined(MEL_MACOSX) || defined(MEL_IOS)
		mAffinity = affinity;
		if ( mHandle != 0 ) //not started yet
		{
			_setAffinity(affinity, mHandle);
		}else
			return true; 
		//@todo android??
	#elif defined(MEL_LINUX) || defined(MEL_EMSCRIPTEN)
		_setAffinity(affinity,mThHandle);			
	#endif
		return result;
	}

	}
	mel::core::Event::EWaitCode mel::core::waitForBarrierThread(const ::mel::parallelism::Barrier& b,unsigned int msecs)
	{
		using ::mel::core::Event;
		struct _Receiver
		{		
			_Receiver():mEvent(false,false){}
			Event::EWaitCode wait(const ::mel::parallelism::Barrier& barrier,unsigned int msecs)
			{
				Event::EWaitCode eventresult;
				//text::info("Waiting for event");
				int evId;
				evId = barrier.subscribeCallback(
					std::function<::mel::core::ECallbackResult( const ::mel::parallelism::BarrierData&)>([this](const ::mel::parallelism::BarrierData& ) 
					{
						mEvent.set();
						//text::info("Event was set");
						return ::mel::core::ECallbackResult::UNSUBSCRIBE; 
					}));
				eventresult = mEvent.wait(msecs); 
				barrier.unsubscribeCallback(evId);
				return eventresult;
			}
			private:
				mel::core::Event mEvent;

		};
		auto receiver = std::make_unique<_Receiver>();
		return receiver->wait(b,msecs);	
	}
}