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

static double* parseValueset(char* valueset, size_t* nValues, double* maximum) {
   size_t nv = 0;
   const int MAX_VALUESET = 64;
   double* result = calloc(MAX_VALUESET, sizeof(double));
   double largest = 0.0;
   char* tmp = valueset;
   char* token;

   if (!result) {
      *maximum = 1.0;
      *nValues = 0;
      return NULL;
   }
   while ((token = strsep(&tmp, ","))) {
      if ((result[nv] = strtod(token, NULL)) > largest)
         largest = result[nv];
      if (++nv >= MAX_VALUESET)
         break;
   }
   *maximum = largest > 0.0 ? largest : 1.0;
   *nValues = nv;
   return result;
}

static void TargetValueMeter_updateValues(Meter* this) {
   char* valueset = Platform_getTargetValueset();
   const Machine* host = this->host;
   double maximum;
   double* values;
   size_t nValues;

   // split into comma-separated list and convert to double
   values = parseValueset(valueset, &nValues, &maximum);

   // prepare value array for GraphMeterMode_draw rendering
   GraphData* data = &this->drawData;
   free(data->values);
   data->values = values;
   data->nValues = nValues;
   data->time = host->realtime; // prevents subsequent values/nValues updates

   this->curItems = 1;
   this->values[0] = 0; // most recent

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%.2f", this->values[0]);
}

static void TargetValueMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   char buffer[20];
   int len;

   len = xSnprintf(buffer, sizeof(buffer), "%.2f ", this->values[0]);
   RichString_appendnAscii(out, CRT_colors[METER_VALUE], buffer, len);
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
   .caption = "Values: "
};
