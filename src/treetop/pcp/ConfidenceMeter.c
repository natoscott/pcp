/*
htop - ConfidenceMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "ConfidenceMeter.h"

#include "CRT.h"
#include "Object.h"
#include "TreeTop.h"
#include "XUtils.h"


static const int ConfidenceMeter_attributes[] = {
   METER_VALUE_OK
};

static void ConfidenceMeter_updateValues(Meter* this) {
   double confidence = Platform_getConfidence();
   this->values[0] = confidence;
   if (confidence <= 0.0) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "(unknown)");
      return;
   }
   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%.3f%%", confidence);
}

static void ConfidenceMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   bool flag = this->values[0] < 90.0;  /* percent confidence */
   int attrs = flag ? CRT_colors[METER_VALUE_WARN] : CRT_colors[METER_VALUE_OK];
   char buffer[20];
   int len;

   len = xSnprintf(buffer, sizeof(buffer), "%.3f%%", this->values[0]);
   RichString_appendnAscii(out, attrs, buffer, len);
}

const MeterClass ConfidenceMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = ConfidenceMeter_display
   },
   .updateValues = ConfidenceMeter_updateValues,
   .defaultMode = BAR_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE) | (1 << LED_METERMODE),
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = 1,
   .total = 100.0,
   .attributes = ConfidenceMeter_attributes,
   .name = "Confidence",
   .uiName = "Confidence",
   .caption = "Acc"
};
