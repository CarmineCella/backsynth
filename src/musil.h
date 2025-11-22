// musil.h
//

#ifndef MUSIL_H
#define MUSIL_H

#include "core.h"

AtomPtr make_env () {
    AtomPtr env = make_atom ();
    env->tail.push_back (make_atom ()); // no parent env
    add_core (env);
    // add_system (env);
    // add_signals (env);
    // add_learning (env);
    // add_plotting (env);
    return env;
}
#endif // MUSIL_H

// eof
