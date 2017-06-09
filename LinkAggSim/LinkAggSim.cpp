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

/*
*   This program is an Ethernet network simulator developed primarily to test Link Aggregation.
*   It creates a simulation environment consisting of network Devices interconnected by Ethernet Links.
*   Devices contain two or more system components including:
*           1) At least one End Station or Bridge component.
*                An End Station component runs protocols, and generates and receives Ethernet Frames.
*                A Bridge component runs protocols, and relays Frames between BridgePorts.
*				  The ports (Iss interfaces) on a End Station or Bridge can be attached to a Mac or a shim.
*                    Ports can also be connected (in the future) to a port (Iss interface)
*                    on another component in the Device via an internal link (iLink).
*           2) At least one Mac, which can be connected with a Link to another Mac in this or another Device.
*           3) Zero or more shims, e.g. a Link Aggregation (Lag) shim or a Configuration Management (Cfm) shim.
*
*/

// LinkAggSim.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Device.h"
#include "Mac.h"
#include "Frame.h"


int _tmain(int argc, _TCHAR* argv[])
{
	//	SimLog::logFile << System::DateTime::Now;
	//	SimLog::logFile << asctime_s(); 
	SimLog::logFile << endl;
	SimLog::Debug = 8; // 6 9

	void send8Frames(EndStn& source);

	void basicLagTest(std::vector<unique_ptr<Device>>& Devices);
	void preferredAggregatorTest(std::vector<unique_ptr<Device>>& Devices);
	void lagLoopbackTest(std::vector<unique_ptr<Device>>& Devices);
	void nonAggregatablePortTest(std::vector<unique_ptr<Device>>& Devices);
	void limitedAggregatorsTest(std::vector<unique_ptr<Device>>& Devices);
	void dualHomingTest(std::vector<unique_ptr<Device>>& Devices);
	void axbkHierarchicalLagTest(std::vector<unique_ptr<Device>>& Devices);
	void distributionTest(std::vector<unique_ptr<Device>>& Devices);
	void waitToRestoreTest(std::vector<unique_ptr<Device>>& Devices);
	void adminVariableTest(std::vector<unique_ptr<Device>>& Devices);

	cout << "*** Start of program ***" << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << "*** Start of program ***" << endl << endl;

	//
	//  Build some Devices
	//
	/**/
	std::vector<unique_ptr<Device>> Devices;   // Pointer to each device stored in a vector

	int brgCnt = 3;
	int brgMacCnt = 8;
	int endStnCnt = 3;
	int endStnMacCnt = 4;

	cout << "   Building Devices:  " << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << "   Building Devices:  " << endl << endl;

	for (int dev = 0; dev < brgCnt + endStnCnt; dev++)
	{
		unique_ptr<Device> thisDev = nullptr;
		if (dev < brgCnt)  // Build Bridges first
		{
			thisDev = make_unique<Device>(brgMacCnt);     // Make a device with brgMacCnt MACs
			thisDev->createBridge(CVlanEthertype);        // Add a C-VLAN bridge component with a bridge port for each MAC
		}
		else               //    then build End Stations
		{
			thisDev = make_unique<Device>(endStnMacCnt);  // Make a device with endStnMacCnt MACs
			thisDev->createEndStation();                  // Add an end station component
		}
		Devices.push_back(move(thisDev));                       // Put Device in vector of Devices
	}

	//
	//  Run the simulation
	//
	cout << endl << "   Running Simulation:  " << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << "   Running Simulation (with Debug level " << SimLog::Debug << "):  " << endl << endl;

	SimLog::Time = 0;

	//
	//  Select tests to run
	//

	// basicLagTest(Devices);
	preferredAggregatorTest(Devices);
	// lagLoopbackTest(Devices);
	// nonAggregatablePortTest(Devices);
	// limitedAggregatorsTest(Devices);
	// dualHomingTest(Devices);
	// axbkHierarchicalLagTest(Devices);
	// distributionTest(Devices);
	// waitToRestoreTest(Devices);
	adminVariableTest(Devices);
	//
	// Clean up devices.
	//

	cout << endl << "    Cleaning up devices:" << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << "    Cleaning up devices:" << endl << endl;

	Devices.clear();

	cout << endl << "*** End of program ***" << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << "*** End of program ***" << endl << endl;

	return 0;
}


/**/
void send9Frames(EndStn& source)
{
	source.generateTestFrame();                                                  // create and transmit un-tagged test frame
	for (unsigned short vid = 0; vid < 8; vid++)
	{
		shared_ptr<VlanTag> pVtag = make_shared<VlanTag>(CVlanEthertype, vid);   // create C-VLAN Tag
		source.generateTestFrame(pVtag);                                         // create and transmit C-VLAN-tagged test frame
	}
}

void printLinkMap(std::vector<unique_ptr<Device>>& Devices)
{
	for (auto& pDev : Devices)
	{
		LinkAgg& devLinkAgg = (LinkAgg&)*(pDev->pComponents[1]);
		for (auto& pAgg : devLinkAgg.pAggregators)
		{
			if (pAgg->getOperational())
			{
				cout << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << pAgg->actorSystem.addrMid
					<< ":" << pAgg->get_aAggID() << "     DWC = " << pAgg->get_aAggOperDiscardWrongConversation()
					<< endl << "     Actor   PortAlg = 0x" << pAgg->get_aAggPortAlgorithm() << "  CDigest = ";
				for (auto& val : pAgg->get_aAggOperConversationListDigest()) cout << val;
				cout << endl << "     Partner PortAlg = 0x" << pAgg->get_aAggPartnerPortAlgorithm() << "  CDigest = ";
				for (auto& val : pAgg->get_aAggPartnerOperConversationListDigest()) cout << val;
				cout << endl << dec << "                ConvID->Link list" << " { ";
				for (int i = 0; i < 8; i++)
				{
					cout << pAgg->get_conversationLink(i) << "  ";
				}
				cout << "}" << endl;
				if (SimLog::Debug > 0)
				{
					SimLog::logFile << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << pAgg->actorSystem.addrMid
						<< ":" << pAgg->get_aAggID()
						<< "  PortAlg = 0x" << pAgg->get_aAggPortAlgorithm()
						<< "  DWC = " << pAgg->get_aAggOperDiscardWrongConversation()
						<< dec << "  ConvID->Link list { ";
					for (int i = 0; i < 8; i++)
					{
						SimLog::logFile << pAgg->get_conversationLink(i) << "  ";
					}
					SimLog::logFile << "}" << endl;
				}
			}
		}
	}
}

/*
*/
void basicLagTest(std::vector<unique_ptr<Device>>& Devices)
{
	int start = SimLog::Time;

	cout << endl << endl << "   Basic LAG Tests:  " << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << endl << endl << "   Basic LAG Tests:  " << endl << endl;

	for (auto& pDev : Devices)
	{
		pDev->reset();   // Reset all devices
	}

	for (int i = 0; i < 1000; i++)
	{
		if (SimLog::Debug > 1)
			SimLog::logFile << "*" << endl;

		//  Make or break connections

		if (SimLog::Time == start + 10)
			Mac::Connect((Devices[0]->pMacs[0]), (Devices[1]->pMacs[0]), 5);   // Connect two Bridges
		// Link 1 comes up with AggPort b00:100 on Aggregator b00:200 and AggPort b01:100 on Aggregator b01:200.
		if (SimLog::Time == start + 100)
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[1]->pMacs[1]), 5);   // Second link between same Bridges
		// Link 2 comes up with AggPort b00:101 on Aggregator b00:200 and AggPort b01:101 on Aggregator b01:200.
		if (SimLog::Time == start + 200)
			Mac::Connect((Devices[0]->pMacs[2]), (Devices[1]->pMacs[2]), 5);   // Third link between same Bridges
		// Link 3 comes up with AggPort b00:102 on Aggregator b00:200 and AggPort b01:102 on Aggregator b01:200.

		if (SimLog::Time == start + 300)
			Mac::Disconnect((Devices[0]->pMacs[0]));                           // Take down first link
		// Link 1 goes down and conversations immediately re-allocated to other links.
		// AggPorts b00:102 and b00:103 remain up on Aggregator b00:200
		if (SimLog::Time == start + 400)
			Mac::Connect((Devices[0]->pMacs[0]), (Devices[1]->pMacs[0]), 5);   // Reconnect first link between same Bridges
		// Link 1 comes back up with one or two (depending on coupled/uncoupled MUX) LACPDU exchanges.

		if (SimLog::Time == start + 500)
			Mac::Disconnect((Devices[0]->pMacs[1]));                           // Take down second link
		// Link 2 goes down and conversations immediately re-allocated to other links.
		// AggPorts b00:100 and b00:102 remain up on Aggregator b00:200
		if (SimLog::Time == start + 600)
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[1]->pMacs[3]), 5);   // Move one end of link to a different port on second bridge
		// Link 2 comes up with AggPort b00:101 on Aggregator b00:200 and AggPort b01:103 on Aggregator b01:200.
		// AggPort b01:100 gets kicked off Aggregator b01:200 since the partner moved to a new port (port_moved signal in RxSM)


		if (SimLog::Time == start + 700)
			Mac::Connect((Devices[0]->pMacs[4]), (Devices[2]->pMacs[0]), 5);   // Connect link between first and third bridges
		// Link 1 of new LAG comes up with AggPort b00:104 on Aggregator b00:204 and AggPort b02:100 on Aggregator b02:200.
		if (SimLog::Time == start + 800)
			Mac::Connect((Devices[0]->pMacs[5]), (Devices[2]->pMacs[2]), 5);   // Connect another link between first and third bridges
		// Link 3 comes up with AggPort b00:105 on Aggregator b00:204 and AggPort b01:102 on b02:200
		// AggPorts b00:102 and b00:103 move to Aggregator b00:201, so both ends of LAG temporarily non-operational.
		//    With small values of aggregateWaitTime there is additional "bouncing" of aggregator operational.

		if (SimLog::Time == start + 990)
			for (auto& pDev : Devices)
			{
				pDev->disconnect();      // Disconnect all remaining links on all devices
			}


		//  Run all state machines in all devices
		for (auto& pDev : Devices)
		{
			pDev->timerTick();     // Decrement timers
			pDev->run(true);   // Run device with single-step true
		}

		//  Transmit from any MAC with frames to transmit
		for (auto& pDev : Devices)
		{
			pDev->transmit();
		}

		SimLog::Time++;
	}
}

/*
*/
void preferredAggregatorTest(std::vector<unique_ptr<Device>>& Devices)
{
	int start = SimLog::Time;
	LinkAgg& dev0Lag = (LinkAgg&)*(Devices[0]->pComponents[1]);  // alias to LinkAgg shim of bridge b00

	cout << endl << endl << "   Preferred Aggregator Tests:  " << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << endl << endl << "   Preferred Aggregator Tests:  " << endl << endl;

	for (auto& pDev : Devices)
	{
		pDev->reset();   // Reset all devices
	}

	for (int i = 0; i < 1000; i++)
	{
		if (SimLog::Debug > 1)
			SimLog::logFile << "*" << endl;

		//  Make or break connections

		if (SimLog::Time == start + 10)
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[1]->pMacs[2]), 5);   // Connect two Bridges
		// Link 2 comes up with AggPort b00:101 on Aggregator b00:201 and AggPort b01:102 on Aggregator b01:202.
		if (SimLog::Time == start + 100)
			Mac::Connect((Devices[0]->pMacs[2]), (Devices[1]->pMacs[3]), 5);   // Second link between same Bridges
		// Link 3 comes up with AggPort b00:102 on Aggregator b00:201 and AggPort b01:103 on Aggregator b01:202.
		if (SimLog::Time == start + 200)
			Mac::Connect((Devices[0]->pMacs[3]), (Devices[1]->pMacs[1]), 5);   // Third link between same Bridges
		// Link 4 comes up with AggPort b00:103 on Aggregator b00:201 and AggPort b01:101 on Aggregator b01:201.
		// AggPorts b01:102 and b01:103 have to move to Aggregator b01:201 which causes both ends of LAG to be temporarily
		//    non-operational.  
		//    With small values of aggregateWaitTime there is additional "bouncing" of aggregator operational.

		if (SimLog::Time == start + 300)
		{
			Mac::Disconnect((Devices[0]->pMacs[1]));                           // Take down first link
		}
		// Link 3 goes down and conversations immediately re-allocated to other links.
		// AggPorts b00:102 and b00:103 remain up on Aggregator b00:201
		if (SimLog::Time == start + 400)
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[1]->pMacs[2]), 5);   // Reconnect first link between same Bridges
		// Link 3 comes back up with one or two (depending on coupled/uncoupled MUX) LACPDU exchanges.

		if (SimLog::Time == start + 500)
			Mac::Disconnect((Devices[0]->pMacs[1]));                           // Take down first link
		// Link 2 goes down and conversations immediately re-allocated to other links.
		// AggPorts b00:102 and b00:103 remain up on Aggregator b00:201
		if (SimLog::Time == start + 600)
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[2]->pMacs[0]), 5);   // Connect that Bridge Port to third device
		// Link 2 comes up with AggPort b00:101 on Aggregator b00:201 and AggPort e02:100 on Aggregator e02:200.
		// In the process AggPort b00:101 commandeers Aggregator b00:201 for a new LAG which forces AggPorts b00:102 and b00:103
		//    to a Aggregator b00:202.  Both ends of this LAG will be temporarily non-operational while it changes Aggregators.
		//    With small values of aggregateWaitTime there is additional "bouncing" of aggregator operational.

		if (SimLog::Time == start + 700)
			Mac::Disconnect((Devices[0]->pMacs[1]));                           // Take down link between first Bridge and third device
		// Link 2 goes down and the corresponding Aggregators go down.
		if (SimLog::Time == start + 800)
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[1]->pMacs[2]), 5);   // Reconnect first link between original Bridge Ports
		// Link 2 comes up with AggPort b00:101 on Aggregator b00:201 and AggPort b01:102 on b01:201
		// AggPorts b00:102 and b00:103 move to Aggregator b00:201, so both ends of LAG temporarily non-operational.
		//    With small values of aggregateWaitTime there is additional "bouncing" of aggregator operational.

		if (SimLog::Time == start + 990)
			for (auto& pDev : Devices)
			{
				pDev->disconnect();      // Disconnect all remaining links on all devices
			}


		//  Run all state machines in all devices
		for (auto& pDev : Devices)
		{
			pDev->timerTick();     // Decrement timers
			pDev->run(true);   // Run device with single-step true
		}

		//  Transmit from any MAC with frames to transmit
		for (auto& pDev : Devices)
		{
			pDev->transmit();
		}

		SimLog::Time++;
	}
}

void lagLoopbackTest(std::vector<unique_ptr<Device>>& Devices)
{
	int start = SimLog::Time;

	cout << endl << endl << "   LAG Loopback Tests:  " << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << endl << endl << "   LAG Loopback Tests:  " << endl << endl;

	for (auto& pDev : Devices)
	{
		pDev->reset();   // Reset all devices
	}

	for (int i = 0; i < 1000; i++)
	{
		if (SimLog::Debug > 1)
			SimLog::logFile << "*" << endl;

		//  Make or break connections

		if (SimLog::Time == start + 10)
			Mac::Connect((Devices[0]->pMacs[0]), (Devices[0]->pMacs[0]), 5);   // Port 0:  same-port-loopback
		// Link 1 comes up with AggPort b00:100 on Aggregator b00:200.

		if (SimLog::Time == start + 100)
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[0]->pMacs[3]), 5);   // Port 1 to 3: diff-port-loopback
		// Link 2 comes up with AggPort b00:101 on Aggregator b00:201 and AggPort b00:103 on Aggregator b00:203.

		if (SimLog::Time == start + 200)
			Mac::Connect((Devices[0]->pMacs[5]), (Devices[0]->pMacs[5]), 5);   // Port 5:  same-port-loopback
		// Link 6 comes up with AggPort b00:105 and joins LAG with link 1 on Aggregator b00:200.

		if (SimLog::Time == start + 300)
			Mac::Connect((Devices[0]->pMacs[2]), (Devices[0]->pMacs[4]), 5);   // Port 2 to 4: diff-port-loopback
		// Link 3 comes up with AggPort b00:102 on Aggregator b00:201 and AggPort b00:104 on Aggregator b00:203
		//    and joins LAG with link 2.

		if (SimLog::Time == start + 400)
			Mac::Disconnect((Devices[0]->pMacs[0]));                           // Take down same-port loopback on port 0
		// Link 1 goes down and leaves link 6 on Aggregator b00:200.

		if (SimLog::Time == start + 500)
			Mac::Disconnect((Devices[0]->pMacs[5]));                           // Take down same-port loopback on port 5
		// Link 6 goes down and Aggregator b00:200 goes down.

		if (SimLog::Time == start + 600)
			Mac::Connect((Devices[0]->pMacs[0]), (Devices[0]->pMacs[5]), 5);   // Port 0 to 5: diff-port-loopback
		// Ultimately ends up with links 1, 2, and 3 forming a LAG with 
		//    AggPorts b00:100, b00:101, and b00:102 on Aggregator b00:200, and
		//    AggPorts b00:103, b00:104, and b00:105 on Aggregator b00:201. 
		//    With small values of aggregateWaitTime the links bounce around to get there as each 
		//        AggPort tries to get to its preferred Aggregator.  
		//    The fact that it doesn't end up using Aggregator b00:203 instead of b00:201 actually depends on the  
		//        order in which the links come up.  
		//        This is technically a bug since it violates the determinism of the "preferred" Aggregator concept.
		//        Since it is an anomaly of the loopback special cases it doesn't seem worth trying to fix.

		if (SimLog::Time == start + 700)
			Mac::Disconnect((Devices[0]->pMacs[0]));                           // Take down diff-port loopback between ports 0 and 5
		// Link 1 goes down and and leaves links 1 and 2 on the LAG with Aggregators b00:200 and b00:201.

		if (SimLog::Time == start + 800)
			Mac::Connect((Devices[0]->pMacs[0]), (Devices[0]->pMacs[0]), 5);   // Port 0:  same-port-loopback
		// Link 1 comes up with AggPort b00:100 on Aggregator b00:200.
		// AggPorts b00:101 and b00:102 are forced off Aggregator b00:200, so
		//    (after some bouncing around if aggregateWaitTime is small)
		//    the LAG with links 2 and 3 ends up with 
		//    AggPorts b00:101 and b00:102 on Aggregator b00:201, and
		//    AggPorts b00:103 and b00:104 on Aggregator b00:203. 



		if (SimLog::Time == start + 990)
			for (auto& pDev : Devices)
			{
				pDev->disconnect();      // Disconnect all remaining links on all devices
			}


		//  Run all state machines in all devices
		for (auto& pDev : Devices)
		{
			pDev->timerTick();     // Decrement timers
			pDev->run(true);   // Run device with single-step true
		}

		//  Transmit from any MAC with frames to transmit
		for (auto& pDev : Devices)
		{
			pDev->transmit();
		}

		SimLog::Time++;
	}
}

void nonAggregatablePortTest(std::vector<unique_ptr<Device>>& Devices)
{
	int start = SimLog::Time;
	// aliases
	LinkAgg& dev0Lag = (LinkAgg&)*(Devices[0]->pComponents[1]);
	LinkAgg& dev1Lag = (LinkAgg&)*(Devices[1]->pComponents[1]);

	cout << endl << endl << "   non-Aggregatable (Individual) Port Tests:  " << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << endl << endl << "   non-Aggregatable (Individual) Port Tests:  " << endl << endl;

	for (auto& pDev : Devices)
	{
		pDev->reset();   // Reset all devices
	}

	for (int i = 0; i < 1000; i++)
	{

		if (SimLog::Time == start + 1)   // Clear "aggregation" bit of AggPort adminState variable
		{                        //    in AggPorts 1 and 4 of the first two Bridges
			dev0Lag.pAggPorts[1]->set_aAggPortActorAdminState
				(dev0Lag.pAggPorts[1]->get_aAggPortActorAdminState() & 0xfb);
			dev1Lag.pAggPorts[1]->set_aAggPortActorAdminState
				(dev1Lag.pAggPorts[1]->get_aAggPortActorAdminState() & 0xfb);
			dev0Lag.pAggPorts[4]->set_aAggPortActorAdminState
				(dev0Lag.pAggPorts[4]->get_aAggPortActorAdminState() & 0xfb);
			dev1Lag.pAggPorts[4]->set_aAggPortActorAdminState
				(dev1Lag.pAggPorts[4]->get_aAggPortActorAdminState() & 0xfb);
		}

		//  Make or break connections

		if (SimLog::Time == start + 100)   // Connect three links between two Bridges
		{
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[1]->pMacs[2]), 5);    // {0,1} is individual 
			Mac::Connect((Devices[0]->pMacs[2]), (Devices[1]->pMacs[3]), 5);
			Mac::Connect((Devices[0]->pMacs[3]), (Devices[1]->pMacs[1]), 5);    // {1,1} is individual
		}
		// Each link (2,3,4) comes up as a separate LAG because of the setting of the "aggregation" control bits.

		if (SimLog::Time == start + 200)   // Add a fourth link between the Bridges
			Mac::Connect((Devices[0]->pMacs[4]), (Devices[1]->pMacs[0]), 5);    // {0,4} is individual 
		// Link 5 comes up as yet another separate LAG.

		if (SimLog::Time == start + 300)   // Add a fifth link between the Bridges
			Mac::Connect((Devices[0]->pMacs[5]), (Devices[1]->pMacs[5]), 5);
		// Link 6 joins the LAG with Link 3 (the only other aggregatable link currently active).

		if (SimLog::Time == start + 400)   // Add a sixth link between the Bridges
			Mac::Connect((Devices[0]->pMacs[0]), (Devices[1]->pMacs[4]), 5);    // {1,4} is individual 
		// Link 1 comes up as yet another separate LAG.

		if (SimLog::Time == start + 500)   // Set "aggregation" bit of AggPort adminState variable
		{                        //    in AggPort 1 of the first Bridge
			dev0Lag.pAggPorts[1]->set_aAggPortActorAdminState
				(dev0Lag.pAggPorts[1]->get_aAggPortActorAdminState() | 0x04);
		}
		// Link 2 initially goes down because changing "aggregation" bit equivalent to changing LAGID.
		// Link 2 comes back up to joint the LAG with Links 3 and 6, but in the process the LAG moves
		//    to the "preferred" Aggregator (b00:201) of AggPort b00:101.


		if (SimLog::Time == start + 990)
		{                                   // Restore "aggregation" bit of all AggPort adminState variables
			dev0Lag.pAggPorts[4]->set_aAggPortActorAdminState
				(dev0Lag.pAggPorts[4]->get_aAggPortActorAdminState() | 0x04);
			dev1Lag.pAggPorts[1]->set_aAggPortActorAdminState
				(dev1Lag.pAggPorts[1]->get_aAggPortActorAdminState() | 0x04);
			dev1Lag.pAggPorts[4]->set_aAggPortActorAdminState
				(dev1Lag.pAggPorts[4]->get_aAggPortActorAdminState() | 0x04);

			for (auto& pDev : Devices)
			{
				pDev->disconnect();      // Disconnect all remaining links on all devices
			}
		}


		//  Run all state machines in all devices
		for (auto& pDev : Devices)
		{
			pDev->timerTick();     // Decrement timers
			pDev->run(true);   // Run device with single-step true
		}

		//  Transmit from any MAC with frames to transmit
		for (auto& pDev : Devices)
		{
			pDev->transmit();
		}

		if (SimLog::Debug > 1)
			SimLog::logFile << "*" << endl;
		SimLog::Time++;
	}
}

void limitedAggregatorsTest(std::vector<unique_ptr<Device>>& Devices)
{
	int start = SimLog::Time;
	// aliases
	LinkAgg& dev0Lag = (LinkAgg&)*(Devices[0]->pComponents[1]);

	cout << endl << endl << "   Limited Aggregator (fewer Aggregators than AggPorts) Tests:  " << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << endl << endl << "   Limited Aggregator (fewer Aggregators than AggPorts) Tests:  " << endl << endl;

	for (auto& pDev : Devices)
	{
		pDev->reset();   // Reset all devices
	}

	for (int i = 0; i < 1000; i++)
	{


		if (SimLog::Time == start + 1)   // Change key for 3 AggPorts (1, 3, 5) but just two Aggregators (1, 4).
		{           //    Note Aggregator 4 is not the "preferred" Aggregator for any of the AggPorts.
			dev0Lag.pAggPorts[1]->set_aAggPortActorAdminKey(0x999);
			dev0Lag.pAggPorts[3]->set_aAggPortActorAdminKey(0x999);
			dev0Lag.pAggPorts[5]->set_aAggPortActorAdminKey(0x999);
			dev0Lag.pAggregators[1]->set_aAggActorAdminKey(0x999);
			dev0Lag.pAggregators[4]->set_aAggActorAdminKey(0x999);
		}

		//  Make or break connections

		if (SimLog::Time == start + 10)   // Create the first link between two Bridges
			Mac::Connect((Devices[0]->pMacs[0]), (Devices[1]->pMacs[0]), 5);
		// Link 1 comes up with AggPort b00:100 on Aggregator b00:200 and AggPort b01:100 on Aggregator b01:200.

		if (SimLog::Time == start + 100)   // Create another link between the Bridges
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[1]->pMacs[1]), 5);
		// Link 2 comes up with AggPort b00:101 on Aggregator b00:201 and AggPort b01:101 on Aggregator b01:201.
		// Link 2 does not form a LAG with Link 1 because the AggPorts (and therefore selected Aggregators)
		//     have different keys.

		if (SimLog::Time == start + 200)   // Create another link between the Bridges
			Mac::Connect((Devices[0]->pMacs[3]), (Devices[1]->pMacs[3]), 5);
		// Link 4 comes up with AggPort b00:103 on Aggregator b00:201 and AggPort b01:103 on Aggregator b01:201.
		// Link 4 joins the LAG with Link 2.

		if (SimLog::Time == start + 300)   // Create another link between the Bridges
			Mac::Connect((Devices[0]->pMacs[5]), (Devices[1]->pMacs[5]), 5);
		// Link 6 comes up with AggPort b00:105 on Aggregator b00:201 and AggPort b01:105 on Aggregator b01:201.
		// Link 6 joins the LAG with Links 2 and 4.

		if (SimLog::Time == start + 400)
			Mac::Disconnect((Devices[0]->pMacs[3]));
		// Link 4 leaves the LAG so just Links 2 and 6 remain.

		if (SimLog::Time == start + 500)
			Mac::Disconnect((Devices[0]->pMacs[5]));
		// Link 6 leaves the LAG so just Link 2 remains.

		if (SimLog::Time == start + 600)   // Create a first link to a new Bridge
			Mac::Connect((Devices[0]->pMacs[3]), (Devices[2]->pMacs[3]), 5);
		// Link 4 comes up with AggPort b00:103 on Aggregator b00:204 and AggPort b02:103 on Aggregator b02:203.
		// Note that AggPort b00:103 has a different key than its "preferred" Aggregator and therefore forms LAG
		//    using Aggregator b00:204.

		if (SimLog::Time == start + 700)   // Create another link to the new Bridge
			Mac::Connect((Devices[0]->pMacs[5]), (Devices[2]->pMacs[5]), 5);
		// Link 6 comes up with AggPort b00:105 on Aggregator b00:204 and AggPort b02:105 on Aggregator b02:203.
		// Link 6 joins the LAG with Link 4.

		if (SimLog::Time == start + 800)   // Create another link to the new Bridge
			Mac::Connect((Devices[0]->pMacs[4]), (Devices[2]->pMacs[4]), 5);
		// Link 5 comes up with AggPort b00:104 on Aggregator b00:202 and AggPort b02:104 on Aggregator b02:204.
		//    Forms a new LAG because AggPort b00:104 has a different key.  Its key also differs from its "preferred"
		//    Aggregator so it takes over a currently unused Aggregator (in this case b00:202).

		if (SimLog::Time == start + 900)   // Create another link to the new Bridge
			Mac::Connect((Devices[0]->pMacs[2]), (Devices[2]->pMacs[2]), 5);
		// Link 3 comes up with AggPort b00:102 on Aggregator b00:202 and AggPort b02:102 on Aggregator b02:202.
		// Link 5 goes down on Aggregator b00:204 and joins LAG with Link 3 on Aggregator b00:202 (the "preferred"
		//    Aggregator of the lowest AggPort in the LAG).


		if (SimLog::Time == start + 990)
		{                                   // Restore key values
			dev0Lag.pAggPorts[1]->set_aAggPortActorAdminKey(defaultActorKey);
			dev0Lag.pAggPorts[3]->set_aAggPortActorAdminKey(defaultActorKey);
			dev0Lag.pAggPorts[5]->set_aAggPortActorAdminKey(defaultActorKey);
			dev0Lag.pAggregators[1]->set_aAggActorAdminKey(defaultActorKey);
			dev0Lag.pAggregators[4]->set_aAggActorAdminKey(defaultActorKey);

			for (auto& pDev : Devices)
			{
				pDev->disconnect();      // Disconnect all remaining links on all devices
			}
		}

		//  Run all state machines in all devices
		for (auto& pDev : Devices)
		{
			pDev->timerTick();     // Decrement timers
			pDev->run(true);   // Run device with single-step true
		}

		//  Transmit from any MAC with frames to transmit
		for (auto& pDev : Devices)
		{
			pDev->transmit();
		}

		if (SimLog::Debug > 1)
			SimLog::logFile << "*" << endl;
		SimLog::Time++;
	}
}

/**/
void dualHomingTest(std::vector<unique_ptr<Device>>& Devices)
{
	int start = SimLog::Time;
	// aliases
	LinkAgg& dev0Lag = (LinkAgg&)*(Devices[0]->pComponents[1]);

	cout << endl << endl << "   Dual-Homing Tests:  " << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << endl << endl << "   Dual-Homing Tests:  " << endl << endl;

	for (auto& pDev : Devices)
	{
		pDev->reset();   // Reset all devices
	}

	for (int i = 0; i < 1000; i++)
	{

		//  Make or break connections

		if (SimLog::Time == start + 10)
		{   // Create two links between Bridges 0 and 1, and one link between Bridges 0 and 2.
			Mac::Connect((Devices[0]->pMacs[0]), (Devices[1]->pMacs[0]), 5);
			Mac::Connect((Devices[0]->pMacs[2]), (Devices[2]->pMacs[2]), 5);
			Mac::Connect((Devices[0]->pMacs[3]), (Devices[1]->pMacs[3]), 5);
		}
		// Links 1 and 4 come up in a LAG on Aggregators b00:200 and b01:200.
		// Link 3 comes up in a LAG on Aggregators b00:202 and b02:202.

		if (SimLog::Time == start + 100)
		{   // Set key of all Aggregators in Bridge 0 except the first Aggregator to a value
			//    not share with any of the AggPorts.  Therefore Bridge 0 can only form a single LAG.
			for (auto pAgg : dev0Lag.pAggregators)
			{
				pAgg->set_aAggActorAdminKey(unusedAggregatorKey);
			}
			dev0Lag.pAggregators[0]->set_aAggActorAdminKey(defaultActorKey);
		}
		// Link 3 goes down because it has no available Aggregators in Bridge 0.

		if (SimLog::Time == start + 200)
			Mac::Disconnect((Devices[0]->pMacs[0]));
		// Link 1 goes down leaving just Link 4 in LAG with Bridge 1.

		if (SimLog::Time == start + 300)
			Mac::Disconnect((Devices[0]->pMacs[3]));
		// Link 4 goes down allowing Link 3 to take over the Aggregator and come up with Bridge 2.

		if (SimLog::Time == start + 400)
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[2]->pMacs[1]), 5);
		// Link 2 joins Link 3 in LAG with Bridge 2.
		//    In the process the LAG moves to Aggregator b02:201 in Bridge 2.

		if (SimLog::Time == start + 500)
			Mac::Connect((Devices[0]->pMacs[3]), (Devices[1]->pMacs[3]), 5);
		// Reconnecting Link 4.  Nothing happens because AggPort b00:103 has no higher priority
		//    to Aggregator b00:200 than Links 2 and 3.

		if (SimLog::Time == start + 500)
			Mac::Connect((Devices[0]->pMacs[0]), (Devices[1]->pMacs[0]), 5);
		// Reconnecting Link 1.  Now the LAG to Bridge 2 (Links 2 and 3) go down and the LAG to
		//    Bridge 1 (Links 1 and 4) take over because Aggregator b00:200 is the "preferred" 
		//    Aggregator for AggPort b00:100.


		if (SimLog::Time == start + 990)
		{   // Restore key for all Aggregators in Bridge 0 to their default value
			for (auto pAgg : dev0Lag.pAggregators)
			{
				pAgg->set_aAggActorAdminKey(defaultActorKey);
			}

			for (auto& pDev : Devices)
			{
				pDev->disconnect();      // Disconnect all remaining links on all devices
			}
		}

		//  Run all state machines in all devices
		for (auto& pDev : Devices)
		{
			pDev->timerTick();     // Decrement timers
			pDev->run(true);       // Run device with single-step true
		}

		//  Transmit from any MAC with frames to transmit
		for (auto& pDev : Devices)
		{
			pDev->transmit();
		}

		if (SimLog::Debug > 1)
			SimLog::logFile << "*" << endl;
		SimLog::Time++;
	}
}

/**/
void axbkHierarchicalLagTest(std::vector<unique_ptr<Device>>& Devices)
{
	/**/
	int start = SimLog::Time;

	cout << endl << endl << "   802.1AXbk Hierarchical LAG Tests:  " << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << endl << endl << "   802.1AXbk Hierarchical LAG Tests:  " << endl << endl;

	// This test uses two Bridges (Devices 0 and 1) and two End Stations (Devices 3 and 4).
	// Need at least four MACs on each End Station.
	// The Bridges should be Provider Bridges (so they do not filter Nearest Customer Bridge DA), and the
	//    End Stations should be connected using strictly point to point S-VLANs (so the S-VLANs can be aggregated).
	//    Currently the bridge models are so rudimentary that using the default bridges will work.

	// Instantiate the "outer Link Aggregation shim" in the End Stations
	for (int sx = 3; sx < 5; sx++)
	{
		unique_ptr<LinkAgg> pOuterLag = make_unique<LinkAgg>();
		for (unsigned short px = 0; px < Devices[sx]->pMacs.size(); px++)    // For each Mac:
		{
			// aliases
			EndStn& station = (EndStn&)*(Devices[sx]->pComponents[0]);
			LinkAgg& innerLag = (LinkAgg&)*(Devices[sx]->pComponents[1]);

			unsigned short sysNum = 0;       // This is a single system device
			unsigned char lacpVersion = 2;  // Outer LinkAgg shim will be version 2
			shared_ptr<AggPort> pAggPort = make_shared<AggPort>(lacpVersion, sysNum, 0x200 + px);    // Create an AggPort/Aggregator pair
			pAggPort->set_aAggPortProtocolDA(NearestCustomerBridgeDA);                       // Outer LinkAgg AggPort uses Nearest Customer Bridge DA for LACPDUs
			pAggPort->setActorSystem(station.SystemId);                    // Assign Aggregation Port/Aggregator to this End Station
			pAggPort->pIss = innerLag.pAggregators[px];                            // Attach the inner LinkAgg Aggregator to this outer AggPort
			pOuterLag->pAggregators.push_back(pAggPort);                        // Put Aggregator in the Device's outer LinkAgg shim
			pOuterLag->pAggPorts.push_back(pAggPort);                           // Put Aggregation Port in the Device's outer LinkAgg shim
			if (px == 0)
			{
				station.pIss = pOuterLag->pAggregators[px];   // Attach the first outer Aggregator to the End Station
			}
			else
			{
				pAggPort->set_aAggActorAdminKey(unusedAggregatorKey);          // Set Admin key of other outer Aggregators to value not shared with any AggPort
				innerLag.pAggPorts[px]->set_aAggActorAdminKey(defaultActorKey);    // Set Admin key of other inner Aggregators to default value
			}
		}
		Devices[sx]->pComponents.push_back(move(pOuterLag));                // Put outer Link Agg shim in Device's Components vector

	}

	for (auto& pDev : Devices)
	{
		pDev->reset();   // Reset all devices
	}

	for (int i = 0; i < 1000; i++)
	{

		//  Make or break connections

		if (SimLog::Time == start + 10)
		{   // Connect first End Station to each Bridge.
			Mac::Connect((Devices[3]->pMacs[0]), (Devices[0]->pMacs[0]), 5);
			Mac::Connect((Devices[3]->pMacs[2]), (Devices[1]->pMacs[0]), 5);
		}

		if (SimLog::Time == start + 150)
		{   // Connect a second link from first End Station to each Bridge.
			Mac::Connect((Devices[3]->pMacs[1]), (Devices[0]->pMacs[1]), 5);
			Mac::Connect((Devices[3]->pMacs[3]), (Devices[1]->pMacs[1]), 5);
		}

		if (SimLog::Time == start + 200)  // Connect second End Station to first Bridge
			Mac::Connect((Devices[4]->pMacs[0]), (Devices[0]->pMacs[2]), 5);

		if (SimLog::Time == start + 300)  // Connect second link from second End Station to first Bridge
			Mac::Connect((Devices[4]->pMacs[2]), (Devices[0]->pMacs[3]), 5);

		if (SimLog::Time == start + 400)  // Connect second End Station to second Bridge
			Mac::Connect((Devices[4]->pMacs[1]), (Devices[1]->pMacs[2]), 5);

		if (SimLog::Time == start + 500)  // Connect second link from first End Station to second Bridge
			Mac::Connect((Devices[4]->pMacs[3]), (Devices[1]->pMacs[3]), 5);

		if (SimLog::Time == start + 600)  // Disconnect first link on first End Station
			Mac::Disconnect((Devices[3]->pMacs[0]));

		if (SimLog::Time == start + 700)  // Disconnect second link on first End Station
			Mac::Disconnect((Devices[3]->pMacs[1]));

		if (SimLog::Time == start + 990)
		{   // Restore key for all Aggregators in Bridge 0 to their default value
			for (auto pAgg : ((LinkAgg&)*(Devices[0]->pComponents[1])).pAggregators)
			{
				pAgg->set_aAggActorAdminKey(defaultActorKey);
			}

			for (auto& pDev : Devices)
			{
				pDev->disconnect();      // Disconnect all remaining links on all devices
			}
		}

		//  Run all state machines in all devices
		for (auto& pDev : Devices)
		{
			pDev->timerTick();     // Decrement timers
			pDev->run(true);   // Run device with single-step true
		}

		//  Transmit from any MAC with frames to transmit in all devices
		for (auto& pDev : Devices)
		{
			pDev->transmit();
		}


		if (SimLog::Debug > 1)
			SimLog::logFile << "*" << endl;
		SimLog::Time++;
	}

	// Destroy the "outer Link Aggregation shim" in the End Stations, and restore connectivity and key values of inner LinkAgg shim
	for (int sx = 3; sx < 5; sx++)
	{
		for (unsigned short px = 0; px < Devices[sx]->pMacs.size(); px++)    // For each Mac:
		{
			// aliases
			EndStn& station = (EndStn&)*(Devices[sx]->pComponents[0]);
			LinkAgg& innerLag = (LinkAgg&)*(Devices[sx]->pComponents[1]);

			if (px == 0)
			{
				station.pIss = innerLag.pAggregators[px];   // Attach the first inner Aggregator to the End Station
			}
			else
			{
				innerLag.pAggPorts[px]->set_aAggActorAdminKey(unusedAggregatorKey);    // Set Admin key of other inner Aggregators to value not shared with any AggPort
			}
		}
		Devices[sx]->pComponents.pop_back();   // remove outerLag from Device's Components vector and let it disappear
	}
	/**/
}

/**/
void distributionTest(std::vector<unique_ptr<Device>>& Devices)
{
	int start = SimLog::Time;
	// aliases
	EndStn& dev3EndStn = (EndStn&)*(Devices[3]->pComponents[0]);
	LinkAgg& dev0LinkAgg = (LinkAgg&)*(Devices[0]->pComponents[1]);
	LinkAgg& dev1LinkAgg = (LinkAgg&)*(Devices[1]->pComponents[1]);
	LinkAgg& dev2LinkAgg = (LinkAgg&)*(Devices[2]->pComponents[1]);

	cout << endl << endl << "   Distribution Tests:  " << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << endl << endl << "   Distribution Tests:  " << endl << endl;

	for (auto& pDev : Devices)
	{
		pDev->reset();   // Reset all devices
	}

	/*
	// To test alternative Conversation ID to Link Number algorithms ...
	// Set link distribution to even-odd in all Bridge 0 Aggregators
	for (auto& pAgg : dev0LinkAgg.pAggregators)
	{
	pAgg->set_convLinkMap(Aggregator::convLinkMaps::EVEN_ODD);
	}
	// Set link distribution to active-standby in all Bridge 2 Aggregators
	for (auto& pAgg : dev2LinkAgg.pAggregators)
	{
	pAgg->set_convLinkMap(Aggregator::convLinkMaps::ACTIVE_STANDBY);
	}
	/**/

	for (int i = 0; i < 1000; i++)
	{

		//  Make or break connections

		if (SimLog::Time == start + 10)
		{   // Create three links between Bridges 0 and 1, and three links between Bridges 0 and 2.
			Mac::Connect((Devices[0]->pMacs[0]), (Devices[1]->pMacs[0]), 5);
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[1]->pMacs[1]), 5);
			Mac::Connect((Devices[0]->pMacs[2]), (Devices[1]->pMacs[2]), 5);  // Bridges 0:1 get Links 1, 2, 3
			Mac::Connect((Devices[0]->pMacs[3]), (Devices[2]->pMacs[3]), 5);
			Mac::Connect((Devices[0]->pMacs[4]), (Devices[2]->pMacs[4]), 5);
			Mac::Connect((Devices[0]->pMacs[5]), (Devices[2]->pMacs[5]), 5);  // Bridges 0:2 get Links 4, 5, 6
		}

		if (SimLog::Time == start + 100)
		{   // Connect one End Station to each Bridge with a pair of links
			Mac::Connect((Devices[0]->pMacs[6]), (Devices[3]->pMacs[0]), 5);
			Mac::Connect((Devices[0]->pMacs[7]), (Devices[3]->pMacs[1]), 5);  // Bridge 0 EndStn 3 get Links 7, 8
			Mac::Connect((Devices[1]->pMacs[4]), (Devices[4]->pMacs[2]), 5);
			Mac::Connect((Devices[1]->pMacs[5]), (Devices[4]->pMacs[3]), 5);  // Bridge 1 EndStn 4 get Links 5, 6
			Mac::Connect((Devices[2]->pMacs[0]), (Devices[5]->pMacs[0]), 5);
			Mac::Connect((Devices[2]->pMacs[1]), (Devices[5]->pMacs[1]), 5);  // Bridge 2 EndStn 5 get Links 1, 2
		}

		if (SimLog::Time == start + 200)
		{
			printLinkMap(Devices);
			send9Frames(dev3EndStn);
			// Mac address hash of EndStn 3 test frame results in a Conversation ID of 0x066b.
			// With default table this ConvID maps to:
			//    EndStn 3 to Bridge 0 Link 8
			//    Bridge 0 to Bridge 1 Link 3
			//    Bridge 1 to EndStn 4 Link 6
			//    Bridge 0 to Bridge 2 Link 4
			//    Bridge 2 to EndStn 5 Link 1
		}

		if (SimLog::Time == start + 300)
		{   // Move one of the links between Bridges 0 and 2 to between Bridges 0 and 1
			Mac::Connect((Devices[0]->pMacs[3]), (Devices[1]->pMacs[3]), 5);  // Bridges 0:1 get Links 1, 2, 3. 4
			// Bridges 0:2 get Links 5, 6
			// Disconnect one of the links at the source End Station
			Mac::Disconnect(Devices[3]->pMacs[0]);                            // Bridge 0 EndStn 3 get Links 8
		}

		if (SimLog::Time == start + 400)
		{
			printLinkMap(Devices);
			send9Frames(dev3EndStn);
			// Mac address hash of EndStn 3 test frame results in a Conversation ID of 0x066b.
			// With default table this ConvID maps to:
			//    EndStn 3 to Bridge 0 Link 8
			//    Bridge 0 to Bridge 1 Link 3
			//    Bridge 1 to EndStn 4 Link 6
			//    Bridge 0 to Bridge 2 Link 6
			//    Bridge 2 to EndStn 5 Link 1
		}

		if (SimLog::Time == start + 500)
		{
			// Test Link Numbers > 7 with the "EIGHT_LINK_SPREAD" convLinkMap
			dev0LinkAgg.pAggPorts[0]->set_aAggPortLinkNumberID(17);
			dev0LinkAgg.pAggPorts[1]->set_aAggPortLinkNumberID(25);

			// Set PortAlgorithm to C_VID in all Bridge 0 Aggregators
			for (auto& pAgg : dev0LinkAgg.pAggregators)
			{
				pAgg->set_aAggPortAlgorithm(Aggregator::portAlgorithms::C_VID);
			}
			// Set PortAlgorithm to C_VID in all Bridge 2 Aggregators
			for (auto& pAgg : dev2LinkAgg.pAggregators)
			{
				pAgg->set_aAggPortAlgorithm(Aggregator::portAlgorithms::C_VID);
			}
			// Now the LAG between Bridges 0 and 2 (Aggregators b00:203 and b02:203) should have DWC true and differPortAlg false 
		}

		if (SimLog::Time == start + 600)
		{
			printLinkMap(Devices);
			send9Frames(dev3EndStn);
			// Now the nine frames should be transmitted on links:
			//    EndStn 3 to Bridge 0 Link 8, 8, 8, 8, 8, 8, 8, 8, 8
			//    Bridge 0 to Bridge 1 Link 3, 3, 17, 3, 3, 4, 3, 17, 17
			//    Bridge 1 to EndStn 4 Link 6, 6, 6, 6, 6, 6, 6, 6, 6
			//    Bridge 0 to Bridge 2 Link 6, 6, 6, 5, 6, 5, 5, 6, 5
			//    Bridge 2 to EndStn 5 Link 1, 1, 1, 2, 1, 2, 2, 1, 2
		}

		if (SimLog::Time == start + 700)
		{
			std::array<unsigned char, 16> AdminTableDigest = { "ADMIN_TABLE    " };
			std::list<unsigned short> portList;

			// Set link distribution to admin-table in Bridge 0 Aggregator
			shared_ptr<Aggregator> pAgg = dev2LinkAgg.pAggregators[0];
			{
				//  set port list for first eight Conversation IDs
				pAgg->set_aAggConversationAdminLink(0, portList = { 3, 2, 1 });
				pAgg->set_aAggConversationAdminLink(1, portList = { 2, 1, 0 });
				pAgg->set_aAggConversationAdminLink(2, portList = { 2, 0 });
				pAgg->set_aAggConversationAdminLink(3, portList = { 2 });
				pAgg->set_aAggConversationAdminLink(4, portList = { 0 });
				pAgg->set_aAggConversationAdminLink(5, portList = { 1 });
				pAgg->set_aAggConversationAdminLink(6, portList = { 1, 0 });
				pAgg->set_aAggConversationAdminLink(7, portList = { 3, 1, 2 });

				//   set the digest for admin-table (should be calculated MD5 of the ConversationAdminLink map)
				pAgg->set_aAggConversationListDigest(AdminTableDigest);
				//   set the admin-table as the selected convLinkMap
				pAgg->set_convLinkMap(Aggregator::convLinkMaps::ADMIN_TABLE);
			}

		}

		if (SimLog::Time == start + 800)
		{
			printLinkMap(Devices);
			send9Frames(dev3EndStn);
			// Now the nine frames should be transmitted on links:
			//    EndStn 3 to Bridge 0 Link 8, 8, 8, 8, 8, 8, 8, 8, 8
			//    Bridge 0 to Bridge 1 Link 3, 3, 17, 3, 3, 4, 3, 17, 17
			//    Bridge 1 to EndStn 4 Link 6, 6, 6, 6, 6, 6, 6, 6, 6
			//    Bridge 0 to Bridge 2 Link 6, 6, 6, 5, 6, 5, 5, 6, 5
			//    Bridge 2 to EndStn 5 Link 2, 2, 2, 2, 2, 0, 1, 1, 1
		}

		if (SimLog::Time == start + 990)
		{
			for (auto& pDev : Devices)
			{
				pDev->disconnect();      // Disconnect all remaining links on all devices
			}
			// TODO:  Need to reset all administrative values to their defaults
		}

		//  Run all state machines in all devices
		for (auto& pDev : Devices)
		{
			pDev->timerTick();     // Decrement timers
			pDev->run(true);       // Run device with single-step true
		}

		//  Transmit from any MAC with frames to transmit
		for (auto& pDev : Devices)
		{
			pDev->transmit();
		}

		if (SimLog::Debug > 1)
			SimLog::logFile << "*" << endl;
		SimLog::Time++;
	}
}

/**/
void waitToRestoreTest(std::vector<unique_ptr<Device>>& Devices)
{
	int start = SimLog::Time;
	// aliases
	LinkAgg& dev0Lag = (LinkAgg&)*(Devices[0]->pComponents[1]);

	cout << endl << endl << "   Wait-To-Restore Timer Tests:  " << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << endl << endl << "   Wait-To-Restore Timer Tests:  " << endl << endl;

	for (auto& pDev : Devices)
	{
		pDev->reset();   // Reset all devices
	}

	for (int i = 0; i < 1000; i++)
	{
		if (SimLog::Time == start + 1)
		{   // Set waitToRestoreTime of all AggPorts in Bridge 0 to 30
			for (auto pPort : dev0Lag.pAggPorts)
			{
				pPort->set_aAggPortWTRTime(30);
			}
			// Set AggPorts 6 and 7 of Bridge 0 for dual-homing
			//     (Change Aggregator 6 and AggPorts 6 and 7 to a new key,
			//      and disable Aggregator 7)
			dev0Lag.pAggregators[6]->set_aAggActorAdminKey(defaultActorKey + 0x100);
			dev0Lag.pAggPorts[6]->set_aAggPortActorAdminKey(defaultActorKey + 0x100);
			dev0Lag.pAggPorts[7]->set_aAggPortActorAdminKey(defaultActorKey + 0x100);
			dev0Lag.pAggregators[7]->setEnabled(false);
		}

		//  Make or break connections

		if (SimLog::Time == start + 10)
		{   // Create three links between Bridge 0 and End Station 3.
			Mac::Connect((Devices[0]->pMacs[0]), (Devices[3]->pMacs[0]), 5);
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[3]->pMacs[1]), 5);
			Mac::Connect((Devices[0]->pMacs[2]), (Devices[3]->pMacs[2]), 5);
		}
		// Links 1, 2 and 3 come up in a LAG on Aggregators b00:200 and e03:200.

		if (SimLog::Time == start + 10)
		{   // Dual home Bridges 0 to Bridges 1 and 2.
			Mac::Connect((Devices[0]->pMacs[6]), (Devices[1]->pMacs[6]), 5);
			Mac::Connect((Devices[0]->pMacs[7]), (Devices[2]->pMacs[7]), 5);
		}
		// Link 7 comes up in a LAG on Aggregators b00:206 and b01:206.
		// Link 8 has no available Aggregators.

		if (SimLog::Time == start + 100)
		{
			Mac::Disconnect((Devices[0]->pMacs[1]));
			Mac::Disconnect((Devices[0]->pMacs[2]));
		}
		// Links 2 and 3 go down leaving just Link 1 in LAG between Bridge 0 and End Station 3.

		if (SimLog::Time == start + 115)
		{
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[3]->pMacs[1]), 5);
			Mac::Connect((Devices[0]->pMacs[2]), (Devices[3]->pMacs[2]), 5);
		}
		// Reconnect Links 2 and 3, starting WTR timers.

		if (SimLog::Time == start + 120)
		{
			Mac::Disconnect((Devices[0]->pMacs[2]));
		}
		// Link 3 goes down again.

		if (SimLog::Time == start + 125)
		{
			Mac::Connect((Devices[0]->pMacs[2]), (Devices[3]->pMacs[2]), 5);
		}
		// Reconnect Link 3, re-starting WTR timer.

		// Link 2 should re-join LAG at around time 155 (time 115 plus WTR plus a LACPDU round trip time)
		// Link 3 should re-join LAG at around time 165

		if (SimLog::Time == start + 200)
		{
			Mac::Disconnect((Devices[0]->pMacs[1]));
			Mac::Disconnect((Devices[0]->pMacs[2]));
		}
		// Links 2 and 3 go down leaving just Link 1 in LAG between Bridge 0 and End Station 3.

		if (SimLog::Time == start + 215)
		{
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[3]->pMacs[1]), 5);
			Mac::Connect((Devices[0]->pMacs[2]), (Devices[3]->pMacs[2]), 5);
		}
		// Reconnect Links 2 and 3, starting WTR timers.
		// Links 2 and 3 rejoin about time 255

		if (SimLog::Time == start + 230)
		{
			Mac::Disconnect((Devices[0]->pMacs[0]));
		}
		// Disconnect Link 1.

		if (SimLog::Time == start + 250)
		{
			Mac::Connect((Devices[0]->pMacs[0]), (Devices[3]->pMacs[0]), 5);
		}
		// Reconnect Link 1, starting WTR timer.
		// Link 1 rejoins about time 290


		if (SimLog::Time == start + 300)
			Mac::Disconnect((Devices[0]->pMacs[6]));
		// Link 7 goes down allowing Link 8 to take over the Aggregator and come up with Bridge 2.

		if (SimLog::Time == start + 350)
			Mac::Connect((Devices[0]->pMacs[6]), (Devices[1]->pMacs[6]), 5);
		// Reconnect Link 7, taking over LAG when WTR timer expires.

		if (SimLog::Time == start + 400)
			Mac::Disconnect((Devices[0]->pMacs[7]));
		// Link 8 goes down, to no effect.

		if (SimLog::Time == start + 450)
			Mac::Connect((Devices[0]->pMacs[7]), (Devices[2]->pMacs[7]), 5);
		// Reconnect Link 8, still no effect.

		// So have links 1, 2, and 3 between bridge 0 and end station 3,
		//   and link 7 between bridge 0 and 1, with link 8 having no available aggregators on bridge 0.

		if (SimLog::Time == start + 500)
		{	// Set waitToRestoreTime of all AggPorts in Bridge 0 to 30 with non-revertive mode
			for (auto pPort : dev0Lag.pAggPorts)
			{
				pPort->set_aAggPortWTRTime(30 | 0x8000);
			}
			Mac::Disconnect((Devices[0]->pMacs[1]));
			Mac::Disconnect((Devices[0]->pMacs[2]));
		}
		// Links 2 and 3 go down leaving just Link 1 in LAG between Bridge 0 and End Station 3.
		// AggPorts b00:101 and b00:102 set non-revertive

		if (SimLog::Time == start + 515)
		{
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[3]->pMacs[1]), 5);
			Mac::Connect((Devices[0]->pMacs[2]), (Devices[3]->pMacs[2]), 5);
		}
		// Reconnect Links 2 and 3, starting WTR timers.

		if (SimLog::Time == start + 520)
		{
			Mac::Disconnect((Devices[0]->pMacs[2]));
		}
		// Link 3 goes down again.

		if (SimLog::Time == start + 525)
		{
			Mac::Connect((Devices[0]->pMacs[2]), (Devices[3]->pMacs[2]), 5);
		}
		// Reconnect Link 3, re-starting WTR timer.

		// Links 2 and 3 do not re-join because non-revertive

		if (SimLog::Time == start + 600)
		{
			Mac::Disconnect((Devices[0]->pMacs[1]));
			Mac::Disconnect((Devices[0]->pMacs[2]));
		}
		// Links 2 and 3 go down so still have just Link 1 in LAG between Bridge 0 and End Station 3.

		if (SimLog::Time == start + 615)
		{
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[3]->pMacs[1]), 5);
			Mac::Connect((Devices[0]->pMacs[2]), (Devices[3]->pMacs[2]), 5);
		}
		// Reconnect Links 2 and 3, starting WTR timers.

		if (SimLog::Time == start + 630)
		{
			Mac::Disconnect((Devices[0]->pMacs[0]));
		}
		// Disconnect Link 1, setting non-revertive.
		// Now all links are non-revertive so all get set to revertive, but link 1 set non-revertive
		//    again because it is still down.
		// Links 2 and 3 come up around time 655.

		if (SimLog::Time == start + 650)
		{
			Mac::Connect((Devices[0]->pMacs[0]), (Devices[3]->pMacs[0]), 5);
		}
		// Reconnect Link 1, starting WTR timer.
		// Link 1 still non-revertive, so does not become active (i.e. not sync, collecting, or distributing).


		if (SimLog::Time == start + 700)
			Mac::Disconnect((Devices[0]->pMacs[6]));
		// Link 7 goes down allowing Link 8 to take over the Aggregator and come up with Bridge 2.

		if (SimLog::Time == start + 750)
			Mac::Connect((Devices[0]->pMacs[6]), (Devices[1]->pMacs[6]), 5);
		// Reconnect Link 7, to no effect because AggPort b00:106 is non-revertive.

		if (SimLog::Time == start + 800)
			Mac::Disconnect((Devices[0]->pMacs[7]));
		// Link 8 goes down, causing both AggPorts b00:106 and b00:107 to be set revertive, and Link 7 comes up.

		if (SimLog::Time == start + 850)
			Mac::Connect((Devices[0]->pMacs[7]), (Devices[2]->pMacs[7]), 5);
		// Reconnect Link 8, no effect.


		if (SimLog::Time == start + 990)
		{   // Restore all default values
			for (auto pAgg : dev0Lag.pAggregators)
			{
				pAgg->set_aAggActorAdminKey(defaultActorKey);
				pAgg->setEnabled(true);
			}
			for (auto pPort : dev0Lag.pAggPorts)
			{
				pPort->set_aAggPortWTRTime(0);
			}
			for (auto& pDev : Devices)
			{
				pDev->disconnect();      // Disconnect all remaining links on all devices
			}
		}

		//  Run all state machines in all devices
		for (auto& pDev : Devices)
		{
			pDev->timerTick();     // Decrement timers
			pDev->run(true);       // Run device with single-step true
		}

		//  Transmit from any MAC with frames to transmit
		for (auto& pDev : Devices)
		{
			pDev->transmit();
		}

		if (SimLog::Debug > 1)
			SimLog::logFile << "*" << endl;
		SimLog::Time++;
	}

}

/**/
void adminVariableTest(std::vector<unique_ptr<Device>>& Devices)
{
	int start = SimLog::Time;
	// aliases
	EndStn& dev3EndStn = (EndStn&)*(Devices[3]->pComponents[0]);
	LinkAgg& dev0LinkAgg = (LinkAgg&)*(Devices[0]->pComponents[1]);
	LinkAgg& dev1LinkAgg = (LinkAgg&)*(Devices[1]->pComponents[1]);
	LinkAgg& dev2LinkAgg = (LinkAgg&)*(Devices[2]->pComponents[1]);

	unsigned short savedKey = dev0LinkAgg.pAggPorts[1]->get_aAggActorAdminKey();

	cout << endl << endl << "   Writing Administrative Variables Tests:  " << endl << endl;
	if (SimLog::Debug > 0)
		SimLog::logFile << endl << endl << "   Writing Administrative Variables Tests:  " << endl << endl;

	for (auto& pDev : Devices)
	{
		pDev->reset();   // Reset all devices
	}


	for (int i = 0; i < 1000; i++)
	{

		//  Make or break connections

		if (SimLog::Time == start + 10)
		{   
			// Set PortAlgorithm to C_VID in all Bridge 1 Aggregators
			for (auto& pAgg : dev1LinkAgg.pAggregators)
			{
				pAgg->set_aAggPortAlgorithm(Aggregator::portAlgorithms::C_VID);
			}
		}

		if (SimLog::Time == start + 40)
		{   // Create three links between Bridges 0 and 1.
			Mac::Connect((Devices[0]->pMacs[1]), (Devices[1]->pMacs[2]), 5);
			Mac::Connect((Devices[0]->pMacs[2]), (Devices[1]->pMacs[3]), 5);
			Mac::Connect((Devices[0]->pMacs[3]), (Devices[1]->pMacs[1]), 5);
		}

		if (SimLog::Time == start + 100)
		{
			dev0LinkAgg.pAggPorts[1]->set_aAggPortActorAdminKey(0x0246);       // change port key
		}


		if (SimLog::Time == start + 200)
		{
			dev0LinkAgg.pAggPorts[1]->set_aAggActorAdminKey(0x0246);           // change aggregator key
		}

		if (SimLog::Time == start + 300)
		{
			dev0LinkAgg.pAggPorts[2]->set_aAggActorSystemPriority(0x0135);     // change aggregator SysID (which changes LAG ID)
		}

		if (SimLog::Time == start + 400)
		{
			dev0LinkAgg.pAggPorts[1]->set_aAggPortActorAdminKey(savedKey);     // restore port key
			dev0LinkAgg.pAggPorts[1]->set_aAggActorAdminKey(savedKey);         // restore aggregator key
			dev0LinkAgg.pAggPorts[2]->set_aAggActorSystemPriority(0);          // restore aggregator SysID (which changes LAG ID)
		}

/*
		if (SimLog::Time == start + 450)                 // Patch up this link until Selection Logic bug is fixed
		{   
			dev0LinkAgg.pAggPorts[3]->setEnabled(false);
		}
		if (SimLog::Time == start + 454)
		{   
			dev0LinkAgg.pAggPorts[3]->setEnabled(true);
		}
/**/

		if (SimLog::Time == start + 500)
		{
			dev0LinkAgg.pAggPorts[1]->set_aAggPortLinkNumberID(18);            // change link number of b00:101
		}

		if (SimLog::Time == start + 600)
		{
			dev0LinkAgg.pAggPorts[1]->set_aAggPortAlgorithm(Aggregator::portAlgorithms::C_VID);       // change port algorithm
		}

		if (SimLog::Time == start + 700)
		{
			dev0LinkAgg.pAggPorts[1]->set_aAggPortLinkNumberID(2);             // restore link number of b00:101
		}

		if (SimLog::Time == start + 800)
		{
			dev0LinkAgg.pAggPorts[1]->set_aAggPortAlgorithm(Aggregator::portAlgorithms::UNSPECIFIED); // restore port algorithm
		}


		if (SimLog::Time == start + 990)
		{
			for (auto& pDev : Devices)
			{
				pDev->disconnect();      // Disconnect all remaining links on all devices
			}
			for (auto& pAgg : dev1LinkAgg.pAggregators)
			{
				pAgg->set_aAggPortAlgorithm(Aggregator::portAlgorithms::UNSPECIFIED);
			}
		}

		//  Run all state machines in all devices
		for (auto& pDev : Devices)
		{
			pDev->timerTick();     // Decrement timers
			pDev->run(true);       // Run device with single-step true
		}

		//  Transmit from any MAC with frames to transmit
		for (auto& pDev : Devices)
		{
			pDev->transmit();
		}

		if (SimLog::Debug > 1)
			SimLog::logFile << "*" << endl;
		SimLog::Time++;
	}

}

/**/
