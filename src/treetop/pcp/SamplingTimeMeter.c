/*
htop - SamplingTimeMeter.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "SamplingTimeMeter.h"

#include "CRT.h"
#include "Object.h"
#include "TreeTop.h"
#include "XUtils.h"


static const int SamplingTimeMeter_attributes[] = {
   METER_VALUE
};

static void SamplingTimeMeter_updateValues(Meter* this) {
   int totalseconds = (int)Platform_getSamplingTime();
   if (totalseconds <= 0.0) {
      xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "(unknown)");
      return;
   }
   int seconds = totalseconds % 60;
   int minutes = (totalseconds / 60) % 60;
   int hours = (totalseconds / 3600) % 24;
   int days = (totalseconds / 86400);

   char daysbuf[32];
   if (days > 1) {
      xSnprintf(daysbuf, sizeof(daysbuf), "%d days, ", days);
   } else if (days == 1) {
      xSnprintf(daysbuf, sizeof(daysbuf), "1 day, ");
   } else {
      daysbuf[0] = '\0';
   }
   char hoursbuf[32];
   if (hours > 1) {
      xSnprintf(hoursbuf, sizeof(hoursbuf), "%d hours ", hours);
   } else if (hours == 1) {
      xSnprintf(hoursbuf, sizeof(hoursbuf), "1 hour ");
   } else {
      hoursbuf[0] = '\0';
   }
   char minsbuf[32];
   if (minutes > 1) {
      xSnprintf(minsbuf, sizeof(minsbuf), "%d mins ", minutes);
   } else if (minutes == 1) {
      xSnprintf(minsbuf, sizeof(minsbuf), "1 min ");
   } else {
      minsbuf[0] = '\0';
   }
   char secsbuf[32];
   if (seconds > 1) {
      xSnprintf(secsbuf, sizeof(secsbuf), "%d secs ", seconds);
   } else if (seconds == 1) {
      xSnprintf(secsbuf, sizeof(secsbuf), "1 min ");
   } else {
      secsbuf[0] = '\0';
   }
   xSnprintf(this->txtBuffer, sizeof(this->txtBuffer), "%s%s%s%s", daysbuf, hoursbuf, minsbuf, secsbuf);
}

const MeterClass SamplingTimeMeter_class = {
   .super = {
      .extends = Class(Meter),
      .delete = Meter_delete
   },
   .updateValues = SamplingTimeMeter_updateValues,
   .defaultMode = TEXT_METERMODE,
   .supportedModes = (1 << TEXT_METERMODE) | (1 << LED_METERMODE),
   .maxItems = 0,
   .total = 0.0,
   .attributes = SamplingTimeMeter_attributes,
   .name = "SamplingTime",
   .uiName = "SamplingTime",
   .caption = "Sampling: "
};
