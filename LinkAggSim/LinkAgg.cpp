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
#include "LinkAgg.h"


LinkAgg::LinkAgg(unsigned short device, unsigned char version)
	: Component(ComponentTypes::LINK_AGG), devNum(device), LacpVersion(version)
{
//	cout << "LagShim Constructor called." << endl;
}


LinkAgg::~LinkAgg()
{
	pAggregators.clear();
	pAggPorts.clear();
//	cout << "LagShim Destructor called." << endl;
}

void LinkAgg::reset()
{
	for (auto& pPort : pAggPorts)              // For each Aggregation Port:
	{
		pPort->reset();

		if (SimLog::Debug > 12)
		{
			AggPort& port = *pPort;
			SimLog::logFile << "Time " << SimLog::Time << ":  Sys " << hex << port.get_aAggActorSystemID()
				<< ":  Port " << hex << port.get_aAggPortActorPort() << dec
				<< ": RxSM " << port.RxSmState << " MuxSM " << port.MuxSmState
				<< " PerSM " << port.PerSmState << " TxSM " << port.TxSmState
				<< " PerT " << port.periodicTimer << " RxT " << port.currentWhileTimer
				<< " Up " << port.pIss->getOperational() << endl;
		}
	}
	for (auto& pAgg : pAggregators)
	{
		pAgg->reset();
	}
}


void LinkAgg::timerTick()
{
	if (!suspended)
	{
		for (auto& pPort : pAggPorts)              // For each Aggregation Port:
		{
			pPort->timerTick();
		}
	}
}


void LinkAgg::run(bool singleStep)
{
	unsigned short nPorts = pAggPorts.size();
	unique_ptr<Frame> pTempFrame = nullptr;

	if (!suspended)       
	{

		// Collect
		for (unsigned short i = 0; i < nPorts; i++)              // For each Aggregation Port:
		{
			pTempFrame = move(pAggPorts[i]->pIss->Indication());   // Get ingress frame, if available, from ISS
			if (pTempFrame)                                        // If there is an ingress frame
			{
				if (SimLog::Debug > 5)
				{
					SimLog::logFile << "Time " << SimLog::Time << "    Lag in Device:Port " << hex << pAggPorts[i]->actorSystem.addrMid
						<< ":" << pAggPorts[i]->actorPort.num << " receiving frame ";
					pTempFrame->PrintFrameHeader();
					SimLog::logFile << dec << endl;
				}
				if ((pTempFrame->MacDA == pAggPorts[i]->lacpDestinationAddress) &&   // if frame has proper DA and contains an LACPDU
					(pTempFrame->getNextEtherType() == SlowProtocolsEthertype) && (pTempFrame->getNextSubType() == LacpduSubType))
				{
					pAggPorts[i]->pRxLacpFrame = move(pTempFrame);                   // then pass to LACP RxSM
				}
				else if (collectFrame(*pAggPorts[i], *pTempFrame))                   // else verify this frame can be collected through this AggPort
				{															         //    and send it up stack through the selected Aggregator
					pAggregators[pAggPorts[i]->actorPortAggregatorIndex]->indications.push(move(pTempFrame));
				}
			}
		}

		// Check for administrative changes to Aggregator configuration
		LinkAgg::LacpSelection::adminAggregatorUpdate(pAggPorts, pAggregators);

		// Run state machines
		int transitions = 0;
		for (unsigned short i = 0; i < nPorts; i++)              // For each Aggregation Port:
		{
//			transitions += AggPort::LacpRxSM::runRxSM(*pAggPorts[i], true);
//			transitions += AggPort::LacpMuxSM::runMuxSM(*pAggPorts[i], true);
			transitions += AggPort::LacpRxSM::run(*pAggPorts[i], true);
			transitions += AggPort::LacpMuxSM::run(*pAggPorts[i], true);
		}
//		updateAggregatorStatus();
		runCSDC();

		//TODO:  Can you aggregate ports with different LACP versions?  Do you need to test for it?  Does partner LACP version need to be in DRCPDU?
		LinkAgg::LacpSelection::runSelection(pAggPorts, pAggregators);

		for (unsigned short i = 0; i < nPorts; i++)              // For each Aggregation Port:
		{
//			transitions += AggPort::LacpPeriodicSM::runPeriodicSM(*pAggPorts[i], true);
			transitions += AggPort::LacpPeriodicSM::run(*pAggPorts[i], true);

			if ((transitions && (SimLog::Debug > 8)) || (SimLog::Debug > 12))
			{
				SimLog::logFile << "Time " << SimLog::Time << ":  Sys " << hex << pAggPorts[i]->get_aAggActorSystemID()
					<< ":  Port " << hex << pAggPorts[i]->get_aAggPortActorPort() << dec
					<< ": RxSM " << pAggPorts[i]->RxSmState << " MuxSM " << pAggPorts[i]->MuxSmState
					<< " PerSM " << pAggPorts[i]->PerSmState << " TxSM " << pAggPorts[i]->TxSmState
					<< " PerT " << pAggPorts[i]->periodicTimer << " RxT " << pAggPorts[i]->currentWhileTimer
					<< " WtrT " << pAggPorts[i]->waitToRestoreTimer
					<< " Up " << pAggPorts[i]->pIss->getOperational()
					<< " Sel " << pAggPorts[i]->portSelected
					<< " Dist " << (bool)(pAggPorts[i]->actorOperPortState.distributing)
					<< " NTT " << pAggPorts[i]->NTT
					<< endl;

			}

//			transitions += AggPort::LacpTxSM::runTxSM(*pAggPorts[i], true);
			transitions += AggPort::LacpTxSM::run(*pAggPorts[i], true);
		}

		// Distribute
		for (unsigned short i = 0; i < nPorts; i++)              // For each Aggregator:
		{
			if (pAggregators[i]->getEnabled() && !pAggregators[i]->requests.empty())  // If there is an egress frame at Aggregator
			{
				pTempFrame = move(pAggregators[i]->requests.front());   // Get egress frame
				pAggregators[i]->requests.pop();
				if (pTempFrame)                                         // Shouldn't be necessary since already tested that there was a frame
				{
					if (SimLog::Debug > 5)
					{
						SimLog::logFile << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << pAggregators[i]->actorSystem.addrMid
							<< ":" << pAggregators[i]->aggregatorIdentifier << " transmitting frame ";
						pTempFrame->PrintFrameHeader();
						SimLog::logFile << dec << endl;
					}
					shared_ptr<AggPort> pEgressPort = distributeFrame(*pAggregators[i], *pTempFrame);   // get egress Aggregation Port
					if (pEgressPort)
					{
						pEgressPort->pIss->Request(move(pTempFrame));     //    Send frame through egress Aggregation Port
					}
				}
			}
		}


	}
}


void LinkAgg::resetCSDC()
{
	for (auto pAgg : pAggregators)            // May want to wrap this in a while loop and repeat until no flags set
	{
		Aggregator& agg = *pAgg;

		agg.partnerPortAlgorithm = agg.partnerAdminPortAlgorithm;
		agg.partnerConversationLinkListDigest = agg.partnerAdminConversationLinkListDigest;
		agg.partnerConversationServiceMappingDigest = agg.partnerAdminConversationServiceMappingDigest;

		agg.differentPortAlgorithms = false;
		agg.differentConversationServiceDigests = false;
		agg.differentPortConversationDigests = false;

		agg.operDiscardWrongConversation = (agg.adminDiscardWrongConversation == adminValues::FORCE_TRUE);

		agg.activeLagLinks.clear();
		agg.conversationLinkVector.fill(0);

		agg.changeActorDistAlg = false;
		agg.changeConvLinkList = false;
		agg.changePartnerAdminDistAlg = false;
		agg.changeDistAlg = false;
		agg.changeLinkState = false;
		agg.changeAggregationLinks = false;
		agg.changeCSDC = false;
	}
	//TODO: verify that some per-port machine resets all conversation masks, link number, local copies of dwc.
}

void LinkAgg::runCSDC()
{
	for (auto pPort : pAggPorts)           // Walk through all Aggregation Ports
	{
		updatePartnerDistributionAlgorithm(*pPort);
	}
	for (auto pAgg : pAggregators)            // May want to wrap this in a while loop and repeat until no flags set
	{
		updateMask(*pAgg);
	}
}

void LinkAgg::updateMask(Aggregator& agg)
{
	int loop = 0;
	do
	{
		if (agg.changeActorDistAlg)
		{
			debugUpdateMask(agg, "updateActorDistributionAlgorithm");
			agg.changeActorDistAlg = false;
			updateActorDistributionAlgorithm(agg);
		}
		else if (agg.changePartnerAdminDistAlg)
		{
			debugUpdateMask(agg, "updatePartnerAdminDistributionAlgorithm");
			agg.changePartnerAdminDistAlg = false;
			updatePartnerAdminDistributionAlgorithm(agg);
		}
		else if (agg.changeDistAlg)
		{
			debugUpdateMask(agg, "compareDistributionAlgorithms");
			agg.changeDistAlg = false;
			compareDistributionAlgorithms(agg);
		}
		else if (agg.changeLinkState)
		{
			debugUpdateMask(agg, "updateActiveLinks");
			agg.changeLinkState = false;
			updateActiveLinks(agg);
		}
		else if (agg.changeAggregationLinks)
		{
			debugUpdateMask(agg, "updateConversationPortVector");
			agg.changeAggregationLinks = false;
			updateConversationPortVector(agg);
		}
		else if (agg.changeCSDC)
		{
			debugUpdateMask(agg, "updateConversationMasks");
			agg.changeCSDC = false;
			updateConversationMasks(agg);
		}
		else if (agg.changeDistributing)
		{
			debugUpdateMask(agg, "updateAggregatorOperational");
			agg.changeDistributing = false;
			updateAggregatorOperational(agg);
		}

		loop++;
	} while (loop < 10);

}

void LinkAgg::debugUpdateMask(Aggregator& thisAgg, std::string routine)
{
	if (SimLog::Debug > 7)
	{
		SimLog::logFile << "Time " << SimLog::Time
			<< ":   Device:Aggregator " << hex << thisAgg.actorSystem.addrMid << ":" << thisAgg.aggregatorIdentifier
			<< " entering updateMask = " << routine
			<< dec << endl;
	}
}

void LinkAgg::updatePartnerDistributionAlgorithm(AggPort& port)
{
	Aggregator& agg = *(pAggregators[port.actorPortAggregatorIndex]);

	agg.changeDistributing |= port.changeActorDistributing;
	port.changeActorDistributing = false;

	agg.changeLinkState |= port.changePortLinkState;
	port.changePortLinkState = false;

	//  If the partner distribution algorithm parameters have changed for any port that is COLLECTING on an Aggregator,
	//     update the Aggregator's distribution algorithm parameters.
	//     Assumes that the portOper... distribution algorithm parameters are null for any port that expects the 
	//     Aggregator to use the partnerAdmin... values (including port that is DEFAULTED, is version 1 (?), 
	//     or did not receive the proper V2 TLVs from the partner.

	if ((SimLog::Debug > 7) && port.changePartnerOperDistAlg)
	{
		SimLog::logFile << "Time " << SimLog::Time
			<< ":   Device:Aggregator " << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
			<< " entering updateMask = updatePartnerDistributionAlgorithm"
			<< dec << endl;
	}

	if (port.actorOperPortState.collecting && (port.portSelected == AggPort::selectedVals::SELECTED))
		// Need to test partner.sync and/or portOperational as well?
	{
		if ((port.changePartnerOperDistAlg) &&
			(port.partnerOperPortAlgorithm == Aggregator::portAlgorithms::NONE))
			// partnerOperPortAlgorithm will only be NONE if no LACPDUv2 exchange with partner, so use partnerAdmin values.
		{
			agg.partnerPortAlgorithm = agg.partnerAdminPortAlgorithm;
			agg.partnerConversationLinkListDigest = agg.partnerAdminConversationLinkListDigest;
			agg.partnerConversationServiceMappingDigest = agg.partnerAdminConversationServiceMappingDigest;
			agg.changeDistAlg = true;
			// Could optimize further processing by only setting change flag if new values differ from old values.
			if ((SimLog::Debug > 3))
			{
				SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
					<< " setting default partner algorithm on Aggregator " << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
			}
		}
		else if (port.changePartnerOperDistAlg)
		{
			agg.partnerPortAlgorithm = port.partnerOperPortAlgorithm;
			agg.partnerConversationLinkListDigest = port.partnerOperConversationLinkListDigest;
			agg.partnerConversationServiceMappingDigest = port.partnerOperConversationServiceMappingDigest;
			agg.changeDistAlg = true;
			// Could optimize further processing by only setting change flag if new values differ from old values.
			if ((SimLog::Debug > 3))
			{
				SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
					<< " setting partner's algorithm on Aggregator " << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
			}
		}
	}
	port.changePartnerOperDistAlg = false;

}

void LinkAgg::updateActorDistributionAlgorithm(Aggregator& thisAgg)
{
	for (auto lagPortIndex : thisAgg.lagPorts)      //   walk list of Aggregation Ports on the LAG
	{
		AggPort& port = *(pAggPorts[lagPortIndex]);

		port.actorOperPortAlgorithm = thisAgg.actorPortAlgorithm;
		port.actorOperConversationLinkListDigest = thisAgg.actorConversationLinkListDigest;
		port.actorOperConversationServiceMappingDigest = thisAgg.actorConversationServiceMappingDigest;
		if (port.actorOperPortState.sync)   //     If the AggPort is attached to Aggregator
		{
			port.NTT = true;                //         then communicate change to partner
			if (SimLog::Debug > 6)
			{
				SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << port.actorSystem.addrMid
					<< ":" << port.actorPort.num << " NTT: Change actor dist algorithm on Link " << dec << port.LinkNumberID
					<< hex << "  Device:Aggregator " << thisAgg.actorSystem.addrMid << ":" << port.actorPortAggregatorIdentifier
					<< dec << endl;
			}
		}
		if (port.actorOperPortState.collecting && (port.portSelected == AggPort::selectedVals::SELECTED))
		{
			thisAgg.changeDistAlg |= true;
			thisAgg.changeAggregationLinks |= thisAgg.changeConvLinkList;
		}
		thisAgg.changeConvLinkList = false;
	}
}

void LinkAgg::updatePartnerAdminDistributionAlgorithm(Aggregator& thisAgg)
{
	for (auto lagPortIndex : thisAgg.lagPorts)      //   walk list of Aggregation Ports on the LAG
	{
		AggPort& port = *(pAggPorts[lagPortIndex]);

		if (port.actorOperPortState.collecting && (port.portSelected == AggPort::selectedVals::SELECTED) &&
			(port.actorOperPortState.defaulted || (port.partnerLacpVersion == 1)))
		{
			thisAgg.partnerPortAlgorithm = thisAgg.partnerAdminPortAlgorithm;
			thisAgg.partnerConversationServiceMappingDigest = thisAgg.partnerAdminConversationServiceMappingDigest;
			thisAgg.partnerConversationLinkListDigest = thisAgg.partnerAdminConversationLinkListDigest;
			thisAgg.changeDistAlg |= true;
		}
	}
}

void LinkAgg::compareDistributionAlgorithms(Aggregator& thisAgg)
{
	bool oldDifferPortConvDigest = thisAgg.differentPortConversationDigests;
	bool oldDifferentDistAlg = (thisAgg.differentPortAlgorithms ||
		                        thisAgg.differentPortConversationDigests ||
	                         	thisAgg.differentConversationServiceDigests);	

	thisAgg.differentPortAlgorithms = ((thisAgg.actorPortAlgorithm != thisAgg.partnerPortAlgorithm) ||
		(thisAgg.actorPortAlgorithm == Aggregator::portAlgorithms::UNSPECIFIED) ||
		(thisAgg.partnerPortAlgorithm == Aggregator::portAlgorithms::UNSPECIFIED));  // actually, testing partner unspecified is redundant
//  temporarily overwriting without check for UNSPECIFIED, since this is the easiest way to make actor and partner match by default
//	thisAgg.differentPortAlgorithms = (thisAgg.actorPortAlgorithm != thisAgg.partnerPortAlgorithm);

	thisAgg.differentPortConversationDigests = (thisAgg.actorConversationLinkListDigest != thisAgg.partnerConversationLinkListDigest);
	thisAgg.differentConversationServiceDigests = (thisAgg.actorConversationServiceMappingDigest != thisAgg.partnerConversationServiceMappingDigest);
	bool differentDistAlg = (thisAgg.differentPortAlgorithms || 
		                     thisAgg.differentPortConversationDigests || 
							 thisAgg.differentConversationServiceDigests);

	bool partnerMightWin = (thisAgg.actorSystem.id >= thisAgg.partnerSystem.id);
	// can't compare Port IDs  here, so set flag if partner System ID <= actor System ID
	thisAgg.changeLinkState |= ((oldDifferentDistAlg != differentDistAlg) && partnerMightWin);
	// standard (currently) says to use differPortConversationDigests, but no harm is using the 'OR' of all differXxx

	bool oldDWC = thisAgg.operDiscardWrongConversation;
	thisAgg.operDiscardWrongConversation = ((thisAgg.adminDiscardWrongConversation == adminValues::FORCE_TRUE) ||
		((thisAgg.adminDiscardWrongConversation == adminValues::AUTO) && !differentDistAlg));
	thisAgg.changeCSDC |= ((oldDWC != thisAgg.operDiscardWrongConversation) && !thisAgg.activeLagLinks.empty());


	//TODO:  report to management if any "differentXXX" flags are set
	//TODO:  Set differentPortConversationDigests if actor and partner Link Numbers don't match 
	//         (are they guaranteed to be stable by the time actor.sync and partner.sync?)
	//         (will changes to actor or partner set the change distribution algorithms flag?)
	if (SimLog::Debug > 6)
	{
		SimLog::logFile << "Time " << SimLog::Time
			<< ":   Device:Aggregator " << hex << thisAgg.actorSystem.addrMid << ":" << thisAgg.aggregatorIdentifier
			<< " differ-Alg/CDigest/SDigest = " << thisAgg.differentPortAlgorithms << "/"
			<< thisAgg.differentPortConversationDigests << "/" << thisAgg.differentConversationServiceDigests
			<< "  adminDWC = " << thisAgg.adminDiscardWrongConversation << "  DWC = " << thisAgg.operDiscardWrongConversation
			<< dec << endl;
	}

}

void LinkAgg::updateActiveLinks(Aggregator& thisAgg)
{
	std::list<unsigned short> newActiveLinkList;  // Build a new list of the link numbers active on the LAG
	for (auto lagPortIndex : thisAgg.lagPorts)      //   from list of Aggregation Ports on the LAG
	{
		AggPort& port = *(pAggPorts[lagPortIndex]);

		bool differentDistAlg = (thisAgg.differentPortAlgorithms ||
			thisAgg.differentPortConversationDigests ||
			thisAgg.differentConversationServiceDigests);

		bool partnerWins = ((thisAgg.actorSystem.id > thisAgg.partnerSystem.id) ||
			                ((thisAgg.actorSystem.id == thisAgg.partnerSystem.id) && (port.actorPort.id > port.partnerOperPort.id)));
			                // Compare Port IDs as well as System IDs, so comparison works with loopback

		if (port.actorOperPortState.collecting && (port.portSelected == AggPort::selectedVals::SELECTED))
		{
			unsigned short oldLinkNumberID = port.LinkNumberID;
			if (!differentDistAlg && partnerWins)
				port.LinkNumberID = port.partnerLinkNumberID;
			else
				port.LinkNumberID = port.adminLinkNumberID;

			newActiveLinkList.push_back(port.LinkNumberID);      // Add its Link Number to the new active link list  

			if (oldLinkNumberID != port.LinkNumberID)
			{
				port.NTT = true;
				if (SimLog::Debug > 6)
				{
					SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << port.actorSystem.addrMid
						<< ":" << port.actorPort.num << " NTT: Link Number changing to  " << dec << port.LinkNumberID << endl;
				}
			}
		}
	}

	newActiveLinkList.sort();                       // Put list of link numbers in ascending order 
	if (thisAgg.activeLagLinks != newActiveLinkList)  // If the active links have changed
	{
		thisAgg.activeLagLinks = newActiveLinkList;   // Save the new list of active link numbers
		thisAgg.changeAggregationLinks |= true;
		//TODO:  For DRNI have to set a flag to let DRF know that list of active ports may have changed.
	}
}


void LinkAgg::updateConversationPortVector(Aggregator& thisAgg)
{
	if (thisAgg.activeLagLinks.empty())         // If there are no links active on the LAG 
	{
		thisAgg.conversationLinkVector.fill(0);          // Clear all Conversation ID to Link associations; 
		// Don't need to update conversation masks because disableCollectingDistributing will clear masks when links go inactive
	}
	else                                        // Otherwise there are active links on the LAG
	{
		std::array<unsigned short, 4096> oldConvLinkVector = thisAgg.conversationLinkVector;

		updateConversationLinkVector(thisAgg);           // Create new Conversation ID to Link associations

		thisAgg.changeCSDC |= (thisAgg.conversationLinkVector != oldConvLinkVector);
		//TODO:  Would be better to have updateConversationLinkVector build a new vector, then copy it into conversationLinkVector if there is a change
	}

	if (SimLog::Debug > 4)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << thisAgg.actorSystem.addrMid
			<< ":" << thisAgg.aggregatorIdentifier << dec << " ConvLinkMap = ";
		for (int k = 0; k < 16; k++) SimLog::logFile << "  " << thisAgg.conversationLinkVector[k];
		SimLog::logFile << endl;
	}
}

void LinkAgg::updateConversationMasks(Aggregator& thisAgg)
{
	for (auto lagPortIndex : thisAgg.lagPorts)      //   from list of Aggregation Ports on the LAG
	{
		AggPort& port = *(pAggPorts[lagPortIndex]);

		port.actorDWC = thisAgg.operDiscardWrongConversation;  // Update port's copy of DWC

		for (int convID = 0; convID < 4096; convID++)   //    for all conversation ID values.
		{
			bool passConvID = (port.actorOperPortState.collecting && (port.portSelected == AggPort::selectedVals::SELECTED) &&
				(thisAgg.conversationLinkVector[convID] == port.LinkNumberID) && (port.LinkNumberID > 0));
				// Determine if link is active and the conversation ID maps to this link number

			port.portOperConversationMask[convID] = passConvID;       // If so then distribute this conversation ID. 
		}

		if (SimLog::Debug > 4)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << port.actorSystem.addrMid
				<< ":" << port.actorPort.num << " Link " << dec << port.LinkNumberID << "   ConvMask = ";
			for (int k = 0; k < 16; k++) SimLog::logFile << "  " << port.portOperConversationMask[k];
			SimLog::logFile << endl;
		}

		// Turn off distribution and collection if convID is moving between links.
		port.distributionConversationMask = port.distributionConversationMask & port.portOperConversationMask;  
		port.collectionConversationMask = port.collectionConversationMask & port.portOperConversationMask;      
	}

	//  Before just cleared distribution/collection masks for Conversation IDs where the port's link number did not match the conversationLinkVector.
	//     Now want to set the distribution/collection mask for Conversation IDs where the port's link number does match.
	//     This assures "break-before-make" behavior so no Conversation ID bit is ever set in the collection mask for more than one port.
	for (auto lagPortIndex : thisAgg.lagPorts)      //   from list of Aggregation Ports on the LAG
	{
		AggPort& port = *(pAggPorts[lagPortIndex]);

		port.distributionConversationMask = port.portOperConversationMask;         // Set distribution mask to the new value

		//TODO:  I think I need more qualification here.  Don't want to collectionConversationMask.set() if not collecting (and SELECTED and partner.sync?)
		if (thisAgg.operDiscardWrongConversation)                                  // If enforcing Conversation-sensitive Collection
			port.collectionConversationMask = port.portOperConversationMask;       //    then make collection mask the same as the distribution mask
		else
			port.collectionConversationMask.set();                                 // Otherwise make collection mask true for all conversation IDs

		//TODO:  Temporarily stuff this in at this point.  In 802.1AX-REV/d0.1 still have editor's notes regarding partnerAdminConversationMask and  ActPar_Sync.
		//   Not going to bother updating this code until resolve those issues.
		if (!port.actorPartnerSync || !port.partnerActorPartnerSync)
		{
			port.currentWhileLongLacpTimer = 50;
			port.longLacpduXmit = (port.enableLongLacpduXmit && (port.partnerLacpVersion >= 2));
		}
		if (port.currentWhileLongLacpTimer == 0)
		{
			port.longLacpduXmit = false;
		}

	}
}

void LinkAgg::updateAggregatorOperational(Aggregator& thisAgg)
{
	bool AggregatorUp = false;

	for (auto lagPortIndex : thisAgg.lagPorts)      //   from list of Aggregation Ports on the LAG
	{
		AggPort& port = *(pAggPorts[lagPortIndex]);

		AggregatorUp |= port.actorOperPortState.distributing;
	}

	if (!AggregatorUp && thisAgg.operational)                    // if Aggregator going down
	{
		thisAgg.operational = false;                // Set Aggregator ISS to not operational
		//TODO:  flush aggregator request queue?

		cout << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << thisAgg.actorSystem.addrMid << ":" << thisAgg.aggregatorIdentifier
			<< " is DOWN" << dec << endl;
		if (SimLog::Time > 0)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << thisAgg.actorSystem.addrMid << ":" << thisAgg.aggregatorIdentifier
				<< " is DOWN" << dec << endl;
		}
	}
	if (AggregatorUp && !thisAgg.operational)                    // if Aggregator going down
	{
		thisAgg.operational = true;                 // Set Aggregator ISS is operational

		cout << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << thisAgg.actorSystem.addrMid << ":" << thisAgg.aggregatorIdentifier
			<< " is UP with Link numbers:  " << dec;
		for (auto link : thisAgg.activeLagLinks)
			cout << link << "  ";
		cout << dec << endl;
		if (SimLog::Time > 0)
		{
			SimLog::logFile << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << thisAgg.actorSystem.addrMid << ":" << thisAgg.aggregatorIdentifier
				<< " is UP with Link numbers:  " << dec;
			for (auto link : thisAgg.activeLagLinks)
				SimLog::logFile << link << "  ";
			SimLog::logFile << dec << endl;
		}
	}
}


void LinkAgg::updateConversationLinkVector(Aggregator& thisAgg)
{
	switch (thisAgg.selectedconvLinkMap)
	{
	case Aggregator::convLinkMaps::ADMIN_TABLE:
		linkMap_AdminTable(thisAgg);
		break;
	case Aggregator::convLinkMaps::ACTIVE_STANDBY:
		linkMap_ActiveStandby(thisAgg);
		break;
	case Aggregator::convLinkMaps::EVEN_ODD:
		linkMap_EvenOdd(thisAgg);
		break;
	case Aggregator::convLinkMaps::EIGHT_LINK_SPREAD:
		linkMap_EightLinkSpread(thisAgg);
		break;
	}
}

void LinkAgg::linkMap_EightLinkSpread(Aggregator& thisAgg)
{
	const int nLinks = 8;  // Must be a power of two
	const int nRows = 8;   // Must be a power of two
	const unsigned short convLinkPriorityList[nRows][nLinks] =
	{
		{ 0, 3, 6, 5, 1, 2, 7, 4 },
		{ 1, 2, 7, 4, 0, 3, 6, 5 },
		{ 2, 5, 0, 7, 3, 4, 1, 6 },
		{ 3, 4, 1, 6, 2, 5, 0, 7 },
		{ 4, 7, 2, 1, 5, 6, 3, 0 },
		{ 5, 6, 3, 0, 4, 7, 2, 1 },
		{ 6, 1, 4, 3, 7, 0, 5, 2 },
		{ 7, 0, 5, 2, 6, 1, 4, 3 }
	};

	std::array<unsigned short, nLinks> activeLinkNumbers = { 0 };

	// First, put the Aggregator's activeLagLinks list to an array, nlinks long, where the position in the array
	//   is determined by the active Link Number modulo nLinks.  If multiple active Link Numbers map to the same
	//   position in the array, the lowest of those Link Numbers will be stored in the array.  If no active Link
	//   Numbers map to a position in the array then the value at that position will be 0.
	for (auto& link : thisAgg.activeLagLinks)
	{
		unsigned short baseLink = link % nLinks;
		if ((activeLinkNumbers[baseLink] == 0) || (link < activeLinkNumbers[baseLink]))
			activeLinkNumbers[baseLink] = link;
	}


	// Then, create the first 8 entries in the conversation-to-link map from the first 8 rows of the Link Priority List table
	for (int row = 0; row < nRows; row++)
	{
		thisAgg.conversationLinkVector[row] = 0;                                       // Link Number 0 is default if no Link Numbers in row are active
		for (int j = 0; j < nLinks; j++)                                            // Search a row of the table
		{                                                                           //   for the first Link Number that is active.
			if ((convLinkPriorityList[row][j] < activeLinkNumbers.size()) && activeLinkNumbers[convLinkPriorityList[row][j]])
			{
				thisAgg.conversationLinkVector[row] = activeLinkNumbers[convLinkPriorityList[row][j]];    //   Copy that Link Number into map
				break;                                                              //       and quit the for loop.
			}
		}
	}

	// Finally, repeat first 8 entries through the rest of the conversation-to-link map
	for (int row = nRows; row < 4096; row++)
	{
		thisAgg.conversationLinkVector[row] = thisAgg.conversationLinkVector[row % nRows];
	}
}
/**/

void LinkAgg::linkMap_EvenOdd(Aggregator& thisAgg)
{
	if (thisAgg.activeLagLinks.empty())              // If there are no active links
	{
		thisAgg.conversationLinkVector.fill(0);         //    then the conversationLinkVector is all zero
	} 
	else                                             // Otherwise ...
	{
		unsigned short evenLink = thisAgg.activeLagLinks.front();    // Even Conversation IDs go to lowest LinkNumberID.
		unsigned short oddLink = thisAgg.activeLagLinks.back();      // Odd Conversation IDs go to highest LinkNumberID.  (Other links are standby)
		for (int j = 0; j < 4096; j += 2)
		{
			thisAgg.conversationLinkVector[j] = evenLink;
			thisAgg.conversationLinkVector[j + 1] = oddLink;
		}
	}
}

void LinkAgg::linkMap_ActiveStandby(Aggregator& thisAgg)
{
	if (thisAgg.activeLagLinks.empty())              // If there are no active links
	{
		thisAgg.conversationLinkVector.fill(0);         //    then the conversationLinkVector is all zero
	}
	else                                             // Otherwise ...
	{
		// Active link will be the lowest LinkNumberID.  All others will be Standby.
		unsigned short activeLink = thisAgg.activeLagLinks.front();
		thisAgg.conversationLinkVector.fill(activeLink);
	}
}

void LinkAgg::linkMap_AdminTable(Aggregator& thisAgg)
{
	thisAgg.conversationLinkVector.fill(0);                           // Start with  conversationLinkVector is all zero
	for (const auto& entry : thisAgg.adminConversationLinkMap)         // For each entry in the administered adminConversationLinkMap table (std::map)
	{
		unsigned short convID = entry.first;                       //    Get the conversation ID.
		for (const auto& link : entry.second)                      //    Walk the prioritized list of desired links for that conversation ID.
		{
			if (isInList(link, thisAgg.activeLagLinks))            //    If the desired link is active on the Aggregator
			{
				thisAgg.conversationLinkVector[convID] = link;        //        then put that link number in the conversationLinkVector
				break;
			}
		}
	}
}

// Is there really no standard library routine to test if a value is in a list?
bool LinkAgg::isInList(unsigned short val, const std::list<unsigned short>& thisList)
{
	bool inList = false;
	for (auto& listVal : thisList)
	{
		if (listVal == val)
		{
			inList = true;
			break;
		}
	}
	return (inList);
}


bool LinkAgg::collectFrame(AggPort& thisPort, Frame& thisFrame)  // const??
{
	bool collect = (thisPort.actorOperPortState.distributing);   // always return false if port not distributing (collecting ??)
	unsigned short convID = frameConvID(thisPort.actorOperPortAlgorithm, thisFrame);

	if (collect && thisPort.actorDWC)                            // if collecting but Discard Wrong Conversation is set
	{
		collect = thisPort.collectionConversationMask[convID];   // then verify this Conversation ID can be received through this AggPort
	}
	if (SimLog::Debug > 6)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << thisPort.actorSystem.addrMid
			<< ":" << thisPort.actorPort.num << " Link " << dec << thisPort.LinkNumberID << "  DWC " << thisPort.actorDWC << hex;
		if (collect)
		{
			SimLog::logFile << " collecting ConvID " << convID << dec << endl;
		}
		else
		{
			SimLog::logFile << " discarding ingress ConvID " << convID << dec << endl;
		}
	}

	return (collect);
}


shared_ptr<AggPort> LinkAgg::distributeFrame(Aggregator& thisAgg, Frame& thisFrame)
{
	shared_ptr<AggPort> pEgressPort = nullptr;
	unsigned short convID = frameConvID(thisAgg.actorPortAlgorithm, thisFrame);

	// Checking conversation mask of each port on lag is inefficient for this implementation, but ensures will  
	//   not send a frame during a transient when the conversation ID of the frame is moving between ports. 
	//   In this simulation will never be attempting to transmit during such a transient, 
	//   but if distribution was handled by a different thread (or in hardware) it could happen.
	// It would also be possible to create an array of port IDs (or port pointers) per Conversation ID 
	//   that was set to null when updating conversation masks and set to appropriate port when all masks updated.
	//   This would be more efficient for this simulation but not done since not as described in standard.
	for (auto lagPortIndex : thisAgg.lagPorts)
	{
		if (pAggPorts[lagPortIndex]->portOperConversationMask[convID])    // will be false if port not distributing or inappropriate port for this convID
		{
			pEgressPort = pAggPorts[lagPortIndex];
		}
	}
	if (SimLog::Debug > 6)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << thisAgg.actorSystem.addrMid
			<< ":" << thisAgg.aggregatorIdentifier;

		if (pEgressPort)
		{
			SimLog::logFile << " distributing ConvID " << convID 
				<< "  to Port " << pEgressPort->actorSystem.addrMid << ":" << pEgressPort->actorPort.num
				<< " Link " << dec << pEgressPort->LinkNumberID << dec << endl;
		}
		else
		{
			SimLog::logFile << " discarding egress ConvID " << convID << dec << endl;
		}
	}

	return (pEgressPort);
}


unsigned short LinkAgg::frameConvID(Aggregator::portAlgorithms algorithm, Frame& thisFrame)  // const??
{
	unsigned short convID = 0;

	switch (algorithm)
	{
	case AggPort::portAlgorithms::C_VID:
		if (thisFrame.getNextEtherType() == CVlanEthertype)
		{
//			convID = ((VlanTag&)(thisFrame.getNextSdu())).Vtag.id;
			convID = (VlanTag::getVlanTag(thisFrame)).Vtag.id;
			
		}
		break;
	case AggPort::portAlgorithms::S_VID:
		if (thisFrame.getNextEtherType() == SVlanEthertype)
		{
//			convID = ((VlanTag&)(thisFrame.getNextSdu())).Vtag.id;
			convID = (VlanTag::getVlanTag(thisFrame)).Vtag.id;
		}
		break;
	default:  // includes 	AggPort::portAlgorithms::UNSPECIFIED
		convID = macAddrHash(thisFrame);
		break;
	}
	return (convID);
}

/*
 *  macAddrHash calculates 12-bit 1's complement sum of the DA and SA fields of the input frame
/**/
unsigned short LinkAgg::macAddrHash(Frame& thisFrame)
{
	unsigned long long hash = 0;
	addr12 tempAddr;                         // addr12 allows treating a 48-bit MAC address as four 12-bit values

	tempAddr.all = thisFrame.MacDA;          // Calculate sum of 12-bit chunks of DA and SA fields.
	hash += tempAddr.high;
	hash += tempAddr.hiMid;
	hash += tempAddr.loMid;
	hash += tempAddr.low;
	tempAddr.all = thisFrame.MacSA;
	hash += tempAddr.high;
	hash += tempAddr.hiMid;
	hash += tempAddr.loMid;
	hash += tempAddr.low;
	hash = (hash & 0x0fff) + (hash >> 12);   // Roll overflow back into sum
	if (hash > 0x0fff) hash++;
	hash = (~hash) & 0x0fff;                 // Take 1's complement

	/*
	if (SimLog::Debug > 9)   
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   DA " << hex << thisFrame.MacDA
			<< "  SA " << thisFrame.MacSA << "  sum " << sum << "  hash "
			<< hash << dec << endl;
	}
	/**/
	return ((unsigned short)hash);
}


