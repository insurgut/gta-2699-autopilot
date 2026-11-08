#pragma once
struct FinishHold {
    bool active=false,takeover=false;
    void reset(){active=false;takeover=false;}
    void begin(){active=true;takeover=false;}
    void input(bool injected,bool pressed){if(active&&!injected&&pressed)takeover=true;}
};
