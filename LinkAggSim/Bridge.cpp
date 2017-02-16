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
#include "Bridge.h"


Bridge::Bridge(unsigned short devNum, unsigned short sysNum, int nPorts)
	: Component(ComponentTypes::BRIDGE)
{
	vlanType = 0;   // Type = 0 means MAC Bridge.  Can change to CVlanEthertype or SVlanEthertype
	SystemId.id = defaultBrdgAddr + (0x10000 * devNum) + (0x1000 * sysNum);

	for (int i = 0; i < nPorts; i++)
	{
		bPorts.push_back(make_unique<BridgePort>());
	}
//	cout << "Bridge Constructor called:  BridgePort count = " << bPorts.size() << endl;
}


Bridge::~Bridge()
{
	bPorts.clear();
//	cout << "Bridge Destructor called." << endl;
}

void Bridge::reset()
{
	//TODO:
}


void Bridge::timerTick()
{
	if (!suspended)
	{
		//TODO:
	}
}


void Bridge::run(bool singleStep)
{
	if (!suspended)
	{
		for (auto& ingress : bPorts)                            // For each BridgePort that may have an ingress Frame
		{
			unique_ptr<Frame> thisFrame = nullptr;              //      Check for an ingress Frame
			if (ingress->pIss)
			{
				thisFrame = std::move(ingress->pIss->Indication());
			}
			if (thisFrame)                                      //      If there is an ingress frame
			{
				for (auto& egress : bPorts)
				{
					if ((ingress != egress) &&
						egress->pIss && egress->pIss->getOperational())  // Then at every other operational BridgePort
					{
						unique_ptr<Frame> frameCopy = make_unique<Frame>(*thisFrame);  // Transmit a copy of the frame
						egress->pIss->Request(move(frameCopy));
					}
				}
			}
		}
	}
}



/**/


BridgePort::BridgePort()
{
	pIss = nullptr;

//	cout << "BridgePort Constructor called." << endl;
//	SimLog::logFile << "BridgePort Constructor called." << endl;
}

/*
BridgePort::BridgePort(const BridgePort& copySource)
{
	pIss = copySource.pIss;  //TODO: This will not work if make pIss a unique_ptr.  Investigate if a move constructor would be better.

//	cout << "BridgePort Copy Constructor called." << endl;
//	SimLog::logFile << "BridgePort Copy Constructor called." << endl;
}
/**/

BridgePort::~BridgePort()
{
	pIss = nullptr;

//	cout << "BridgePort Destructor called." << endl;
//	SimLog::logFile << "BridgePort Destructor called." << endl;
}

