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



// void AggPort::LacpMuxSM::resetMuxSM(AggPort& port)
void AggPort::LacpMuxSM::reset(AggPort& port)
{
	port.waitWhileTimer = 0;
	port.waitToRestoreTimer = 0;
	port.MuxSmState = enterDetached(port);
}

/**/
// void AggPort::LacpMuxSM::timerTickMuxSM(AggPort& port)
void AggPort::LacpMuxSM::timerTick(AggPort& port)
{
	if (port.waitWhileTimer > 0) port.waitWhileTimer--;
	if (port.waitToRestoreTimer > 0) port.waitToRestoreTimer--;
}
/**/

// int AggPort::LacpMuxSM::runMuxSM(AggPort& port, bool singleStep)
int AggPort::LacpMuxSM::run(AggPort& port, bool singleStep)
{
	bool transitionTaken = false;
	int loop = 0;

//	if (port.waitWhileTimer > 0) port.waitWhileTimer--;

	do
	{
		transitionTaken = stepMuxSM(port);
		loop++;
	} while (!singleStep && transitionTaken && loop < 10);
	if (!transitionTaken) loop--;
	return (loop);
}



bool AggPort::LacpMuxSM::stepMuxSM(AggPort& port)
{
	MuxSmStates nextMuxSmState = MuxSmStates::NO_STATE;
	bool transitionTaken = false;

	if (port.actorLacpVersion == 1)  // This has the old state machine with LACPv1 and new (2017) state machine with LACPv2 for convenience
		//  It doesn't have to be that way.  Old machine can work with v2 LACPDUs and new machine can work with v1 LACPDUs
	{
		// No global transitions
		switch (port.MuxSmState)
		{
		case MuxSmStates::DETACHED:
			if ((port.portSelected == selectedVals::SELECTED) || (port.portSelected == selectedVals::STANDBY))
				nextMuxSmState = enterWaiting(port);
			break;
		case MuxSmStates::WAITING:
			port.ReadyN = (port.waitWhileTimer == 0);
			if ((port.portSelected == selectedVals::SELECTED) && port.Ready)
				nextMuxSmState = enterAttached(port);
			else if (port.portSelected == selectedVals::UNSELECTED)
				nextMuxSmState = enterDetached(port);
			break;
		case MuxSmStates::ATTACHED:
			if ((port.portSelected == selectedVals::SELECTED) && port.partnerOperPortState.sync)
				if (port.policy_coupledMuxControl)
					nextMuxSmState = enterCollDist(port);
				else
					nextMuxSmState = enterCollecting(port);
			else if (port.portSelected != selectedVals::SELECTED)
				nextMuxSmState = enterDetached(port);
			break;
		case MuxSmStates::COLLECTING:
			if ((port.portSelected == selectedVals::SELECTED) && port.partnerOperPortState.sync
				&& port.partnerOperPortState.collecting)
				nextMuxSmState = enterDistributing(port);
			else if ((port.portSelected != selectedVals::SELECTED) ||
				(!port.partnerOperPortState.sync))
				nextMuxSmState = enterAttached(port);
			break;
		case MuxSmStates::DISTRIBUTING:
			if ((port.portSelected != selectedVals::SELECTED) ||
				!port.partnerOperPortState.sync || !port.partnerOperPortState.collecting)
				nextMuxSmState = enterCollecting(port);
			break;
		case MuxSmStates::COLL_DIST:
			if ((port.portSelected != selectedVals::SELECTED) ||
				(!port.partnerOperPortState.sync))
				nextMuxSmState = enterAttached(port);
			break;
		default:
			nextMuxSmState = enterDetached(port);
			break;
		}
	}
	else  // (port.actorLacpVersion > 1)
	{
		bool portGoodToGo = port.actorAttached && port.PortEnabled && (port.waitToRestoreTimer == 0) && port.wtrRevertOK;
		// Global transitions:
		if (port.portSelected != selectedVals::SELECTED)
		{
			if (port.MuxSmState != MuxSmStates::DETACHED)
				nextMuxSmState = enterDetached(port);
		}
		else if (port.actorAttached && !port.PortEnabled &&
			((port.waitToRestoreTimer != port.waitToRestoreTime) ||
			 ((port.MuxSmState != MuxSmStates::ATTACHED_WTR) && (port.waitToRestoreTime == 0))))
		{
			nextMuxSmState = enterAttachedWtr(port);
		}
		//	else if (portGoodToGo && !port.partnerOperPortState.sync)
		else if (portGoodToGo && !port.partnerOperPortState.sync && (!port.actorOperPortState.sync || port.actorOperPortState.collecting))
		{
		//	if (port.MuxSmState != MuxSmStates::ATTACHED)
				nextMuxSmState = enterAttached(port);
		}
		else if (portGoodToGo && port.partnerOperPortState.sync &&
		//		!port.policy_coupledMuxControl && !port.partnerOperPortState.collecting && !port.actorOperPortState.collecting)
				!port.policy_coupledMuxControl && !port.partnerOperPortState.collecting &&
				(!port.actorOperPortState.collecting || port.actorOperPortState.distributing))
		{
			nextMuxSmState = enterCollecting(port);
		}
		else if (portGoodToGo && port.partnerOperPortState.sync &&
				(port.policy_coupledMuxControl || port.partnerOperPortState.collecting) && !port.actorOperPortState.distributing)
		{
			nextMuxSmState = enterCollDist(port);
		}
		// State-specific transitions:
		else switch (port.MuxSmState)
		{
		case MuxSmStates::DETACHED:
			if ((port.portSelected == selectedVals::SELECTED) && !port.actorAttached && port.Ready)
				nextMuxSmState = enterAttach(port);
			break;
		case MuxSmStates::ATTACH:
		case MuxSmStates::ATTACHED_WTR:
		case MuxSmStates::ATTACHED:
		case MuxSmStates::COLLECTING:
			break;
		case MuxSmStates::COLL_DIST:
			//		if (!port.policy_coupledMuxControl && !port.partnerOperPortState.collecting)
			//			nextMuxSmState = enterCollecting(port);
			break;
		case MuxSmStates::WAITING:
		case MuxSmStates::DISTRIBUTING:
		default:
			nextMuxSmState = enterDetached(port);
			break;
		}
	}
	


	if (nextMuxSmState != MuxSmStates::NO_STATE)
	{
		port.MuxSmState = nextMuxSmState;
		transitionTaken = true;
	}
	else {}   // no change to MuxSmState (or anything else) 

	return (transitionTaken);
}
/**/


AggPort::LacpMuxSM::MuxSmStates AggPort::LacpMuxSM::enterDetached(AggPort& port)
{
	if (SimLog::Debug > 3)
	{
		SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
		    << " is DETACHED from Aggregator "  << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
	}
	if (port.actorLacpVersion == 1)
	{
		detachMuxFromAggregator(port);
		port.actorOperPortState.sync = false;
		if (port.policy_coupledMuxControl)
		{
			port.actorOperPortState.collecting = false;
			disableCollectingDistributing(port);
			port.actorOperPortState.distributing = false;
		}
		else
		{
			disableDistributing(port);
			port.actorOperPortState.distributing = false;
			port.actorOperPortState.collecting = false;
			disableCollecting(port);
		}
		port.NTT = true;
		port.ReadyN = false;
		port.Ready = false;
	}
	else  // (port.actorLacpVersion > 1)
	{
		if (port.actorOperPortState.collecting)   // Not in standard -- other than debug messages does it make a difference?
			disableCollectingDistributing(port);
		port.actorOperPortState.distributing = false;
		port.actorOperPortState.collecting = false;
		if (port.actorOperPortState.sync)
			port.NTT = true;
		port.actorOperPortState.sync = false;
		detachMuxFromAggregator(port);
		port.waitToRestoreTimer = 0;
		port.Ready = false;
	}

	return (MuxSmStates::DETACHED);
}

AggPort::LacpMuxSM::MuxSmStates AggPort::LacpMuxSM::enterWaiting(AggPort& port)
{
	if ((SimLog::Debug > 3))
	{
		SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
			<< " is WAITING for Aggregator " << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
	}

	port.waitWhileTimer = AggregateWaitTime;         // Wait a little to see if other ports are attaching 
	port.ReadyN = (port.waitWhileTimer == 0);

	return (MuxSmStates::WAITING);
}

AggPort::LacpMuxSM::MuxSmStates AggPort::LacpMuxSM::enterAttach(AggPort& port)
{
	if ((SimLog::Debug > 3))
	{
		SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
			<< " is ATTACH for Aggregator " << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
	}

	attachMuxToAggregator(port);

	return (MuxSmStates::ATTACH);
}

AggPort::LacpMuxSM::MuxSmStates AggPort::LacpMuxSM::enterAttachedWtr(AggPort& port)
{
	if ((SimLog::Debug > 3))
	{
		SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
			<< " is ATTACHED_WTR for Aggregator " << port.actorPortAggregatorIdentifier;
		if (!port.wtrRevertiveMode)
			SimLog::logFile << " (non-revertive) ";
		SimLog::logFile << "  ***" << dec << endl;
	}

	disableCollectingDistributing(port);
	port.actorOperPortState.sync = false;
	port.actorOperPortState.collecting = false;
	port.actorOperPortState.distributing = false;
	port.waitToRestoreTimer = port.waitToRestoreTime;
	port.wtrRevertOK = port.wtrRevertiveMode;
//	port.NTT = true;                                  // Not in standard.  Does it make a difference since !Port_Enabled?

	return (MuxSmStates::ATTACHED_WTR);
}

AggPort::LacpMuxSM::MuxSmStates AggPort::LacpMuxSM::enterAttached(AggPort& port)
{
	if ((SimLog::Debug > 3))
	{
		SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
			<< " is ATTACHED to Aggregator " << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
	}
	if (port.actorLacpVersion == 1)
	{
		attachMuxToAggregator(port);
		port.actorOperPortState.sync = true;
		if (port.policy_coupledMuxControl)
		{
			if (port.actorOperPortState.collecting || port.actorOperPortState.collecting)  // why did I have this?  was it supposed to be something else?
			{
				port.actorOperPortState.collecting = false;
				disableCollectingDistributing(port);
				port.actorOperPortState.distributing = false;
			}
		}
		else
		{
			port.actorOperPortState.collecting = false;
			disableCollecting(port);
		}
		port.NTT = true;
	}
	else  // (port.actorLacpVersion > 1)
	{
		if (port.actorOperPortState.collecting)   // Not in standard -- other than debug messages does it make a difference?
			disableCollectingDistributing(port);
		port.actorOperPortState.sync = true;
		port.actorOperPortState.collecting = false;
		port.actorOperPortState.distributing = false;
		port.NTT = true;
	}

	return (MuxSmStates::ATTACHED);
}



AggPort::LacpMuxSM::MuxSmStates AggPort::LacpMuxSM::enterCollecting(AggPort& port)
{
	if ((SimLog::Debug > 3))
	{
		SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
			<< " is COLLECTING on Aggregator " << port.actorPortAggregatorIdentifier 
//			<< "  (partnerOperPortState = 0x" << (unsigned short)(port.partnerOperPortState.state) << " ) "
			<< "  ***" << dec << endl;
	}
	if (port.actorLacpVersion == 1)
	{
		enableCollecting(port);
		port.actorOperPortState.collecting = true;
		disableDistributing(port);
		port.actorOperPortState.distributing = false;
		port.NTT = true;
	}
	else  // (port.actorLacpVersion > 1)
	{
		if (!port.actorOperPortState.collecting)
		{
			port.NTT = true;
			enableCollecting(port);
		}
		else
			disableDistributing(port);
		// For new mux don't have to disable distributing since never pass through COLLECTING when going down,
		//    but do need to set Sync True since may go directly from ATTACH to COLLECTING
		// disableDistributing(port);
		port.actorOperPortState.sync = true;
		port.actorOperPortState.collecting = true;
		port.actorOperPortState.distributing = false;
	}

	return (MuxSmStates::COLLECTING);
}

AggPort::LacpMuxSM::MuxSmStates AggPort::LacpMuxSM::enterDistributing(AggPort& port)
{
	if ((SimLog::Debug > 3))
	{
		SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
			<< " is DISTRIBUTING on Aggregator " << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
	}

	port.actorOperPortState.distributing = true;
	enableDistributing(port);
//	port.NTT = true;

	return (MuxSmStates::DISTRIBUTING);
}

AggPort::LacpMuxSM::MuxSmStates AggPort::LacpMuxSM::enterCollDist(AggPort& port)
{
	if ((SimLog::Debug > 3))
	{
		SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
			<< " is COLL_DIST on Aggregator " << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
	}
	if (port.actorLacpVersion == 1)
	{
		port.actorOperPortState.distributing = true;
		enableCollectingDistributing(port);
		port.actorOperPortState.collecting = true;
		port.NTT = true;
	}
	else  // (port.actorLacpVersion > 1)
	{
		if (!port.actorOperPortState.collecting)
			enableCollectingDistributing(port);
		else 
			enableDistributing(port);
		port.actorOperPortState.sync = true;
		port.actorOperPortState.collecting = true;
		port.actorOperPortState.distributing = true;
		port.NTT = true;
	}

	return (MuxSmStates::COLL_DIST);
}


void AggPort::LacpMuxSM::attachMuxToAggregator(AggPort& port)
{
	port.actorAttached = true;    // would be set true when attachment process completes

}

void AggPort::LacpMuxSM::detachMuxFromAggregator(AggPort& port)
{
	port.actorAttached = false;   // set false when the detachment process is initiated

}

// so far all the rest of these work for both v1 and v2

void AggPort::LacpMuxSM::enableCollecting(AggPort& port)
{
	port.changePartnerOperDistAlg = true;
	port.changePortLinkState = true;
}

void AggPort::LacpMuxSM::disableCollecting(AggPort& port)
{
	// no function in simulation
	port.changePortLinkState = true;
}

void AggPort::LacpMuxSM::enableDistributing(AggPort& port)
{
//	if (!port.actorOperPortState.distributing)  // Don't need to test this since only one way into this state
	// If were going to add this test would have to set distributing true after calling enableDistributing

	port.changeActorDistributing = true;

	if ((SimLog::Debug > 0))
	{
		SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
			<< " is UP on Aggregator " << port.actorSystem.addrMid << ":" << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
	}
	cout << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
		<< " is UP on Aggregator " << port.actorSystem.addrMid << ":" << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
}

void AggPort::LacpMuxSM::disableDistributing(AggPort& port)
{
	if (port.actorOperPortState.distributing)
	{
		port.changeActorDistributing = true;  

		if ((SimLog::Debug > 0))
		{
			SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
				<< " is DOWN on Aggregator " << port.actorSystem.addrMid << ":" << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
		}
		cout << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
			<< " is DOWN on Aggregator " << port.actorSystem.addrMid << ":" << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
	}
}

void AggPort::LacpMuxSM::enableCollectingDistributing(AggPort& port)
{
	port.changeActorDistributing = true;
	port.changePartnerOperDistAlg = true;
	port.changePortLinkState = true;

	if ((SimLog::Debug > 0))
	{
		SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
			<< " is UP on Aggregator " << port.actorSystem.addrMid << ":" << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
	}
	cout << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
		<< " is UP on Aggregator " << port.actorSystem.addrMid << ":" << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;

}

void AggPort::LacpMuxSM::disableCollectingDistributing(AggPort& port)
{
	port.changeActorDistributing = true;  

	port.portOperConversationMask.reset();
	port.distributionConversationMask.reset();
	port.collectionConversationMask.reset();
	port.actorDWC = false;
	port.LinkNumberID = port.adminLinkNumberID;
	port.changePortLinkState = true;
	port.actorPartnerSync = (port.portOperConversationMask == port.partnerOperConversationMask);


	if ((SimLog::Debug > 0))
	{
		SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
			<< " is DOWN on Aggregator " << port.actorSystem.addrMid << ":" << port.actorSystem.addrMid << ":" << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
	}
	cout << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
		<< " is DOWN on Aggregator " << port.actorSystem.addrMid << ":" << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;

}


/**/

