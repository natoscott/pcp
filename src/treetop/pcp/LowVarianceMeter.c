/*
htop - LowVarianceMeter.c
(C) 2024 Nathan Scott
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "LowVarianceMeter.h"

#include "CRT.h"
#include "Object.h"
#include "TreeTop.h"
#include "XUtils.h"


static const int LowVarianceMeter_attributes[] = {
   METER_VALUE_OK
};

static void LowVarianceMeter_updateValues(Meter* this) {
   unsigned int features, variance;
   int sts = Platform_getLowVariance(&features, &variance);
   if (sts < 0) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "(unknown)");
      return;
   }
   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s%s%s%s", daysbuf, hoursbuf, minsbuf, secsbuf);
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
