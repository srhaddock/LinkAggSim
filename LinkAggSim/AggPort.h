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
#include "Aggregator.h"
#include "Lacpdu.h"

class Lacpdu;


class AggPort : public Aggregator
{
	friend class LinkAgg;

public:
	AggPort(unsigned char version = 2, unsigned short systemNum = 0, unsigned short portNum = 0);
	~AggPort();
	AggPort(AggPort& copySource) = delete;             // Disable copy constructor
	AggPort& operator= (const AggPort&) = delete;      // Disable assignment operator

	shared_ptr<Iss> pIss;

	void reset();
	void timerTick();
	void run(bool singleStep);

private:
	static const int fastPeriodicTime = 1900;
	static const int slowPeriodicTime = 3 * fastPeriodicTime;
	static const int shortTimeout = 3 * fastPeriodicTime;
	static const int longTimeout = 3 * slowPeriodicTime;
	static const int churnDetectionTime = 250;
	static const int AggregateWaitTime = 10;      // MuxSM: Standard says should be 50 ?
	static const int txLimitInterval = fastPeriodicTime;      // TxSM: Standard says should be the same as fastPeriodicTime
	static const int txLimit = 20;               // TxSM: Standard says should be 3
	static const int WTRLimit = 32768;           //         i.e. 0x8000 meaning set MSB of a 16bit value

	enum selectedVals { UNSELECTED, SELECTED, STANDBY };
	selectedVals portSelected;    // Set by Selection;          Reset by: Selection, RxSM;   Used by: MuxSM
	bool PortEnabled;
	bool LacpEnabled;
	bool newPartner;
	bool PortMoved;
	bool ReadyN;                  // Set by MuxSM;              Reset by: MuxSM;             Used by: Selection
	bool Ready;                   // Set by Selection;          Reset by: Selection;         Used by: MuxSM
	bool policy_coupledMuxControl;

	unique_ptr<Frame> pRxLacpFrame;

	unsigned short aggregationPortIdentifier;                    // ifindex  
	portId actorPort;                                            // Port Priority and Port Number (initialized to same as ifindex, but can be changed)
	unsigned short actorAdminPortKey;
	unsigned short actorOperPortKey;
	LacpPortState actorAdminPortState;
	LacpPortState actorOperPortState;
	sysId partnerAdminSystem;
	sysId partnerOperSystem;
	portId partnerAdminPort;
	portId partnerOperPort;
	unsigned short partnerAdminKey;
	unsigned short partnerOperKey;
	LacpPortState partnerAdminPortState;
	LacpPortState partnerOperPortState;
	bool NTT;
	bool LacpTxEnabled;
	unsigned short actorPortAggregatorIdentifier;                 // The aggregatorIdentifier which this Aggregation Port has selected or attached.
	unsigned short actorPortAggregatorIndex;                      // The index of the Aggregator which this Aggregation Port has selected or attached.
	bool individualAggregationPort;
	unsigned long long lacpDestinationAddress;

	// Version 2 variables
	bool enableLongLacpduXmit;
	bool longLacpduXmit;
	int currentWhileLongLacpTimer;
	int waitToRestoreTime;     
	bool wtrRevertiveMode;
	bool wtrRevertOK;
	unsigned short adminLinkNumberID;
	unsigned short LinkNumberID;
	unsigned short partnerLinkNumberID;
	std::bitset<4096> compOperConversationMask;
	std::bitset<4096> portOperConversationMask;
	std::bitset<4096> collectionConversationMask;
	std::bitset<4096> partnerAdminConversationMask;
	std::bitset<4096> partnerOperConversationMask;
	bool changeActorOperDist;
	bool actorPartnerSync;
	bool partnerActorPartnerSync;
	bool actorDWC;
	bool partnerDWC;
	bool partnerPSI;
	bool actorAttached;
	bool updateLocal;

	std::array<unsigned char, 16> actorOperConversationLinkListDigest;
	std::array<unsigned char, 16> partnerOperConversationLinkListDigest;
	std::array<unsigned char, 16> actorOperConversationServiceMappingDigest;
	std::array<unsigned char, 16> partnerOperConversationServiceMappingDigest;
	portAlgorithms actorOperPortAlgorithm;
	portAlgorithms partnerOperPortAlgorithm;

	bool changePartnerOperDistAlg;    // Signal from RxSM or MuxSM that a Partner distribution algorithm value has changed
	bool changeActorAdmin;            // Signals that management has changed an Aggregation Port ActorAdmin value
	bool changePartnerAdmin;          // Signals that management has changed an Aggregation Port PartnerAdmin value
	bool changeAdminLinkNumberID;     // Signals that management has changed the Aggregaton Port Link Number

public:
	/*
	*   802.1AX Standard managed objects access routines
	*/
	unsigned short get_aAggPortID();                              // ifindex
	void set_aAggPortActorSystemID(unsigned long long sysAddr);   // Sets the Address portion of the System ID
	unsigned long long get_aAggPortActorSystemID();               // Returns full 64 bit System ID (including System Priority and Address) ??
	void set_aAggPortActorSystemPriority(unsigned short pri);     // Sets the Priority portion of the System ID
	unsigned short get_aAggPortActorSystemPriority();             // Returns the Priority portion of the System ID
	void set_aAggPortActorAdminKey(unsigned short key);
	unsigned short get_aAggPortActorAdminKey();
	unsigned short get_aAggPortActorOperKey();
	void set_aAggPortPartnerAdminSystemID(unsigned long long sysAddr);
	unsigned long long get_aAggPortPartnerAdminSystemID();
	unsigned long long get_aAggPortPartnerOperSystemID();
	void set_aAggPortPartnerAdminSystemPriority(unsigned short pri);
	unsigned short get_aAggPortPartnerAdminSystemPriority();
	unsigned short get_aAggPortPartnerOperSystemPriority();
	void set_aAggPortPartnerAdminKey(unsigned short key);
	unsigned short get_aAggPortPartnerAdminKey();
	unsigned short get_aAggPortPartnerOperKey();
	unsigned short get_aAggPortSelectedAggID();
	unsigned short get_aAggPortAttachedAggID();
	void set_aAggPortActorPort(unsigned short portNum);
	unsigned short get_aAggPortActorPort();
	void set_aAggPortActorPortPriority(unsigned short pri);
	unsigned short get_aAggPortActorPortPriority();
	void set_aAggPortPartnerAdminPort(unsigned short portNum);
	unsigned short get_aAggPortPartnerAdminPort();
	unsigned short get_aAggPortPartnerOperPort();
	void set_aAggPortPartnerAdminPortPriority(unsigned short pri);
	unsigned short get_aAggPortPartnerAdminPortPriority();
	unsigned short get_aAggPortPartnerOperPortPriority();
	void set_aAggPortActorAdminState(unsigned char state);
	unsigned char get_aAggPortActorAdminState();
	LacpPortState get_aAggPortActorOperState();
	void set_aAggPortPartnerAdminState(LacpPortState state);
	LacpPortState get_aAggPortPartnerAdminState();
	LacpPortState get_aAggPortPartnerOperState();
	bool get_aAggPortAggregateOrIndividual();
	void set_aAggPortLinkNumberID(unsigned short link);
	unsigned short get_aAggPortLinkNumberID();
	unsigned short get_aAggPortOperLinkNumberID();    // not in standard
	void set_aAggPortWTRTime(unsigned short wtr);
	unsigned short get_aAggPortWTRTime();
	void set_aAggPortProtocolDA(unsigned long long addr);
	unsigned long long get_aAggPortProtocolDA();

	void set_aAggPortEnableLongPDUXmit(bool enable);
	bool get_aAggPortEnableLongPDUXmit();

	void set_aAggPortPartnerAdminConversationMask(std::bitset<4096> mask);  // No managed object for this in standard
	std::bitset<4096> get_aAggPortPartnerAdminConversationMask();

private:

	static class LacpRxSM
	{
	public:

		enum RxSmStates { NO_STATE, INITIALIZE, PORT_DISABLED, LACP_DISABLED, EXPIRED, DEFAULTED, CURRENT };

		/*
		static void resetRxSM(AggPort& port);
		static void timerTickRxSM(AggPort& port);
		static int runRxSM(AggPort& port, bool singleStep);
		/**/
		static void reset(AggPort& port);
		static void timerTick(AggPort& port);
		static int run(AggPort& port, bool singleStep);

	private:
		static bool stepRxSM(AggPort& port);
		static RxSmStates enterInitialize(AggPort& port);
		static RxSmStates enterPortDisabled(AggPort& port);
		static RxSmStates enterLacpDisabled(AggPort& port);
		static RxSmStates enterExpired(AggPort& port);
		static RxSmStates enterDefaulted(AggPort& port);
		static RxSmStates enterCurrent(AggPort& port, const Lacpdu& rxLacpdu);
		static void updateDefaultSelected(AggPort& port);
		static void recordDefault(AggPort& port);
		static void updateSelected(AggPort& port, const Lacpdu& rxLacpdu);
		static void updateNTT(AggPort& port, const Lacpdu& rxLacpdu);
		static void recordPdu(AggPort& port, const Lacpdu& rxLacpdu);
//		static void recordPartnerPortState(AggPort& port, aggPortState inputState);
		static bool comparePartnerViewOfActor(AggPort& port, const Lacpdu& rxLacpdu);
		static bool compareActorViewOfPartner(AggPort& port, const Lacpdu& rxLacpdu);
		static void actorAdminChange(AggPort& port);                              // Compare actor admin values to oper values and update oper values
		//  LACPv2:
		static void recordVersion2Defaults(AggPort& port);
		static void recordPortAlgorithmTlv(AggPort& port, const Lacpdu& rxLacpdu);
		static void recordConversationPortDigestTlv(AggPort& port, const Lacpdu& rxLacpdu);
		static void recordConversationServiceMappingDigestTlv(AggPort& port, const Lacpdu& rxLacpdu);
		static void recordReceivedConversationMaskTlv(AggPort& port, const Lacpdu& rxLacpdu);
		static void updateLinkNumber(AggPort& port);
	};

	LacpRxSM::RxSmStates RxSmState;
	int currentWhileTimer;


	static class LacpMuxSM
	{
	public:

		enum MuxSmStates { NO_STATE, DETACHED, WAITING, ATTACHED, COLLECTING, DISTRIBUTING, COLL_DIST, ATTACH, ATTACHED_WTR };

		/*
		static void resetMuxSM(AggPort& port);
		static void timerTickMuxSM(AggPort& port);
		static int runMuxSM(AggPort& port, bool singleStep);
		/**/
		static void reset(AggPort& port);
		static void timerTick(AggPort& port);
		static int run(AggPort& port, bool singleStep);

	private:
		static bool stepMuxSM(AggPort& port);
		static MuxSmStates enterDetached(AggPort& port);
		static MuxSmStates enterWaiting(AggPort& port);
		static MuxSmStates enterAttached(AggPort& port);
		static MuxSmStates enterCollecting(AggPort& port);
		static MuxSmStates enterDistributing(AggPort& port);
		static MuxSmStates enterCollDist(AggPort& port);
		static MuxSmStates enterAttach(AggPort& port);
		static MuxSmStates enterAttachedWtr(AggPort& port);
		static void attachMuxToAggregator(AggPort& port);
		static void detachMuxFromAggregator(AggPort& port);
		static void enableCollecting(AggPort& port);
		static void disableCollecting(AggPort& port);
		static void enableDistributing(AggPort& port);
		static void disableDistributing(AggPort& port);
		static void enableCollectingDistributing(AggPort& port);
		static void disableCollectingDistributing(AggPort& port);
	};

	LacpMuxSM::MuxSmStates MuxSmState;
	int waitWhileTimer;
	int waitToRestoreTimer;

	static class LacpPeriodicSM
	{
	public:

		enum PerSmStates { NO_STATE, NO_PERIODIC, FAST_PERIODIC, SLOW_PERIODIC, PERIODIC_TX };

		/*
		static void resetPeriodicSM(AggPort& port);
		static void timerTickPeriodicSM(AggPort& port);
		static int runPeriodicSM(AggPort& port, bool singleStep);
		/**/
		static void reset(AggPort& port);
		static void timerTick(AggPort& port);
		static int run(AggPort& port, bool singleStep);

	private:
		static bool stepPeriodicSM(AggPort& port);
		static PerSmStates enterNoPeriodic(AggPort& port);
		static PerSmStates enterFastPeriodic(AggPort& port);
		static PerSmStates enterSlowPeriodic(AggPort& port);
		static PerSmStates enterPeriodicTx(AggPort& port);
	};

	LacpPeriodicSM::PerSmStates PerSmState;
	int periodicTimer;



	static class LacpTxSM
	{
	public:

		enum TxSmStates { NO_STATE, NO_TX, RESET_TX_COUNT, TX_LACPDU };

		/*
		static void resetTxSM(AggPort& port);
		static void timerTickTxSM(AggPort& port);
		static int runTxSM(AggPort& port, bool singleStep);
		/**/
		static void reset(AggPort& port);
		static void timerTick(AggPort& port);
		static int run(AggPort& port, bool singleStep);

	private:

		static bool stepTxSM(AggPort& port);
		static TxSmStates enterNoTx(AggPort& port);
		static TxSmStates enterResetTxCount(AggPort& port);
		static TxSmStates enterTxLacpdu(AggPort& port);
		static bool transmitLacpdu(AggPort& port);
		static void prepareLacpdu(AggPort& port, Lacpdu& myLacpdu);
	};

	LacpTxSM::TxSmStates TxSmState;
	int txCount;
	int txLimitTimer;



};

