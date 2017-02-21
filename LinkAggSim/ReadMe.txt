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
========================================================================
    CONSOLE APPLICATION : LinkAggSim Project Overview
========================================================================

This file contains a summary of what is in each of the files that
make up the LinkAggSim application.

LinkAggSim.cpp, LinkAggSim Output.txt
    This is the main application source file.
	Includes setting up the simulation environment by creating Devices, and a selection
	of test functions to simulate specific network scenarios.
	The simulation outputs messages to the console window while it is running,
	and creates a text file ("LinkAggSim Output.txt") containing additional
	detail about events in the simulation.

The remaining files can be divided into two basic categories:  those that create the 
	simulation environment, and those that are specific to Link Aggregation.

Files creating the simulation environment:
	Mac.h, Mac.cpp
		Class Component is the base class for all functional elements that can be simulated.
		Class Iss is the base class for a Service Access Point used to interconnect
			components within a Device.
		Class IssQ inherits Iss and becomes a base class for Components that provide 
			request and indication queues, rather than handle one frame at a time.
		Class Mac inherits both Component and IssQ, and is the Component that allows
			interconnection of Devices.

	Device.h, Device.cpp
		Class Device is basically just a shell that contains a collection of Components.
		Class End Station is a Componnent that generates and receives test frames, and
			contains a pointer to the Iss of the underlying service layer (typically a
			Mac or an Aggregator).

	Bridge.h, Bridge.cpp
		Class Bridge implements the functions of an IEEE 802.1Q Bridge Component,
			and a vector of BridgePorts.
			Currently it is very rudimentary (VLAN-unaware, non-Learning, no topology
			control protocols such as RSTP/MSTP or SPB).
		Class BridgePort contains the attributes specific to an individual Port on Bridge,
			and a pointer to the Iss of the underlying service layer (typically a Mac or
			an Aggregator).

	Frame.h, Frame.cpp
		Class Frame contains the parameters of an IEEE 802 ISS Service Request (i.e.   
			egress frame) or ISS Service Indication (i.e. ingress frame).  
			This includes the destination address (MAC DA), source address (MAC SA),
			VLAN Identifier, priority, drop eligible flag, and the MAC Service Data
			Unit (SDU; i.e. the contents of the frame).
			The MAC SDU is implemented a pointer to a class that inherits the Sdu base
			class.
		Class Sdu is the base class for all types of "data units" (i.e. frame contents).
			It includes the EtherType of the data unit, a subType (0 if the EtherType
			does not have subtypes), and a pointer (potentially null) to the next Sdu in
			the frame.
		Class VlanTag inherits Sdu and implements a VLAN Tag.
			Its EtherType is either the C-VLAN or S-VLAN EtherType.  It contains the 
			VLAN control word consisting of the priority, drop eligible flag, and VLAN
			Identifier, as well as a pointer to the next SDU in the frame.
		Class TestSdu inherits Sdu and contains a single integer of user defined data.
			End Stations generate test frames containing a TestSdu.  

Files implementing Link Aggregation:
	LinkAgg.h, LinkAgg.cpp, LacpSelectionLogic.cpp
		Class LinkAgg inherits the Component base class and implements an IEEE 802.1AX 
			Link Aggregation shim (a shim is the term used in the IEEE 802 architecture  
			for a networking sub-layer that provides an ISS Service Access Point to a  
			client sub-layer, and uses the ISS of an underlying sub-layer).
			LinkAgg includes a vector of Aggregators, a vector of Aggregation Ports,
			as well as functions that access the parameters of more than one Aggregator
			or Aggregation Port (Collection, Distribution, Selection Logic, etc.)

	Aggregator.h, Aggregator.cpp
		Class Aggregator inherits IssQ and implements an "Aggregator", which functions
			as a sort-of virtual MAC that access a set of links (a Link Aggregation Group
			(LAG)) rather than an individual link.  Each Aggregator includes of list of
			the Aggregation Ports in its LAG.  The Aggregator "collects" ingress frames
			from all Aggregation Ports in the LAG, and "distributes" egress frames among
			the Aggregation Ports in the LAG.

	AggPort.h, AggPort.cpp, LacpRxSM.cpp, LacpMuxSM.cpp, LacpPeriodicSM.cpp, LacpTxSM.cpp
		Class AggPort implements an "Aggregation Port".  Each Aggregation Port has a pointer
			to the Iss of the underlying service layer (typically a MAC), and a pointer to
			its selected Aggregator.  The Link Aggregation Control Protocol (LACP) selects 
			an Aggregator based on local ("actor") parameters, and the corresponding 
			parameters of the remote ("partner") Aggregation Port in the Device to which 
			it is connected.  
			The AggPort has nested classes to implement the LACP state machines (Receive, 
			Multiplexer, Periodic, and Transmit).

	Lacpdu.h, Lacpdu.cpp
		Class Lacpdu inherits Sdu and includes all of the fields of a Link Aggregation 
			Control Protocol Data Unit.  These fields contain the parameters exchanged 
			between actor and partner Aggregation Ports.
			

/////////////////////////////////////////////////////////////////////////////
Other standard files:

StdAfx.h, StdAfx.cpp
    These files are used to build a precompiled header (PCH) file
    named LinkAggSim.pch and a precompiled types file named StdAfx.obj.

	Added global variables for simulation (class SimLog) Time, logFile, and Debug.

LinkAggSim.vcxproj
    This is the main project file for VC++ projects generated using an Application Wizard.
    It contains information about the version of Visual C++ that generated the file, and
    information about the platforms, configurations, and project features selected with the
    Application Wizard.

LinkAggSim.vcxproj.filters
    This is the filters file for VC++ projects generated using an Application Wizard. 
    It contains information about the association between the files in your project 
    and the filters. This association is used in the IDE to show grouping of files with
    similar extensions under a specific node (for e.g. ".cpp" files are associated with the
    "Source Files" filter).



/////////////////////////////////////////////////////////////////////////////
Other notes:

AppWizard uses "TODO:" comments to indicate parts of the source code you
should add to or customize.

/////////////////////////////////////////////////////////////////////////////
