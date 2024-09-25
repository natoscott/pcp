/*
htop - FeatureTable.c
(C) 2024 Nathan Scott
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "pcp/FeatureTable.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "Machine.h"
#include "Macros.h"
#include "Object.h"
#include "Platform.h"
#include "Settings.h"
#include "XUtils.h"

#include "pcp/Metric.h"
#include "pcp/PCPMachine.h"
#include "pcp/TreeTop.h"
#include "pcp/Feature.h"


FeatureTable* FeatureTable_new(Machine* host) {
   FeatureTable* this = xCalloc(1, sizeof(FeatureTable));
   Object_setClass(this, Class(FeatureTable));

   Table* super = &this->super;
   Table_init(super, Class(Row), host);

   return this;
}

void FeatureTable_done(FeatureTable* this) {
   Table_done(&this->super);
}

static void FeatureTable_delete(Object* cast) {
   FeatureTable* this = (FeatureTable*) cast;
   FeatureTable_done(this);
   free(this);
}

static inline float Feature_float(int metric, int id, int offset, float fallback) {
   pmAtomValue value;
   if (Metric_instance(metric, id, offset, &value, PM_TYPE_FLOAT))
      return value.f;
   return fallback;
}

static void FeatureTable_updateInfo(Feature* fp, int id, int offset) {
   pmAtomValue value;

   if (!Metric_instance(PCP_MODEL_FEATURES, id, offset, &value, PM_TYPE_STRING))
      value.cp = xStrdup("<unknown>");
   String_safeStrncpy(fp->name, value.cp, sizeof(fp->name));
   free(value.cp);

   fp->importance = Feature_float(PCP_MODEL_IMPORTANCE, id, offset, 0);
   fp->mutualinfo = Feature_float(PCP_MODEL_MUTUALINFO, id, offset, 0);
}

static Feature* FeatureTable_getFeature(FeatureTable* this, int id, bool* preExisting) {
   const Table* super = &this->super;
   Feature* fp = (Feature*) Hashtable_get(super->table, id);
   *preExisting = fp != NULL;
   if (fp) {
      assert(Vector_indexOf(super->rows, this, Row_idEqualCompare) != -1);
      assert(Feature_getId(fp) == id);
   } else {
      fp = Feature_new(super->host);
      Feature_setId(fp, id);
   }
   return fp;
}

static void FeatureTable_goThroughEntries(FeatureTable* this) {
   Table* super = &this->super;

   int id = -1, offset = -1;
   /* for every important feature from the model ... */
   while (Metric_iterate(PCP_MODEL_FEATURES, &id, &offset)) {
      bool preExisting;
      Feature* fp = FeatureTable_getFeature(this, id, &preExisting);
      fp->offset = offset >= 0 ? offset : 0;

      FeatureTable_updateInfo(fp, id, offset);

      Row* row = (Row*) fp;
      if (!preExisting)
         Table_add(super, row);
      row->updated = true;
      row->show = true;
   }
}

static void FeatureTable_prepareEntries(Table* super) {
   Table_prepareEntries(super);
   Platform_updateMap();
}

static void FeatureTable_iterateEntries(Table* super) {
   FeatureTable* this = (FeatureTable*) super;
   FeatureTable_goThroughEntries(this);
}

const TableClass FeatureTable_class = {
   .super = {
      .extends = Class(Table),
      .delete = FeatureTable_delete,
   },
   .prepare = FeatureTable_prepareEntries,
   .iterate = FeatureTable_iterateEntries,
   .cleanup = Table_cleanupEntries,
};
