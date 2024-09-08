/*
htop - TreeTopProcessTable.c
(C) 2024 Nathan Scott
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "pcp/TreeTopProcessTable.h"

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
#include "Process.h"
#include "Settings.h"
#include "XUtils.h"

#include "pcp/Metric.h"
#include "pcp/PCPMachine.h"
#include "pcp/TreeTop.h"
#include "pcp/TreeTopProcess.h"


ProcessTable* ProcessTable_new(Machine* host, Hashtable* pidMatchList) {
   TreeTopProcessTable* this = xCalloc(1, sizeof(TreeTopProcessTable));
   Object_setClass(this, Class(ProcessTable));

   ProcessTable* super = &this->super;
   ProcessTable_init(super, Class(TreeTopProcess), host, pidMatchList);

   return super;
}

void ProcessTable_delete(Object* cast) {
   TreeTopProcessTable* this = (TreeTopProcessTable*) cast;
   ProcessTable_done(&this->super);
   free(this);
}

static inline float Metric_instance_float(int metric, int id, int offset, float fallback) {
   pmAtomValue value;
   if (Metric_instance(metric, id, offset, &value, PM_TYPE_FLOAT))
      return value.f;
   return fallback;
}

static void TreeTopProcessTable_updateID(Process* process, int id, int offset) {
   Process_setThreadGroup(process, id);
   //Metric_instance_u32(PCP_PROC_TGID, pid, offset, 1));
   Process_setParent(process, 1);
   //Metric_instance_u32(PCP_PROC_PPID, pid, offset, 1));
}

static void TreeTopProcessTable_updateInfo(TreeTopProcess* tp, int id, int offset, char* feature, size_t length) {
   pmAtomValue value;

   if (!Metric_instance(PCP_MODEL_FEATURES, id, offset, &value, PM_TYPE_STRING))
      value.cp = xStrdup("<unknown>");
   String_safeStrncpy(feature, value.cp, length);
   free(value.cp);

   tp->importance = Metric_instance_float(PCP_MODEL_IMPORTANCE, id, offset, 0);
   tp->mutualinfo = Metric_instance_float(PCP_MODEL_MUTUALINFO, id, offset, 0);
}

#if 0
static char* setString(Metric metric, int id, int offset, char* string) {
   if (string)
      free(string);
   pmAtomValue value;
   if (Metric_instance(metric, id, offset, &value, PM_TYPE_STRING))
      string = value.cp;
   else
      string = NULL;
   return string;
}

static void TreeTopProcessTable_updateSID(Process* process, int id, int offset) {
   tp->sid = setString(PCP_MODEL_FEATURE_SID, id, offset, tp->sid);
}
#endif

static void TreeTopProcessTable_updateCmdline(Process* process, int id, int offset, const char* comm) {
   (void)id; (void)offset;
   Process_updateComm(process, comm);
}

static bool TreeTopProcessTable_updateProcesses(TreeTopProcessTable* this) {
   ProcessTable* pt = (ProcessTable*) this;
   int id = -1, offset = -1;

   /* for every important feature from the model ... */
   while (Metric_iterate(PCP_MODEL_FEATURES, &id, &offset)) {
      bool preExisting;
      Process* proc = ProcessTable_getProcess(pt, id, &preExisting, TreeTopProcess_new);
      TreeTopProcess* tp = (TreeTopProcess*) proc;
      TreeTopProcessTable_updateID(proc, id, offset);
      tp->offset = offset >= 0 ? offset : 0;

      char feature[MAX_NAME + 1];
      TreeTopProcessTable_updateInfo(tp, id, offset, feature, sizeof(feature));

      if (!preExisting) {
         TreeTopProcessTable_updateCmdline(proc, id, offset, feature);
         ProcessTable_add(pt, proc);
      }

      pt->totalTasks++;
      proc->super.show = true;
      proc->super.updated = true;
   }
   return true;
}

void ProcessTable_goThroughEntries(ProcessTable* super) {
   Platform_updateMap();
   TreeTopProcessTable* this = (TreeTopProcessTable*) super;
   TreeTopProcessTable_updateProcesses(this);
}
