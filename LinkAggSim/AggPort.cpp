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
#include "AggPort.h"
#include "Aggregator.h"

// const unsigned char defaultPortState = 0x43; 

AggPort::AggPort(unsigned char version, unsigned short systemNum, unsigned short portNum)
	: Aggregator(version, systemNum, portNum)
{
	PortEnabled = false;                                           // Set by Receive State Machine based on ISS.Operational
	LacpEnabled = true;                                            //TODO:  what changes LacpEnabled?
	newPartner = false;
	PortMoved = false;   
	portSelected = UNSELECTED;
	ReadyN = false;                  // Set by MuxSM;              Reset by: MuxSM;             Used by: Selection
	Ready = false;                   // Set by Selection;          Reset by: Selection;         Used by: MuxSM
	policy_coupledMuxControl = false;
	changeActorOperDist = false;

	pRxLacpFrame = nullptr;
	pIss = nullptr;

	aggregationPortIdentifier = (systemNum * 0x1000) + 0x0100 + portNum;
//	SimLog::logFile << "Building AggPort " << hex << aggregationPortIdentifier << " in System " << actorSystem.id << dec << endl;
	actorPort.pri = 0;
	actorPort.num = aggregationPortIdentifier;
	actorAdminPortKey = (systemNum * 0x100) + defaultActorKey;
	actorOperPortKey = actorAdminPortKey;
	actorAdminPortState.state = defaultPortState;
	actorOperPortState = actorAdminPortState;
	partnerAdminSystem.id = 0;
	partnerOperSystem = partnerAdminSystem;
	partnerAdminPort.pri = 0;
	partnerAdminPort.num = portNum;
	partnerOperPort = partnerAdminPort;
	partnerAdminKey = (systemNum * 0x100) + defaultPartnerKey + portNum;
	partnerOperKey = partnerAdminKey;
	partnerAdminPortState.state = defaultPartnerPortState;
	partnerOperPortState = partnerAdminPortState;
	NTT = false;
	LacpTxEnabled = false;                                            // Set by Periodic State Machine
	actorPortAggregatorIdentifier = 0;                                // The aggregatorIdentifier to which this Aggregation Port is attached.
	actorPortAggregatorIndex = 0;                                     // The index of the Aggregator to which this Aggregation Port is attached.
	individualAggregationPort = false;
	lacpDestinationAddress = SlowProtocolsDA;

	//TODO:  LACPv2 variables
	enableLongLacpduXmit = true;
	longLacpduXmit = true;
	waitToRestoreTime = 30;
	adminLinkNumberID = (portNum & 0x00ff) + 1;      // Add 1 because portNum starts at zero which is reserved for "no link"
	LinkNumberID = adminLinkNumberID;
	partnerLinkNumberID = adminLinkNumberID;
	actorAttached = false;
	actorDWC = false;
	partnerDWC = false;

	actorOperPortAlgorithm = portAlgorithms::UNSPECIFIED;
	actorOperConversationLinkListDigest.fill(0);
	actorOperConversationServiceMappingDigest.fill(0);
	partnerOperPortAlgorithm = portAlgorithms::NONE;
	partnerOperConversationLinkListDigest.fill(0);
	partnerOperConversationServiceMappingDigest.fill(0);

	changePartnerOperDistAlg = false;
	changeActorAdmin = false;
	changePartnerAdmin = false;
	changeAdminLinkNumberID = false;

	//  cout << "AggPort Constructor called." << endl;
	//	SimLog::logFile << "AggPort Constructor called." << endl;
}


AggPort::~AggPort()
{
	pRxLacpFrame = nullptr;
	pIss = nullptr;

	//  cout << "AggPort Destructor called." << endl;
	//	SimLog::logFile << "AggPort Destructor called." << endl;
}


/**/
void AggPort::reset()
{
	AggPort::LacpRxSM::reset(*this);
	AggPort::LacpMuxSM::reset(*this);
	AggPort::LacpPeriodicSM::reset(*this);
	AggPort::LacpTxSM::reset(*this);

	// Reset Selection Logic variables
	Ready = false;                   // Set by Selection;          Reset by: Selection;         Used by: MuxSM
	// TODO: clearing association with an Aggregator does not remove port from that aggregator's lagPort list (may be problematic if not all of LinkAgg is reset)
	actorPortAggregatorIdentifier = 0;                                // The aggregatorIdentifier to which this Aggregation Port is attached.
	actorPortAggregatorIndex = 0;                                     // The index of the Aggregator to which this Aggregation Port is attached.
	individualAggregationPort = false;
	actorOperPortAlgorithm = portAlgorithms::UNSPECIFIED;
	actorOperConversationLinkListDigest.fill(0);
	actorOperConversationServiceMappingDigest.fill(0);
	actorDWC = false;
	Ready = false;                   // Set by Selection;          Reset by: Selection;         Used by: MuxSM
	changeActorOperDist = false;
	changePartnerOperDistAlg = false;
	longLacpduXmit = true;            // controlled by LinkAgg

}

void AggPort::timerTick()
{
	AggPort::LacpRxSM::timerTick(*this);
	AggPort::LacpMuxSM::timerTick(*this);
	AggPort::LacpPeriodicSM::timerTick(*this);
	AggPort::LacpTxSM::timerTick(*this);
}

void AggPort::run(bool singleStep)
{
	AggPort::LacpRxSM::run(*this, singleStep);
	AggPort::LacpMuxSM::run(*this, singleStep);
	AggPort::LacpPeriodicSM::run(*this, singleStep);
	AggPort::LacpTxSM::run(*this, singleStep);
}


/**/
/*
*   802.1AX Standard managed objects access routines
*/
unsigned short AggPort::get_aAggPortID()                              // ifindex
{
	return (aggregationPortIdentifier);
}

void AggPort::set_aAggPortActorSystemID(unsigned long long sysAddr)   // Sets the Address portion of the System ID
{
	changeActorSystem = (sysAddr != actorSystem.addr);
	actorSystem.addr = sysAddr;
}

unsigned long long AggPort::get_aAggPortActorSystemID()               // Returns full 64 bit System ID (including System Priority and Address) ??
{
//	return (actorSystem.addr);                                        // Returns the Address portion of the System ID
	return (actorSystem.id);                                          // Returns full 64 bit System ID (including System Priority and Address)
}

void AggPort::set_aAggPortActorSystemPriority(unsigned short pri)     // Sets the Priority portion of the System ID
{
	changeActorSystem = (pri != actorSystem.pri);
	actorSystem.pri = pri;
}

unsigned short AggPort::get_aAggPortActorSystemPriority()             // Returns the Priority portion of the System ID
{
	return (actorSystem.pri);
}

void AggPort::set_aAggPortActorAdminKey(unsigned short key)
{
	actorAdminPortKey = key;
	changeActorAdmin = true;
}

unsigned short AggPort::get_aAggPortActorAdminKey()
{
	return (actorAdminPortKey);
}

unsigned short AggPort::get_aAggPortActorOperKey()
{
	return (actorOperPortKey);
}

void AggPort::set_aAggPortPartnerAdminSystemID(unsigned long long sysAddr)
{
	partnerAdminSystem.addr = sysAddr;
	changePartnerAdmin = true;
}

unsigned long long AggPort::get_aAggPortPartnerAdminSystemID()
{
//	return (partnerAdminSystem.addr);                                   // Returns the Address portion of the System ID
	return (partnerAdminSystem.id);                                     // Returns full 64 bit System ID (including System Priority and Address)
}

unsigned long long AggPort::get_aAggPortPartnerOperSystemID()
{
//	return (partnerOperSystem.addr);                                   // Returns the Address portion of the System ID
	return (partnerOperSystem.id);                                     // Returns full 64 bit System ID (including System Priority and Address)
}

void AggPort::set_aAggPortPartnerAdminSystemPriority(unsigned short pri)
{
	partnerAdminSystem.pri = pri;
	changePartnerAdmin = true;
}

unsigned short AggPort::get_aAggPortPartnerAdminSystemPriority()
{
	return (partnerAdminSystem.pri);
}

unsigned short AggPort::get_aAggPortPartnerOperSystemPriority()
{
	return (partnerOperSystem.pri);
}

void AggPort::set_aAggPortPartnerAdminKey(unsigned short key)
{
	partnerAdminKey = key;
	changePartnerAdmin = true;
}

unsigned short AggPort::get_aAggPortPartnerAdminKey()
{
	return (partnerAdminKey);
}

unsigned short AggPort::get_aAggPortPartnerOperKey()
{
	return (partnerOperKey);
}

unsigned short AggPort::get_aAggPortSelectedAggID()
{
	if (portSelected == selectedVals::SELECTED)
		return (actorPortAggregatorIdentifier);
	else
		return (0);
}

unsigned short AggPort::get_aAggPortAttachedAggID()
{
	if (actorOperPortState.sync)                      // TODO:: may need to change to actorAttached
		return (actorPortAggregatorIdentifier);
	else
		return (0);
}

void AggPort::set_aAggPortActorPort(unsigned short portNum)
{   //TODO: In standard need actorPort to be >0 (6.3.4), but in sim more convenient to match index which starts at 0
	actorPort.num = portNum;
	changeActorAdmin = true;
}

unsigned short AggPort::get_aAggPortActorPort()
{
	return (actorPort.num);
}

void AggPort::set_aAggPortActorPortPriority(unsigned short pri)
{
	actorPort.pri = pri;
	changeActorAdmin = true;
}

unsigned short AggPort::get_aAggPortActorPortPriority()
{
	return (actorPort.pri);
}

void AggPort::set_aAggPortPartnerAdminPort(unsigned short portNum)
{
	partnerAdminPort.num = portNum;
	changePartnerAdmin = true;
}

unsigned short AggPort::get_aAggPortPartnerAdminPort()
{
	return (partnerAdminPort.num);
}

unsigned short AggPort::get_aAggPortPartnerOperPort()
{
	return (partnerOperPort.num);
}

void AggPort::set_aAggPortPartnerAdminPortPriority(unsigned short pri)
{
	partnerAdminPort.pri = pri;
	changePartnerAdmin = true;
}

unsigned short AggPort::get_aAggPortPartnerAdminPortPriority()
{
	return (partnerAdminPort.pri);
}

unsigned short AggPort::get_aAggPortPartnerOperPortPriority()
{
	return (partnerOperPort.pri);
}

void AggPort::set_aAggPortActorAdminState(unsigned char state)
{
//	cout << "    Setting admin state from 0x" <<  hex << (short)actorAdminPortState.state << " to 0x" << (unsigned short)state << dec << endl;
	actorAdminPortState.state = state;
	changeActorAdmin = true;
}

unsigned char AggPort::get_aAggPortActorAdminState()
{
	return (actorAdminPortState.state);
}

LacpPortState AggPort::get_aAggPortActorOperState()
{
	return (actorOperPortState);
}

void AggPort::set_aAggPortPartnerAdminState(LacpPortState state)
{
	partnerAdminPortState = state;
	changePartnerAdmin = true;
}

LacpPortState AggPort::get_aAggPortPartnerAdminState()
{
	return (partnerAdminPortState);
}

LacpPortState AggPort::get_aAggPortPartnerOperState()
{
	return (partnerOperPortState);
}

bool AggPort::get_aAggPortAggregateOrIndividual()
{
	return (!individualAggregationPort);
}

void AggPort::set_aAggPortProtocolDA(unsigned long long addr)
{
	lacpDestinationAddress = addr;
}

unsigned long long AggPort::get_aAggPortProtocolDA()
{
	return (lacpDestinationAddress);
}

void AggPort::set_aAggPortWTRTime(int time)
{
	waitToRestoreTime = time;
}

int AggPort::get_aAggPortWTRTime()
{
	return (waitToRestoreTime);
}

void AggPort::set_aAggPortEnableLongPDUXmit(bool enable)
{
	enableLongLacpduXmit = enable;
}

bool AggPort::get_aAggPortEnableLongPDUXmit()
{
	return (enableLongLacpduXmit);
}

void AggPort::set_aAggPortLinkNumberID(unsigned short link)
{
	if ((link > 0) && (link != adminLinkNumberID))
	{
		adminLinkNumberID = link;
		changeAdminLinkNumberID = true;    // Signal RxSM that admin Link Number has changed
	}
}

unsigned short AggPort::get_aAggPortLinkNumberID()
{
	return (adminLinkNumberID);
}

unsigned short AggPort::get_aAggPortOperLinkNumberID()
{
	return (adminLinkNumberID);
}

void AggPort::set_aAggPortPartnerAdminConversationMask(std::bitset<4096> mask)  // No managed object for this in standard
{
	partnerAdminConversationMask = mask;
	changePartnerAdmin = true;  
}

std::bitset<4096> AggPort::get_aAggPortPartnerAdminConversationMask()
{
	return(partnerAdminConversationMask);
}


