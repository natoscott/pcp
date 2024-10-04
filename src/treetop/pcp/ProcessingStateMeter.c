/*
htop - ProcessingStateMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ProcessingStateMeter.h"

#include "CRT.h"
#include "Object.h"
#include "TreeTop.h"
#include "XUtils.h"


static const int ProcessingStateMeter_attributes[] = {
   PROCESS_SHADOW, /* waiting */
   PROCESS_THREAD_BASENAME, /* training */
   PROCESS_THREAD_COMM, /* sampling */
   PROCESS_COMM, /* explaining */
   PROCESS_TOMB, /* unknown */
};

static void ProcessingStateMeter_updateValues(Meter* this) {
   char* target = Platform_getProcessingState();
   this->meterData = target;
   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s", target);
}

static void ProcessingMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   char* state = (char*)this->meterData;
   int attr;

   if (strcmp(state, "waiting") == 0)
      attr = ProcessingStateMeter_attributes[0];
   else if (strcmp(state, "training") == 0)
      attr = ProcessingStateMeter_attributes[1];
   else if (strcmp(state, "sampling") == 0)
      attr = ProcessingStateMeter_attributes[2];
   else if (strcmp(state, "explaining") == 0)
      attr = ProcessingStateMeter_attributes[3];
   else
      attr = ProcessingStateMeter_attributes[4];

   RichString_appendnAscii(out, CRT_colors[PROCESS_SHADOW], "[", 1);
   RichString_appendnAscii(out, CRT_colors[attr], state, strlen(state));
   RichString_appendnAscii(out, CRT_colors[PROCESS_SHADOW], "]", 1);
}

const MeterClass ProcessingStateMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = ProcessingMeter_display
   },
   .updateValues = ProcessingStateMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE) | (1 << LED_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = ProcessingStateMeter_attributes,
   .name = "ProcessingState",
   .uiName = "ProcessingState",
   .caption = "Now: "
};
