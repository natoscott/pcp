/*
htop - Feature.c
(C) 2024 Nathan Scott
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "pcp/Feature.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "CRT.h"
#include "Macros.h"
#include "Feature.h"
#include "ProvideCurses.h"
#include "RichString.h"
#include "XUtils.h"


const FeatureFieldData Feature_fields[] = {
   [0] = { .name = "" },
   [MODEL_FEATURE] = { .name = "MODEL_FEATURE", .title = "                                Key Explanatory Metrics ", .description = "Most important metrics (features) globally" },
   [MODEL_IMPORTANCE] = { .name = "MODEL_IMPORTANCE", .title = "IMPORTANCE ", .description = "Model-based feature importance measure" },
   [MODEL_MUTUALINFO] = { .name = "MODEL_MUTUALINFO", .title = "MUTUALINFO ", .description = "Mutual information with the target variable" },
   [LOCAL_FEATURE] = { .name = "LOCAL_FEATURE", .title = "                                      Important Metrics ", .description = "Most important metrics (features) from local SHAP" },
   [LOCAL_IMPORTANCE] = { .name = "LOCAL_IMPORTANCE", .title = "SHAP VALUE ", .description = "SHAP value importance measure" },
   [LOCAL_MUTUALINFO] = { .name = "LOCAL_MUTUALINFO", .title = "MUTUALINFO ", .description = "Mutual information for high SHAP value features" },
   [OPTMIN_FEATURE] = { .name = "OPTMIN_FEATURE", .title = "                           Key Metrics for Optimisation ", .description = "Important metrics for optimisation based on minima perturbations" },
   [OPTMIN_CHANGE] = { .name = "OPTMIN_CHANGE", .title = "DELTA ", .description = "Change in prediction with minima perturbations" },
   [OPTMIN_DIRECTION] = { .name = "OPTMIN_DIRECTION", .title = "DIRECTION", .description = "Direction of change with minima perturbations" },
   [OPTMAX_FEATURE] = { .name = "OPTMAXFEATURE", .title = "                           Key Metrics for Optimisation ", .description = "Important metrics for optimisation based on maxima perturbations" },
   [OPTMAX_CHANGE] = { .name = "OPTMAX_CHANGE", .title = "DELTA ", .description = "Change in prediction with maxima perturbations" },
   [OPTMAX_DIRECTION] = { .name = "OPTMAX_DIRECTION", .title = "DIRECTION", .description = "Direction of change with maxima perturbations" },
   // End of list
};

Feature* Feature_new(const Machine* host) {
   Feature* this = xCalloc(1, sizeof(Feature));
   Object_setClass(this, Class(Feature));
   Row_init(&this->super, host);
   return (Feature*)this;
}

void Feature_done(Feature* this) {
   Row_done(&this->super);
   free(this->direction);
   free(this->change);
}

static void Feature_delete(Object* cast) {
   Feature* this = (Feature*) cast;
   Feature_done(this);
   free(this);
}

static const char* Feature_name(Row* rp) {
   Feature* fp = (Feature*) rp;
   return fp->name;
}

static void Feature_writeChange(const Feature* fp, RichString* str) {
}

static void Feature_writeDirection(const Feature* fp, RichString* str) {
}

static void Feature_writeName(const Feature* fp, RichString* str) {
   char buffer[256]; buffer[255] = '\0';
   int baseattr = CRT_colors[PROCESS_THREAD];
   int shadow = CRT_colors[PROCESS_SHADOW];
   int attr = CRT_colors[PROCESS_COMM];
   size_t end, n = sizeof(buffer) - 1;
   char* inst;

   end = snprintf(buffer, n, "%55s ", fp->name);
   RichString_appendWide(str, baseattr, buffer);
   if ((inst = strchr(buffer, '[')) != NULL) {
      n = (size_t)(inst - buffer);
      RichString_setAttrn(str, shadow, n, 1);
      RichString_setAttrn(str, attr, n + 1, end - n - 3);
      RichString_setAttrn(str, shadow, end - 2, 1);
   }
}

static void Feature_writeValue(RichString* str, double value) {
   int shadowColor = CRT_colors[PROCESS_SHADOW];
   char buffer[16];

   if (!isNonnegative(value)) {
      RichString_appendAscii(str, shadowColor, "        N/A ");
      return;
   }

   int len;
   if (value < 1.0)
      len = snprintf(buffer, sizeof(buffer), " %9.5f ", value);
   else if ((double)(int)value == value)
      len = snprintf(buffer, sizeof(buffer), " %9.0f ", value);
   else
      len = snprintf(buffer, sizeof(buffer), " %9.1f ", value);
   if (len < 0)
      RichString_appendAscii(str, shadowColor, "        ??? ");
   else
      RichString_appendnAscii(str, shadowColor, buffer, len);
}

static void Feature_writeField(const Row* super, RichString* str, RowField field) {
   const Feature* fp = (const Feature*) super;

   switch ((int)field) {
   case LOCAL_IMPORTANCE:
   case MODEL_IMPORTANCE:
      Feature_writeValue(str, fp->importance);
      return;
   case LOCAL_MUTUALINFO:
   case MODEL_MUTUALINFO:
      Feature_writeValue(str, fp->mutualinfo);
      return;
   case OPTMAX_DIRECTION:
   case OPTMIN_DIRECTION:
      Feature_writeDirection(fp, str);
   case OPTMAX_CHANGE:
   case OPTMIN_CHANGE:
      Feature_writeChange(fp, str);
      return;
   case LOCAL_FEATURE:
   case MODEL_FEATURE:
   case OPTMAX_FEATURE:
   case OPTMIN_FEATURE:
      Feature_writeName(fp, str);
      return;
   default:
      break;
   }
   Feature_writeField(&fp->super, str, field);
}

static int Feature_compareByKey(const Row* v1, const Row* v2, int key) {
   const Feature* f1 = (const Feature*)v1;
   const Feature* f2 = (const Feature*)v2;

   switch (key) {
   case LOCAL_FEATURE:
   case MODEL_FEATURE:
      return SPACESHIP_NULLSTR(f1->name, f2->name);
   case LOCAL_MUTUALINFO:
   case MODEL_MUTUALINFO:
      return SPACESHIP_NUMBER(f1->mutualinfo, f2->mutualinfo);
   case LOCAL_IMPORTANCE:
   case MODEL_IMPORTANCE:
      return SPACESHIP_NUMBER(f1->importance, f2->importance);
   default:
      return Row_compare(v1, v2);
   }
}

static int Feature_compare(const void* v1, const void* v2) {
   const Feature* f1 = (const Feature*)v1;
   const Feature* f2 = (const Feature*)v2;
   const ScreenSettings* ss = f1->super.host->settings->ss;
   RowField key = ScreenSettings_getActiveSortKey(ss);
   int result = Feature_compareByKey(v1, v2, key);

   // Implement tie-breaker (needed to make tree mode more stable)
   if (!result)
      return SPACESHIP_NUMBER(Feature_getId(f1), Feature_getId(f2));

   return (ScreenSettings_getActiveDirection(ss) == 1) ? result : -result;
}

const RowClass Feature_class = {
   .super = {
      .extends = Class(Row),
      .display = Row_display,
      .delete = Feature_delete,
      .compare = Feature_compare,
   },
   .sortKeyString = Feature_name,
   .writeField = Feature_writeField,
};
