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

#include "Metric.h"
#include "Table.h"

typedef enum FeatureTableType_ {
    TABLE_MODEL_IMPORTANCE,
    TABLE_LOCAL_IMPORTANCE,
    TABLE_OPTIM_IMPORTANCE,
} FeatureTableType;

typedef struct FeatureTable_ {
   Table super;
   Metric feature;
   FeatureTableType table_type;
} FeatureTable;

extern const TableClass FeatureTable_class;

FeatureTable* FeatureTable_new(struct Machine_* host, FeatureTableType type);

void FeatureTable_done(FeatureTable* this);

#endif
