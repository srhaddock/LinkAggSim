/*
Copyright 2017 Stephen Haddock Consulting, LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once
#include "Mac.h"

/*
*   Class BridgePort is a "port" on a Bridge. A "port" is an interface pointing to an ISS Service Access Point.
*      The ISS SAP can be provided, for example, by a Mac or by an Aggregator in a LinkAgg shim.
*   For the purposes of testing Link Aggregation the BridgePort needs very little functionality, however the
*      structure allows the addition of more fields and methods as the Bridge functionality gets more fully developed.
*/
/**/
class BridgePort
{
public:
	BridgePort();
	~BridgePort();
	BridgePort(const BridgePort& copySource) = delete;       // Disable copy constructor
	BridgePort& operator= (const BridgePort&) = delete;      // Disable assignment operator

	shared_ptr<Iss> pIss;

};
/**/

/*
*   Class Bridge is a system component that can be contained in a Device.
*       A  Bridge has two or more BridgePorts.
*
*       Currently the Bridge is not VLAN-aware, and does not learn addresses, so it simply
*           replicates a Frame received at one port to be transmitted on all other ports.
*       Currently it does not implement any loop prevention protocols, so beware of
*           generating data frames in a simulation with Bridges connected in a loop!
*/
/**/
class Bridge : public Component
{
public:
	Bridge(unsigned short devNum = 0, unsigned short sysNum = 0, int nPorts = 8);
	~Bridge();
	Bridge(Bridge& copySource) = delete;             // Disable copy constructor
	Bridge& operator= (const Bridge&) = delete;      // Disable assignment operator

	unsigned short vlanType;
	sysId SystemId;
	std::vector<unique_ptr<BridgePort>> bPorts;

	void reset();
	void timerTick();
	void run(bool singleStep);    // Receives a Frame from each BridgePort, replicates
	                                    //     and transmits the Frame on all other BridgePorts

};
/**/
