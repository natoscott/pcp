/*
htop - ElapsedTimeMeter.c
(C) 2024 Nathan Scott.
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
   DYNAMIC_GREEN,
   DYNAMIC_BLUE,
   DYNAMIC_CYAN,
   DYNAMIC_MAGENTA,
   DYNAMIC_YELLOW,
};

static void ElapsedTimeMeter_updateValues(Meter* this) {
   int count = Platform_getElapsedTimes(this->values, 5);
   (void)count; // compiler warning, non-debug builds
   assert(count == 5);

   double train = this->values[0];
   double sample = this->values[1];
   double explain = this->values[2] + this->values[3] + this->values[4];
   double total = train + sample + explain;
   this->total = MAXIMUM(total, this->total);

   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%.1f/%.1f/%.1f", train, sample, explain);
}

static void ElapsedTimeMeter_display(const Object* cast, RichString* out) {
   const Meter* this = (const Meter*)cast;
   char buffer[20];
   int len;

   len = xSnprintf(buffer, sizeof(buffer), "%.1f", this->values[0]);
   RichString_appendnAscii(out, CRT_colors[DYNAMIC_GREEN], buffer, len);

   RichString_appendAscii(out, CRT_colors[METER_TEXT], "tr ");
   len = xSnprintf(buffer, sizeof(buffer), "%.1f", this->values[1]);
   RichString_appendnAscii(out, CRT_colors[DYNAMIC_BLUE], buffer, len);
   RichString_appendAscii(out, CRT_colors[METER_SHADOW], "sa ");

   float explain = this->values[2] + this->values[3] + this->values[4];
   len = xSnprintf(buffer, sizeof(buffer), "%.1f", explain);
   RichString_appendnAscii(out, CRT_colors[DYNAMIC_GREEN], buffer, len);
   RichString_appendAscii(out, CRT_colors[METER_SHADOW], "xp");
}

const MeterClass ElapsedTimeMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete,
      .display = ElapsedTimeMeter_display,
   },
   .updateValues = ElapsedTimeMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = METERMODE_DEFAULT_SUPPORTED,
   .maxItems = 5,
   .total = 5.0,
   .attributes = ElapsedTimeMeter_attributes,
   .name = "ElapsedTime",
   .uiName = "ElapsedTime",
   .caption = "Time: "
};
