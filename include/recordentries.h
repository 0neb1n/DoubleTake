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
 * @file   recordentires.h
 * @brief  Managing record entry for each thread. Since each thread will have different entries,
 There is no need to use lock here at all.
 The basic idea of having pool is 
 to reduce unnecessary memory allocation and deallocation operations, similar 
 to slab manager of Linux system. However, it is different here.
 There is no memory deallocation for each pool. 
 In the same epoch, we keep allocating from 
 this pool and udpate correponding counter, updating to next one.
 When epoch ends, we reset the counter to 0 so that we can reuse all 
 memory and there is no need to release the memory of recording entries. 
            
 * @author Tongping Liu <http://www.cs.umass.edu/~tonyliu>
 */
#ifndef _RECORDENTRIES_H_
#define _RECORDENTRIES_H_

#include "xdefines.h"
#include "mm.h"

template <class Entry>
class RecordEntries {
public:
  RecordEntries() 
  {
  }

  void initialize(int entries) {
    void * ptr;
    int i = 0;

    _size = alignup(entries * sizeof(Entry), xdefines::PageSize);
    ptr = MM::mmapAllocatePrivate(_size);
    if(ptr == NULL)  {
      PRWRN("%d fail to allocate sync event pool entries : %s\n", getpid(), strerror(errno));
      ::abort();
    }

  //  PRINT("recordentries.h::initialize at _cur at %p. memory from %p to 0x%lx\n", &_cur, ptr, (((unsigned long)ptr) + _size));	
    // start to initialize it.
    _start = (Entry *)ptr;
    _cur = 0;
    _total = entries;
    _iter = 0;
    return;
  }

  Entry * alloc() {
    Entry * entry = NULL;
    if (_cur < _total) {
      entry = (Entry *)&_start[_cur];
      _cur++;
    } else {
      // There are no enough entries now; re-allocate new entries now.
      PRWRN ("Not enough entries, now _cur %lu, _total %lu at %p!!!\n", _cur, _total, &_cur);
      ::abort();
    }
    return entry;
  }

  void cleanup() {
    _iter = 0;
    _cur = 0;
  }

  void prepareRollback() {
    _iter = 0;
  }

  void prepareIteration() {
    _iter = 0;
  }

  inline Entry * getEntry(size_t index) {
    return &_start[index];
  }

  Entry * nextIterEntry() {
    _iter++;
    if(_iter < _cur) {
      return getEntry(_iter);
    }
    else {
      return NULL;
    }
  }
  
  Entry * retrieveIterEntry() {
    Entry * entry = NULL;
    if(_iter < _cur) {
      entry = getEntry(_iter);
      _iter++;
    }
    return entry;
  }

  // No change on iteration entry.
  Entry * getEntry() {
    Entry * entry = NULL;
    if(_iter < _cur) {
      entry = getEntry(_iter);
    }
    return entry;
  }

  // Only called in the replay
  Entry * firstIterEntry() {
    return &_start[_iter];
  }

  size_t getEntriesNumb() {
    return _cur;
  }

private:
  Entry * _start; 
  size_t _total; 
  size_t _cur;
  size_t _size; 
  size_t _iter; 
};

#endif
