// -*- C++ -*-

/*
  Copyright (c) 2012, University of Massachusetts Amherst.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

/*
 * @file   watchpoint.cpp
 * @brief  Including the implemenation of watch point handler.
 *         Since we have to call functions in other header files, then it is 
 *         impossible to put this function into the header file.
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */

#include "watchpoint.h"
#include "memtrack.h"

bool watchpoint::addWatchpoint(void * addr, size_t value, faultyObjectType objtype, void * objectstart, size_t objectsize) {
  bool hasWatchpoint = true;
    
  if(objtype == OBJECT_TYPE_OVERFLOW) { 
 //   PRINT("DoubleTake: Buffer overflow at address %p with value 0x%lx. size %lx start %p\n", addr, value, objectsize, objectstart);
    //PRINT("DoubleTake: Buffer overflow at address %p with value 0x%lx. \n", addr, value);
  }
  else {
    assert(objtype == OBJECT_TYPE_USEAFTERFREE);
    PRINT("DoubleTake: Use-after-free error at address %p with value 0x%lx. \n", addr, value);
  }

  if(_numWatchpoints < xdefines::MAX_WATCHPOINTS) {
    // Record watch point information
    _wp[_numWatchpoints].faultyaddr = addr;
    _wp[_numWatchpoints].objectstart = objectstart;
    _wp[_numWatchpoints].objtype = objtype;
  //  _wp[_numWatchpoints].objectsize = objectsize;
    _wp[_numWatchpoints].faultyvalue = value;
    _wp[_numWatchpoints].currentvalue = value;
    _numWatchpoints++;
  }
  else  {
    hasWatchpoint = false;
  }
  
  // Add it to memtrack too.
  memtrack::getInstance().insert(objectstart, objectsize, objtype);

  return !hasWatchpoint;
}

// Handle those traps on watchpoints now.  
void watchpoint::trapHandler(int sig, siginfo_t* siginfo, void* context)
{
  ucontext_t * trapcontext = (ucontext_t *)context;
  size_t * addr = (size_t *)trapcontext->uc_mcontext.gregs[REG_RIP]; // address of access

  // Find faulty object. 
  faultyObject * object;
	// If it is a read, we only care about this if it is use-after-free error
  if(!watchpoint::getInstance().findFaultyObject(&object)) {
  //  PRERR("Can't find faulty object!!!!\n");
		return;
  }

  // Check whether this trap is caused by libdoubletake library. 
  // If yes, then we don't care it since libdoubletake can fill the canaries.
  if(selfmap::getInstance().isDoubleTakeLibrary(addr)) {
    return;
  }

  //  PRINF("CAPTURING write at %p: ip %lx. signal pointer %p, code %d. \n", addr, trapcontext->uc_mcontext.gregs[REG_RIP], siginfo->si_ptr, siginfo->si_code);
	faultyObjectType faultType; 

	faultType = memtrack::getInstance().getFaultType(object->objectstart, object->faultyaddr);
	if(faultType == OBJECT_TYPE_NO_ERROR) {
		return;
	}	
	
	// Check whether this callsite is the same as the previous callsite.
	void * callsites[xdefines::CALLSITE_MAXIMUM_LENGTH];
  int depth = selfmap::getCallStack((void **)&callsites);	

	// If current callsite is the same as the previous one, we do not want to report again.
	if(watchpoint::getInstance().checkAndSaveCallsite(object, depth, (void **)&callsites)) {
		return;
	}

  // Now we should check whether objectstart is existing or not.
  if(faultType == OBJECT_TYPE_OVERFLOW) {
    PRINT("\nCaught heap overflow at %p. Current call stack:\n", object->faultyaddr);
  }
  else if(faultType == OBJECT_TYPE_USEAFTERFREE) {
    PRINT("\nCaught use-after-free error at %p. Current call stack:\n", object->faultyaddr);
  }
  selfmap::getInstance().printCallStack(depth, (void **)&callsites);

  // Check its allocation or deallocation inf 
  if(object->objectstart != NULL) {
    // Now we should check memtrack status.
    memtrack::getInstance().print(object->objectstart, faultType);
  }
}
 
