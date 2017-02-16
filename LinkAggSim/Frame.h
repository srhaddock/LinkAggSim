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

/*
*   The structure of a Frame in this simulation is a Frame header that includes a pointer to the first Sdu in a chain
*       of one or more Sdus.  The final Sdu in the chain has a nullptr for the pointer to the next Sdu.
*   The Frame header includes the parameters of ISS/EISS request/indication service primitives described in IEEE Std 802.1AC
*       and IEEE 802.1Q.  These parameters include the destination MAC address, source MAC address, VLAN_identifier, priority,
*       drop_eligible, etc. 
*   The Sdu base class includes an EtherType and a SubType (which is zero for EtherTypes that don't support sub types) to identify
*       the derived class.  Derived classes add parameters specific to the derived type (e.g. VLAN tag, LACPDU, DRCPDU, ...).
*       There is no difference between a "tag", a "Protocol Data Unit (PDU)", and a "Service Data Unit (SDU)" as far as this
*       simulation is concerned.
*   A Frame object is instantiated using "std::make_unique<Frame>" and is accessed via a "std::unique_ptr<Frame>" (or a reference).
*       An object of a class derived from Sdu is created using, for example, "std::make_shared<Lacpdu>" and is accessed via a 
*       a "std::shared_ptr<Lacpdu>" or "std::shared_ptr<Sdu>" (or a reference). This means there is a single "owner" of any Frame,
*       and that Frame can be modified by inserting or removing "tags" without affecting other Frames in the simulation.  When a 
*       Frame is replicated (for example within a Bridge), a "shallow" copy is made.  This means the pointer to the Sdu in the 
*       Frame header is copied, but the Sdu itself is not copied.  It is therefore critical that Sdus are not modified once they
*       created, or it could result in unintentional changes to other Frames elsewhere in the simultated network.
*   Frames propagate through the simulated network by moving the unique pointer using the "transmit()" method of the Mac
*       (to move between Devices), and the "indicate()" and "request()" methods of classes derived from the Iss base class 
*       (to move between Components within a Device).
*/

class Sdu
{
public:
	Sdu(unsigned short sduEtherType = 0, unsigned short sduSubType = 0);
	Sdu(const Sdu& copySource);  // Copy constructor
	~Sdu();

	int TimeStamp;

	unsigned short getEtherType() const;
	unsigned short getSubType() const;
	const Sdu& getNextSdu() const;
	unsigned short getNextEtherType() const;
	unsigned short getNextSubType() const;

protected:
	unsigned short etherType;
	unsigned short subType;
	std::shared_ptr<Sdu> pNextSdu;

	friend class Frame;
};


/**/
class Frame
{
public:
	Frame(unsigned long long frameDA = 0, unsigned long long frameSA = 0, shared_ptr<Sdu> pframeSdu = nullptr);
	Frame(const Frame& frameCopy);      // copy constructor
	//	Frame(const Frame&& frameCopy);     // move constructor
	~Frame();

	int TimeStamp;


	// Service Primitive parameters
	unsigned long long MacDA;
	unsigned long long MacSA;
	unsigned short VlanIdentifier;  // Always zero in frames at an ISS; Always non-zero in frames at an EISS
	unsigned short Priority;
	bool DropEligible;
	//	unsigned short sapIdentifier;         //TODO:  ISS primitive SAP Identifier
	//	unsigned long connectionIdentifier;   //TODO:  ISS primitive Connection Identifier

	const Sdu& getNextSdu() const;
	unsigned short Frame::getNextEtherType() const;
	unsigned short Frame::getNextSubType() const;
	unique_ptr<Frame> InsertTag(shared_ptr<Sdu> pNewSdu) const;
	unique_ptr<Frame> Frame::RemoveTag() const;
	//	void Frame::PrintSduList() const;
	void Frame::PrintFrameHeader() const;

protected:
	std::shared_ptr<Sdu> pNextSdu;
};

/**/
union vlanControlWord
{
	unsigned short tci;           // VLAN Tag Control Information
	struct 
	{
		unsigned short id : 12;   //  0..11  Vlan Identifier
		unsigned short de :  1;   //     12  Drop Eligible  (can't be bool because then won't get into tci)
		unsigned short pri : 3;   // 13..15  Priority
	};
};

class VlanTag : public Sdu  
{
public:
	VlanTag(unsigned short type = CVlanEthertype, unsigned short identifier = 0, unsigned short priority = 0, bool dropEligible = false);
	VlanTag(const VlanTag& copySource);  // Copy constructor
	~VlanTag();

	vlanControlWord Vtag;
	static const VlanTag& getVlanTag(Frame& taggedFrame);        // Returns a constant reference to the Vlan tag

};


/**/


class TestSdu : public Sdu
{
public:
	TestSdu(int scratchData = 0);
	~TestSdu();

	static const TestSdu& getTestSdu(Frame& testFrame);        // Returns a constant reference to the Test Sdu
	int scratchPad;
};
/**/

