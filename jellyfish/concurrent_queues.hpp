/*  This file is part of Jellyfish.

    Jellyfish is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Jellyfish is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Jellyfish.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __JELLYFISH_CONCURRENT_QUEUES_HPP__
#define __JELLYFISH_CONCURRENT_QUEUES_HPP__

#include <sys/time.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <new>
#include <stdio.h>

#include <jellyfish/atomic_gcc.hpp>

/* Concurrent queue. No check whether enqueue would overflow the queue.
 */

template<class Val>
class concurrent_queue {
  Val                   **queue;
  unsigned int            size;
  unsigned int volatile   head;
  unsigned int volatile   tail;
  bool volatile           closed;
  
public:
  concurrent_queue(unsigned int _size) : size(_size + 1), head(0), tail(0) { 
    queue = new Val *[size];
    memset(queue, 0, sizeof(Val *) * size);
    closed = false;
  }
  ~concurrent_queue() { delete [] queue; }

  void enqueue(Val *v);
  Val *dequeue();
  bool is_closed() { return closed; }
  void close() { closed = true; }
  bool has_space() { return head != tail; }
  bool is_low() { 
    int len = head - tail;
    if(len < 0)
      len += size;
    return (unsigned int)(4*len) <= size;
  }
};

template<class Val>
void concurrent_queue<Val>::enqueue(Val *v) {
  int done = 0;
  unsigned int chead;

  chead = head;
  while(!done) {
    unsigned int nhead = (chead + 1) % size;

    done = (atomic::gcc::cas(&queue[chead], (Val*)0, v) == (Val*)0);
    chead = atomic::gcc::cas(&head, chead, nhead);
  }
}

template<class Val>
Val *concurrent_queue<Val>::dequeue() {
  int done = 0;
  Val *res;
  unsigned int ctail, ntail;

  ctail = tail;
  //  __sync_synchronize();
  while(!done) {
    //    if(ctail == head)
    //      return NULL;
    if(atomic::gcc::cas(&head, ctail, ctail) == ctail)
      return NULL;

    ntail = (ctail + 1) % size;
    ntail = atomic::gcc::cas(&tail, ctail, ntail);
    done = ntail == ctail;
    ctail = ntail;
  }
  res = queue[ctail];
  queue[ctail] = NULL;
  return res;
}
  
#endif
