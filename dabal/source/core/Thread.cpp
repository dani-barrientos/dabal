#include <assert.h>
#include <core/Thread.h>
#include <mpl/MemberEncapsulate.h>

using mpl::makeMemberEncapsulate;
#include <core/TLS.h>
using core::TLS;
#include <spdlog/spdlog.h>

static TLS::TLSKey gCurrentThreadKey;
static bool gCurrentThreadKeyCreated = false;
static CriticalSection gCurrrentThreadCS;

#if defined(_MACOSX) || defined(_IOS)
#import <Foundation/Foundation.h>
#endif
#if defined(_MACOSX) || defined(_IOS)
#import <sys/sysctl.h>
#include <pthread.h>
#import <mach/thread_act.h>
#elif defined(DABAL_POSIX)
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
	#ifdef DABAL_ANDROID
	#import <sys/syscall.h>
	#endif
#endif


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
#elif defined(_MACOSX) || defined(_IOS)
	unsigned int result = 0;
	size_t size = sizeof(result);
	if (sysctlbyname("hw.ncpu", &result, &size, NULL, 0))
		result = 1;
	return result;
#elif defined(DABAL_POSIX)
	unsigned int count = 1;
	count = sysconf(_SC_NPROCESSORS_ONLN);
	return count;
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
#elif defined(_MACOSX) || defined(_IOS)
    //macos and ios doesnt' allow affinity manipulation
	result = 0;
#elif defined(_ANDROID)
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
#elif defined(_MACOSX) || defined(_IOS)
#define HANDLETYPE core::ThreadId
#elif defined(DABAL_POSIX)
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
#elif defined(_MACOSX) || defined(_IOS)
    //@todo macos X (and assume same for iOS) doesn't allow to setthe core for each thread. instead, it uses "groups" indetifying wich threads belong to
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
            Logger::getLogger()->error("Error setting thread affinity. Operation not supported");
            break;
        default:
            Logger::getLogger()->errorf("Error setting thread affinity: %d",err);
    }
     */
    return true;
#elif defined(DABAL_POSIX)
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
		//Logger::getLogger()->errorf("Error setting thread affinity. %d", 1, err);
		spdlog::error("Error setting thread affinity. {}", err);
	}
	
#endif
	return result;
}
//Set affinity for current thread
bool setAffinity(uint64_t affinity)
{
#ifdef _WINDOWS
    HANDLE h = GetCurrentThread();
    return _setAffinity(affinity,h);
#elif defined(_MACOSX) || defined(_IOS)
    return _setAffinity(affinity,0);  //i don't know how to get current thread id on macos, but it doesn't matter because affinity in this sense can't be modified
#elif defined(DABAL_POSIX)
    return _setAffinity(affinity,gettid());
#endif
}
DABAL_CORE_OBJECT_TYPEINFO_IMPL_ROOT(Thread);

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
#if defined(DABAL_POSIX)
	void* _threadProc(void* param) {
		Thread* t=(Thread*)param;
		assert(t && "NULL Thread!");
        if ( t->mAffinity != 0 )
            t->setAffinity(t->mAffinity);
#if defined(_MACOSX) || defined(_IOS)
		//Create the auto-release pool so anyone can call Objective-C code safely
		t->mARP=[[NSAutoreleasePool alloc] init];
#endif		
		t->mFunction();
		//t->mResult = t->runInternal();
#if defined(_MACOSX) || defined(_IOS)
		//Destroy auto-release pool
		[(NSAutoreleasePool*)t->mARP release];
#endif
		return NULL;
}
#endif	

Thread::Thread()
{
	_initialize();
}
void Thread::_initialize()
{
#ifdef _WINDOWS
	mID = 0;
#endif

#if defined (DABAL_POSIX)
	mJoined = false;
#endif
#if defined (_MACOSX) || defined(_IOS)
	mARP = nil;
#endif
	mHandle = 0;
	mResult = 0;
	mPriority = TP_NORMAL;
	gCurrrentThreadCS.enter();
	if ( !gCurrentThreadKeyCreated )
	{
		TLS::createKey( gCurrentThreadKey );
		gCurrentThreadKeyCreated = true;
	}
	gCurrrentThreadCS.leave();

#ifdef _WINDOWS
	mHandle=CreateThread(
		NULL,
		0,
		_threadProc,
		this,
		CREATE_SUSPENDED,
		&mID);	
#endif
#if defined (_MACOSX) || defined(_IOS)
	if ([NSThread isMultiThreaded]!=YES) {
		NSThread* t=[[NSThread alloc] init];
		[t start];
		assert([NSThread isMultiThreaded]==YES && "Cocoa is not running in multi-threaded mode!");
		[t release];
	}
#endif
#if defined (_MACOSX) || defined(_IOS) 
	mPriorityMin = sched_get_priority_min(SCHED_RR);
	mPriorityMax = sched_get_priority_max(SCHED_RR);
#elif defined(DABAL_POSIX)
	mPriorityMin=sched_get_priority_min(SCHED_OTHER);
	mPriorityMax=sched_get_priority_max(SCHED_OTHER);
#endif
}

Thread::~Thread()
{
#ifdef _WINDOWS
		//maybe the thread was not started
	if (mHandle && !CloseHandle(mHandle))
	{
		//Logger::getLogger()->warnf("Thread: unable to close handle %d",1,mHandle);
		mHandle=0;
	}
	mID=0;
#endif
#if defined (DABAL_POSIX)
	if (mHandle && !mJoined) 
	{
		join();
	}
#endif
}

void Thread::sleep(const unsigned int millis) {
#ifdef _WINDOWS
	Sleep(millis);
#endif
#if defined (DABAL_POSIX)
	struct timespec req={0},rem={0};
	time_t sec=(int)(millis/1000);
	long millisec=millis-(sec*1000);
    req.tv_sec=sec;
    req.tv_nsec=millisec*1000000L;
    nanosleep(&req,&rem);
#endif
}

#if defined(DABAL_POSIX)
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

	
#ifdef _WINDOWS
	if (mHandle) {
		DWORD sc=ResumeThread(mHandle);
		if (sc!=1 && sc!=0)
		{
		//	sacar error del estilo  ("Thread not started (suspended count=%d)!",1,sc);
		}
	}
#endif
#if defined (DABAL_POSIX)
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
		#ifndef _ANDROID
		pthread_cancel(mHandle);
		#endif
		throw std::runtime_error("Unable to destroy thread attribute!");
	}	
#endif	
}


/*
::tasking::EGenericProcessResult Thread::suspendInternal(uint64_t millis,Process* proc) {
	mPauseEV.wait();
	return ::tasking::EGenericProcessResult::KILL;
}
unsigned int Thread::suspend()
{
	if ( mState == THREAD_RUNNING )
	{
		post(makeMemberEncapsulate(&Thread::suspendInternal,this)); //post as the first task for next iteration
		mState=THREAD_SUSPENDED; //aunque todav�a no se haya hecho el wait,
								 //por la forma en que funcionan los eventos da igual, porque si se hace
								 //un resume justo antes de procesarse la tarea "suspendInternal", implica que el set
								 //hace que el siguiente wait no espere, lo cual es lo correcto
	}
	return 1;
}

unsigned int Thread::resume() {
	if ( mState == THREAD_SUSPENDED )
	{
		mState = THREAD_RUNNING;
#ifdef _WINDOWS
		//DWORD sc=ResumeThread(mHandle);
		mPauseEV.set();
	}
	//return mHandle?sc:0;
	return 1;
#endif
#if defined (DABAL_POSIX)
		mPauseEV.set();
	}
	return 1;

#endif
}
*/
/*
void Thread::terminate(unsigned int exitCode)
{

	mEnd = true;
	if ( mState == THREAD_SUSPENDED )
	{
		resume();
	}else if ( mState == THREAD_INIT )
	{
		//TODO vigilar, esto no est� bien. Puede ocurrir que en este momento est� arrancado el hilo
		#ifdef _WINDOWS
		//in Windows we need to ResumeThread if it wasn't started. Other way it won't be removed from memory( I don't know why..)
		ResumeThread( mHandle );
		#endif
	}
	mExitCode = exitCode;
}

unsigned int Thread::runInternal() {

	TLS::setValue( gCurrentThreadKey, this );
	if ( !mEnd )
	{
		//don't run loop if it's finished. It could be possible to terminate a thread before is started, so it
		//won't be executed
		spdlog::debug("Thread::runInternal");
		mResult=run();
		//Triger finalization callbacks and invoke onThreadEnd 
		threadEnd();
	}
	mState = THREAD_FINISHED;
	return mResult;
}
*/
bool Thread::join(unsigned int millis)
{
#ifdef _WINDOWS
	//@todo por qu� no usar WaitForSingleObject?
	DWORD status=WaitForMultipleObjects(1, &mHandle, TRUE, millis);
	return status!=WAIT_TIMEOUT;
#endif
#if defined (DABAL_POSIX)
	int err = pthread_join(mHandle, NULL/*result*/);
	mJoined=!err;
	if (err) {
		spdlog::error("Error joining thread: err = {}", err);
	}
	return mJoined;	
#endif
}

void Thread::yield(YieldPolicy yp) {
#ifdef _WINDOWS
	if (yp==YP_ANY_THREAD_ANY_PROCESSOR) {
		Sleep(0);
	}
	else {
		SwitchToThread();
	}
#elif defined(_MACOSX ) || defined(_IOS) || defined(_ANDROID)
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
#if defined(DABAL_POSIX)
void Thread::setPriority(ThreadPriority tp) {
	if (mPriority==tp)
		return;
	mPriority=tp;

    if (mHandle) {
        sched_param sp;
        sp.sched_priority = priority2pthread(mPriority,mPriorityMin,mPriorityMax);
#ifdef _ANDROID
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

Thread* Thread::getCurrentThread()
{
	if ( gCurrentThreadKeyCreated ) //not multithread-safe but it shouldn't be a problem
	{
		return (Thread*)TLS::getValue( gCurrentThreadKey );
	}else
	{
		return NULL;
	}
}
uint64_t Thread::getAffinity() const
{
	//@todo  uff, en Windows es mortal esto
	return 0;
}
bool Thread::setAffinity(uint64_t affinity)
{
	bool result = false;
#ifdef _WINDOWS
	DWORD_PTR aff = (DWORD_PTR)affinity;
	DWORD_PTR oldAff = SetThreadAffinityMask(mHandle, aff);
	result = oldAff != 0;
#elif defined(_MACOSX) || defined(_IOS)
	mAffinity = affinity;
    if ( mHandle != 0 ) //not started yet
    {
        _setAffinity(affinity, mHandle);
    }else
        return true; //@todo
#elif defined(DABAL_POSIX)
	mAffinity = affinity;
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
	if (mThHandle != 0)
	{
		int ok = sched_setaffinity(mThHandle, sizeof(cpuset), &cpuset);
		result = (ok == 0);
		if (!result)
		{
			int err = errno;
			//Logger::getLogger()->errorf("Error setting thread affinity. %d", 1, err);
		}
	}
	else
		return true;
#endif
	return result;
}

}
::core::Event::EWaitCode core::waitForBarrierThread(const ::parallelism::Barrier& b,unsigned int msecs)
{
	using ::core::Event;
	struct _Receiver
	{		
		_Receiver():mEvent(false,false){}
		Event::EWaitCode wait(const ::parallelism::Barrier& barrier,unsigned int msecs)
		{
			Event::EWaitCode eventresult;
		 	//spdlog::info("Waiting for event");
			int evId;
			evId = barrier.subscribeCallback(
				std::function<::core::ECallbackResult( const ::parallelism::BarrierData&)>([this](const ::parallelism::BarrierData& ) 
				{
					 mEvent.set();
				    //spdlog::info("Event was set");
					return ::core::ECallbackResult::UNSUBSCRIBE; 
				}));
			eventresult = mEvent.wait(msecs); 
			barrier.unsubscribeCallback(evId);
			return eventresult;
		}
		private:
			::core::Event mEvent;

	};
	auto receiver = std::make_unique<_Receiver>();
	return receiver->wait(b,msecs);	
}