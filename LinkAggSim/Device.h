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
#include "Bridge.h"
#include "Frame.h"
#include "LinkAgg.h"


/*
*   Class EndStn (End Station) is a system component that can be contained in a Device.
*       An End Station has a single port (Iss interface).
*
*       Currently the End Station serves simply as a frame generator and receiver.
*           It generates test frames with a sequence number.
*           When a frame is received it is counted and discarded.
*/
/**/
class EndStn : public Component
{
public:
	EndStn(unsigned short devNum, unsigned short sysNum);
	~EndStn();
	EndStn(EndStn& copySource) = delete;             // Disable copy constructor
	EndStn& operator= (const EndStn&) = delete;      // Disable assignment operator

	sysId SystemId;
	int sequenceNumber;
	unsigned int rxFrameCount;

	void reset();
	void timerTick();
	void run(bool singleStep);

	void generateTestFrame(shared_ptr<Sdu> pTag = nullptr);

	// protected:
	shared_ptr<Iss> pIss;
};


/*
 *    Class Device represents a piece of networking equipment that can be connected to other Devices via its Macs.
 *        Devices contain two or more system components including:
 *           -- At least one Mac, which can be connected (linked) to another Mac in this or another Device.
 *           -- At least one End Station or Bridge component.  The ports (Iss interface) on a End Station or Bridge 
 *                can be attached to a Mac or a shim. Ports can also be connected to a port (Iss interface)
 *                on another component in the Device via an internal link (iLink). 
 *           -- Zero or more shims, e.g. a Link Aggregation shim.
 */
/**/
class Device : public Component
{
public:
	Device(int numMacs = 1);
	~Device();
	Device(Device& copySource) = delete;             // Disable copy constructor
	Device& operator= (const Device&) = delete;      // Disable assignment operator

	static unsigned short getDeviceCount();
	unsigned short getDeviceNumber();

	std::vector<unique_ptr<Component>> pComponents;
	std::vector<shared_ptr<Mac>> pMacs;

	virtual void reset() override;                         // Initialize all operational parameters of all Components and Macs
	       //   reset() does not restore default configuration, just initial state of operational variables
	       //   Should I add a restoreDefault() routine?
	virtual void timerTick() override;                     // If not suspended, Tick timers in all Components and Macs
	virtual void run(bool singleStep) override;            // If not suspended, Make one or more pass through all state machines in all Components and Macs

	void transmit();                      // If not suspended, Transmit a Frame from any Macs in Device with a Frame ready to transmit
	void disconnect();                    // Disconnect all Macs in the Device that are connected to other Macs

	void createBridge(unsigned short type = 0);
	void createEndStation();

protected:
	static unsigned short devCnt;
	unsigned short devNum;

};
/**/

/**/


