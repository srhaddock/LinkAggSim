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

#include "stdafx.h"
#include "Device.h"
#include "Frame.h"
#include "LinkAgg.h"
#include "Aggregator.h"
#include "Mac.h"



unsigned short Device::devCnt = 0;

Device::Device(int numMacs)	
	: Component(ComponentTypes::DEVICE)
{
	devNum = devCnt++;
//	std::vector<shared_ptr<Mac>> Macs(numMacs, make_shared<Mac>());
	for (unsigned short i = 0; i < numMacs; i++)
	{
		macIdentifier thisMacId;
		thisMacId.dev = devNum;
		thisMacId.sap = i;
//		pMacs.push_back(make_shared<Mac>(thisMacId));
		pMacs.push_back(make_shared<Mac>(devNum, i));
	}

//	cout << "Device Constructor called. devNum = " << devNum << " and Count = " << devCnt << endl;
}

/*
Device::Device(Device& copySource) 
	: Component(copySource.type)
{
//	cout << "Device Copy Constructor called." << endl;
}
/**/

Device::~Device()
{
	for (auto& pMac : pMacs)            // Disconnect all Macs to prevent potential cross pointer "deadly embrace"
		Mac::Disconnect(pMac);         //    that would prevent destruction of the Mac objects
	pComponents.clear();
	pMacs.clear();
//	cout << "Device Destructor called." << endl;
}

unsigned short Device::getDeviceCount()
{
	return (devCnt);
}

unsigned short Device::getDeviceNumber()
{
	return (devNum);
}

void Device::reset()                              // Initialize all operational parameters of all Components and Macs
{
	for (auto& pComp : pComponents)
	{
		pComp->reset();
	}
	for (auto& pMac : pMacs)
	{
		pMac->reset();
	}
}

void Device::timerTick()                          // If not suspended, Tick timers in all Components and Macs
{   
	if (!suspended) 
	{ 
		for (auto& pComp : pComponents)
		{
			pComp->timerTick();
		}
		for (auto& pMac : pMacs)
		{
			pMac->timerTick();
		}
	}
}

void Device::run(bool singleStep)                 // If not suspended, Make one or more pass through all state machines in all Components and Macs
{
	if (!suspended)
	{
		for (auto& pComp : pComponents)
		{
			pComp->run(singleStep);
		}
		for (auto& pMac : pMacs)
		{
			pMac->run(singleStep);
		}
	}
}

void Device::transmit()                           // If not suspended, Transmit a Frame from any Macs in Device with a Frame ready to transmit
{
	for (auto& pMac : pMacs)
		if (!suspended)
		{
			pMac->Transmit();
		}

}

void Device::disconnect()                         // Disconnect all Macs in the Device that are connected to other Macs
{
	for (auto& pMac : pMacs)
		Mac::Disconnect(pMac);
}



void Device::createBridge(unsigned short type)
{
	/**/
	unsigned short sysNum = 0;       // This is a single system device
	int nPorts = pMacs.size();       // Number of Bridge Ports = number of Macs in Device
	unique_ptr<Bridge> pBridge = make_unique<Bridge>(devNum, sysNum, nPorts);  // Make a Bridge with a BridgePort for each Mac
	pBridge->vlanType = type;							               // Set as MAC, C-VLAN, or S-VLAN Bridge

	unique_ptr<LinkAgg> pLag = make_unique<LinkAgg>();

	for (unsigned short i = 0; i < nPorts; i++)    // For each Mac:
	{
//		shared_ptr<Aggregator> pAggregator = make_shared<Aggregator>();    // Create an Aggregator
		shared_ptr<AggPort> pAggPort = make_shared<AggPort>(pLag->LacpVersion, sysNum, i);    // Create an Aggregation Port/Aggregator pair
		pAggPort->setActorSystem(pBridge->SystemId);                       // Assign Aggregation Port/Aggregator to this bridge
		pBridge->bPorts[i]->pIss = pAggPort;                               // Attach the Aggregation Port/Aggregator to a BridgePort
		pAggPort->pIss = pMacs[i];                                         // Attach the Mac to the Aggregation Port
		pMacs[i]->updateMacSystemId(pBridge->SystemId.id);                 // Update Mac address/sapId with device type and sysNum
		pLag->pAggregators.push_back(pAggPort);                              // Put Aggregator in the Device's Lag shim
		pLag->pAggPorts.push_back(pAggPort);                                 // Put Aggregation Port in the Device's Lag shim
	}

	pComponents.push_back(move(pBridge));                             // Put the Bridge in the Device Components vector
	pComponents.push_back(move(pLag));                                // Put the Link Aggregation shim in the Device Components vector
	/**/
}


void Device::createEndStation()
{
	/**/
	unsigned short sysNum = 0;       // This is a single system device
	int nPorts = pMacs.size();       // Number of Aggregation Ports = number of Macs in Device
	unique_ptr<EndStn> pStation = make_unique<EndStn>(devNum, sysNum);  // Make an End Station

	unique_ptr<LinkAgg> pLag = make_unique<LinkAgg>();

	if (nPorts == 1)     // If single Mac then leave Lag shim with no Aggregators or Aggregation Ports
	{
		pStation->pIss = pMacs[0];                          // Attach the Mac to the End Station
	} 
	else                // Else, if more than one Mac, initialize Lag shim
	{
		for (unsigned short i = 0; i < nPorts; i++)    // For each Mac:
		{
//			shared_ptr<Aggregator> pAggregator = make_shared<Aggregator>();    // Create an Aggregator
			shared_ptr<AggPort> pAggPort = make_shared<AggPort>(pLag->LacpVersion, sysNum, i);             // Create an Aggregation Port/Aggregator pair
			pAggPort->setActorSystem(pStation->SystemId);                      // Assign Aggregation Port/Aggregator to this End Station
			pAggPort->pIss = pMacs[i];                                         // Attach the Mac to the Aggregation Port
			pMacs[i]->updateMacSystemId(pStation->SystemId.id);                // Update Mac address/sapId with device type and sysNum
			pLag->pAggregators.push_back(pAggPort);                              // Put Aggregator in the Device's Lag shim
			pLag->pAggPorts.push_back(pAggPort);                                 // Put Aggregation Port in the Device's Lag shim
			if (i == 0)
			{
				pStation->pIss = pLag->pAggregators[i];                          // Attach the first Aggregator to the End Station

			} else {
				pAggPort->set_aAggActorAdminKey(unusedAggregatorKey);          // Set Admin key of other Aggregators to value not shared with any AggPort
			}
		}
	} 

	pComponents.push_back(move(pStation));                            // Put the End Station in the Device Components vector
	pComponents.push_back(move(pLag));                                // Put the Link Aggregation shim in the Device Components vector
	/**/
}



EndStn::EndStn(unsigned short devNum, unsigned short sysNum)
	: Component(ComponentTypes::END_STATION)
{
	SystemId.id = defaultEndStnAddr + (0x10000 * devNum) + (0x1000 * sysNum);
	sequenceNumber = 0;
	rxFrameCount = 0;
	pIss = nullptr;

//	cout << "EndStn Constructor called." << endl;
}

/*
EndStn::EndStn(EndStn& copySource)
	: Component(copySource.type)
{
	SystemId.id = copySource.SystemId.id;
	pIss = copySource.pIss;
	sequenceNumber = copySource.sequenceNumber;
	rxFrameCount = copySource.rxFrameCount;

//	cout << "EndStn Copy Constructor called." << endl;
}
/**/

EndStn::~EndStn()
{
	pIss = nullptr;
//	cout << "EndStn Destructor called." << endl;
}

void EndStn::reset()
{
	//TODO:
}

void EndStn::timerTick()
{
	if (!suspended)
	{
		// Nothing to do given current EndStn functionality
	}
}


void EndStn::run(bool singleStep)
{
	if (!suspended) 
	{ 
		//TODO:  Currently run always single steps.  When singleStep is false should iterate until rx queue empty
		unique_ptr<Frame> pFrame = std::move(pIss->Indication());
		if (pFrame)
		{
			rxFrameCount++;

			if (SimLog::Debug > 5)
			{
				SimLog::logFile << "Time " << SimLog::Time << "    EndStation " << hex << SystemId.addr << " received frame ";
				pFrame->PrintFrameHeader();
				SimLog::logFile << dec << endl;
				if ((pFrame->getNextEtherType() == PlaypenEthertypeA) && (pFrame->getNextSubType() == 1))
				{
					SimLog::logFile << "   Test frame sequence number = " << ((TestSdu&)(pFrame->getNextSdu())).scratchPad << endl;
				}
				if ((pFrame->getNextEtherType() == SlowProtocolsEthertype) && (pFrame->getNextSubType() == LacpduSubType))
				{
					SimLog::logFile << "   Lacpdu System = " << hex << ((Lacpdu&)(pFrame->getNextSdu())).actorSystem.id
						<< "  Port = " << ((Lacpdu&)(pFrame->getNextSdu())).actorPort.id << dec << endl;
				}

			}
		}
	}
}

void EndStn::generateTestFrame(shared_ptr<Sdu> pTag)
{
	if (pIss->getOperational())  // Transmit frame only if MAC won't immediately discard
	{
		unsigned long long thisSA = SystemId.addr;
		shared_ptr<Sdu> thisSdu = std::make_shared<TestSdu>(sequenceNumber);
		unique_ptr<Frame> thisFrame = make_unique<Frame>(defaultDA, thisSA, thisSdu);
//		unique_ptr<Frame> thisFrame = make_unique<Frame>(defaultDA, SystemId.addr, thisSdu);  // Why won't SystemId.addr work?
		if (pTag) 
			thisFrame = thisFrame->InsertTag(pTag);
		thisFrame->TimeStamp = SimLog::Time;   // May get overwritten at MAC request queue
		pIss->Request(move(thisFrame));
	}
	sequenceNumber++;    // Increment sequence number (even if MAC would have immediately discarded frame)
}