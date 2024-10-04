/*
htop - TargetValueMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "TargetValueMeter.h"

#include "CRT.h"
#include "Machine.h"
#include "Object.h"
#include "Platform.h"
#include "RichString.h"
#include "XUtils.h"


static const int TargetValueMeter_attributes[] = {
   METER_VALUE,
};

static const char* const TargetValueMeter_captions[] = {
   "Currently: ",
   "Lag",
};

static void TargetValueMeter_updateValues(Meter* this) {
   const Machine* host = this->host;
   double* values, maximum;
   size_t nValues;

   values = Platform_getTargetValueset(&nValues, &maximum);

   // prepare value array for GraphMeterMode_draw rendering
   GraphData* data = &this->drawData;
   free(data->values);
   data->values = values;
   data->nValues = nValues;
   data->time = host->realtime;
   data->time.tv_sec++; // workaround, skip logic in GraphMeterMode_draw

   this->total = maximum;
   this->curItems = 1;
   this->values[0] = values[0];
   if (this->mode == GRAPH_METERMODE) {
      this->caption = TargetValueMeter_captions[1];
   } else {
      this->caption = TargetValueMeter_captions[0];
   }

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%.2f", this->values[0]);
}

static void TargetValueMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   char buffer[20];
   int len;

   len = xSnprintf(buffer, sizeof(buffer), "%.2f ", this->values[0]);
   RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, len);
}

static void TargetValueMeter_done(Meter* this) {
   /* reset to starting point for safe delete */
   this->caption = NULL;
}

const MeterClass TargetValueMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = TargetValueMeter_display,
   },
   .updateValues = TargetValueMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = 1,
   .total = 100.0,
   .attributes = TargetValueMeter_attributes,
   .name = "TargetValue",
   .uiName = "Values",
   .description = "Target values preceding the current timestamp",
   .caption = TargetValueMeter_captions[0],
   .done = TargetValueMeter_done
};
