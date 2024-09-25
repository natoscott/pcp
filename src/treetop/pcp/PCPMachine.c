/*
htop - PCPMachine.c
(C) 2024 Nathan Scott
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "pcp/PCPMachine.h"

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


static void PCPMachine_scan(PCPMachine* this) {
   (void)this;
}

void Machine_scan(Machine* super) {
   PCPMachine* host = (PCPMachine*) super;

   for (int metric = 0; metric < PCP_METRIC_COUNT; metric++)
      Metric_enable(metric, true);

   struct timeval timestamp;
   if (Metric_fetch(&timestamp) != true)
      return;

   double sample = host->timestamp;
   host->timestamp = pmtimevalToReal(&timestamp);
   host->period = (host->timestamp - sample) * 100;

   PCPMachine_scan(host);
}

Machine* Machine_new(UsersTable* usersTable, uid_t userId) {
   PCPMachine* this = xCalloc(1, sizeof(PCPMachine));
   Machine* super = &this->super;

   Machine_init(super, usersTable, userId);

   struct timeval timestamp;
   gettimeofday(&timestamp, NULL);
   this->timestamp = pmtimevalToReal(&timestamp);

   this->cpu = NULL;

   Platform_updateTables(super);

   return super;
}

void Machine_delete(Machine* super) {
   PCPMachine* this = (PCPMachine*) super;
   Machine_done(super);
   free(this->values);
   free(this);
}
