#ifndef OPTIMIZE_H
#define OPTIMIZE_H

#include "instruction.h"
#include "function.h"

bool resolveDelaySlots(tInstList &collInstList);
bool optimizeInstructions(Function &aFunction, unsigned uCompletePassesDone);

#endif
