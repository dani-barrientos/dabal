#pragma once

#include <tasking/GenericProcessDefs.h>
#include <tasking/Process.h>
using tasking::Process;

#include <functional>
namespace tasking
{
	/**
	* @brief A Process constructed from a functor
	* This Process it's executed until given functor returns EGenericProcessResult::KILL. This functor is given the current state wich is:
	*		INIT : first time it's executed
	*		RUN : second and next times
	*		KILL : a kill request it's done. 
	*/
	class DABAL_API GenericProcess : public Process
	{
		DABAL_CORE_OBJECT_TYPEINFO;
	public:
		typedef std::function<EGenericProcessResult (uint64_t,Process*)> GenericCallback;
	private:
		GenericCallback					mProcessCallback;
		std::function<bool ()> 			mKillCallback;
		volatile EGenericProcessResult	mUpdateResult;

	public:
		/**
		* constructor
		* @todo hacer los constructores convenientes
		*/
		GenericProcess( );
		~GenericProcess();
		
		/**
		* set callback to execute on process update
		* @param[in] functor A functor with signature bool f(unsigned int msegs, Process* )
		*/
		template <class F> void setProcessCallback( F&& functor );
		inline const GenericProcess::GenericCallback& getProcessCallback() const;
		template <class F> void setKillCallback( F&& functor );
		
		/*
		* set callback to execute on process init
		* @param[in] functor. A functor with signature bool f(unsigned int msegs, Process* )
		
		template <class F>
		void setInitCallback( F functor );
		*/
		/*
		* set callback to execute on process kill. it must return boolean (see Process::onKill)
		* @param functor. A functor with signature bool f(unsigned int msegs, Process* )
		*
		template <class F>
		void setKillCallback( F functor );*/

	protected:
		void onInit(uint64_t msegs) override;
		/**
		* overridden from Process
		*/
		bool onKill() override;
		/**
		* overridden from Process
		* @param msegs    msegs
		* @remarks If callback method return false. Process is killed.
		*/
		void onUpdate(uint64_t msegs) override;

	};
	template <class F> void GenericProcess::setProcessCallback( F&& functor )
	{
		// delete mProcessCallback;
		// mProcessCallback = new GenericCallback( ::std::forward<F>(functor),::core::use_functor );
		mProcessCallback = std::forward<F>(functor);
	}
	const GenericProcess::GenericCallback& GenericProcess::getProcessCallback() const
	{
		return mProcessCallback;
	}
	template <class F> void GenericProcess::setKillCallback( F&& functor )
	{
		mKillCallback = std::forward<F>(functor);
	}
	

};