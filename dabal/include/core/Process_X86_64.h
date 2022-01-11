#pragma once

namespace core
{
    struct MThreadAttributtes;
}
using core::MThreadAttributtes;
#ifdef _MSC_VER
extern "C"   void resizeStack(  MThreadAttributtes* process,  unsigned int newSize ) ;
#else
extern "C"   void  resizeStack(  MThreadAttributtes* process,  unsigned int newSize );
#endif
namespace core
{
    struct MThreadAttributtes
    {
    public:
        volatile void* mIniSP;
        volatile void* mStackEnd;
        volatile unsigned char* mStack;
        volatile void* mIniRBP;
        volatile void* mIniRBX;
        volatile void* mRegisters[4]; //r12.r15
        volatile bool mSwitched;
        volatile unsigned int mStackSize;
        volatile unsigned int mCapacity;

    };
}
