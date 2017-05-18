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

/*
bool AggPort::LacpMuxSM::stepMuxSM(AggPort& port)
{
	MuxSmStates nextMuxSmState = MuxSmStates::NO_STATE;
	bool transitionTaken = false;

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


	if (nextMuxSmState != MuxSmStates::NO_STATE)
	{
		port.MuxSmState = nextMuxSmState;
		transitionTaken = true;
	}
	else {}   // no change to MuxSmState (or anything else) 

	return (transitionTaken);
}
/**/

bool AggPort::LacpMuxSM::stepMuxSM(AggPort& port)
{
	MuxSmStates nextMuxSmState = MuxSmStates::NO_STATE;
	bool transitionTaken = false;

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
	else if (portGoodToGo && !port.partnerOperPortState.sync)
	{
		if (port.MuxSmState != MuxSmStates::ATTACHED)
			nextMuxSmState = enterAttached(port);
	}
	else if (portGoodToGo && port.partnerOperPortState.sync &&
		!port.policy_coupledMuxControl && !port.partnerOperPortState.collecting && !port.actorOperPortState.collecting)
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
		if (!port.policy_coupledMuxControl && !port.partnerOperPortState.collecting)
			nextMuxSmState = enterCollecting(port);
		break;
	case MuxSmStates::WAITING:
	case MuxSmStates::DISTRIBUTING:
	default:
		nextMuxSmState = enterDetached(port);
		break;
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

	// For 2017 Mux machine, comment out next line and add the following two
	// port.ReadyN = false;  // Otherwise selection logic could set Ready at same time as Selected, before Mux enters Waiting and starts timer.
	port.Ready = false;
	port.waitToRestoreTimer = 0;

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

AggPort::LacpMuxSM::MuxSmStates AggPort::LacpMuxSM::enterAttached(AggPort& port)
{
	if ((SimLog::Debug > 3))
	{
		SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
			<< " is ATTACHED to Aggregator " << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
	}

	// for new Mux have already attached mux to aggregator, so comment next line out
	// attachMuxToAggregator(port);
	port.actorOperPortState.sync = true;
	// for new Mux, always disable collecting and distributing in ATTACHED, so change next line to simply "if (true)"
	// if (port.policy_coupledMuxControl)
	if (true)
	{
		if (port.actorOperPortState.collecting || port.actorOperPortState.collecting)
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

	enableCollecting(port);
	port.actorOperPortState.collecting = true;
	// For new mux don't have to disable distributing since never pass through COLLECTING when going down,
	//    but do need to set Sync True since may go directly from ATTACH to COLLECTING
	// disableDistributing(port);
	port.actorOperPortState.sync = true;
	port.actorOperPortState.distributing = false;
	port.NTT = true;

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

	port.actorOperPortState.distributing = true;
	enableCollectingDistributing(port);
	port.actorOperPortState.collecting = true;
	// For new mux set sync true.  Doesn't seem necessary since doesn't seem like you should be able to get here directly from
	//     ATTACH, but won't hurt.
	port.actorOperPortState.sync = true;
	port.NTT = true;

	return (MuxSmStates::COLL_DIST);
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
	port.waitToRestoreTimer = port.waitToRestoreTime;
	port.wtrRevertOK = port.wtrRevertiveMode;
	port.NTT = true;                                  // Not in standard.  Does it make a difference since !Port_Enabled?

	return (MuxSmStates::ATTACHED_WTR);
}


void AggPort::LacpMuxSM::attachMuxToAggregator(AggPort& port)
{
	port.actorAttached = true;
}

void AggPort::LacpMuxSM::detachMuxFromAggregator(AggPort& port)
{
	port.actorAttached = false;
}

void AggPort::LacpMuxSM::enableCollecting(AggPort& port)
{
	port.changePartnerOperDistAlg = true;
}

void AggPort::LacpMuxSM::disableCollecting(AggPort& port)
{
	// no function in simulation
}

void AggPort::LacpMuxSM::enableDistributing(AggPort& port)
{
//	if (!port.actorOperPortState.distributing)  // Don't need to test this since only one way into this state
	// If were going to add this test would have to set distributing true after calling enableDistributing

	port.changeActorOperDist = true;

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
		port.changeActorOperDist = true;  

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
	port.changeActorOperDist = true;
	port.changePartnerOperDistAlg = true;

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
	port.changeActorOperDist = true;  

	if ((SimLog::Debug > 0))
	{
		SimLog::logFile << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
			<< " is DOWN on Aggregator " << port.actorSystem.addrMid << ":" << port.actorSystem.addrMid << ":" << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;
	}
	cout << "Time " << SimLog::Time << hex << ":  *** Port " << port.actorSystem.addrMid << ":" << port.actorPort.num
		<< " is DOWN on Aggregator " << port.actorSystem.addrMid << ":" << port.actorPortAggregatorIdentifier << "  ***" << dec << endl;

}


/**/

