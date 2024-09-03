/*
treetop - FeaturesMeter.c
(C) 2024 Nathan Scott
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "FeaturesMeter.h"

#include "CRT.h"
#include "Object.h"
#include "TreeTop.h"
#include "XUtils.h"


static const int FeaturesMeter_attributes[] = {
   METER_VALUE
};

static void FeaturesMeter_updateValues(Meter* this) {
   int total, missing, mutual, variance;
   Platform_getFeatures(&total, &mutual, &missing, &variance);

   this->total = MAXIMUM(total, this->total);
   this->values[0] = total == -1 ? NAN : total;
   this->values[1] = missing == -1 ? NAN : missing;
   this->values[2] = mutual == -1 ? NAN : missing;
   this->values[3] = variance == -1 ? NAN : variance;

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%dtot %dvar %dmi %dnan", total, variance, mutual, missing);
}

const MeterClass FeaturesMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = FeaturesMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE) | (1 << LED_METERMODE),
   .maxItems = 4,
   .total = 256.0,
   .attributes = FeaturesMeter_attributes,
   .name = "Features",
   .uiName = "Features",
   .caption = "Features: "
};
