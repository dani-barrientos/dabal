﻿// sample.cpp : Defines the entry point for the application.
//
#include "test_callbacks/test.h"
#include "test_threading/test.h"
#include "test_parallelism/test.h"
#include "test_execution/test.h"
#include <CommandLine.h>
using tests::CommandLine;
#include <TestManager.h>
using tests::TestManager;
#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>  // support for loading levels from the environment variable
#include <spdlog/fmt/ostr.h> // support for user defined types


#define LIST_OPTION "list"
#define TEST_OPTION "t"
/**
 * main tests execution.
 * command line:
 *  -list list all available test
 *  -t <NAME>  execute given test
 * eachs test has it's own command line arguments
 **/

int main(int argc, const char* argv[])
{	
//ver cómo furrula y 
	//tebngo que ver que tenga funcionalidad similar a los appedern, por el tema de redirigir la salida a donde quiera->creo que son los sink
	spdlog::info("Probando spdlog {}.{}.{}  !", SPDLOG_VER_MAJOR, SPDLOG_VER_MINOR, SPDLOG_VER_PATCH);
	spdlog::error("Prueba error");
	spdlog::set_level(spdlog::level::err); // Set global log level to err
	spdlog::info("otra prueba");
	spdlog::error("Prueba error 2");
	spdlog::set_level(spdlog::level::debug); // Set global log level to debug
	#ifdef NDEBUG
	spdlog::info( "ESTAMOS EN RELEASE");
	#else
	spdlog::info( "ESTAMOS EN DEBUG");
	#endif
	
  	std::cout << "Running main with "<<argc<<" arguments:\n";
	for(int i =0;i<argc;++i)	
	{
		spdlog::debug("\t{}\n",argv[i]);
	}
	CommandLine::createSingleton(argc,argv);
	// const char* arg[] = {"kk","-list","-t","callbacks"};	
	// CommandLine::createSingleton(4,arg);
	test_callbacks::registerTest();
	test_threading::registerTest(); 
	test_parallelism::registerTest();
	test_execution::registerTest();

	if ( CommandLine::getSingleton().getOption(LIST_OPTION) != std::nullopt )
	{
		const auto& testmap = TestManager::getSingleton().getTests();
		for(const auto& [key,val]:testmap)
		{
			std::cout << key << " -> " << val.first <<'\n';
		}
	}
	TestManager::TestType currTest = nullptr;
	auto testOpt = CommandLine::getSingleton().getOption(TEST_OPTION);
	if (  testOpt != std::nullopt )
	{
		const string& name = testOpt.value();
		currTest = TestManager::getSingleton().getTest(name);
		std::cout << "Running test: " << name << '\n';
		
	}
	// else
	// 	currTest = TestManager::getSingleton().getTest(test_threading::TEST_NAME);
	
	if (currTest)
		return currTest();
	else return 0;
	
}
