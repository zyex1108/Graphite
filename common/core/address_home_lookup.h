#ifndef ADDRESSHOMELOOKUP_H_
#define ADDRESSHOMELOOKUP_H_

#include "assert.h"
#include "pin.H"
#include "debug.h"
#include <iostream> 
#include <sstream> 
#include <vector> 
#include <math.h>

using namespace std;

/* TODO abstract MMU stuff to a configure file to allow
 * user to specify number of memory controllers, and
 * the address space that each is in charge of.  Default behavior:
 * AHL assumes that every core has a memory controller,
 * and that each shares an equal number of address spaces.
 *
 * Now possible to setAddrBoundaries, either through constructor
 * or during runtime!
 *
 * each core has an AHL, but they must keep their data consistent 
 * regarding boundaries!
 *
 * Maybe allow the ability to have public and private memory space?
 */

//bytes per cache_line
extern LEVEL_BASE::KNOB<UINT32> g_knob_line_size;

class AddressHomeLookup {
	public:
		AddressHomeLookup (UINT32 num_nodes, UINT32 log_block_size, INT32 ahl_id);
		~AddressHomeLookup(void);
		UINT32 find_home_for_addr(ADDRINT address) const;

	private:
		UINT32 num_nodes;
		INT32 ahl_id;
		ADDRINT mask;
		UINT32 log_block_size;
};
/*
class AddressHomeLookup
{
	public:
		AddressHomeLookup(UINT32 num_nodes, INT32 ahl_id);
		AddressHomeLookup(vector< pair<ADDRINT,ADDRINT> > addr_bounds, INT32 ahl_id);
		virtual ~AddressHomeLookup();
	  
		// TODO: change this return type to a node number type 
		UINT32 find_home_for_addr(ADDRINT) const;
		//TODO make this static
		void setAddrBoundaries(vector< pair<ADDRINT, ADDRINT> > addr_bounds);

	private:
		//TODO make these static since all instances need to be consistent?
		vector< pair<ADDRINT, ADDRINT> > address_boundaries;
		vector<UINT64> bytes_per_core;

	   UINT32 num_nodes;
		INT32 ahl_id; //debugging purposes only. tie to core id
		ADDRINT mask;
		UINT32 m;
};
*/


#endif /*ADDRESSHOMELOOKUP_H_*/
