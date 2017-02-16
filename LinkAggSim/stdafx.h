// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

// TODO: reference additional headers your program requires here

#include <iostream>
#include <iomanip>
#include <fstream>
#include <memory>
#include <array>
#include <stack>
#include <queue>
#include <list>
#include <bitset>
#include <map>


using std::cout;
using std::endl;
using std::setw;
using std::hex;
using std::dec;
using std::unique_ptr;
using std::make_unique;
using std::shared_ptr;
using std::make_shared;
using std::move;

/*
*   Class SimLog contains static variables to have global scope in the simulation:
*      -- Time:  Each time increment is one single-step of the simulation.  No direct correlation to any unit of real time.
*      -- logFile:  a text file for messages regarding events in the simulation.
*/
/**/
class SimLog
{
public:
	SimLog();
	~SimLog();

	static int Time;
	static std::ofstream logFile;
	static int Debug;
};
/**/

enum adminValues { FORCE_FALSE, FORCE_TRUE, AUTO };
enum ComponentTypes { DEVICE, MAC, BRIDGE, END_STATION, LINK_AGG };

const unsigned short CVlanEthertype = 0x8100;
const unsigned short SVlanEthertype = 0x88a8;
const unsigned short SlowProtocolsEthertype = 0x8809;
const unsigned char LacpduSubType = 0x01;
const unsigned short DrniEthertype = 0x8952;
const unsigned char DrcpduSubType = 0x01;
const unsigned short PlaypenEthertypeA = 0x88b5;  // IEEE Std 802a-2003 Local Experimental Ethertype 1
const unsigned short PlaypenEthertypeB = 0x88b6;  // IEEE Std 802a-2003 Local Experimental Ethertype 2

const long long NearestCustomerBridgeDA = 0x0180c2000000; 
const long long SlowProtocolsDA         = 0x0180c2000002;
const long long NearestNonTpmrBridgeDA  = 0x0180c2000003;
const long long NearestBridgeDA         = 0x0180c200000e;

union sysId
{
	unsigned long long id;
	unsigned long long addr : 48;
	struct
	{
		unsigned short addrLow;
		unsigned short addrMid;
		unsigned short addrHigh;
		unsigned short pri;
	};
};

/**/
const unsigned long long defaultOUI = 0x24a600000000;   // Local individual (unicast) address
const unsigned long long defaultBrdgAddr = 0x24a60b000000;   // OUI for Bridges:  top two hex digits after OUI expected to be device number
const unsigned long long defaultEndStnAddr = 0x24a60e000000;   // OUI for End Stations:  top two hex digits after OUI expected to be device number
const unsigned long long defaultDA = 0x30be00ffffff;   // Local group (multicast) address

/**/
