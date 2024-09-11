/*
htop - TargetMetricMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "TargetMetricMeter.h"

#include "CRT.h"
#include "Object.h"
#include "TreeTop.h"
#include "XUtils.h"


static const int TargetMetricMeter_attributes[] = {
   METER_VALUE
};

static void TargetMetricMeter_updateValues(Meter* this) {
   char* target = Platform_getTargetMetric();
   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s", target);
}

const MeterClass TargetMetricMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = TargetMetricMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE) | (1 << LED_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = TargetMetricMeter_attributes,
   .name = "TargetMetric",
   .uiName = "TargetMetric",
   .caption = "Target: "
};
