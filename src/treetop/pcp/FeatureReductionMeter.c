/*
htop - FeatureReductionMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "FeatureReductionMeter.h"

#include "CRT.h"
#include "Object.h"
#include "TreeTop.h"
#include "XUtils.h"


static const int FeatureReductionMeter_attributes[] = {
   METER_VALUE_OK
};

static void FeatureReductionMeter_updateValues(Meter* this) {
   
}

const MeterClass FeatureReductionMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = FeatureReductionMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE) | (1 << LED_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = FeatureReductionMeter_attributes,
   .name = "FeatureReduction",
   .uiName = "FeatureReduction",
   .caption = "Window: "
};
