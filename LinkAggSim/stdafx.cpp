// stdafx.cpp : source file that includes just the standard includes
// TestLinkAgg.pch will be the pre-compiled header
// stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"


SimLog::SimLog()
{
}

SimLog::~SimLog()
{
}

// SimLog;  // init globals: Time and logfile
int SimLog::Time = 0;
int SimLog::Debug = 0;
std::ofstream SimLog::logFile("LinkAggSim output.txt");


// TODO: reference any additional headers you need in STDAFX.H
// and not in this file
