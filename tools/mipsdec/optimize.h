#ifndef OPTIMIZE_H
#define OPTIMIZE_H

#include "instruction.h"

bool resolveDelaySlots(tInstList &collInstList);
bool optimizeInstructions(tInstList &collInstList, unsigned uCompletePassesDone);

#endif
