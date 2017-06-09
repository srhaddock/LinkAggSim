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
#include "AggPort.h"




/*
void AggPort::LacpSelection::resetSelection(AggPort& port)
{

}
/**/

void LinkAgg::LacpSelection::adminAggregatorUpdate(std::vector<shared_ptr<AggPort>>& pAggPorts, std::vector<shared_ptr<Aggregator>>& pAggregators)
{
	for (unsigned short ax = 0; ax < pAggregators.size(); ax++)
	{
		Aggregator& agg = *pAggregators[ax];

		if (agg.changeActorSystem)                                                   // If ActorSystem identifier has changed
		{
			clearAggregator(agg, pAggPorts);                                         //    then kick all ports off Aggregator
			pAggPorts[ax]->portSelected = AggPort::selectedVals::UNSELECTED;         //    and unselect corresponding AggPort
			agg.changeActorSystem = false;
		}
		if (agg.actorOperAggregatorKey != agg.actorAdminAggregatorKey)               // If admin key has changed
		{
			clearAggregator(agg, pAggPorts);                                         //    then kick all ports off Aggregator
			agg.actorOperAggregatorKey = agg.actorAdminAggregatorKey;                //    and update oper key
		}
		if (!agg.getEnabled() && !agg.lagPorts.empty())                              // If aggregator disabled and LAG port list not empty
		{
			clearAggregator(agg, pAggPorts);                                         //    then kick all ports off Aggregator
		}
	}
}



void LinkAgg::LacpSelection::runSelection(std::vector<shared_ptr<AggPort>>& pAggPorts, std::vector<shared_ptr<Aggregator>>& pAggregators)
{

	for (unsigned short px = 0; px < pAggPorts.size(); px++)   // walk through AggPort array using px as index
	{
		/*
		*   First, if any AggPort has a new partner, check to see if that partner was previously attached to another AggPort.
		*     If so then generate PortMoved signal to old AggPort.
		*/
		if (pAggPorts[px]->newPartner)                        // if this port has a new partner
		{
			if (SimLog::Debug > 5)
			{
				SimLog::logFile << "Time " << SimLog::Time << ":  *** Port " << hex
					<< pAggPorts[px]->actorSystem.addrMid << ":" << pAggPorts[px]->actorPort.id
					<< " has new partner   " << pAggPorts[px]->partnerOperSystem.addrMid << ":" << pAggPorts[px]->partnerOperPort.id;
			}
			for (unsigned short opx = 0; opx < pAggPorts.size(); opx++)   // walk through AggPort array using opx as index
			{
				if ((opx != px) &&                                        // if there is another port with the same partner info
					(pAggPorts[opx]->partnerOperSystem.id == pAggPorts[px]->partnerOperSystem.id) &&
					(pAggPorts[opx]->partnerOperPort.id == pAggPorts[px]->partnerOperPort.id) &&
					(pAggPorts[opx]->partnerOperKey == pAggPorts[px]->partnerOperKey) &&
					(pAggPorts[opx]->partnerOperPortState.aggregation == pAggPorts[px]->partnerOperPortState.aggregation))
				{
					pAggPorts[opx]->PortMoved = true;                      //    then set port_moved flag for RxSM of the other port
					if (SimLog::Debug > 5)
					{
						SimLog::logFile << " that moved from port " << pAggPorts[opx]->actorSystem.addrMid << ":" << pAggPorts[opx]->actorPort.id;
					}
				}
			}
			if (SimLog::Debug > 5)
			{
				SimLog::logFile << dec << endl;
			}
			pAggPorts[px]->newPartner = false;
		}

		/*
		*   Start of actual selection logic.  This happens in three sections:
		*     1)  See if port can join an existing LAG.
		*     2)  Otherwise, if policy is to choose preferred aggregator even if disrupts an active LAG, then see if can join preferred aggregator.
		*     3)  Otherwise see if can find an Aggregator with no attached ports that are active.
		*/
		//TODO:  What action should be taken if MUX machine gets to DETACHED before changeActorOperDist is processed?
		//   Maybe need to qualify Selection Logic with "DETACHED && !changeActorOperDist" so cannot select new aggregator
		//   until changeActorOperDist processed on old aggregator (in which case remove checks on "SELECTED || sync").
		//   Or maybe just move runCSDC() before runSelection()?

		if ((pAggPorts[px]->portSelected == AggPort::selectedVals::UNSELECTED) && !pAggPorts[px]->actorAttached
//			&& !pAggPorts[px]->changeActorOperDist   //TODO: do I need an equivalent for new updateMask ???
			&& !pAggPorts[px]->Ready)
			// New mux machine added the !Ready term to above, but shouldn't hurt with old mux.  
			//    This is redundant with !actorAttached if qualified with UNSELECTED.
			//    If not qualified with UNSELECTED (i.e. allow selection logic to change selected aggregator while wait_while timer
			//    running) then need the !Ready term so that don't also allow selected aggregator to change in the ATTACH state.
		{

			pAggregators[pAggPorts[px]->actorPortAggregatorIndex]->lagPorts.remove(px);  // remove AggPort from lagPort list of previously selected Aggregator
			pAggregators[pAggPorts[px]->actorPortAggregatorIndex]->selectedLagPorts.remove(px);  // remove AggPort from lagPort list of previously selected Aggregator

			unsigned short chosenAggregatorIndex = pAggPorts.size();   // assume no aggregator chosen yet

			if (!pAggPorts[px]->actorOperPortState.aggregation)       // If port is not aggregatable (i.e. is Individual)
//				&& pAggregators[px]->getEnabled())                     //     and the aggregator is enabled
			{
				// This implements a policy that setting an AggPort "Individual" permanently reserves the corresponding Aggregator.
				// This happens regardless of whether the Aggregator ActorSystem and Key match the AggPort,
				//   or even whether the Aggregator is enabled (only time a AggPort will select a disabled Aggregator).
				clearAggregator(*pAggregators[px], pAggPorts);        //     Then kick other ports (if any) off own aggregator
				chosenAggregatorIndex = px;                           //       and make own aggregrator the chosen aggregator
				if (SimLog::Debug > 8) SimLog::logFile << "Time " << SimLog::Time << "       Individual Port ";
			}
			else if (pAggPorts[px]->partnerOperPortState.aggregation)            // Else if partner is also aggregatable
			{
				chosenAggregatorIndex = findMatchingAggregator(px, pAggPorts);   //     Then see if can join an existing LAG
				if (SimLog::Debug > 8) SimLog::logFile << "Time " << SimLog::Time << "                  Port ";
			}
			if (SimLog::Debug > 8) SimLog::logFile << hex << pAggPorts[px]->actorSystem.id << ":" << pAggPorts[px]->actorPort.id << dec
				<< " finds matching Aggregator " << chosenAggregatorIndex;

			if (chosenAggregatorIndex > px)                           // if not joining existing LAG or only on higher aggregator
			{
				/*
				*   This section is what optimizes determinism over minimizing movement of ports among aggregators.
				*   Putting a link on its preferred aggregator (as long as their keys match), or having it join a
				*   LAG on a lower aggregator, assures that LAGs deterministically form on the aggregator associated with the
				*   lowest port in the LAG regardless of the sequence (history) of links going up and down.  The tradeoff
				*   is that it may result in moving links for the same LAG from a higher aggregator, and/or kick off other ports
				*   that have used this aggregator while this port was down.  (The latter only occurs when
				*   there are fewer aggregators than ports with a given key.)
				*
				*   TODO:  The code below will kick ports of this port's preferred aggregator only if this port is operational.
				*   Do we want to claim the aggregator even if this port is not operational if no other ports on it are operational?
				*/
				if (allowableAggregator(*pAggPorts[px], *pAggregators[px]) &&    // If preferred aggregator has matching key
					pAggPorts[px]->PortEnabled && pAggPorts[px]->wtrRevertOK)    //   and the port is operational and revertive
				{
					if (SimLog::Debug > 8)
					{
						SimLog::logFile << endl << "Time " << SimLog::Time << ":  *** Port " << hex
							<< pAggPorts[px]->actorSystem.addrMid << ":" << pAggPorts[px]->actorPort.id << dec
							<< " Commandeering own Aggregator: WTR timer =  " << pAggPorts[px]->waitToRestoreTimer
							// << " and syncPortsInLAG = " << pAggregators[px]->syncPortsInLAG
							<< dec << endl;
					}

					clearAggregator(*pAggregators[px], pAggPorts);              //    Then kick other ports (if any) off preferred aggregator
					if (chosenAggregatorIndex < pAggregators.size())            //    If there was a matching LAG on a higher aggregator
					{
						clearAggregator(*pAggregators[chosenAggregatorIndex], pAggPorts);  //    Then kick ports off that aggregator
						                                                                   //      so they will later re-join on this aggregator
					}
					chosenAggregatorIndex = px;                                //     Make preferred aggregator the chosen aggregator                          
				}
				if (SimLog::Debug > 8) SimLog::logFile << "  ... " << chosenAggregatorIndex;
				/*
				*   This section will attempt to find an aggregator with no active ports.
				*   
				*   If the above "preferred aggregator" section is executed then this section will only be reached
				*     if there are fewer aggregators (enabled with a particular key value) than ports (with that key value).
				*
				*   If the above "preferred aggregator" section is not executed then this section will still give some
				*     preference to the preferred aggregator, but will not select it if any ports on it are active.
				/**/
				if (chosenAggregatorIndex >= pAggregators.size() &&            // If have not yet chosen an aggregator
					pAggPorts[px]->PortEnabled)                                //   and the port is operational
				{
					if (allowableAggregator(*pAggPorts[px], *pAggregators[px]) &&    // Then if preferred aggregator has matching key
						!activeAggregator(*pAggregators[px], pAggPorts))             //   and preferred aggregator has no active ports
					{
						clearAggregator(*pAggregators[px], pAggPorts);               //     Then kick other ports (if any) off preferred aggregator
						pAggPorts[px]->wtrRevertOK = true;                           //     Port becomes revertive if no active ports on preferred aggregator
						chosenAggregatorIndex = px;                                  //     Make preferred aggregrator the chosen aggregator
					}
					else //TODO:  should I put in a bias toward a previous aggregator, or go right to choosing first available aggregator?
					{                                                          // Otherwise look for another aggregator with no active ports
						unsigned short possibleAggregatorIndex = findAvailableAggregator(*pAggPorts[px], pAggPorts, pAggregators);
						if (possibleAggregatorIndex < pAggregators.size())             //   If found another aggregator
						{
							if (pAggPorts[px]->wtrRevertOK)                            //       and this port is revertive
							{
								chosenAggregatorIndex = possibleAggregatorIndex;       //       Then choose that aggregator
								clearAggregator(*pAggregators[chosenAggregatorIndex], pAggPorts);  //     Then kick other ports (if any) off that aggregator
							}
							else
							{                                                          //       Otherwise set this port to revertive, so next cycle can compete 
								pAggPorts[px]->wtrRevertOK = true;                     //           with any current ports that will also get set to revertive
							}
						}
					}

				}
				if (SimLog::Debug > 8) SimLog::logFile << "  ... " << chosenAggregatorIndex;
			}
			if (SimLog::Debug > 8) SimLog::logFile << dec << endl;

			/*
			*   If have chosen an aggregator, then update the LAGID etc of that aggregator and put port on aggregator's port list.
			*   On the port, set selected and update the selected aggregator variable
			*/
			if (chosenAggregatorIndex < pAggregators.size())
			{
				pAggregators[chosenAggregatorIndex]->partnerSystem.id = pAggPorts[px]->partnerOperSystem.id;      // Update LAGID of aggregator (in case taking over)
				pAggregators[chosenAggregatorIndex]->partnerOperAggregatorKey = pAggPorts[px]->partnerOperKey;
				pAggregators[chosenAggregatorIndex]->aggregatorIndividual =
					(!pAggPorts[px]->actorOperPortState.aggregation ||
					 !pAggPorts[px]->partnerOperPortState.aggregation); 
				pAggregators[chosenAggregatorIndex]->aggregatorLoopback = 
					((pAggPorts[px]->actorSystem.addr == pAggPorts[px]->partnerOperSystem.addr) &&
					 (pAggPorts[px]->actorPort.num == pAggPorts[px]->partnerOperPort.num));
				// Does it matter if we change aggregator LAGID while there may be ports with previous LAGID still detaching?
				//TODO:  I don't think it matters because won't attach ports with new LAGID until all previous are detached.   
				//   or maybe just don't set Ready until all ports on LAG are Selected and ReadyN

				pAggPorts[px]->actorOperPortAlgorithm = pAggregators[chosenAggregatorIndex]->actorPortAlgorithm;
				pAggPorts[px]->actorOperConversationLinkListDigest = pAggregators[chosenAggregatorIndex]->actorConversationLinkListDigest;
				pAggPorts[px]->actorOperConversationServiceMappingDigest = pAggregators[chosenAggregatorIndex]->actorConversationServiceMappingDigest;
				//TODO:  do I need to set actorDWC here as well?  No.  Gets updated when link becomes active and updateConversationMasks

				pAggPorts[px]->portSelected = AggPort::selectedVals::SELECTED;                             // Set SELECTED
				pAggregators[chosenAggregatorIndex]->lagPorts.push_back(px);                               // Put this port on port list of chosen aggregator
				pAggregators[chosenAggregatorIndex]->selectedLagPorts.push_back(pAggPorts[px]->actorPortAggregatorIdentifier);  // Put this port on port list of chosen aggregator
				// Should be safe to add to list without checking to see if it is already on list
				pAggPorts[px]->actorPortAggregatorIndex = chosenAggregatorIndex;                           // Update the selected aggregator index for this port 
				pAggPorts[px]->actorPortAggregatorIdentifier = pAggregators[chosenAggregatorIndex]->aggregatorIdentifier;
				
				if (pAggPorts[px]->actorLacpVersion > 1)         // For new Mux, start wait_while_timer from here rather than in Mux machine
					pAggPorts[px]->waitWhileTimer = AggPort::AggregateWaitTime;         // Wait a little to see if other ports are attaching 

				/*
				*  Would set portSelected to STANDBY instead of SELECTED if there was some condition that prevented the port from attaching
				*    to the chosen aggregator, but the port cannot choose another aggregator without having two LAGs with with same LAGID.
				*    In this case would also have to add code to change STANDBY to SELECTED when that condition changed.
				*    An example of such a condition would be if the implementation of the aggregator Mux limited the number of
				*    links in a LAG, and the aggregator already had the maximum number of links attached.
				*    This implementation has no such limitations and therefore never sets portSelected to STANDBY.
				/**/

				// DEBUG:
				if (SimLog::Debug > 3)
				{
					SimLog::logFile << "Time " << SimLog::Time << ":  *** Port " << hex
						<< pAggPorts[px]->actorSystem.addrMid << ":" << pAggPorts[px]->actorPort.id
						<< " selects aggregator " << pAggregators[chosenAggregatorIndex]->aggregatorIdentifier
						<< "  LAGID " << pAggPorts[px]->actorSystem.id << ":" << pAggPorts[px]->actorOperPortKey
						<< ":" << pAggPorts[px]->partnerOperSystem.id << ":" << pAggPorts[px]->partnerOperKey << " with ports : ";
					for (auto lagPortIndex : pAggregators[chosenAggregatorIndex]->lagPorts)
					{
						SimLog::logFile << "  " << pAggPorts[lagPortIndex]->actorPort.id << ":" << pAggPorts[lagPortIndex]->portSelected;
					}
					SimLog::logFile << dec << endl;

				}
			}
		}

	}

	for (auto pAgg : pAggregators)
//	for (unsigned short j = 0; j < pAggregators.size(); j++)    
	{

		int collPortsInLAG = 0;
		int distPortsInLAG = 0;
		pAgg->aggregatorReady = true;
		bool aggregatorActive = activeAggregator(*pAgg, pAggPorts);

		for (auto lagPortIndex : pAgg->lagPorts)    // Walk through all AggPorts on Aggregator
			{
				AggPort& port = *(pAggPorts[lagPortIndex]);

				if ((port.portSelected == AggPort::selectedVals::SELECTED) && port.actorOperPortState.collecting)
					collPortsInLAG++;               // Count the AggPorts that are collecting
				if ((port.portSelected == AggPort::selectedVals::SELECTED) && port.actorOperPortState.distributing)
					distPortsInLAG++;               // Count the AggPorts that are distributing

				// Set aggregatorReady if all ports on Aggregator are either already attached or are selected and wait_while expired
				// For new mux, replace the following with a test of actorAttached and wait_while_timer expired
				if (port.actorLacpVersion == 1)
				{
					pAgg->aggregatorReady &= (port.actorOperPortState.sync ||  //TODO:  should this test actorAttached rather than actor.sync ?
					 	(port.ReadyN && (port.portSelected == AggPort::selectedVals::SELECTED)));
				}
				else
				{
					pAgg->aggregatorReady &= (port.actorAttached ||      // this could be changed to just check port.Ready instead of port.actorAttached
						((port.portSelected == AggPort::selectedVals::SELECTED) && (port.waitWhileTimer == 0)));
				}


				if (!aggregatorActive && port.PortEnabled)   // If aggregator is not active but has enabled ports
					port.wtrRevertOK = true;                 //    Then set those ports to revertive
			}
//		}
		pAgg->receiveState = (collPortsInLAG > 0);           //TODO:  receiveState is not used anywhere and should be removed
		pAgg->transmitState = (distPortsInLAG > 0);          //TODO:  transmitState is not used anywhere and should be removed

		for (auto lagPortIndex : pAgg->lagPorts)    // Walk through all AggPorts on Aggregator again
		{
			AggPort& port = *(pAggPorts[lagPortIndex]);

//			port.Ready = pAgg->aggregatorReady;
			if (pAgg->aggregatorReady)                   // If aggregator is ready then set Ready of all AggPorts
				port.Ready = true;                       //    Note this structure will not clear AggPort Ready when !aggregatorReady
				                                         //    because Ready only cleared when enter DETACHED
			                                             // Could also get rid of pAgg->aggregatorReady and just make it a local Boolean
		}
		/**/
	}

	/*
	* This will cause Ready to be set on any port that is currently unselected and it's previously selected aggregator now has no 
	* entries in lagPorts list.  Problem if use !Ready as test for running selection logic as proposed in Mick's write-up of new Mux machine.
	for (auto& pPort : pAggPorts)                   // for each port copy aggregatorReady flag of selected aggregator to that port
	{
		pPort->Ready = pAggregators[pPort->actorPortAggregatorIndex]->aggregatorReady;
	}
	/**/
}


int LinkAgg::LacpSelection::findMatchingAggregator(int thisIndex, std::vector<shared_ptr<AggPort>>& pAggPorts)
{
	std::vector<shared_ptr<AggPort>> pAggregators = pAggPorts;
	AggPort& thisPort = *pAggPorts[thisIndex];
	bool foundMatch = false;
	unsigned short ax = 0;
	int chosen = pAggPorts.size();   // no aggregator chosen yet

	for (ax = 0; ax < pAggregators.size(); ax++)     // walk through Aggregator array using ax as index
	{
		foundMatch = (matchAggregatorLagid(thisPort, *pAggregators[ax]));  // look for aggregator with same LAGID as this port

		if (foundMatch && (thisPort.actorSystem.addr == thisPort.partnerOperSystem.addr))   // special cases if loopback to same system
		{
			if (thisPort.actorPort.num == thisPort.partnerOperPort.num)       //  if same-port-loopback (tx to rx)
			{                                                                //    Then can only join LAG if all ports are same-port-loopback
				foundMatch &= pAggregators[ax]->aggregatorLoopback;            //    (but otherwise have to find another aggregator)

				if (SimLog::Debug > 2)
					SimLog::logFile << "Time " << SimLog::Time << ":     Port " << hex << thisPort.actorPort.num << " same-port-loopback foundMatch "
					<< foundMatch << " agg " << pAggregators[ax]->aggregatorIdentifier << " agg_Loopback " << pAggregators[ax]->aggregatorLoopback
					<< dec << endl;
			}
			else                                                                                      //  else loopback is to different port
			{
				for (int k = 0; k < thisIndex; k++)  // Ports in different-port-loopback cannot have both ends of link in same LAG
				{
					if ((thisPort.partnerOperPort.num == pAggPorts[k]->actorPort.num) &&  //  so if other end of loopback has
						(pAggPorts[k]->actorPortAggregatorIndex == ax))                                 //  already selected this aggregator
						foundMatch = false;                                                           //   then find another aggregator
				}
				foundMatch &= !pAggregators[ax]->aggregatorLoopback;     // find another if this aggregator chosen by a same-port-looback

				if (SimLog::Debug > 2)
					SimLog::logFile << "Time " << SimLog::Time << ":     Port " << hex << thisPort.actorPort.num << " diff-port-loopback foundMatch "
					<< foundMatch << " agg " << pAggregators[ax]->aggregatorIdentifier << " agg_Loopback " << pAggregators[ax]->aggregatorLoopback
					<< dec << endl;
			}
		}
		/*
		* Comment out for now because I don't think aggregator could have aggregatorLoopback true if its LAGID matched the port
		*   and the port is not in loopback.  (If port had different actor and partner System IDs then could not match LAGID with
		*   an aggregator that had the same actor and partner System IDs.
		*
		else                                                                                         // else this port not a loopback
		{
		foundMatch &= !Aggregators[ax].aggregatorLoopback;         //  find another if this aggregator chosen by a same-port-looback
		}
		/**/

		if (foundMatch)
		{
			if (ax < chosen)
				chosen = ax;                                           // choose lowest matching aggregator
			else
				clearAggregator(*pAggregators[ax], pAggPorts);         // kick all ports off any higher matching aggregatos
		                                                               //   to force all ports with same LAG ID to move to lowest matching aggregator
			// This ensures don't end up with two aggregators with the same LAGID, which could happen if change the LAGID of a lower aggregator
			//    to match the LAGID of a higher aggregator that already has ports attached.
			// This puts in a bias for the lowest aggregator.  
			// I think if wanted to minimize disruption, then instead of automatically clearing higher aggregator:
			//   if (higher aggregator has ports with Selected = SELECTED)
			//   {
			//       if (chosen aggregator does not have ports with Selected = SELECTED)
			//           chosen = ax;                                  // choose aggregator that already has ports selected
			//       else
			//           clearAggregator(*pAggregators[ax], pAggPorts);  // otherwise clear the higher aggregator
			//   }
		}
	}

	return (chosen);
}


/**/
bool LinkAgg::LacpSelection::matchAggregatorLagid(AggPort& port, Aggregator& agg)
{
	bool matchingLAGID = ((port.actorOperPortKey == agg.actorOperAggregatorKey) &&
                          (port.actorSystem.id == agg.actorSystem.id) &&
		                  (port.partnerOperSystem.id == agg.partnerSystem.id) &&
		                  (port.partnerOperKey == agg.partnerOperAggregatorKey));

	bool individual = (!port.actorOperPortState.aggregation ||
		               !port.partnerOperPortState.aggregation ||
		                agg.aggregatorIndividual);
	return (matchingLAGID && !individual && agg.getEnabled());
}


bool LinkAgg::LacpSelection::allowableAggregator(AggPort& port, Aggregator& agg)      // Allowable aggregator just matches actor part of LAGID
{
	return ((port.actorOperPortKey == agg.actorOperAggregatorKey) &&                  // Aggregator must have same key as port
            (port.actorSystem.id == agg.actorSystem.id) &&                            //    the same SysId as the port
		     agg.getEnabled());                                                       //    and the be enabled.
}

/**/
void LinkAgg::LacpSelection::clearAggregator(Aggregator& agg, std::vector<shared_ptr<AggPort>>& pAggPorts)
{
	for (auto lagPortIndex : agg.lagPorts)
	{
		AggPort& port = *(pAggPorts[lagPortIndex]);

		port.portSelected = AggPort::selectedVals::UNSELECTED;  //   then set it to UNSELECTED so will detach

		if (port.actorSystem.addr == port.partnerOperSystem.addr)   // special cases if loopback to same system
		{
			for (unsigned short i = 0; i < pAggPorts.size(); i++)                     // find index of partner port 
			{                                                             //    on other end of loopback link
				if (pAggPorts[i]->actorPort.num == port.partnerOperPort.num)
				{
					pAggPorts[i]->portSelected = AggPort::selectedVals::UNSELECTED;  //   and set it UNSELECTED as well

				}
			}
		}
	}
}


int LinkAgg::LacpSelection::findAvailableAggregator(AggPort& thisPort,
	std::vector<shared_ptr<AggPort>>& pAggPorts, std::vector<shared_ptr<Aggregator>>& pAggregators)
{
	bool foundAggregator = false;
	unsigned short ax = 0;

	for (ax = 0; ax < pAggregators.size(); ax++)     // walk through Aggregator array using ax as index
	{
		foundAggregator = (allowableAggregator(thisPort, *pAggregators[ax]) &&    // Look for an aggregator with a matching key
			!activeAggregator(*pAggregators[ax], pAggPorts) &&                 //   and has no active ports
			pAggPorts[ax]->actorOperPortState.aggregation);         //   and is not monopolized by an Individual port
		if (foundAggregator) break;
	}

	return (ax);
}

bool LinkAgg::LacpSelection::activeAggregator(Aggregator& agg,
	std::vector<shared_ptr<AggPort>>& pAggPorts)
{
	bool activePorts = false;

	for (auto lagPortIndex : agg.lagPorts)         // walk through all ports on this Aggregator
	{
		activePorts |= (pAggPorts[lagPortIndex]->PortEnabled && pAggPorts[lagPortIndex]->wtrRevertOK);         
		                                           //     Aggregator is active if any port on Aggregator is enabled and revertive
	}

	return (activePorts);
}
/**/
