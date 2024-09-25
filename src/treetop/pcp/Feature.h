#ifndef HEADER_Feature
#define HEADER_Feature
/*
htop - Feature.h
(C) 2024 Nathan Scott
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <stdbool.h>

#include "Machine.h"
#include "Object.h"
#include "Row.h"


typedef struct Feature_ {
   Row super;
   char name[MAX_NAME];

   /* default result offset to use for searching metrics */
   unsigned int offset;

   char min_max[4];
   float difference;
   float importance;
   float mutualinfo;
} Feature;

typedef struct FeatureFieldData_ {
   /* Name (displayed in setup menu) */
   const char* name;

   /* Title (display in main screen); must have same width as the printed values */
   const char* title;

   /* Description (displayed in setup menu) */
   const char* description;

   /* Scan flag to enable scan-method otherwise not run */
   uint32_t flags;

   /* Whether the values are process identifiers; adjusts the width of title and values if true */
   bool pidColumn;

   /* Whether the column should be sorted in descending order by default */
   bool defaultSortDesc;

   /* Whether the column width is dynamically adjusted (the minimum width is determined by the title length) */
   bool autoWidth;

   /* Whether the title of a column with dynamically adjusted width is right aligned (default is left aligned) */
   bool autoTitleRightAlign;
} FeatureFieldData;

#define LAST_PROCESSFIELD LAST_RESERVED_FIELD
extern const FeatureFieldData Feature_fields[LAST_PROCESSFIELD];

#define Feature_getId(f_)       ((f_)->super.id)
#define Feature_setId(f_, id_)  ((f_)->super.id = (id_))

extern const RowClass Feature_class;

Feature* Feature_new(const Machine* host);

void Feature_done(Feature* this);

#endif
