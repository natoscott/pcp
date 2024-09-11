/*
htop - TargetTimestampMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "TargetTimestampMeter.h"

#include "CRT.h"
#include "Object.h"
#include "TreeTop.h"
#include "XUtils.h"


static const int TargetTimestampMeter_attributes[] = {
   METER_SHADOW
};

static void TargetTimestampMeter_updateValues(Meter* this) {
   double target = Platform_getTargetTimestamp();
   struct tm tms;
   time_t seconds = (int)target;
   char buffer[64];

   target -= seconds;
   target *= 1000000; // usec component
   pmLocaltime(&seconds, &tms);
   // 2012-05-10 08:47:47.462172
   strftime(buffer, sizeof(buffer), "%F %H:%M:%S", &tms);
   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s.%d", buffer, (int)target);
}

const MeterClass TargetTimestampMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = TargetTimestampMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE) | (1 << LED_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = TargetTimestampMeter_attributes,
   .name = "TargetTimestamp",
   .uiName = "Timestamp",
   .caption = "@ "
};
