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
#include "Aggregator.h"
#include "Mac.h"


Aggregator::Aggregator(unsigned char version, unsigned short systemNum, unsigned short portNum)
//	: IssQ()
{
	enabled = true;
	operational = false;
	actorSystem.id = 0;
	actorLacpVersion = version;
	aggregatorMacAddress = 0;                                         // MAC-SA for all Aggregator clients
	aggregatorIdentifier = (systemNum * 0x1000) + 0x0200 + portNum;   // ifindex
	aggregatorIndividual = false;
	actorAdminAggregatorKey = (systemNum * 0x100) + defaultActorKey;
	actorOperAggregatorKey = actorAdminAggregatorKey;
	partnerSystem.id = 0;
	partnerOperAggregatorKey = 0;
	receiveState = false;
	transmitState = false;
	collectorMaxDelay = 20;                    // Standard says this is a constant, but read-write managed object per Aggregator. 

	aggregatorReady = false;

	selectedconvLinkMap = Aggregator::convLinkMaps::EIGHT_LINK_SPREAD;
	actorPortAlgorithm = portAlgorithms::UNSPECIFIED;
	actorAdminConversationLinkListDigest.fill(0);
	actorConversationLinkListDigest = convLinkMapEightLinkSpreadDigest;
	actorAdminConversationServiceMappingDigest.fill(0);
	actorConversationServiceMappingDigest.fill(0);
	partnerAdminPortAlgorithm = portAlgorithms::UNSPECIFIED;
	partnerAdminConversationLinkListDigest.fill(0);
	partnerAdminConversationServiceMappingDigest.fill(0);
	partnerPortAlgorithm = portAlgorithms::UNSPECIFIED;
	partnerConversationLinkListDigest.fill(0);
	partnerConversationServiceMappingDigest.fill(0);
	adminDiscardWrongConversation = adminValues::AUTO;
	operDiscardWrongConversation = false;

	changeActorSystem = false;

	changeActorDistAlg = false;
	changeConvLinkList = false;
	changePartnerAdminDistAlg = false;
	changeDistAlg = false;
	changeLinkState = false;
	changeAggregationLinks = false;
	changeCSDC = false;

	//  cout << "Aggregator Constructor called." << endl;
	//	SimLog::logFile << "Aggregator Constructor called." << endl;
}


Aggregator::~Aggregator()
{
	lagPorts.clear();     

	//  cout << "Aggregator Destructor called." << endl;
	//	SimLog::logFile << "Aggregator Destructor called." << endl;
}

void Aggregator::setEnabled(bool val)
{
	enabled = val;
}

bool Aggregator::getOperational() const
{
	return (operational);  
}

unsigned long long Aggregator::getMacAddress() const                  // MAC-SA for all Aggregator clients
{
	return (aggregatorMacAddress);
}

void Aggregator::reset()
{
	operational = false;
	aggregatorIndividual = false;
	actorOperAggregatorKey = actorAdminAggregatorKey;
	partnerSystem.id = 0;
	partnerOperAggregatorKey = 0;
	lagPorts.clear();       //TODO: Clearing lagPort list without setting those ports to UNSELECTED may be problematic, but those ports aren't accessible
	selectedLagPorts.clear();
	receiveState = false;
	transmitState = false;

	aggregatorReady = false;

	changeActorSystem = false;

	// all of the following is in resetCSDC()
	partnerPortAlgorithm = partnerAdminPortAlgorithm;
	partnerConversationLinkListDigest = partnerAdminConversationLinkListDigest;
	partnerConversationServiceMappingDigest = partnerAdminConversationServiceMappingDigest;
	differentPortAlgorithms = false;
	differentConversationServiceDigests = false;
	differentPortConversationDigests = false;
	operDiscardWrongConversation = false;
	activeLagLinks.clear();
	conversationLinkVector.fill(0);
	conversationPortVector.fill(0);

	changeActorDistAlg = false;
	changeConvLinkList = false;
	changePartnerAdminDistAlg = false;
	changeDistAlg = false;
	changeLinkState = false;
	changeAggregationLinks = false;
	changeCSDC = false;
}

bool Aggregator::digestIsNull(std::array<unsigned char, 16>& digest)
{
	bool nullDigest = true;
	for (auto entry : digest)
		nullDigest &= (entry == 0);
	return(nullDigest);
}

//TODO:  add override of Indication that drops frame if collectorMaxDelay exceeded ??

void Aggregator::setActorSystem(sysId id)  //TODO: do I really need this or should it be done with standard access routines?
{
	changeActorSystem = (id.id != actorSystem.id);                   // Set flag to notify Selection Logic of change
	actorSystem = id;
	aggregatorMacAddress = (actorSystem.addr & 0x0fffffffff000) + (aggregatorIdentifier & 0x0fff);
}

/*
 *   802.1AX Standard managed objects access routines
 */

unsigned short Aggregator::get_aAggID()                              // ifindex
{
	return (aggregatorIdentifier);
}

unsigned long long Aggregator::get_aAggMACAddress()                  // MAC-SA for all Aggregator clients
{
	return (aggregatorMacAddress);
}

void Aggregator::set_aAggActorSystemID(unsigned long long sysAddr)   // Sets the Address portion of the System ID
{
	changeActorSystem = (sysAddr != actorSystem.addr);               // Set flag to notify Selection Logic of change
	actorSystem.addr = sysAddr;
}

unsigned long long Aggregator::get_aAggActorSystemID()               // Returns full 64 bit System ID (including System Priority and Address) ??
{
//	return (actorSystem.addr);                                          // Returns the Address portion of the System ID
	return (actorSystem.id);                                            // Returns full 64 bit System ID (including System Priority and Address)
}

void Aggregator::set_aAggActorSystemPriority(unsigned short pri)     // Sets the Priority portion of the System ID
{
	changeActorSystem = (pri != actorSystem.pri);                    // Set flag to notify Selection Logic of change
	actorSystem.pri = pri;
}

unsigned short Aggregator::get_aAggActorSystemPriority()             // Returns the Priority portion of the System ID
{
	return (actorSystem.pri);
}

void Aggregator::set_aAggActorAdminKey(unsigned short key)
{
	actorAdminAggregatorKey = key;                                   // Change to this detected in Selection Logic by comparing to oper value
}

unsigned short Aggregator::get_aAggActorAdminKey()
{
	return (actorAdminAggregatorKey);
}

unsigned short Aggregator::get_aAggActorOperKey()
{
	return (actorOperAggregatorKey);
}

unsigned long long Aggregator::get_aAggPartnerSystemID()
{
//	return (partnerSystem.addr);                                          // Returns the Address portion of the System ID
	return (partnerSystem.id);                                            // Returns full 64 bit System ID (including System Priority and Address)
}

unsigned short Aggregator::get_aAggPartnerSystemPriority()
{
	// The way the Selection Logic implementation uses the partnerSystem variable it needs to be set when a port selects the Aggregator,
	//   but the standard managed object is specified as zero when no ports are attached.
	bool attached = false;
	for (auto px : lagPorts)    // verify at least one port on list is attached to aggregator
	{
		//TODO:: to do this I need Aggregator to be able to access a AggPort by AggPort index
	}
	//  Could maintain a numPortsAttached variable for each Aggregator to test and return zero if no ports attached,
	//     but that trick won't work for get_aAggPortList() if try to build a vector of attached actorPort.num from lagPorts list.
	return (partnerSystem.pri);
}

unsigned short Aggregator::get_aAggPartnerOperKey()
{
	return (partnerOperAggregatorKey);
}

void Aggregator::set_aAggAdminState(bool enable)                     // Sets ISS Enabled;
{
	setEnabled(enable);
}

bool Aggregator::get_aAggAdminState()                                // Gets ISS Enabled;
{
	return (getEnabled());
}

bool Aggregator::get_aAggOperState()                                 // Gets ISS Operational;
{
	return (getOperational());
}

void Aggregator::set_aCollectorMaxDelay(unsigned short delay)
{
	collectorMaxDelay = delay;
}

unsigned short Aggregator::get_aAggCollectorMaxDelay()
{
	return (collectorMaxDelay);
}

bool Aggregator::get_aAggAggregateOrIndividual()
{
	return (!aggregatorIndividual);
}

std::list<unsigned short> Aggregator::get_aAggPortList()   
{
	return (selectedLagPorts);  // List of Aggregation Port Identifiers attached to this aggregator  //TODO:  currently list includes selected or attached
}


// Version 2 attributes
void Aggregator::set_aAggPortAlgorithm(Aggregator::portAlgorithms alg)
{
	if (actorPortAlgorithm != alg)   // If new value is not the same as the old
	{
		actorPortAlgorithm = alg;                // Store new value
		changeActorDistAlg = true;               //   and set flag so change is processed
	}
}

Aggregator::portAlgorithms Aggregator::get_aAggPortAlgorithm()
{
	return(actorPortAlgorithm);
}

void Aggregator::set_aAggPartnerAdminPortAlgorithm(Aggregator::portAlgorithms alg)
{
	partnerAdminPortAlgorithm = alg;
	changePartnerAdminDistAlg = true;
}

Aggregator::portAlgorithms Aggregator::get_aAggPartnerAdminPortAlgorithm()
{
	return (partnerAdminPortAlgorithm);
}

void Aggregator::set_aAggPartnerAdminConversationListDigest(std::array<unsigned char, 16> digest)
{
	partnerAdminConversationLinkListDigest = digest;
	changePartnerAdminDistAlg = true;
}

std::array<unsigned char, 16> Aggregator::get_aAggPartnerAdminConversationListDigest()
{
	return (partnerAdminConversationLinkListDigest);
}

void Aggregator::set_aAggPartnerAdminConvServiceMappingDigest(std::array<unsigned char, 16> digest)
{
	partnerAdminConversationServiceMappingDigest = digest;
	changePartnerAdminDistAlg = true;
}

std::array<unsigned char, 16> Aggregator::get_aAggPartnerAdminConvServiceMappingDigest()
{
	return (partnerAdminConversationServiceMappingDigest);
}

void Aggregator::set_aAggAdminDiscardWrongConversation(adminValues DWC)
{
	adminDiscardWrongConversation = DWC;
	if (!activeLagLinks.empty())               // if aggregator has any active links
		changeDistAlg = true;                  //    then set flag to re-evaluate operational DWC
}

adminValues Aggregator::get_aAggAdminDiscardWrongConversation()
{
	return (adminDiscardWrongConversation);
}

bool Aggregator::get_aAggOperDiscardWrongConversation()    // not in standard
{
	return (operDiscardWrongConversation);
}

//  void Aggregator::set_aAggAdminServiceConversationMap(???);  //TODO:  what form do these tables take when read and written?
//  ??? Aggregator::get_aAggAdminServiceConversationMap();

void Aggregator::set_aAggConversationAdminLink(unsigned short cid, std::list<unsigned short>linkNumberList)
{
	if (cid <= 4095)                                                     // if the cid is valid then:
	{
		if (adminConversationLinkMap.find(cid) != adminConversationLinkMap.end())  // if map contains an entry for that cid
		{
			adminConversationLinkMap.erase(cid);                               //    then remove the old entry
		}
		if (!linkNumberList.empty() && (linkNumberList.front() != 0))      // if the new entry contains at least one non-zero link number
		{
			adminConversationLinkMap.insert(make_pair(cid, linkNumberList));   //    then insert the new map entry
		}
		// recalculate Digest
		changeConvLinkList = true;                 //   and set flag so change is processed
		changeActorDistAlg = true;                 //   and set flag so change is processed
	}
}

std::list<unsigned short> Aggregator::get_aAggConversationAdminLink(unsigned short cid)
{
	std::list<unsigned short> linkNumberList;
	
	if (adminConversationLinkMap.find(cid) != adminConversationLinkMap.end())  // if map contains an entry for that cid
	{
		linkNumberList = adminConversationLinkMap.at(cid);     //    then copy the prioritized list of link numbers for that cid
	}
	return(linkNumberList);
}

//  This function is to facilitate testing of the above get/set functions
void Aggregator::printConversationAdminLink()
{
	if (adminConversationLinkMap.empty())
		SimLog::logFile << "    No entries in the Conversation Link Map" << endl;
	else
	{
		for (const auto& entry : adminConversationLinkMap)
		{
			SimLog::logFile << "    " << entry.first << ":   ";
			for (const auto& link : entry.second)
				SimLog::logFile << link << "  ";
			SimLog::logFile << endl;
		}
	}
}


void Aggregator::set_aAggConversationListDigest(std::array<unsigned char, 16> digest)
{
	actorAdminConversationLinkListDigest = digest;
	changeActorDistAlg = true;                 //   and set flag so change is processed
}

std::array<unsigned char, 16>Aggregator::get_aAggConversationListDigest()
{
	return (actorAdminConversationLinkListDigest);
}



unsigned short Aggregator::get_conversationLink(unsigned short cid)
{
	return (conversationLinkVector[cid]);
}

Aggregator::portAlgorithms Aggregator::get_aAggPartnerPortAlgorithm()
{
	return (partnerPortAlgorithm);
}

void Aggregator::set_convLinkMap(Aggregator::convLinkMaps choice)
{
	selectedconvLinkMap = choice;
	switch (selectedconvLinkMap)
	{
	case Aggregator::convLinkMaps::ADMIN_TABLE:
		actorConversationLinkListDigest = actorAdminConversationLinkListDigest;
		break;
	case Aggregator::convLinkMaps::ACTIVE_STANDBY:
		actorConversationLinkListDigest = convLinkMapActiveStandbyDigest;
		break;
	case Aggregator::convLinkMaps::EVEN_ODD:
		actorConversationLinkListDigest = convLinkMapEvenOddDigest;
		break;
	case Aggregator::convLinkMaps::EIGHT_LINK_SPREAD:
		actorConversationLinkListDigest = convLinkMapEightLinkSpreadDigest;
		break;
	}
	// recalculate Digest
	changeActorDistAlg = true;                 //   and set flag so change is processed
}

Aggregator::convLinkMaps Aggregator::get_convLinkMap()
{
	return (selectedconvLinkMap);
}

std::array<unsigned char, 16> Aggregator::get_aAggOperConversationListDigest()
{
	return (actorConversationLinkListDigest);
}

std::array<unsigned char, 16> Aggregator::get_aAggPartnerOperConversationListDigest()
{
	return (partnerConversationLinkListDigest);
}


/*
// Not in standard:
Aggregator::portAlgorithms Aggregator::get_aAggPartnerPortAlgorithm();
std::array<unsigned char, 16> Aggregator::get_aAggPartnerConversationListDigest();
std::array<unsigned char, 16> Aggregator::get_aAggPartnerConvServiceMappingDigest();
std::array<unsigned char, 16> Aggregator::get_aAggConversationListDigest();
std::array<unsigned char, 16> Aggregator::get_aAggConvServiceMappingDigest();
/**/


