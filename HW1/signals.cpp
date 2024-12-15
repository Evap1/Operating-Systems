#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    // TODO: Add your implementation
    std::cout << "smash: got ctrl-C" << std::endl;

    // kill forgound job
    pid_t fg_id = SmallShell::getInstance().getFGPID();

    if (fg_id != INITIAL_FG){
        if(kill(fg_id, SIGKILL) == -1) {
            perror("smash error: kill failed");
            return;
        }
        std::cout << "smash: process " << fg_id <<" was killed" << std::endl;
        SmallShell::getInstance().setFGPID(INITIAL_FG);
    }
    return;
}

