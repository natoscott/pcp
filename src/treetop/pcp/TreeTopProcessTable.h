#ifndef HEADER_TreeTopProcessTable
#define HEADER_TreeTopProcessTable
/*
htop - TreeTopProcessTable.h
(C) 2024 Nathan Scott
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/types.h>

#include "Hashtable.h"
#include "ProcessTable.h"
#include "UsersTable.h"

#include "pcp/Platform.h"


typedef struct TreeTopProcessTable_ {
   ProcessTable super;
} TreeTopProcessTable;

#endif
