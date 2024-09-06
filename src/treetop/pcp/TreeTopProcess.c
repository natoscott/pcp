/*
htop - TreeTopProcess.c
(C) 2024 Nathan Scott
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "pcp/TreeTopProcess.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "CRT.h"
#include "Macros.h"
#include "Process.h"
#include "ProvideCurses.h"
#include "RichString.h"
#include "XUtils.h"

#include "pcp/PCPDynamicColumn.h"


const ProcessFieldData Process_fields[] = {
   [0] = { .name = "" },
   [PID] = { .name = "PID", .title = "PID", .description = "Process/thread ID", .flags = 0, .pidColumn = true, },
   [FEATURE] = { .name = "FEATURE", .title = "                                Key Explanatory Metrics ", .description = "Most important metrics (features) globally" },
   [IMPORTANCE] = { .name = "IMPORTANCE", .title = "IMPORTANCE ", .description = "Model-based feature importance measure" },
   [MUTUALINFO] = { .name = "MUTUALINFO", .title = "MUTUALINFO ", .description = "Mutual information with the target variable" },
};

Process* TreeTopProcess_new(const Machine* host) {
   TreeTopProcess* this = xCalloc(1, sizeof(TreeTopProcess));
   Object_setClass(this, Class(TreeTopProcess));
   Process_init(&this->super, host);
   return (Process*)this;
}

void Process_delete(Object* cast) {
   TreeTopProcess* this = (TreeTopProcess*) cast;
   Process_done((Process*)cast);
   free(this);
}

static void TreeTop_writeFeature(const TreeTopProcess* tp, RichString* str) {
   char buffer[256]; buffer[255] = '\0';
   int baseattr = CRT_colors[PROCESS_THREAD];
   int shadow = CRT_colors[PROCESS_SHADOW];
   int attr = CRT_colors[PROCESS_COMM];
   size_t end, n = sizeof(buffer) - 1;
   char* inst;

   end = snprintf(buffer, n, "%55s ", tp->super.procComm);
   RichString_appendWide(str, baseattr, buffer);
   if ((inst = strchr(buffer, '[')) != NULL) {
      n = (size_t)(inst - buffer);
      RichString_setAttrn(str, shadow, n, 1);
      RichString_setAttrn(str, attr, n + 1, end - n - 3);
      RichString_setAttrn(str, shadow, end - 2, 1);
   }
}

static void TreeTop_writeValue(RichString* str, double value) {
   int shadowColor = CRT_colors[PROCESS_SHADOW];
   char buffer[16];

   if (!isNonnegative(value)) {
      RichString_appendAscii(str, shadowColor, "        N/A ");
   } else {
      int len = snprintf(buffer, sizeof(buffer), " %9.5f ", value);
      RichString_appendnAscii(str, shadowColor, buffer, len);
   }
}

static void TreeTopProcess_rowWriteField(const Row* super, RichString* str, ProcessField field) {
   const TreeTopProcess* tp = (const TreeTopProcess*) super;

   switch ((int)field) {
   case IMPORTANCE: TreeTop_writeValue(str, tp->importance); return;
   case MUTUALINFO: TreeTop_writeValue(str, tp->mutualinfo); return;
   case FEATURE:
      TreeTop_writeFeature(tp, str);
      return;
   default:
      break;
   }
   Process_writeField(&tp->super, str, field);
}

static int TreeTopProcess_compareByKey(const Process* v1, const Process* v2, ProcessField key) {
   const TreeTopProcess* t1 = (const TreeTopProcess*)v1;
   const TreeTopProcess* t2 = (const TreeTopProcess*)v2;

   switch (key) {
   case MUTUALINFO:
      return SPACESHIP_NUMBER(t1->mutualinfo, t2->mutualinfo);
   case IMPORTANCE:
      return SPACESHIP_NUMBER(t1->importance, t2->importance);
   default:
      if (key < LAST_PROCESSFIELD)
         return Process_compareByKey_Base(v1, v2, key);
      return PCPDynamicColumn_compareByKey(t1, t2, key);
   }
}

const ProcessClass TreeTopProcess_class = {
   .super = {
      .super = {
         .extends = Class(Process),
         .display = Row_display,
         .delete = Process_delete,
         .compare = Process_compare
      },
      .isHighlighted = Process_rowIsHighlighted,
      .isVisible = Process_rowIsVisible,
      .matchesFilter = Process_rowMatchesFilter,
      .compareByParent = Process_compareByParent,
      .sortKeyString = Process_rowGetSortKey,
      .writeField = TreeTopProcess_rowWriteField,
   },
   .compareByKey = TreeTopProcess_compareByKey,
};
