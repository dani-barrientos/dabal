#include "test_samples.h"
#include <tasking/utilities.h>
#include <execution/RunnableExecutor.h>
#include <execution/ThreadPoolExecutor.h>
#include <functional>
using std::string;
#include <memory>
using namespace std::literals::string_literals;
//code for the samples in the documentation
template <class ExecutorType> void _sampleBasic(ExecutorType ex)
{	
	auto th = ThreadRunnable::create();
	th->fireAndForget([ex] () mutable
	{
		auto res = ::tasking::waitForFutureMThread(
			execution::launch(ex,[](float param) noexcept
			{	
				return param+6.f;
			},10.5f)
			| execution::next( [](float& param) noexcept
			{
				return std::to_string(param);
			})
			| execution::parallel( 
				[](string& str)
				{					
					text::info("Parallel 1. {}",str+" hello!");
				},
				[](string& str)
				{
					text::info("Parallel 2. {}",str+" hi!");
				},
				[](string& str)
				{
					text::info("Parallel 2. {}",str+" whats up!");
				}
			)
		);
		if (res.isValid())
		{
			::text::info("Result value = {}",res.value());
		}
	},0,::tasking::Runnable::_killFalse);
}
template <class ExecutorType> void _sampleReference(ExecutorType ex)
{	
	string str = "Hello";
	auto th = ThreadRunnable::create();
	/*th->fireAndForget([ex,&str] () mutable
	{
		auto res = ::tasking::waitForFutureMThread(
			execution::launch(ex,[](string& str) noexcept ->string&
			{	
				//First job
				str += " Dani.";
				return str;
			},std::ref(str))
			| execution::next( [](string& str) noexcept -> string&
			{
				//Second job 
				str+= " How are you?";
				return str;
			})
			| execution::next( [](string& str ) noexcept
			{
				//Third job
				str += "Bye!";
				return str;
			})
			| execution::next( [](string& str ) noexcept
			{
				//Fourth job
				str += "See you!";
				return str;
			})
		);
		if (res.isValid())
		{
			::text::info("Result value = {}",res.value());
			::text::info("Original str = {}",str);
		}
	},0,::tasking::Runnable::_killFalse);
	*/
	th->fireAndForget([ex,&str] () mutable
	{
		auto res = ::tasking::waitForFutureMThread(
			execution::start(ex)
			| execution::inmediate(std::ref(str))
			| execution::next([](string& str) noexcept ->string&
			{	
				//First job
				str += " Dani.";
				return str;
			})
			| execution::next( [](string& str) noexcept -> string&
			{
				//Second job 
				str+= " How are you?";
				return str;
			})
			| execution::next( [](string& str ) noexcept
			{
				//Third job
				str += "Bye!";
				return str;
			})
			| execution::next( [](string& str ) noexcept
			{
				//Fourth job
				str += "See you!";
				return str;
			})
		);
		if (res.isValid())
		{
			::text::info("Result value = {}",res.value());
			::text::info("Original str = {}",str);
		}
	},0,::tasking::Runnable::_killFalse);

}
template <class ExecutorType> void _sampleError1(ExecutorType ex)
{	
	string str = "Hello";
	auto th = ThreadRunnable::create();
	th->fireAndForget([ex,&str] () mutable
	{
		try
		{
		auto res = ::tasking::waitForFutureMThread(
			execution::launch(ex,[](string& str) noexcept ->string&
			{	
				//First job
				str += " Dani.";
				return str;
			},std::ref(str))
			| execution::next( [](string& str) -> string&
			{
				//Second job 
				/*if ( ::tasking::Process::wait(1000) != ::tasking::Process::ESwitchResult::ESWITCH_OK)
					return str;*/
				str+= " How are you?";
				throw std::runtime_error("This is an error");
				//return str;
			})
			| execution::next( [](string& str ) noexcept
			{
				//Third job. Will never be executed!!!
				str += "Bye!";
				return str;
			})
			| execution::next( [](string& str ) noexcept
			{
				//Fourth job. Will never be executed
				str += "See you!";
				return str;
			})
			
		);
			::text::info("Result value = {}",res.value());
		}
		catch(core::WaitException& e)
		{
			::text::error("Some error occured!! Code= {}, Reason: {}",e.getCode(),e.what());
		}
		catch(std::exception& e)
		{
			::text::error("Some error occured!! Reason: {}",e.what());
		}
		::text::info("Original str = {}",str);
	},0,::tasking::Runnable::_killFalse);
}
template <class ExecutorType> void _sampleError2(ExecutorType ex)
{	
	string str = "Hello";
	auto th = ThreadRunnable::create();
	th->fireAndForget([ex,&str] () mutable
	{
		try
		{
			auto res = ::tasking::waitForFutureMThread(
				execution::launch(ex,[](string& str) noexcept ->string&
				{	
					//First job
					str += " Dani.";
					return str;
				},std::ref(str))
				| execution::next( [](string& str) -> string&
				{
					//Second job 
					str+= " How are you?";
					throw std::runtime_error("This is an error");
					//return str;
				})
				| execution::next( [](string& str ) noexcept
				{
					//Third job. Will never be executed!!!
					str += "Bye!";
					return str;
				})
				| execution::catchError( [](std::exception_ptr err)
				{
					return "Error caught!! ";
				})
				| execution::next( [](string& str ) noexcept
				{
					//Fourth job. 
					str += "See you!";
					return str;
				})
			);
			::text::info("Result value = {}",res.value());
		}
		catch(core::WaitException& e)
		{
			::text::error("Some error occured!! Code= {}, Reason: {}",e.getCode(),e.what());
		}catch(std::exception& e)
		{
			::text::error("Some error occured!! Reason: {}",e.what());
		}
		::text::info("Original str = {}",str);
	},0,::tasking::Runnable::_killFalse);
}
void _sampleTransfer()
{	
	auto th = ThreadRunnable::create();
	execution::Executor<Runnable> exr(th);
	exr.setOpts({true,false,false});
	parallelism::ThreadPool::ThreadPoolOpts opts;
	auto myPool = std::make_shared<parallelism::ThreadPool>(opts);
	parallelism::ThreadPool::ExecutionOpts exopts;
	execution::Executor<parallelism::ThreadPool> extp(myPool);
	extp.setOpts({true,false});
	th->fireAndForget([exr,extp] () mutable
	{
		
		try
		{
			auto res = ::tasking::waitForFutureMThread(

				execution::start(exr)
				| execution::inmediate("Hello "s)
				| execution::next( [](string& str) noexcept
				{
					//Second job 
					text::info("NEXT: {}",str);
					return str + ". How are you?";
				})
				| execution::transfer(extp)
				| execution::loop(0,10, [](int idx, string& str) noexcept
				{
					text::info("Iteration {}", str + std::to_string(idx));
				})
				| execution::next( [](string& str ) noexcept
				{
					//Fourth job. 
					str += "See you!";
					return str;
				})
			);
			::text::info("Result value = {}",res.value());
		}
		catch(core::WaitException& e)
		{
			::text::error("Some error occured!! Code= {}, Reason: {}",e.getCode(),e.what());
		}catch(std::exception& e)
		{
			::text::error("Some error occured!! Reason: {}",e.what());
		}
	},0,::tasking::Runnable::_killFalse);
// @todo	el prolema es que si no pongo esto no se ejecuta nada, por temas del kill y demas, ya que es el propio hilo
	//::core::Thread::sleep(10000);
}
template <class ExecutorType1,class ExecutorType2> void _sampleSeveralFlows(ExecutorType1 ex1,ExecutorType2 ex2)
{	
	auto th = ThreadRunnable::create();
	th->fireAndForget([ex1,ex2] () mutable
	{
        //first flow in one of the executors
        auto job1 = execution::start(ex1)
        | execution::next( []()
        {
            ::tasking::Process::wait(3000); //only possible if the executor as microthreading behaviour
            text::info("First job");
            return "First Job";
        });

        //second job in the other executor
        auto job2 = execution::start(ex1)
        | execution::parallel( []() noexcept
        {
            ::tasking::Process::wait(300); //only possible if the executor as microthreading behaviour
            text::info("second job, t1");
        },
        []() noexcept
        {
            ::tasking::Process::wait(100); //only possible if the executor as microthreading behaviour
            text::info("second job, t2");

        })
        | execution::next( []() noexcept
        {
            return 10;
        });
        //third job in the same executor as before
        //second job in the other executor
        auto job3 = execution::start(ex1)
        | execution::parallel_convert<std::tuple<int,float>>(
         []() noexcept
        {
            ::tasking::Process::wait(300); //only possible if the executor as microthreading behaviour
            text::info("second job, t1");
            return 5;
        },
        []() noexcept
        {
            ::tasking::Process::wait(100); //only possible if the executor as microthreading behaviour
            text::info("second job, t2");
            return 8.7f;
        });
       
		try
		{
            //on_all need to be executed in a context of some excutor, so one of them is given
            auto res = ::tasking::waitForFutureMThread(execution::on_all(ex2,job1,job2,job3));
            //the result of the job merging is as a tuple, where each elements corresponds to the job in same position
            auto& val = res.value();
            ::text::info("Result value = [{},{},({},{})]",std::get<0>(val),std::get<1>(val),std::get<0>(std::get<2>(val)),std::get<1>(std::get<2>(val)));
        }
		catch(core::WaitException& e)
		{
			::text::error("Some error occured!! Code= {}, Reason: {}",e.getCode(),e.what());
		}
	},0,::tasking::Runnable::_killFalse);
}
class MyClass1
{
	public:
		float f1(float p1,float p2) noexcept
		{
			return p1+p2;
		};
		string f2(float& p)
		{
			return std::to_string(p);
		}
};
//comentar el tema del noexcept del bind y enlace a mi pregunta
template <class ExecutorType> void _sampleCallables(ExecutorType ex)
{
	auto th = ThreadRunnable::create();
	MyClass1 obj;
	 using namespace std::placeholders;
	th->fireAndForget([ex,&obj] () mutable
	{

		auto res = ::tasking::waitForFutureMThread(
			execution::launch(ex,
				std::bind(&MyClass1::f1,&obj,6.7f,_1),10.5f)
			| std::bind(&MyClass1::f2,&obj,_1) tengo problema con referencia en floAt
			
			| execution::parallel( 
				[](string& str)
				{					
					text::info("Parallel 1. {}",str+" hello!");
				},
				[](string& str)
				{
					text::info("Parallel 2. {}",str+" hi!");
				},
				[](string& str)
				{
					text::info("Parallel 2. {}",str+" whats up!");
				}
			)
		);
		if (res.isValid())
		{
			::text::info("Result value = {}",res.value());
		}
	},0,::tasking::Runnable::_killFalse);

}
//más ejemplos: otro empezando con start y un inmediate; referencias,transferencia a executor, gestion errores, que no siempre sea una lambda, que se usen cosas microhililes.....
//me falta el PARALLEL_CONVERT
//tal vez algun ejemplo serio de verdad como colofon

void test_execution::samples()
{
	text::set_level(text::level::info);
	auto th = ThreadRunnable::create(true);			
	execution::Executor<Runnable> exr(th);
	exr.setOpts({true,false,false});
	parallelism::ThreadPool::ThreadPoolOpts opts;
	auto myPool = std::make_shared<parallelism::ThreadPool>(opts);
	parallelism::ThreadPool::ExecutionOpts exopts;
	execution::Executor<parallelism::ThreadPool> extp(myPool);
	extp.setOpts({true,true});
    
	//_sampleBasic(exr);	
//	_sampleBasic(extp);
//	_sampleReference(exr);
	//_sampleError1(exr);
	//_sampleError2(extp);
	//_sampleTransfer();
   // _sampleSeveralFlows(exr,extp);
   _sampleCallables(exr);
	text::info("HECHO");
}