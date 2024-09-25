#ifndef HEADER_FeatureTable
#define HEADER_FeatureTable
/*
htop - FeatureTable.h
(C) 2024 Nathan Scott
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>
#include <sys/types.h>

#include "Table.h"

typedef struct FeatureTable_ {
   Table super;
} FeatureTable;

extern const TableClass FeatureTable_class;

FeatureTable* FeatureTable_new(struct Machine_* host);

void FeatureTable_done(FeatureTable* this);

#endif
