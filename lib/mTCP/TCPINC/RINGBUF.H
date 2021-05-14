
/*

   mTCP Ringbuf.H
   Copyright (C) 2006-2020 Michael B. Brutman (mbbrutman@gmail.com)
   mTCP web page: http://www.brutman.com/mTCP


   This file is part of mTCP.

   mTCP is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   mTCP is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with mTCP.  If not, see <http://www.gnu.org/licenses/>.


   Description: RingBuffer data structure used by TCP sockets.

   Changes:

   2011-05-27: Initial release as open source software

*/




// RingBuffer is used by TCP to manage the queues of packets.  Performance
// is fairly important as these queues are traversed fairly often.
//
// Alignment will always be at least a word.  Our counters are words and
// we store pointers which are words or double words.


#ifndef RINGBUFFER_H
#define RINGBUFFER_H



// RINGBUFFER_SIZE must be a power of 2.

#define RINGBUFFER_SIZE TCP_SOCKET_RING_SIZE
#define RINGBUFFER_MASK (RINGBUFFER_SIZE-1) 



class RingBuffer {

  public:

    uint16_t  first; // Index to first item to be dequeued.
    uint16_t  next;  // Index to next place to enqueue an item.

    // Entries can tell us if we are empty or full.  It is easier to maintain
    // and use a counter than it is to do compare the indexes.
    uint16_t  entries;

    void    *ring[ RINGBUFFER_SIZE ];


    // Do not do this unless you know the number of entries is already zero.
    inline void init( ) { first = next = entries = 0; }

    inline int16_t enqueue( void *data ) {
      if ( entries == RINGBUFFER_SIZE ) return -1;
      ring[next] = data;
      next++;
      next = next & RINGBUFFER_MASK;
      entries++;
      return 0;
    }

    inline void *dequeue( void ) {
      if ( entries == 0 ) return NULL;
      uint16_t i = first;
      first++;
      first = first & RINGBUFFER_MASK;
      entries--;
      return ring[i];
    }

    inline void *peek( void ) {
      if ( entries == 0 ) {
	return NULL;
      }
      else {
	return ring[first];
      }
    }

    inline uint16_t hasRoom( void ) { return ( entries < RINGBUFFER_SIZE ); }

};


#endif
