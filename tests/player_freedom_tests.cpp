#include "fc/PlayerFreedomPolicy14.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
int main() {
    using namespace fc; int n = 0;
    auto ck=[&](bool ok){++n;if(!ok)throw std::runtime_error("player freedom policy #"+std::to_string(n));};
    PlayerIntent14 p;
    ck(p.Observe(true,100,0,1,true,true).closeDialogue);
    p.Reset();p.Activate(100,1);
    auto a=p.Observe(true,100,200,1.1,true,true);
    ck(a.voluntaryDialogue&&a.voluntaryScene&&!a.closeDialogue&&!a.releaseScene);
    ck(p.Observe(true,100,200,600,true,true).voluntaryDialogue);
    ck(p.Observe(true,101,200,601,true,true).closeDialogue);
    p.Reset();p.Activate(100,1);ck(p.Observe(true,101,0,1.1,true,true).closeDialogue);
    p.Reset();p.Activate(100,1);ck(p.Observe(true,100,0,3.01,true,true).closeDialogue);
    p.Reset();p.Activate(100,1);ck(p.Observe(true,100,0,1.1,true,false).closeDialogue);
    p.Reset();ck(!p.Observe(true,0,200,1,true,true).closeDialogue);
    a=p.Observe(true,0,200,1.21,true,true);ck(a.closeDialogue&&a.releaseScene);
    p.Reset();p.Activate(100,1);p.Observe(true,0,0,1.1,true,true);
    ck(p.Observe(true,100,200,1.15,true,true).voluntaryScene);
    p.Observe(false,0,0,1.2,true,true);ck(p.Observe(true,100,0,1.3,true,true).closeDialogue);
    p.Reset();p.Activate(100,1);p.Observe(true,100,200,1.1,true,true);
    ck(p.Observe(false,0,200,1.2,true,true).releaseScene);
    p.Reset();ck(p.Observe(false,0,200,1,true,true).releaseScene);
    p.Activate(100,2);p.Observe(false,0,0,2,false,true);
    ck(p.Observe(true,100,0,2.1,true,true).closeDialogue);
    p.Reset();p.Activate(0x14,1);ck(p.Observe(true,0x14,0,1.1,true,true).closeDialogue);
    p.Reset();p.Activate(100,std::numeric_limits<double>::infinity());
    ck(p.Observe(true,100,0,1,true,true).closeDialogue);
    p.Reset();p.Activate(100,1);p.Observe(true,100,200,1.1,true,true);
    a=p.Observe(true,100,201,1.2,true,true);ck(a.voluntaryDialogue&&!a.voluntaryScene&&a.releaseScene);
    ck(!p.Observe(true,100,0,std::numeric_limits<double>::quiet_NaN(),true,true).closeDialogue);
    // A rejected speaker consumes the pending target token; it cannot be reused.
    p.Reset();p.Activate(100,1);p.Observe(true,101,0,1.1,true,true);
    ck(p.Observe(true,100,0,1.2,true,true).closeDialogue);
    // New invalid activations and paused UI revoke pending permission.
    p.Reset();p.Activate(100,1);p.CancelPending();ck(p.Observe(true,100,0,1.1,true,true).closeDialogue);
    p.Reset();p.Activate(100,1);p.Pause(false);ck(p.Observe(true,100,0,1.1,true,true).closeDialogue);
    // Pausing an established same-speaker conversation does not destroy consent.
    p.Reset();p.Activate(100,1);p.Observe(true,100,200,1.1,true,true);p.Pause(true);
    ck(p.Observe(true,100,200,10,true,true).voluntaryDialogue);
    // An unresolved speaker is allowed briefly, never forever after consent.
    p.Reset();p.Activate(100,1);p.Observe(true,100,200,1.1,true,true);
    ck(p.Observe(true,0,200,10,true,true).voluntaryDialogue);
    a=p.Observe(true,0,200,10.21,true,true);ck(a.closeDialogue&&a.releaseScene);
    ck(p.Observe(true,100,200,10.3,true,true).closeDialogue);
    // A new player scene must appear within the original opening window.
    p.Reset();p.Activate(100,1);p.Observe(true,100,0,1.1,true,true);
    a=p.Observe(true,100,200,4,true,true);ck(a.voluntaryDialogue&&a.releaseScene&&!a.voluntaryScene);
    p.Reset();p.Activate(100,1);p.Observe(true,100,0,1.1,true,true);
    ck(p.Observe(true,100,200,2,true,true).voluntaryScene);
    p.Reset();p.Activate(100,1);p.Observe(true,100,0,1.1,true,true);p.Pause(true);
    ck(p.Observe(true,100,200,1.2,true,true).releaseScene);
    p.Reset();p.Activate(100,1);p.Activate(0,1.1);ck(p.Observe(true,100,0,1.2,true,true).closeDialogue);
    p.Reset();p.Observe(true,0,0,1,true,false);ck(p.Observe(true,0,0,1.21,true,false).closeDialogue);
    p.Reset();p.Activate(100,1);p.Observe(true,0,0,1.1,true,true);p.Pause(true);
    ck(p.Observe(true,0,0,1.31,true,true).closeDialogue);
    std::cout << "PASS: " << n << " player intent policy assertions; not engine/game validation.\n";
}
