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

const unsigned short defaultActorKey = 0x0088;
const unsigned short defaultPartnerKey = 0x0070;
const unsigned short unusedAggregatorKey = 0x0911;
static const std::array<unsigned char, 16> convLinkMapActiveStandbyDigest = { "ACTIVE_STANDBY " };
static const std::array<unsigned char, 16> convLinkMapEvenOddDigest = { "EVEN_ODD       " };
static const std::array<unsigned char, 16> convLinkMapEightLinkSpreadDigest = { "8_LINK_SPREAD  " };
//TODO:: calculate the real MD5 signatures of the fixed adminConversationLinkMaps

/**/
union portId
{
	unsigned long id;
	struct
	{
		unsigned short num;
		unsigned short pri;
	};
};

union LacpPortState
{
	unsigned char state;
	struct
	{
		unsigned char lacpActivity : 1;          // LSB
		unsigned char lacpShortTimeout : 1;
		unsigned char aggregation : 1;
		unsigned char sync : 1;
		unsigned char collecting : 1;
		unsigned char distributing : 1;
		unsigned char defaulted : 1;
		unsigned char expired : 1;               // MSB 
	};
};

const unsigned char defaultPortState = 0x47;         // defaulted, aggregation (not individual), short timeout, active (not passive)
const unsigned char defaultPartnerPortState = 0x7f;  // defaulted, distributing, collecting, sync, aggregation, short timeout, active
//  defaultPartnerPortState must have collecting set to true if the AggPort and selected Aggregator are to come up when RxSM is defaulted.
//     For consistency the defaultPartnerPortState also has sync and distributing set to true.

union AggPortConversationMaskState
{
	unsigned char state;
	struct
	{
		unsigned char actorPartnerSync : 1;
		unsigned char PSI : 1;
		unsigned char DWC : 1;
		unsigned char rsvd : 5;
	};
};

/**/


class Aggregator : public IssQ
{
	friend class AggPort;
	friend class LinkAgg;
	friend class DistributedRelay;


public:
	Aggregator(unsigned char version = 2, unsigned short systemNum = 0, unsigned short portNum = 0);
	virtual ~Aggregator();
	Aggregator(Aggregator& copySource) = delete;             // Disable copy constructor
	Aggregator& operator= (const Aggregator&) = delete;      // Disable assignment operator

	sysId actorSystem;
	unsigned char actorLacpVersion;
	unsigned char partnerLacpVersion;

	virtual void setEnabled(bool val);
	bool getOperational() const;
	virtual unsigned long long getMacAddress() const;
	void setActorSystem(sysId id);
	void reset();

	// public LACPv2 variables
	enum portAlgorithms { NONE = 0, UNSPECIFIED = 0x0080c200, C_VID, S_VID, I_SID, TE_SID, ECMP_FLOW_HASH };
	enum convLinkMaps { ADMIN_TABLE, ACTIVE_STANDBY, EVEN_ODD, EIGHT_LINK_SPREAD };

protected:
	bool operational;
	unsigned long long aggregatorMacAddress;                  // MAC-SA for all Aggregator clients
	unsigned short aggregatorIdentifier;                      // ifindex
	bool aggregatorIndividual;
	bool aggregatorLoopback;
	unsigned short actorAdminAggregatorKey;
	unsigned short actorOperAggregatorKey;
	sysId partnerSystem;
	unsigned short partnerOperAggregatorKey;
	bool receiveState;
	bool transmitState;
	std::list<unsigned short> lagPorts;              // List of Aggregation Port index for all ports attached or selected
	std::list<unsigned short> selectedLagPorts;      // List of Aggregation Port Identifiers for all ports attached or selected
	unsigned short collectorMaxDelay;

	bool aggregatorReady;

	bool changeActorSystem;
	bool changeDistributing;

	bool changeActorDistAlg;                  //
	bool changeConvLinkList;                  //
	bool changePartnerAdminDistAlg;           //
	bool changeDistAlg;                       //
	bool changeLinkState;                     //
	bool changeAggregationLinks;              //
	bool changeCSDC;                          //


	// private LACPv2 variables
	adminValues adminDiscardWrongConversation;
	bool operDiscardWrongConversation;
	std::array<unsigned char, 16> actorAdminConversationLinkListDigest;           // Admin value set by management
	std::array<unsigned char, 16> actorConversationLinkListDigest;                // value in use selected by selectedconvLinkMap
	std::array<unsigned char, 16> partnerAdminConversationLinkListDigest;         // Admin value set by management; used when actorOperPortState.defaulted
	std::array<unsigned char, 16> partnerConversationLinkListDigest;              // value in use (default or as received from partner LACPDU)
	std::array<unsigned char, 16> actorAdminConversationServiceMappingDigest;     // Admin value set by management 
	std::array<unsigned char, 16> actorConversationServiceMappingDigest;          // value in use selected by ...
	std::array<unsigned char, 16> partnerAdminConversationServiceMappingDigest;   // Admin value set by management; used when actorOperPortState.defaulted
	std::array<unsigned char, 16> partnerConversationServiceMappingDigest;        // value in use (default or as received from partner LACPDU)
	portAlgorithms actorPortAlgorithm;
	portAlgorithms partnerAdminPortAlgorithm;
	portAlgorithms partnerPortAlgorithm;
	std::map<unsigned short, std::list<unsigned short>> adminConversationLinkMap;      // map of lists of Link Numbers keyed by CID
	bool differentConversationServiceDigests;
	bool differentPortAlgorithms;
	bool differentPortConversationDigests;
	std::list<unsigned short> activeLagLinks;  // contains Link Number ID if AggPort attached and distributing, else Link Number = 0
	std::array<unsigned short, 4096> conversationLinkVector;  // Contains LinkNumberID of AggPort for each Conversation ID.
	std::array<unsigned short, 4096> conversationPortVector;  // Contains PortNumber of AggPort for each Conversation ID.
	convLinkMaps selectedconvLinkMap;


	static bool digestIsNull(std::array<unsigned char, 16>& digest);

	/*
	*   802.1AX Standard managed objects access routines
	*/
public:
	unsigned short get_aAggID();                              // ifindex
	unsigned long long get_aAggMACAddress();                  // MAC-SA for LACP frames
	void set_aAggActorSystemID(unsigned long long sysAddr);   // Sets the Address portion of the System ID
	unsigned long long get_aAggActorSystemID();               // Returns full 64 bit System ID (including System Priority and Address) ??
	void set_aAggActorSystemPriority(unsigned short pri);     // Sets the Priority portion of the System ID
	unsigned short get_aAggActorSystemPriority();             // Returns the Priority portion of the System ID
	void set_aAggActorAdminKey(unsigned short key);
	unsigned short get_aAggActorAdminKey();
	unsigned short get_aAggActorOperKey();
	unsigned long long get_aAggPartnerSystemID();
	unsigned short get_aAggPartnerSystemPriority();
	unsigned short get_aAggPartnerOperKey();
	void set_aAggAdminState(bool enable);                     // Sets ISS Enabled;
	bool get_aAggAdminState();                                // Gets ISS Enabled;
	bool get_aAggOperState();                                 // Gets ISS Operational;
	void set_aCollectorMaxDelay(unsigned short delay);
	unsigned short get_aAggCollectorMaxDelay();
	bool get_aAggAggregateOrIndividual();
	std::list<unsigned short> get_aAggPortList();             // List of Aggregation Port Identifiers attached to this aggregator  //TODO:  currently list includes selected or attached
	//TODO:   maybe add all statistics counter attributes

	// Version 2 attributes
	void set_aAggPortAlgorithm(portAlgorithms alg);
	portAlgorithms get_aAggPortAlgorithm();
	void set_aAggPartnerAdminPortAlgorithm(portAlgorithms alg);
	portAlgorithms get_aAggPartnerAdminPortAlgorithm();
	void set_aAggPartnerAdminConversationListDigest(std::array<unsigned char, 16> digest);
	std::array<unsigned char, 16> get_aAggPartnerAdminConversationListDigest();
	void set_aAggPartnerAdminConvServiceMappingDigest(std::array<unsigned char, 16> digest);
	std::array<unsigned char, 16> get_aAggPartnerAdminConvServiceMappingDigest();
	void set_aAggConversationAdminLink(unsigned short cid, std::list<unsigned short>linkNumberList);   
	std::list<unsigned short> get_aAggConversationAdminLink(unsigned short cid);
//  void set_aAggAdminServiceConversationMap(???);
//  ??? get_aAggAdminServiceConversationMap();
	void set_aAggAdminDiscardWrongConversation(adminValues DWC);
	adminValues get_aAggAdminDiscardWrongConversation();
	bool get_aAggOperDiscardWrongConversation();    // not in standard

	void set_aAggConversationListDigest(std::array<unsigned char, 16> digest);
	std::array<unsigned char, 16> get_aAggConversationListDigest();


	
	// Not in standard:
	void printConversationAdminLink();
	unsigned short get_conversationLink(unsigned short cid);
	portAlgorithms get_aAggPartnerPortAlgorithm();
	void set_convLinkMap(convLinkMaps choice);
	convLinkMaps get_convLinkMap();
	std::array<unsigned char, 16> get_aAggOperConversationListDigest();
	std::array<unsigned char, 16> get_aAggPartnerOperConversationListDigest();

	/*
	portAlgorithms get_aAggPartnerPortAlgorithm();
	std::array<unsigned char, 16> get_aAggPartnerConversationListDigest();
	std::array<unsigned char, 16> get_aAggPartnerConvServiceMappingDigest();
	std::array<unsigned char, 16> get_aAggConversationListDigest();
	std::array<unsigned char, 16> get_aAggConvServiceMappingDigest();
	/**/
	


};

