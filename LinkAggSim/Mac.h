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
#include "Frame.h"
// #include "queue.h"


/*
*  Iss is an interface between the client of a service and the provider of that service.  
*    It is a blend of the MAC Service (MS) and Internal Sublayer Service (ISS) specified in IEEE Std 802.1AC, 
*    and the Enhanced Internal Sublayer Service (EISS) specified in IEEE Std 802.1Q.
*    This simulation does not distinguish between the MS, ISS, EISS except in the values of the interface  
*    parameters and the request/indication service primitive (e.g. egress/ingress Frame) parameters.
*    It follows the addressing model of IEEE Std 802.3 where the service provider (e.g. an 802.3 MAC) has a
*    MAC address that is used as the source address in frames generated by the client (e.g. the MAC client).
*  When inherited by a class providing a Service, the Iss creates a Service Access Point (SAP) that can 
*    be pointed to by a variable in the class that is the client of the service.  For example, a MAC class 
*    (provider of the MAC service) inherits the EissIss class to create a SAP that can be pointed to by a
*    Bridge Port (client of the service).
*/

class Iss
{
public:
	Iss();     // Constructor
	~Iss();    // Destructor
	Iss(Iss& copySource) = delete;             // Disable copy constructor
	Iss& operator= (const Iss&) = delete;      // Disable assignment operator

	virtual bool getEnabled() const;                       // Allows client to determine if the interface is enabled.
	virtual bool getPointToPoint() const;                  // Allows client to determine if the service connects to only one other SAP.
	virtual bool getOperational() const;                   // Allows client to determine if the interface is operational.
	
	// Pure virtual functions must be provided by the derived class
	virtual unsigned long long getMacAddress() const = 0;  // Allows client to determine the MAC address of the interface.
	virtual unique_ptr<Frame> Indication() = 0;            // Returns an Indication (ingress frame) to client; Returns nullptr if no indication
	virtual void Request(unique_ptr<Frame> pFrameIn) = 0;  // Accepts a Request f (egress frame) from client

protected:
	bool enabled;         // Read-only by the client;  the derived class determines when/how this variable is set/cleared.
	bool operPointToPoint;    // Read-only by the client;  the derived class determines when/how this variable is set/cleared.
	adminValues adminPointToPoint;  // Not accessible by the client;  the derived class determines when/how this variable is set/cleared.
};


/*
*  IssQ is a variant of an Iss that provides indication and request queues at the SAP.
*/

class IssQ : public Iss
{
public:
	IssQ();
	~IssQ();
	IssQ(IssQ& copySource) = delete;             // Disable copy constructor
	IssQ& operator= (const IssQ&) = delete;      // Disable assignment operator

	virtual void Request(unique_ptr<Frame> pFrameIn) override;
	virtual unique_ptr<Frame> Indication() override;

protected:         
	std::queue<unique_ptr<Frame>> requests;     
	std::queue<unique_ptr<Frame>> indications;
};


union macIdentifier
{
	unsigned long id;
	struct
	{
		unsigned short sap;
		unsigned short dev;
	};
};

/*
*  Component is an abstract class inherited by all functional elements in the simulation.
*    Every component has the basic reset/timerTick/run methods that allow the component behavior to be simulated.
*/
class Component
{
public:
	Component(ComponentTypes type);     // Constructor
	~Component();                       // Destructor
	Component(Component& copySource) = delete;             // Disable copy constructor
	Component& operator= (const Component&) = delete;      // Disable assignment operator

	virtual ComponentTypes getCompType() const;            // Allows simulation to determine the component type.
	virtual bool getSuspended() const;                     // Allows simulation to determine if the component is suspended.
	virtual void setSuspended(bool val);                   // Allows simulation to suspend/release the component

	// Pure virtual functions must be provided by the derived class
	virtual void reset() = 0;                  // Resets the operational variables of the component (whether or not suspended).
	//     Does not reset administrative variables to default settings.
	virtual void timerTick() = 0;              // Decrements any timers associated with this component (when not suspended).
	virtual void run(bool singleStep) = 0;     // Executes operational methods of the component (when not suspended).

protected:
	ComponentTypes type;      // Read-only by the simulation;  set by the constructor
	bool suspended;           // Read-write by the simulation;  inhibits timerTick() and run() operations
	//    Suspending, reseting, and releasing simulates a power-cycle or reboot of the component
};



/*
*   A Mac provides means of sending Frames between Devices.
*   By inheriting IssQ, a Mac object provides an ISS SAP (with request and indication queues) to its client (e.g. a 
*      End Station, Bridge Port, Link Aggregation Port, etc.).
*   The Mac class includes static members for connecting and disconnecting Links between Macs, and for transmitting
*      Frames across those Links.  Per the IEEE 802 Client/Service model, a service request from a client at the SAP
*      of one MAC results in a service indication (with identical parameters) provided to the client at the SAP of the
*      connected MAC after a Link specific time delay.
*   The Mac class inherits both an enabled (from IssQ) and suspended (from Component) variable.  
*      A Mac will not transmit or receive while suspended (however it may continue to receive when in a suspended Device).
*          Being suspended does not automatically make a Mac non-operational.
*      When disabled a Mac is non-operational and will not transmit or receive.
*          A disabled Mac is detected by its partner, and its partner will also become non-operational.
*/

class Mac : public Component, public IssQ
{
public:
	Mac();
	Mac(unsigned short dev, unsigned short sap);
	~Mac();    
	Mac(Mac& copySource) = delete;             // Disable copy constructor
	Mac& operator= (const Mac&) = delete;      // Disable assignment operator

	virtual void Request(unique_ptr<Frame> pFrameIn) override;
	virtual unique_ptr<Frame> Indication() override;
	virtual bool getOperational() const override;
	virtual void setEnabled(bool val);                    // Provides administrative control over the enabled parameter
	virtual void setAdminPointToPoint(adminValues val);   // Provides administrative control over the operPointToPoint parameter
	virtual adminValues getAdminPointToPoint() const;
	virtual unsigned long long getMacAddress() const;
	virtual unsigned short getSapId() const;              // Identifies this SAP (unique within a Device)
	virtual unsigned short getDevNum() const;             // Identifies the Device containing this MAC
	virtual unsigned long getMacId() const;               // Union of the Device Number and SAP Identifier
	virtual void updateMacSystemId(unsigned long long value);

	virtual void reset() override;
	virtual void timerTick() override;
	virtual void run(bool singleStep) override;

	virtual void Transmit();       
	  
	static void Connect(shared_ptr<Mac> macA, shared_ptr<Mac> macB, unsigned short delay = 0);
	static void Disconnect(shared_ptr<Mac> macA);

protected:
	unsigned long long macAddress;
	macIdentifier macId;

	shared_ptr<Mac> linkPartner;
	unsigned short linkDelay;
};
