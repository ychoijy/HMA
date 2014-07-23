#ifndef EVENT_H
#define EVENT_H

#include "msim.hh"
#include "config.hh"
#include <string.h>

using namespace std;

class Event {
  private:
    Tick arrival, delay;

  public:
    Addr vaddr;
    byte cid; // core ID
    byte pid; // process ID
    Addr paddr;
    bool authorized; // to deal with timestamp change issues
    bool isRead;
    bool hold;

    Event(Tick ts, byte cid, byte pid, Addr vaddr, byte itype){
      this->arrival=ts;
      this->delay=0;
      this->cid=cid;
      this->pid=pid;
      this->vaddr=vaddr;
      this->paddr=vaddr; // hack: paddr initialized with vaddr
      authorized = false;
      hold = false;
      setType(itype);
    };

    //~Event(){ delete this; };
    
    void setType (byte itype) {
      isRead = (itype==READ);
    };

    void setArrivalTick (Tick arrival) {
      this->arrival = arrival;
    };

    Tick getIssueTick(){
      return arrival + delay;
    };
    
    void setDelay(Tick delay){
      this->delay = delay;
    };

    void addDelay(Tick more) {
      delay += more;
    };
};

/*class SysCallEvent:public Event{
  private:
    uint8_t syscall_no; // number of the sytem call
    int fd;
    int size;
  
  public:
    SysCallEvent(const char*);
    ~SysCallEvent();
};*/
#endif
