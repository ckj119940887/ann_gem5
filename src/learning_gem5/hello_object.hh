#ifndef CKJ_OBJECT_HH
#define CKJ_OBJECT_HH

#include "params/HelloObject.hh"
#include "sim/sim_object.hh"
#include "learning_gem5/goodbye_object.hh"

class HelloObject: public SimObject {
  private:
    void processEvent();
    EventFunctionWrapper event;
    Tick latency;
    int timesLeft;
    std::string myName;

    GoodbyeObject& goodbye;

  public:
    HelloObject(HelloObjectParams *p);  
    void startup();
};

#endif
