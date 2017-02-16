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
#include "Frame.h"

/**/
Sdu::Sdu(unsigned short sduEtherType, unsigned short sduSubType)
	: etherType(sduEtherType), subType(sduSubType)
{
	TimeStamp = SimLog::Time;
	pNextSdu = nullptr;
	//	cout << "            Sdu Constructor called" << endl;
}

Sdu::Sdu(const Sdu& copySource)  // Copy constructor
{
	etherType = copySource.etherType;
	subType = copySource.subType;
	TimeStamp = copySource.TimeStamp;  // Sdu TimeStamp is copied so retains time at which contents of SDU were current
	pNextSdu = copySource.pNextSdu;
	//	cout << "***** Sdu Copy Constructor executed *****" << endl;
}

Sdu::~Sdu()
{
	pNextSdu = nullptr;

	//	cout << "            Sdu Destructor called" << endl;
}

const shared_ptr<Sdu> pNullSdu = make_shared<Sdu>(0, 0);
const Sdu& NullSdu = *pNullSdu;

unsigned short Sdu::getEtherType() const
{
	return (etherType);
}

unsigned short Sdu::getSubType() const
{
	return (subType);
}

const Sdu& Sdu::getNextSdu() const
{
	if (pNextSdu == nullptr)
	{
		return(NullSdu);
	}
	return (*pNextSdu);
}

unsigned short Sdu::getNextEtherType() const
{
	if (pNextSdu == nullptr)
	{
		return (0);
	}
	return (pNextSdu->etherType);
}

unsigned short Sdu::getNextSubType() const
{
	if (pNextSdu == nullptr)
	{
		return (0);
	}
	return (pNextSdu->subType);
}





Frame::Frame(unsigned long long frameDA, unsigned long long frameSA, shared_ptr<Sdu> pframeSdu)
	: MacDA(frameDA), MacSA(frameSA), pNextSdu(pframeSdu)
{
	TimeStamp = SimLog::Time;   // Frame TimeStamp is initially time of construction; may be overwritten when queued
	VlanIdentifier = 0;
	Priority = 0;
	DropEligible = false;
	//	cout << "Frame constructor called" << endl;
}

Frame::Frame(const Frame& CopySource)
{
	TimeStamp = SimLog::Time;   // Frame TimeStamp is initially time of construction of this copy; may be overwritten when queued
	MacDA = CopySource.MacDA;
	MacSA = CopySource.MacSA;
	VlanIdentifier = CopySource.VlanIdentifier;
	Priority = CopySource.Priority;
	DropEligible = CopySource.DropEligible;
	pNextSdu = CopySource.pNextSdu;           // shallow copy is fine
	//	cout << "Frame copy constructor called" << endl;
}

/*
Frame::Frame(const Frame&& frameCopy)
{
//	cout << "Frame move constructor called" << endl;

}

/**/
Frame::~Frame()
{
	pNextSdu = nullptr;

	//	cout << "Frame destructor called" << endl;
}

/**/
const Sdu& Frame::getNextSdu() const
{
	if (pNextSdu == nullptr)
	{
		return(*pNullSdu);
	}
	return (*pNextSdu);
}

unsigned short Frame::getNextEtherType() const
{
	if (pNextSdu == nullptr)
	{
		return (0);
	}
	return (pNextSdu->etherType);
}

unsigned short Frame::getNextSubType() const
{
	if (pNextSdu == nullptr)
	{
		return (0);
	}
	return (pNextSdu->subType);
}

unique_ptr<Frame> Frame::InsertTag(shared_ptr<Sdu> pNewSdu) const
{
	unique_ptr<Frame> pNewFrame = make_unique<Frame>(*this);
	pNewSdu->pNextSdu = pNewFrame->pNextSdu;
	pNewFrame->pNextSdu = pNewSdu;
	return (std::move(pNewFrame));
}

unique_ptr<Frame>  Frame::RemoveTag() const
{
	//	shared_ptr<Sdu> pTag = pNextSdu;
	unique_ptr<Frame> pNewFrame = make_unique<Frame>(*this);
	if (this->pNextSdu != nullptr)
	{
		pNewFrame->pNextSdu = this->pNextSdu->pNextSdu;
	}
	//	pTag->pNextSdu = nullptr;
	return(std::move(pNewFrame));
}


/*

void Frame::PrintSduList() const
{
cout << std::hex << "Frame at " << this << "  with SA " << MacSA;
if (pNextSdu == nullptr)
{
cout << "  has no SDUs." << std::dec << endl;
}
else
{
cout << "  has SDUs:" << endl;
shared_ptr<Sdu> pSdu = pNextSdu;
while (pSdu != nullptr)
{
cout << "    SDU at " << pSdu << "  with type = " << pSdu->etherType << endl;
pSdu = pSdu->pNextSdu;
}
}
}
/**/

void Frame::PrintFrameHeader() const
{
	SimLog::logFile << hex << MacDA << ":" << MacSA;
	shared_ptr<Sdu> pSdu = pNextSdu;
	while (pSdu)
	{
		if (pSdu->etherType == CVlanEthertype) SimLog::logFile << ":CVtag=" << ((VlanTag&)(*pSdu)).Vtag.tci;
		else if (pSdu->etherType == SVlanEthertype) SimLog::logFile << ":SVtag=" << ((VlanTag&)(*pSdu)).Vtag.tci;
		else SimLog::logFile << ":" << pSdu->etherType << ":" << (short)pSdu->subType;
		pSdu = pSdu->pNextSdu;
	}
	SimLog::logFile << dec;
}

/**/
VlanTag::VlanTag(unsigned short type, unsigned short identifier, unsigned short priority, bool dropEligible) 
	: Sdu(type, 0)
//	, Vtag.id(identifier), Vtag.pri(priority), Vtag.de(dropEligible)   // doesn't seem to like ".id" in initializer list
{
	Vtag.id = identifier;
	Vtag.pri = priority;
	Vtag.de = dropEligible;
//	cout << "        VlanTag Constructor called" << endl;
}

VlanTag::VlanTag(const VlanTag& copySource)  // Copy constructor
	: Sdu(copySource)
{
	Vtag = copySource.Vtag;
//	cout << "***** VlanTag Copy Constructor executed *****" << endl;
}

VlanTag::~VlanTag()
{
//	cout << "        VlanTag Destructor called" << endl;
}

const VlanTag& VlanTag::getVlanTag(Frame& taggedFrame)        // Returns a constant reference to the Vlan tag
{
	// Will generate an error if getNextEtherType and getNextSubType were not validated before calling 

	return((const VlanTag&)taggedFrame.getNextSdu());
	//	return ((const Lacpdu&)*(taggedFrame.pNextSdu));
}

/**/
TestSdu::TestSdu(int scratchData) 
	: Sdu(PlaypenEthertypeA, 1), scratchPad(scratchData)
{
//	cout << "    TestSdu Constructor called" << endl;

}

TestSdu::~TestSdu()
{
//	cout << "    TestSdu Destructor called" << endl;

}

const TestSdu& TestSdu::getTestSdu(Frame& testFrame)        // Returns a constant reference to the Test Sdu
{
	// Will generate an error if getNextEtherType and getNextSubType were not validated before calling 

	return((const TestSdu&)testFrame.getNextSdu());
	//	return ((const TestSdu&)*(testFrame.pNextSdu));
}

/**/