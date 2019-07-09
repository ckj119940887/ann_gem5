#include "learning_gem5/hello_object.hh"
#include "debug/Hello.hh"

HelloObject::HelloObject(HelloObjectParams *p):
    SimObject(p), 
    event([this]{processEvent();}, name()),
    latency(p->time_to_wait), 
    timesLeft(p->number_of_fires),
    myName(p->name),
    goodbye(*(p->goodbye_object))
{
    DPRINTF(Hello, "this is HelloObject constructor\n", myName);
    //panic_if(!goodbye, "HelloObject must have a non-null GoodbyeObject");
}

HelloObject*
HelloObjectParams::create() {
    return new HelloObject(this);
}

void 
HelloObject::processEvent() {
    timesLeft--;
    DPRINTF(Hello, "Processing the event !\n");

    if(timesLeft <= 0) {
        DPRINTF(Hello, "done firing !\n");
        goodbye.sayGoodbye(myName);
    } else {
        schedule(event, curTick() + latency);
    }
}

void
HelloObject::startup() {
    schedule(event, latency);
}