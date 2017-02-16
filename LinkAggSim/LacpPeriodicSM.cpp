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

/**/
// void AggPort::LacpPeriodicSM::resetPeriodicSM(AggPort& port)
void AggPort::LacpPeriodicSM::reset(AggPort& port)
{
	port.PerSmState = enterNoPeriodic(port);
}

/**/
// void AggPort::LacpPeriodicSM::timerTickPeriodicSM(AggPort& port)
void AggPort::LacpPeriodicSM::timerTick(AggPort& port)
{
	if (port.periodicTimer > 0) port.periodicTimer--;
}
/**/

// int AggPort::LacpPeriodicSM::runPeriodicSM(AggPort& port, bool singleStep)
int AggPort::LacpPeriodicSM::run(AggPort& port, bool singleStep)
{
	bool transitionTaken = false;
	int loop = 0;

//	if (port.periodicTimer > 0) port.periodicTimer--;         // decrement timer
	do
	{
		transitionTaken = stepPeriodicSM(port);
		loop++;
	} while (!singleStep && transitionTaken && loop < 10);
	if (!transitionTaken) loop--;
	return (loop);
}


bool AggPort::LacpPeriodicSM::stepPeriodicSM(AggPort& port)
{
	PerSmStates nextPerSmState = PerSmStates::NO_STATE;
	bool transitionTaken = false;
	bool globalTransition = (port.PerSmState != PerSmStates::NO_PERIODIC) &&
		(!port.PortEnabled || !port.LacpEnabled || (!port.actorOperPortState.lacpActivity && !port.partnerOperPortState.lacpActivity));

	if (globalTransition)
		nextPerSmState = enterNoPeriodic(port);         // Global transition, but only take if will change something
	else switch (port.PerSmState)
	{
	case PerSmStates::NO_PERIODIC:
		if (port.PortEnabled && port.LacpEnabled && (port.actorOperPortState.lacpActivity || port.partnerOperPortState.lacpActivity))
			nextPerSmState = enterFastPeriodic(port);
		break;
	case PerSmStates::FAST_PERIODIC:
		if (port.periodicTimer == 0)
			nextPerSmState = enterPeriodicTx(port);
		else if (!port.partnerOperPortState.lacpShortTimeout)
			nextPerSmState = enterSlowPeriodic(port);
		break;
	case PerSmStates::SLOW_PERIODIC:
		if ((port.periodicTimer == 0) || port.partnerOperPortState.lacpShortTimeout)
			nextPerSmState = enterPeriodicTx(port);
		break;
	case PerSmStates::PERIODIC_TX:
		if (port.partnerOperPortState.lacpShortTimeout)
			nextPerSmState = enterFastPeriodic(port);
		else // (!partnerOperPortState.lacpShortTimeout)
			nextPerSmState = enterSlowPeriodic(port);
		break;
	default:
		nextPerSmState = enterNoPeriodic(port);
		break;
	}

	if (nextPerSmState != PerSmStates::NO_STATE)
	{
		port.PerSmState = nextPerSmState;
		transitionTaken = true;
	}
	else {}   // no change to PerSmState (or anything else) 

	return (transitionTaken);
}


AggPort::LacpPeriodicSM::PerSmStates AggPort::LacpPeriodicSM::enterNoPeriodic(AggPort& port)
{
	port.periodicTimer = 0;
	port.LacpTxEnabled = false;

	return (PerSmStates::NO_PERIODIC);
}


AggPort::LacpPeriodicSM::PerSmStates AggPort::LacpPeriodicSM::enterFastPeriodic(AggPort& port)
{
	port.periodicTimer = fastPeriodicTime;
	port.LacpTxEnabled = true;

	return (PerSmStates::FAST_PERIODIC);
}


AggPort::LacpPeriodicSM::PerSmStates AggPort::LacpPeriodicSM::enterSlowPeriodic(AggPort& port)
{
	port.periodicTimer = slowPeriodicTime;

	return (PerSmStates::SLOW_PERIODIC);
}


AggPort::LacpPeriodicSM::PerSmStates AggPort::LacpPeriodicSM::enterPeriodicTx(AggPort& port)
{
	port.NTT = true;
	if (SimLog::Debug > 6)
	{
		SimLog::logFile << "Time " << SimLog::Time << ":   Device:Port " << hex << port.actorSystem.addrMid
			<< ":" << port.actorPort.num << " NTT: Periodic " << dec << endl;
	}

	return (PerSmStates::PERIODIC_TX);
}
