/*
htop - ElapsedTimeMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ElapsedTimeMeter.h"

#include "CRT.h"
#include "Object.h"
#include "TreeTop.h"
#include "XUtils.h"


static const int ElapsedTimeMeter_attributes[] = {
   METER_VALUE
};

static void ElapsedTimeMeter_updateValues(Meter* this) {
   int count = Platform_getElapsedTimes(this->values, 5);
   (void)count; // non-debug builds
   assert(count == 5);

   double train = this->values[0];
   double sample = this->values[1];
   double explain = this->values[2] + this->values[3] + this->values[4];

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%0.2f train, %.2f sample, %.2f explain", train, sample, explain);
}

const MeterClass ElapsedTimeMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = ElapsedTimeMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = 5,
   .total = 0.0,
   .attributes = ElapsedTimeMeter_attributes,
   .name = "ElapsedTime",
   .uiName = "ElapsedTime",
   .caption = "All: "
};
