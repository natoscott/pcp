#ifndef HEADER_TreeTopProcess
#define HEADER_TreeTopProcess
/*
htop - TreeTopProcess.h
(C) 2024 Nathan Scott
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Machine.h"
#include "Object.h"
#include "Process.h"


typedef struct TreeTopProcess_ {
   Process super;

   /* default result offset to use for searching metrics */
   unsigned int offset;

   float importance;
   float mutualinfo;
} TreeTopProcess;

typedef TreeTopProcess PCPProcess; // hack: dynamic columns

extern const ProcessFieldData Process_fields[LAST_PROCESSFIELD];

extern const ProcessClass TreeTopProcess_class;

Process* TreeTopProcess_new(const Machine* host);

void Process_delete(Object* cast);

#endif
