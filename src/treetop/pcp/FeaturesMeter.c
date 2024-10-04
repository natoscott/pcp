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
   size_t total, missing, mutual, variance;
   Platform_getFeatures(&total, &mutual, &missing, &variance);

   this->total = MAXIMUM(total, this->total);
   this->values[0] = total == -1 ? NAN : total;
   this->values[1] = variance == -1 ? NAN : variance;
   this->values[2] = mutual == -1 ? NAN : mutual;
   this->values[3] = missing == -1 ? NAN : missing;

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%zu %zuva %zumi %zuna", total, variance, mutual, missing);
}

static void FeaturesMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   char buffer[20];
   int len;

   len = xSnprintf(buffer, sizeof(buffer), "%zu", (size_t)this->values[0]);
   RichString_appendnAscii(out, CRT_colors[METER_TEXT], buffer, len);

   len = xSnprintf(buffer, sizeof(buffer), " %zu", (size_t)this->values[1]);
   RichString_appendnAscii(out, CRT_colors[METER_SHADOW], buffer, len);
   RichString_appendAscii(out, CRT_colors[METER_TEXT], "va");

   len = xSnprintf(buffer, sizeof(buffer), " %zu", (size_t)this->values[2]);
   RichString_appendnAscii(out, CRT_colors[METER_SHADOW], buffer, len);
   RichString_appendAscii(out, CRT_colors[METER_TEXT], "mi");

   len = xSnprintf(buffer, sizeof(buffer), " %zu", (size_t)this->values[3]);
   RichString_appendnAscii(out, CRT_colors[METER_SHADOW], buffer, len);
   RichString_appendAscii(out, CRT_colors[METER_TEXT], "na");
}

const MeterClass FeaturesMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = FeaturesMeter_display,
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
