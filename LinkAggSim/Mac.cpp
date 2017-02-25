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
#include "Mac.h"

/**/
Iss::Iss()
{
	enabled = false;
	operPointToPoint = true;
	adminPointToPoint = adminValues::AUTO;
//	std::cout << "Iss Constructor called." << std::endl;
}

Iss::~Iss()
{
//	std::cout << "Iss Destructor called." << std::endl;
}

bool Iss::getEnabled() const
{
	return enabled;
}

bool Iss::getPointToPoint() const
{
	return operPointToPoint;
}

bool Iss::getOperational() const
{
	return enabled;
}
/**/



IssQ::IssQ()   // default constructor
{
//	std::cout << "IssQ Constructor called." << std::endl;
}

IssQ::~IssQ()
{
	while (!requests.empty())        // Flush all frames in flight
		requests.pop();
	while (!indications.empty())     // Flush all frames already delivered 
		indications.pop();

	//	std::cout << "IssQ Destructor called." << std::endl;
}

void IssQ::Request(unique_ptr<Frame> pFrameIn)
{
	pFrameIn->TimeStamp = SimLog::Time;   // Use frame time stamp to measure residence time in queue
	if (getOperational()) 
		requests.push(std::move(pFrameIn));  // A request sent to a non-operational ISS will be discarded.
}

unique_ptr<Frame> IssQ::Indication() 
{
//		if (getOperational() && !indications.empty())   // Disallow indications from a non-operational ISS?
		if (!indications.empty())
			{
			unique_ptr<Frame> pFrame = std::move(indications.front());
			indications.pop();
			return (std::move(pFrame));
		} else {
			return (nullptr);
		}
}

/**/

Component::Component(ComponentTypes thisType)
	: type(thisType)
{
	suspended = false;
	//	cout << "            Component Constructor called" << endl;
}

/*
Component::Component(Component& copySource)
{
//	cout << "            Component Copy Constructor called" << endl;
}
/**/

Component::~Component()
{
	//	cout << "            Component Destructor called" << endl;
}

ComponentTypes Component::getCompType() const
{
	return (type);
}

bool Component::getSuspended() const
{
	return (suspended);
}

void Component::setSuspended(bool val)
{
	suspended = val;
}

/**/


Mac::Mac()
	: Component(ComponentTypes::MAC)
{
	enabled = true;
	macAddress = defaultOUI;
//	macId.id = 0;
	linkPartner = nullptr;
//	std::cout << "Mac Constructor called" << std::endl;
};

Mac::Mac(unsigned short dev, unsigned short sap)
	: Component(ComponentTypes::MAC)
{
	enabled = true;
	macAddress = defaultOUI + (dev * 0x10000) + sap;
	macId.dev = dev;
	macId.sap = sap;
//	std::cout << "Mac Constructor called:  device = " << macId.dev << "  sapId = " << macId.sap << std::endl;
}


/*
Mac::Mac(Mac& copySource)
	: Component(copySource.type)
{
	//TODO:  copy Mac stuff or explicitly disable copy constructor?
//	std::cout << "Mac Copy Constructor called." << std::endl;
}
/**/

Mac::~Mac()
{
	if (linkPartner)
		Disconnect(linkPartner);
	//	std::cout << "Mac Destructor called:  device = " << macId.dev << "  sapId = " << macId.sap << std::endl;
};

void Mac::setEnabled(bool val)
{
	enabled = val;
}

bool Mac::getOperational() const
{
	return (enabled && linkPartner && linkPartner->enabled);
}

void Mac::Request(unique_ptr<Frame> pFrameIn)
{
	pFrameIn->TimeStamp = SimLog::Time;   // Use frame time stamp to measure residence time in queue
	if (getOperational() && !suspended)
		requests.push(std::move(pFrameIn));  // A request sent to a non-operational ISS will be discarded.
}

unique_ptr<Frame> Mac::Indication()
{
	//		if (getOperational() && !indications.empty())   // Disallow indications from a non-operational ISS?
	if (!indications.empty() && !suspended)
	{
		unique_ptr<Frame> pFrame = std::move(indications.front());
		indications.pop();
		return (std::move(pFrame));
	}
	else {
		return (nullptr);
	}
}

void Mac::setAdminPointToPoint(adminValues val)
{
	adminPointToPoint = val;
	switch (adminPointToPoint)
	{
	case adminValues::FORCE_FALSE:
		operPointToPoint = false;
		break;
	case adminValues::AUTO:          // All MAC connections in this simulation are made with point-to-point links
	case adminValues::FORCE_TRUE:
	default:
		operPointToPoint = true;
		break;
	}
}

adminValues Mac::getAdminPointToPoint() const
{
	return (adminPointToPoint);
}

unsigned long long Mac::getMacAddress() const
{
	return (macAddress);
}


unsigned short Mac::getSapId() const
{
	return macId.sap;
}

void Mac::updateMacSystemId(unsigned long long value)
{
	macId.sap  = (macId.sap  & 0xffffffff0fff) | (value & 0x00000000f000);  // replace sysNum portion of SAP id
	macAddress = (macAddress & 0xfffff0ff0fff) | (value & 0x000000f0f000);  // replace device type and sysNum portion of MAC address
}

unsigned short Mac::getDevNum() const
{
	return macId.dev;
}

unsigned long Mac::getMacId() const
{
	return macId.id;
}

void Mac::reset()
{
	while (!requests.empty())        // Flush all frames in flight
		requests.pop();
	while (!indications.empty())     // Flush all frames already delivered 
		indications.pop();
}

void Mac::timerTick()
{
	if (!suspended) 
	{ 
		// Nothing to do
	}
}

void Mac::run(bool singleStep)
{
	if (!suspended) 
	{ 
		// Nothing to do
	}
}


void Mac::Transmit()
{
	if (!requests.empty() && !suspended)
	{
		if (getOperational())
		{
			unsigned short txTime = (requests.front())->TimeStamp;
			if (SimLog::Time >= (txTime + linkDelay))
			{
				unique_ptr<Frame> pTempFrame = std::move(requests.front());     // move the pointer to the frame from the requests queue to the temp variable
				requests.pop();                                                 // pop the null pointer left on the queue after the move
				if (linkPartner && linkPartner->enabled && !(linkPartner->suspended))
				{
					linkPartner->indications.push(std::move(pTempFrame));       // push the pointer to the frame onto the other MAC indications queue
				}
			}
		}
		else  // can get here if this Mac or its Partner were disabled without disconnecting
		{
			while (!requests.empty())        // Flush all frames in flight
				requests.pop();
//			while (!indications.empty())     // Flush all frames already delivered (is this desirable?)
//				indications.pop();
		}
	}
}


void Mac::Connect(shared_ptr<Mac> macA, shared_ptr<Mac> macB, unsigned short delay)
{
	if (SimLog::Debug > 0) SimLog::logFile << endl << "Time " << SimLog::Time << ":    ***** Connecting  " 
		<< hex << "  MAC " << macA->macId.dev << ":" << macA->macId.sap << " to MAC "
		<< macB->macId.dev << ":" << macB->macId.sap << " *****" << dec << endl;
	cout << endl << "Time " << SimLog::Time << ":    ***** Connecting  " 
		<< hex << "  MAC " << macA->macId.dev << ":" << macA->macId.sap << " to MAC "
		<< macB->macId.dev << ":" << macB->macId.sap << " *****" << dec << endl;

	if (macA->linkPartner) Disconnect(macA);
	if (macB->linkPartner) Disconnect(macB);

	macA->linkPartner = macB;
	macB->linkPartner = macA;
	macA->linkDelay = delay;
	macB->linkDelay = delay;
}

void Mac::Disconnect(shared_ptr<Mac> macA)
{
	if (macA->linkPartner)
	{
		if (SimLog::Debug > 0) SimLog::logFile << endl << "Time " << SimLog::Time << ":    ***** Disconnecting  "
			<< hex << "  MAC " << macA->macId.dev << ":" << macA->macId.sap << " to MAC "
			<< macA->linkPartner->macId.dev << ":" << macA->linkPartner->macId.sap << " *****" << dec << endl;
		cout << endl << "Time " << SimLog::Time << ":    ***** Disconnecting  "
			<< hex << "  MAC " << macA->macId.dev << ":" << macA->macId.sap << " to MAC "
			<< macA->linkPartner->macId.dev << ":" << macA->linkPartner->macId.sap << " *****" << dec << endl;

		if (macA->linkPartner->linkPartner == macA)    // if Partner's Partner is this Mac
		{
			while (!(macA->linkPartner->requests.empty()))        // Flush all Partner's frames in flight
				macA->linkPartner->requests.pop();
//			while (!(macA->linkPartner->indications.empty()))     // Flush all frames already delivered to Partner (is this desirable?)
//				macA->linkPartner->indications.pop();
			macA->linkPartner->linkPartner = nullptr;             // then clear linkPartner's link
		}
		while (!(macA->requests.empty()))        // Flush all frames in flight
			macA->requests.pop();
//		while (!(macA->indications.empty()))     // Flush all frames already delivered (is this desirable?)
//			macA->indications.pop();
		macA->linkPartner = nullptr;             // clear this Mac's link
	}
}




