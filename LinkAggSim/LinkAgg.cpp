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

		// Run state machines
		int transitions = 0;
		for (unsigned short i = 0; i < nPorts; i++)              // For each Aggregation Port:
		{
//			transitions += AggPort::LacpRxSM::runRxSM(*pAggPorts[i], true);
//			transitions += AggPort::LacpMuxSM::runMuxSM(*pAggPorts[i], true);
			transitions += AggPort::LacpRxSM::run(*pAggPorts[i], true);
			transitions += AggPort::LacpMuxSM::run(*pAggPorts[i], true);
		}
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
		//TODO:  Can you aggregate ports with different LACP versions?  Do you need to test for it?  Does partner LACP version need to be in DRCPDU?
		//TODO:  Update aggregator status before or after transmit?
		updateAggregatorStatus();

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

void LinkAgg::updateAggregatorStatus()
{
	for (auto pPort : pAggPorts)           // Walk through all Aggregation Ports
	{
		if (pPort->changeActorOperDist           // Set changeAggregationPorts as logical "or" of the changeActorOperDist flag
			&& ((pPort->portSelected == AggPort::selectedVals::SELECTED) || pPort->actorOperPortState.sync)
			)
		{
			pAggregators[pPort->actorPortAggregatorIndex]->changeAggregationPorts = true;
		}
		// Don't clear changeActorOperDist yet -- will be done later.
		//TODO:  What action should be taken if MUX machine gets to DETACHED before changeActorOperDist is processed?
		//   Maybe need to qualify Selection Logic with "DETACHED && !changeActorOperDist" so cannot select new aggregator
		//   until changeActorOperDist processed on old aggregator (in which case remove checks on "SELECTED || sync")

		//  If the partner distribution algorithm parameters have changed for any port that is COLLECTING on an Aggregator,
		//     update the Aggregator's distribution algorithm parameters.
		//     Assumes that the portOper... distribution algorithm parameters are null for any port that expects the 
		//     Aggregator to use the partnerAdmin... values (including port that is DEFAULTED, is version 1 (?), 
		//     or did not receive the proper V2 TLVs from the partner.
		if (pPort->actorOperPortState.collecting)  // need to test SELECTED?
		{
			Aggregator& agg = *(pAggregators[pPort->actorPortAggregatorIndex]);
			if ((pPort->changePartnerOperDistAlg || agg.changePartnerAdminDistAlg) &&
				(pPort->partnerOperPortAlgorithm == Aggregator::portAlgorithms::NONE))
				// partnerOperPortAlgorithm will only be NONE if no LACPDUv2 exchange with partner, so use partnerAdmin values.
			{
				agg.partnerPortAlgorithm = agg.partnerAdminPortAlgorithm;
				agg.partnerConversationLinkListDigest = agg.partnerAdminConversationLinkListDigest;
				agg.partnerConversationServiceMappingDigest = agg.partnerAdminConversationServiceMappingDigest;
				agg.changePartnerDistributionAlgorithm = true;
				// Could optimize further processing by only setting change flag if new values differ from old values.
				pPort->changePartnerOperDistAlg = false;
			}
			else if (pPort->changePartnerOperDistAlg)
			{
				agg.partnerPortAlgorithm = pPort->partnerOperPortAlgorithm;
				agg.partnerConversationLinkListDigest = pPort->partnerOperConversationLinkListDigest;
				agg.partnerConversationServiceMappingDigest = pPort->partnerOperConversationServiceMappingDigest;
				agg.changePartnerDistributionAlgorithm = true;
				// Could optimize further processing by only setting change flag if new values differ from old values.
				pPort->changePartnerOperDistAlg = false;
			}
		}
	}



	for (auto pAgg : pAggregators)         // Walk through all Aggregators
	{
		if (pAgg->changeActorDistributionAlgorithm || pAgg->changePartnerDistributionAlgorithm)
		{
			pAgg->differentPortAlgorithms = ((pAgg->actorPortAlgorithm != pAgg->partnerPortAlgorithm) ||
				(pAgg->actorPortAlgorithm == Aggregator::portAlgorithms::UNSPECIFIED) ||
				(pAgg->partnerPortAlgorithm == Aggregator::portAlgorithms::UNSPECIFIED));  // actually, testing partner unspecified is redundant
			pAgg->differentPortConversationDigests = (pAgg->actorConversationLinkListDigest != pAgg->partnerConversationLinkListDigest);
			pAgg->differentConversationServiceDigests = (pAgg->actorConversationServiceMappingDigest != pAgg->partnerConversationServiceMappingDigest);
			pAgg->operDiscardWrongConversation = ((pAgg->adminDiscardWrongConversation == adminValues::FORCE_TRUE) ||
				((pAgg->adminDiscardWrongConversation == adminValues::AUTO) &&
				!(pAgg->differentPortAlgorithms || pAgg->differentPortConversationDigests || pAgg->differentConversationServiceDigests)));
			//TODO:  report to management if any "differentXXX" flags are set
			//TODO:  Set differentPortConversationDigests if actor and partner Link Numbers don't match 
			//         (are they guaranteed to be stable by the time actor.sync and partner.sync?)
			//         (will changes to actor or partner set the change distribution algorithms flag?)
			if (SimLog::Debug > 6)
			{
				SimLog::logFile << "Time " << SimLog::Time 
					<< ":   Device:Aggregator " << hex << pAgg->actorSystem.addrMid  << ":" << pAgg->aggregatorIdentifier 
					<< " differ-Alg/CDigest/SDigest = " << pAgg->differentPortAlgorithms << "/"
					<< pAgg->differentPortConversationDigests << "/" << pAgg->differentConversationServiceDigests
					<< "  adminDWC = " << pAgg->adminDiscardWrongConversation << "  DWC = " << pAgg->operDiscardWrongConversation
					<< dec << endl;
			}
		}

		if (pAgg->changeAggregationPorts ||              // If a port has gone up or down on the aggregator
			pAgg->changeActorDistributionAlgorithm ||    //   or distribution algorithms have changed
			pAgg->changePartnerDistributionAlgorithm)
			//TODO:  Need to set changeAggregationPorts if Link Number changes or if aAggConversationAdminLink[] changed by management
			//  Currently aAggConversationAdminLink[] is hard coded in the convLinkPriorityList
			//TODO:  Don't really need to do this if actorPortAlgorithm or actorConversationServiceMappingDigest changed
			//    because these only affect mapping frames to CIDs, but not mapping CIDs to active links.
			//    Likewise with all partner distribution algorithm changes.
			//TODO:  To clean this up probably need to walk through pAgg->lagPorts twice, once to push out variable changes if the 
			//    distribution algorithms changed, and again to rebuild active link list if ports went up or down
			//    or actorConversationAdminLink (or digest of) changed.
		{
			std::list<unsigned short> newActiveLinkList;  // Build a new list of the link numbers active on the LAG
//			for (auto pPort : pAgg->lagPorts)             //   from list of Aggregation Ports on the LAG
			for (auto lagPortIndex : pAgg->lagPorts)      //   from list of Aggregation Ports on the LAG
			{
				AggPort& port = *(pAggPorts[lagPortIndex]);

				if (pAgg->changeActorDistributionAlgorithm) // If the actor distribution algorithm parameters have been administratively changed
				{                                         //     then push Aggregator values out to each AggPort on the Aggregator
					port.actorOperPortAlgorithm = pAgg->actorPortAlgorithm;
					port.actorOperConversationLinkListDigest = pAgg->actorConversationLinkListDigest;
					port.actorOperConversationServiceMappingDigest = pAgg->actorConversationServiceMappingDigest;
					port.actorDWC = pAgg->operDiscardWrongConversation;
					if (port.actorOperPortState.sync)   //     If the AggPort is attached to Aggregator
					{
						port.NTT = true;                //         then communicate change to partner
						if (SimLog::Debug > 6)
						{
							SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << port.actorSystem.addrMid
								<< ":" << port.actorPort.num << " NTT: Change actor dist algorithm on Link " << dec << port.LinkNumberID
								<< hex << "  Device:Aggregator " << pAgg->actorSystem.addrMid << ":" << port.actorPortAggregatorIdentifier 
								<< dec << endl;
						}
					}
				}
				if (pAgg->changePartnerDistributionAlgorithm)
				{
					port.actorDWC = pAgg->operDiscardWrongConversation;
					// If want the Aggregator partner distribution algorithm parameters to contain the most recently received, as opposed to
					//   most recently modified, value of any AggPort received partner distribution algorithm variables,
					//   then need to push Aggregator values out to any AggPort that is "actor.distributing && !changePartnerOperDistAlg".
					//   The latter part of test prevents overwriting a value received while this code is executing (if LacpRxSM is truly asynchronous).
				}

				port.changeActorOperDist = true;    // Set change flag for all ports on aggregator since change to any one 
				                                      //   could change distribution mask for all.
				                                      //   But don't need to do this if only partner algorithms changed?

				if (port.actorOperPortState.distributing)                // For any port that is distributing
				{
					newActiveLinkList.push_back(port.LinkNumberID);      // Add its Link Number to the new active link list  
				}
			}

			if (newActiveLinkList.empty())         // If there are no links active on the LAG 
			{
				if (pAgg->operational)                    // if Aggregator was operational
				{
					//TODO:  flush aggregator request queue?

					cout << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << pAgg->actorSystem.addrMid << ":" << pAgg->aggregatorIdentifier
						<< " is DOWN" << dec << endl;
					if (SimLog::Time > 0)
					{
						SimLog::logFile << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << pAgg->actorSystem.addrMid << ":" << pAgg->aggregatorIdentifier
							<< " is DOWN" << dec << endl;
					}
				}
				pAgg->activeLagLinks.clear();             // Clear the old list of link numbers active on the LAG; 
				pAgg->operational = false;                // Set Aggregator ISS to not operational
			}
			else                                      // There are active links on the LAG
			{ 
				newActiveLinkList.sort();                       // Put list of link numbers in ascending order 
				if (pAgg->activeLagLinks != newActiveLinkList)  // If the active links have changed
				{
					cout << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << pAgg->actorSystem.addrMid << ":" << pAgg->aggregatorIdentifier
						<< " is UP with Link numbers:  " << dec;
					for (auto link : newActiveLinkList)
						cout << link << "  ";
					cout << dec << endl;
					if (SimLog::Time > 0)
					{
						SimLog::logFile << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << pAgg->actorSystem.addrMid << ":" << pAgg->aggregatorIdentifier
							<< " is UP with Link numbers:  " << dec;
						for (auto link : newActiveLinkList)
							SimLog::logFile << link << "  ";
						SimLog::logFile << dec << endl;
					}
				}
//				std::vector<unsigned short> activeLinkVector ( newActiveLinkList);  // create a vector from a list??
				pAgg->activeLagLinks = newActiveLinkList;   // Save the new list of active link numbers
				pAgg->operational = true;                   // The Aggregator ISS is operational
			}
			updateConversationLinkMap(*pAgg);           // Create new Conversation ID to Link associations
			pAgg->changeAggregationPorts = false;    // Finally, clear status change flags
			pAgg->changeActorDistributionAlgorithm = false;
			pAgg->changePartnerDistributionAlgorithm = false;
			//TODO:  For DRNI have to set a flag to let DRF know that list of active ports may have changed.

			if (SimLog::Debug > 4)
			{
				SimLog::logFile << "Time " << SimLog::Time << ":   Device:Aggregator " << hex << pAgg->actorSystem.addrMid
					<< ":" << pAgg->aggregatorIdentifier << dec << " ConvLinkMap = ";
				for (int k = 0; k < 16; k++) SimLog::logFile << "  " << pAgg->conversationLinkMap[k];
				SimLog::logFile << endl;
			}
		}
	}

	for (auto pPort : pAggPorts)
	{
		if (pPort->changeActorOperDist)
		{
			if (pPort->actorOperPortState.distributing)          // If Aggregation Port is distributing 
			{                                                   //    then create new distribution and collection masks
				for (int convID = 0; convID < 4096; convID++)   //    for all conversation ID values.
				{
					bool passConvID = ((pAggregators[pPort->actorPortAggregatorIndex]->conversationLinkMap[convID] == pPort->LinkNumberID) &&
						               (pPort->LinkNumberID > 0));       // Determine if conversation ID maps to this port's link number
					                                                 
					pPort->portOperConversationMask[convID] = passConvID;   // If so then distribute this conversation ID. 
					pPort->collectionConversationMask &= passConvID;        // Turn off collection if link number doesn't match.
					                                                        // Keep change flag set so can come back and turn on collection
					                                                        //    for a matching link number after the conversation ID has been
					                                                        //    cleared in the collection mask for all other ports on the aggregator.

				}
				if (SimLog::Debug > 4)
				{
					SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << pPort->actorSystem.addrMid
						<< ":" << pPort->actorPort.num << " Link " << dec << pPort->LinkNumberID << "   ConvMask = ";
					for (int k = 0; k < 16; k++) SimLog::logFile << "  " << pPort->portOperConversationMask[k];
					SimLog::logFile << endl;
				}
			}
			else                                             // Else Aggregation Port not Distributing
			{
				pPort->portOperConversationMask.reset();        //    so no conversation IDs are distributed
				pPort->collectionConversationMask.reset();      //    or collected.
				pPort->changeActorOperDist = false;             //    Clear change flag since done with this Agg Port
			}
		}
	}
	//TODO:  Supposed to do this after have updated all ports (true at this point) and IppAllUpdate is false
	//          but not clear the IppAllUpdate test is really necessary
	//  Before just cleared collection mask for Conversation IDs where the port's link number did not match the conversationLinkMap.
	//     Now want to set the collection mask for Conversation IDs where the port's link number does match.
	//     This assures "break-before-make" behavior so no Conversation ID bit is ever set in the collection mask for more than one port.
	for (auto pPort : pAggPorts)
	{
		if (pPort->changeActorOperDist)
		{
			pPort->collectionConversationMask = pPort->portOperConversationMask;   // Make collection mask the same as the distribution mask
			pPort->changeActorOperDist = false;                                    //    Clear change flag since done with this Agg Port
		}

		//TODO:  Think should update actorPartnerSync at this point (for all ports, or could do it earlier for any ports that changed).
		if (!pPort->actorPartnerSync || !pPort->partnerActorPartnerSync)
		{
			pPort->currentWhileLongLacpTimer = 50;
			pPort->longLacpduXmit = (pPort->enableLongLacpduXmit && (pPort->partnerLacpVersion >= 2));
		}
		if (pPort->currentWhileLongLacpTimer == 0)
		{
			pPort->longLacpduXmit = false;
		}

	}
}



void LinkAgg::updateConversationLinkMap(Aggregator& thisAgg)
{
	switch (thisAgg.selectedConvPortList)
	{
	case Aggregator::convPortLists::ADMIN_TABLE:
		linkMap_AdminTable(thisAgg);
		break;
	case Aggregator::convPortLists::ACTIVE_STANDBY:
		linkMap_ActiveStandby(thisAgg);
		break;
	case Aggregator::convPortLists::EVEN_ODD:
		linkMap_EvenOdd(thisAgg);
		break;
	case Aggregator::convPortLists::EIGHT_LINK_SPREAD:
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
		thisAgg.conversationLinkMap[row] = 0;                                       // Link Number 0 is default if no Link Numbers in row are active
		for (int j = 0; j < nLinks; j++)                                            // Search a row of the table
		{                                                                           //   for the first Link Number that is active.
			if ((convLinkPriorityList[row][j] < activeLinkNumbers.size()) && activeLinkNumbers[convLinkPriorityList[row][j]])
			{
				thisAgg.conversationLinkMap[row] = activeLinkNumbers[convLinkPriorityList[row][j]];    //   Copy that Link Number into map
				break;                                                              //       and quit the for loop.
			}
		}
	}

	// Finally, repeat first 8 entries through the rest of the conversation-to-link map
	for (int row = nRows; row < 4096; row++)
	{
		thisAgg.conversationLinkMap[row] = thisAgg.conversationLinkMap[row % nRows];
	}
}
/**/

void LinkAgg::linkMap_EvenOdd(Aggregator& thisAgg)
{
	if (thisAgg.activeLagLinks.empty())              // If there are no active links
	{
		thisAgg.conversationLinkMap.fill(0);         //    then the ConversationLinkMap is all zero
	} 
	else                                             // Otherwise ...
	{
		unsigned short evenLink = thisAgg.activeLagLinks.front();    // Even Conversation IDs go to lowest LinkNumberID.
		unsigned short oddLink = thisAgg.activeLagLinks.back();      // Odd Conversation IDs go to highest LinkNumberID.  (Other links are standby)
		for (int j = 0; j < 4096; j += 2)
		{
			thisAgg.conversationLinkMap[j] = evenLink;
			thisAgg.conversationLinkMap[j + 1] = oddLink;
		}
	}
}

void LinkAgg::linkMap_ActiveStandby(Aggregator& thisAgg)
{
	if (thisAgg.activeLagLinks.empty())              // If there are no active links
	{
		thisAgg.conversationLinkMap.fill(0);         //    then the ConversationLinkMap is all zero
	}
	else                                             // Otherwise ...
	{
		// Active link will be the lowest LinkNumberID.  All others will be Standby.
		unsigned short activeLink = thisAgg.activeLagLinks.front();
		thisAgg.conversationLinkMap.fill(activeLink);
	}
}

void LinkAgg::linkMap_AdminTable(Aggregator& thisAgg)
{
	thisAgg.conversationLinkMap.fill(0);                           // Start with  ConversationLinkMap is all zero
	for (const auto& entry : thisAgg.conversationPortList)         // For each entry in the administered conversationPortList table (std::map)
	{
		unsigned short convID = entry.first;                       //    Get the conversation ID.
		for (const auto& link : entry.second)                      //    Walk the prioritized list of desired links for that conversation ID.
		{
			if (isInList(link, thisAgg.activeLagLinks))            //    If the desired link is active on the Aggregator
			{
				thisAgg.conversationLinkMap[convID] = link;        //        then put that link number in the conversationLinkMap
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


