/*
htop - pcp/TreeTop.c
(C) 2024 Nathan Scott
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "pcp/TreeTop.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pcp/mmv_stats.h>

#include "CPUMeter.h"
#include "ClockMeter.h"
#include "DateMeter.h"
#include "DateTimeMeter.h"
#include "DynamicColumn.h"
#include "DynamicMeter.h"
#include "DynamicScreen.h"
#include "HostnameMeter.h"
#include "Macros.h"
#include "Meter.h"
#include "ProcessTable.h"
#include "Settings.h"
#include "SysArchMeter.h"
#include "XUtils.h"

#include "ConfidenceMeter.h"
#include "ElapsedTimeMeter.h"
#include "FeaturesMeter.h"
#include "SampleIntervalMeter.h"
#include "SamplingTimeMeter.h"
#include "TargetMetricMeter.h"
#include "TargetTimestampMeter.h"
#include "TargetValueMeter.h"
#include "TrainingTimeMeter.h"
#include "TrainingWindowMeter.h"

#include "pcp/Metric.h"
#include "pcp/PCPDynamicColumn.h"
#include "pcp/PCPDynamicMeter.h"
#include "pcp/PCPDynamicScreen.h"
#include "pcp/PCPMachine.h"
#include "pcp/TreeTopProcessTable.h"


Platform* pcp;

const ScreenDefaults Platform_defaultScreens[] = {
  { .name = "Model feature importance",
    .columns = "FEATURE IMPORTANCE MUTUALINFO",
  },
};

const unsigned int Platform_numberOfDefaultScreens = ARRAYSIZE(Platform_defaultScreens);

const SignalItem Platform_signals[] = {
   { .name = " 0 Cancel",    .number = 0 },
};

const unsigned int Platform_numberOfSignals = ARRAYSIZE(Platform_signals);

const MeterClass* const Platform_meterTypes[] = {
   &ConfidenceMeter_class,
   &ElapsedTimeMeter_class,
   &FeaturesMeter_class,
   &SampleIntervalMeter_class,
   &SamplingTimeMeter_class,
   &TargetMetricMeter_class,
   &TargetTimestampMeter_class,
   &TargetValueMeter_class,
   &TrainingTimeMeter_class,
   &TrainingWindowMeter_class,
   &BlankMeter_class,
   &ClockMeter_class,
   &DateMeter_class,
   &DateTimeMeter_class,
   &HostnameMeter_class,
   &SysArchMeter_class,
   NULL
};

static const char* Platform_metricNames[] = {
   [PCP_TARGET_METRIC] = "mmv.treetop.server.target.metric",
   [PCP_TARGET_TIMESTAMP] = "mmv.treetop.server.target.timestamp",
   [PCP_TARGET_VALUESET] = "mmv.treetop.server.target.valueset",
   [PCP_SAMPLING_COUNT] = "mmv.treetop.server.sampling.count",
   [PCP_SAMPLING_INTERVAL] = "mmv.treetop.server.sampling.interval",
   [PCP_SAMPLING_ELAPSED] = "mmv.treetop.server.sampling.elapsed_time",
   [PCP_TRAINING_COUNT] = "mmv.treetop.server.training.count",
   [PCP_TRAINING_INTERVAL] = "mmv.treetop.server.training.interval",
   [PCP_TRAINING_WINDOW] = "mmv.treetop.server.training.window",
   [PCP_TRAINING_BOOSTED] = "mmv.treetop.server.training.boosted_rounds",
   [PCP_TRAINING_ELAPSED] = "mmv.treetop.server.training.elapsed_time",
   [PCP_FEATURES_ANOMALIES] = "mmv.treetop.server.features.anomalies",
   [PCP_FEATURES_MISSING] = "mmv.treetop.server.features.missing_values",
   [PCP_FEATURES_MUTUALINFO] = "mmv.treetop.server.features.mutual_information",
   [PCP_FEATURES_VARIANCE] = "mmv.treetop.server.features.variance",
   [PCP_FEATURES_TOTAL] = "mmv.treetop.server.features.total",
   [PCP_MODEL_CONFIDENCE] = "mmv.treetop.server.explaining.model.confidence",
   [PCP_MODEL_FEATURES] = "mmv.treetop.server.explaining.model.features",
   [PCP_MODEL_IMPORTANCE] = "mmv.treetop.server.explaining.model.importance",
   [PCP_IMPORTANCE_TYPE] = "mmv.treetop.server.explaining.model.importance_type",
   [PCP_MODEL_MUTUALINFO] = "mmv.treetop.server.explaining.model.mutual_information",
   [PCP_MODEL_FEATURES] = "mmv.treetop.server.explaining.model.features",
   [PCP_MODEL_ELAPSED] = "mmv.treetop.server.explaining.model.elapsed_time",
   [PCP_SHAP_FEATURES] = "mmv.treetop.server.explaining.shap.features",
   [PCP_SHAP_VALUES] = "mmv.treetop.server.explaining.shap.values",
   [PCP_SHAP_MUTUALINFO] = "mmv.treetop.server.explaining.shap.mutual_information",
   [PCP_SHAP_ELAPSED] = "mmv.treetop.server.explaining.shap.elapsed_time",
   [PCP_OPTMAX_CHANGE] = "mmv.treetop.server.optimising.maxima.change",
   [PCP_OPTMAX_DIRECTION] = "mmv.treetop.server.optimising.maxima.direction",
   [PCP_OPTMAX_FEATURES] = "mmv.treetop.server.optimising.maxima.features",
   [PCP_OPTMIN_CHANGE] = "mmv.treetop.server.optimising.minima.change",
   [PCP_OPTMIN_DIRECTION] = "mmv.treetop.server.optimising.minima.direction",
   [PCP_OPTMIN_FEATURES] = "mmv.treetop.server.optimising.minima.features",
   [PCP_OPTIMA_ELAPSED] = "mmv.treetop.server.optimising.elapsed_time",

   [PCP_METRIC_COUNT] = NULL
};

#ifndef HAVE_PMLOOKUPDESCS
/*
 * pmLookupDescs(3) exists in latest versions of libpcp (5.3.6+),
 * but for older versions we provide an implementation here. This
 * involves multiple round trips to pmcd though, which the latest
 * libpcp version avoids by using a protocol extension.  In time,
 * perhaps in a few years, we could remove this back-compat code.
 */
int pmLookupDescs(int numpmid, pmID* pmids, pmDesc* descs) {
   int count = 0;

   for (int i = 0; i < numpmid; i++) {
      /* expect some metrics to be missing - e.g. PMDA not available */
      if (pmids[i] == PM_ID_NULL)
         continue;

      int sts = pmLookupDesc(pmids[i], &descs[i]);
      if (sts < 0) {
         if (pmDebugOptions.appl0)
            fprintf(stderr, "Error: cannot lookup metric %s(%s): %s\n",
                    pcp->names[i], pmIDStr(pcp->pmids[i]), pmErrStr(sts));
         pmids[i] = PM_ID_NULL;
         continue;
      }

      count++;
   }
   return count;
}
#endif

size_t Platform_addMetric(Metric id, const char* name) {
   unsigned int i = (unsigned int)id;

   if (i >= PCP_METRIC_COUNT && i >= pcp->totalMetrics) {
      /* added via configuration files */
      size_t j = pcp->totalMetrics + 1;
      pcp->fetch = xRealloc(pcp->fetch, j * sizeof(pmID));
      pcp->pmids = xRealloc(pcp->pmids, j * sizeof(pmID));
      pcp->names = xRealloc(pcp->names, j * sizeof(char*));
      pcp->descs = xRealloc(pcp->descs, j * sizeof(pmDesc));
      memset(&pcp->descs[i], 0, sizeof(pmDesc));
   }

   pcp->pmids[i] = pcp->fetch[i] = PM_ID_NULL;
   pcp->names[i] = name;
   return ++pcp->totalMetrics;
}

/* global state from the environment and command line arguments */
pmOptions opts;

static void* MMV_init();
static void MMV_done(void* map);

bool Platform_init(void) {
   pcp = xCalloc(1, sizeof(Platform));
   pcp->map = MMV_init();

#if 0
   const char* source;
   if (opts.context == PM_CONTEXT_ARCHIVE) {
      source = opts.archives[0];
   } else if (opts.context == PM_CONTEXT_HOST) {
      source = opts.nhosts > 0 ? opts.hosts[0] : "local:";
   } else {
      opts.context = PM_CONTEXT_HOST;
      source = "local:";
   }
#endif

   //
   // TODO: PCP_TMP_DIR for MMV
   // TODO: exec server.py child process (pass args)
   // TODO: MMV shared library through local context (server and client)
   //
   // but for now, use PMCD:
   //
   int sts;
   opts.context = PM_CONTEXT_HOST;
   sts = pmNewContext(opts.context, "local:");
   /* with no host requested, fallback to PM_CONTEXT_LOCAL shared libraries */
   if (sts < 0) {
      opts.context = PM_CONTEXT_LOCAL;
      sts = pmNewContext(opts.context, NULL);
   }
   if (sts < 0) {
      fprintf(stderr, "Cannot setup PCP metric source: %s\n", pmErrStr(sts));
      Platform_done();
      return false;
   }
   /* setup timezones and other general startup preparation completion */
   if (pmGetContextOptions(sts, &opts) < 0 || opts.errors) {
      pmflush();
      Platform_done();
      return false;
   }

   pcp->context = sts;
   pcp->fetch = xCalloc(PCP_METRIC_COUNT, sizeof(pmID));
   pcp->pmids = xCalloc(PCP_METRIC_COUNT, sizeof(pmID));
   pcp->names = xCalloc(PCP_METRIC_COUNT, sizeof(char*));
   pcp->descs = xCalloc(PCP_METRIC_COUNT, sizeof(pmDesc));

   if (opts.context == PM_CONTEXT_ARCHIVE) {
      gettimeofday(&pcp->offset, NULL);
      pmtimevalDec(&pcp->offset, &opts.start);
   }

   for (size_t i = 0; i < PCP_METRIC_COUNT; i++)
      Platform_addMetric(i, Platform_metricNames[i]);
   pcp->meters.offset = PCP_METRIC_COUNT;

   PCPDynamicMeters_init(&pcp->meters);

   pcp->columns.offset = PCP_METRIC_COUNT + pcp->meters.cursor;
   PCPDynamicColumns_init(&pcp->columns);
   PCPDynamicScreens_init(&pcp->screens, &pcp->columns);

   sts = pmLookupName(pcp->totalMetrics, pcp->names, pcp->pmids);
   if (sts < 0) {
      fprintf(stderr, "Error: cannot lookup metric names: %s\n", pmErrStr(sts));
      Platform_done();
      return false;
   }

   sts = pmLookupDescs(pcp->totalMetrics, pcp->pmids, pcp->descs);
   if (sts < 1) {
      if (sts < 0)
         fprintf(stderr, "Error: cannot lookup descriptors: %s\n", pmErrStr(sts));
      else /* ensure we have at least one valid metric to work with */
         fprintf(stderr, "Error: cannot find a single valid metric, exiting\n");
      Platform_done();
      return false;
   }

   /* extract values needed for default setup */
   for (size_t i = 0; i < PCP_METRIC_COUNT; i++)
      Metric_enable(i, true);

   /* enable metrics for all dynamic columns and dynamic screens */
   size_t total = pcp->columns.offset + pcp->columns.count;
   for (size_t i = pcp->columns.offset; i < total; i++)
      Metric_enable(i, true);

   Metric_fetch(NULL);

   return true;
}

void Platform_dynamicColumnsDone(Hashtable* columns) {
   PCPDynamicColumns_done(columns);
}

void Platform_dynamicMetersDone(Hashtable* meters) {
   PCPDynamicMeters_done(meters);
}

void Platform_dynamicScreensDone(Hashtable* screens) {
   PCPDynamicScreens_done(screens);
}

void Platform_done(void) {
   MMV_done(pcp->map);
   pmDestroyContext(pcp->context);
   if (pcp->result)
      pmFreeResult(pcp->result);
   free(pcp->release);
   free(pcp->fetch);
   free(pcp->pmids);
   free(pcp->names);
   free(pcp->descs);
   free(pcp);
}

void Platform_setBindings(Htop_Action* keys) {
   /* no platform-specific key bindings */
   (void)keys;
}

double Platform_getConfidence(void) {
   pmAtomValue value;
   if (Metric_values(PCP_MODEL_CONFIDENCE, &value, 1, PM_TYPE_DOUBLE) == NULL)
      return 0.0;
   return value.d;
}

void Platform_getFeatures(size_t* total, size_t* missing, size_t* mutual, size_t* variance) {
   pmAtomValue value;
   if (Metric_values(PCP_FEATURES_TOTAL, &value, 1, PM_TYPE_32) == NULL)
      *total = 0;
   else
      *total = value.l;
   if (Metric_values(PCP_FEATURES_MISSING, &value, 1, PM_TYPE_32) == NULL)
      *missing = 0;
   else
      *missing = value.l;
   if (Metric_values(PCP_FEATURES_MUTUALINFO, &value, 1, PM_TYPE_32) == NULL)
      *mutual = 0;
   else
      *mutual = value.l;
   if (Metric_values(PCP_FEATURES_VARIANCE, &value, 1, PM_TYPE_32) == NULL)
      *variance = 0;
   else
      *variance = value.l;
}

int Platform_getElapsedTimes(double* values, int count) {
   pmAtomValue value;
   int i = 0;

   memset(values, 0, sizeof(double) * count);
   Metric_values(PCP_TRAINING_ELAPSED, &value, 1, PM_TYPE_DOUBLE);
   values[i++] = value.d;
   if (i == count) return i;
   Metric_values(PCP_SAMPLING_ELAPSED, &value, 1, PM_TYPE_DOUBLE);
   values[i++] = value.d;
   if (i == count) return i;
   Metric_values(PCP_MODEL_ELAPSED, &value, 1, PM_TYPE_DOUBLE);
   values[i++] = value.d;
   if (i == count) return i;
   Metric_values(PCP_SHAP_ELAPSED, &value, 1, PM_TYPE_DOUBLE);
   values[i++] = value.d;
   if (i == count) return i;
   Metric_values(PCP_OPTIMA_ELAPSED, &value, 1, PM_TYPE_DOUBLE);
   values[i++] = value.d;
   return i;
}

double Platform_getTrainingInterval(void) {
   pmAtomValue value;
   if (Metric_values(PCP_TRAINING_INTERVAL, &value, 1, PM_TYPE_DOUBLE) == NULL)
      return 0.0;
   return value.d;
}

int Platform_getSampleCount(void) {
   pmAtomValue value;
   if (Metric_values(PCP_SAMPLING_COUNT, &value, 1, PM_TYPE_32) == NULL)
      return -1;
   return value.l;
}

double Platform_getSampleInterval(void) {
   pmAtomValue value;
   if (Metric_values(PCP_SAMPLING_INTERVAL, &value, 1, PM_TYPE_DOUBLE) == NULL)
      return 0.0;
   return value.d;
}

double Platform_getTrainingTime(void) {
   pmAtomValue value;
   if (Metric_values(PCP_TRAINING_ELAPSED, &value, 1, PM_TYPE_DOUBLE) == NULL)
      return 0.0;
   return value.d;
}

double Platform_getSamplingTime(void) {
   pmAtomValue value;
   if (Metric_values(PCP_SAMPLING_ELAPSED, &value, 1, PM_TYPE_DOUBLE) == NULL)
      return 0.0;
   return value.d;
}

double Platform_getTrainingWindow(void) {
   pmAtomValue value;
   if (Metric_values(PCP_TRAINING_WINDOW, &value, 1, PM_TYPE_DOUBLE) == NULL)
      return 0.0;
   return value.d;
}

char* Platform_getTargetMetric(void) {
   pmAtomValue value;
   if (Metric_values(PCP_TARGET_METRIC, &value, 1, PM_TYPE_STRING) == NULL)
      return NULL;
   return value.cp;
}

char* Platform_getTargetTimestamp(void) {
   pmAtomValue value;
   if (Metric_values(PCP_TARGET_TIMESTAMP, &value, 1, PM_TYPE_STRING) == NULL)
      return NULL;
   return value.cp;
}

double* Platform_getTargetValueset(size_t *count, double* maximum) {
   pmAtomValue value[MAX_METER_GRAPHDATA_VALUES];
   double* values, largest = 0.0;

   *count = 0;
   *maximum = 0.0;

   if (Metric_values(PCP_TARGET_VALUESET, &value[0], MAX_METER_GRAPHDATA_VALUES, PM_TYPE_DOUBLE) == NULL)
      return NULL;

   size_t instances = Metric_instanceCount(PCP_TARGET_VALUESET);
   if ((values = calloc(instances, sizeof(double))) == NULL)
      return NULL;

   for (size_t i = 0; i < instances; i++) {
      largest = MAXIMUM(largest, value[i].d);
      values[i] = value[i].d;
   }
   *maximum = largest;
   *count = instances;
   return values;
}

#if 0
int Platform_getUptime(void) {
   return 0;
}

unsigned int Platform_getMaxCPU(void) {
   return 1;
}
#endif

pid_t Platform_getMaxPid(void) {
   return INT_MAX;
}

#if 0
long long Platform_getBootTime(void) {
   return 0;
}
#endif

double Platform_setCPUValues(Meter* this, int cpu) {
   (void)this; (void)cpu;
   return 0;
}

void Platform_getHostname(char* buffer, size_t size) {
   const char* hostname = pmGetContextHostName(pcp->context);
   String_safeStrncpy(buffer, hostname, size);
}

void Platform_getRelease(char** string) {
   *string = NULL;
}

char* Platform_getProcessEnv(pid_t pid) {
   (void)pid;
   return NULL;
}

FileLocks_ProcessData* Platform_getProcessLocks(pid_t pid) {
   (void)pid;
   return NULL;
}

void Platform_longOptionsUsage(ATTR_UNUSED const char* name) {
   printf(
"   --host=HOSTSPEC              metrics source is PMCD at HOSTSPEC [see PCPIntro(1)]\n"
"   --hostzone                   set reporting timezone to local time of metrics source\n"
"   --timezone=TZ                set reporting timezone\n");
}

CommandLineStatus Platform_getLongOption(int opt, ATTR_UNUSED int argc, char** argv) {
   /* libpcp export without a header definition */
   extern void __pmAddOptHost(pmOptions*, char*);

   switch (opt) {
      case PLATFORM_LONGOPT_HOST:  /* --host=HOSTSPEC */
         if (argv[optind][0] == '\0')
            return STATUS_ERROR_EXIT;
         __pmAddOptHost(&opts, optarg);
         return STATUS_OK;

      case PLATFORM_LONGOPT_HOSTZONE:  /* --hostzone */
         if (opts.timezone) {
            pmprintf("%s: at most one of -Z and -z allowed\n", pmGetProgname());
            opts.errors++;
         } else {
            opts.tzflag = 1;
         }
         return STATUS_OK;

      case PLATFORM_LONGOPT_TIMEZONE:  /* --timezone=TZ */
         if (argv[optind][0] == '\0')
            return STATUS_ERROR_EXIT;
         if (opts.tzflag) {
            pmprintf("%s: at most one of -Z and -z allowed\n", pmGetProgname());
            opts.errors++;
         } else {
            opts.timezone = optarg;
         }
         return STATUS_OK;

      default:
         break;
   }

   return STATUS_ERROR_EXIT;
}

void Platform_gettime_realtime(struct timeval* tv, uint64_t* msec) {
   if (gettimeofday(tv, NULL) == 0) {
      /* shift by start offset to stay in lock-step with realtime (archives) */
      if (pcp->offset.tv_sec || pcp->offset.tv_usec)
         pmtimevalDec(tv, &pcp->offset);
      *msec = ((uint64_t)tv->tv_sec * 1000) + ((uint64_t)tv->tv_usec / 1000);
   } else {
      memset(tv, 0, sizeof(struct timeval));
      *msec = 0;
   }
}

void Platform_gettime_monotonic(uint64_t* msec) {
   if (pcp->result) {
      struct timeval* tv = &pcp->result->timestamp;
      *msec = ((uint64_t)tv->tv_sec * 1000) + ((uint64_t)tv->tv_usec / 1000);
   } else {
      *msec = 0;
   }
}

Hashtable* Platform_dynamicMeters(void) {
   return pcp->meters.table;
}

void Platform_dynamicMeterInit(Meter* meter) {
   PCPDynamicMeter* this = Hashtable_get(pcp->meters.table, meter->param);
   if (this)
      PCPDynamicMeter_enable(this);
}

void Platform_dynamicMeterUpdateValues(Meter* meter) {
   PCPDynamicMeter* this = Hashtable_get(pcp->meters.table, meter->param);
   if (this)
      PCPDynamicMeter_updateValues(this, meter);
}

void Platform_dynamicMeterDisplay(const Meter* meter, RichString* out) {
   PCPDynamicMeter* this = Hashtable_get(pcp->meters.table, meter->param);
   if (this)
      PCPDynamicMeter_display(this, meter, out);
}

Hashtable* Platform_dynamicColumns(void) {
   return pcp->columns.table;
}

const char* Platform_dynamicColumnName(unsigned int key) {
   PCPDynamicColumn* this = Hashtable_get(pcp->columns.table, key);
   if (this) {
      Metric_enable(this->id, true);
      if (this->super.caption)
         return this->super.caption;
      if (this->super.heading)
         return this->super.heading;
      return this->super.name;
   }
   return NULL;
}

bool Platform_dynamicColumnWriteField(const Process* proc, RichString* str, unsigned int key) {
   PCPDynamicColumn* this = Hashtable_get(pcp->columns.table, key);
   if (this) {
      PCPDynamicColumn_writeField(this, proc, str);
      return true;
   }
   return false;
}

Hashtable* Platform_dynamicScreens(void) {
   return pcp->screens.table;
}

void Platform_defaultDynamicScreens(Settings* settings) {
   PCPDynamicScreen_appendScreens(&pcp->screens, settings);
}

void Platform_addDynamicScreen(ScreenSettings* ss) {
   PCPDynamicScreen_addDynamicScreen(&pcp->screens, ss);
}

void Platform_addDynamicScreenAvailableColumns(Panel* availableColumns, const char* screen) {
   Hashtable* screens = pcp->screens.table;
   PCPDynamicScreens_addAvailableColumns(availableColumns, screens, screen);
}

void Platform_updateTables(Machine* host) {
   PCPDynamicScreen_appendTables(&pcp->screens, host);
   PCPDynamicColumns_setupWidths(&pcp->columns);
}

static mmv_metric2_t metrics[] = {
    {   .name = "target",
        .item = 1,
        .type = MMV_TYPE_STRING,
        .semantics = MMV_SEM_INSTANT,
        .dimension = MMV_UNITS(0,0,0,0,0,0),
        .shorttext = "Prediction target metric",
        .helptext = "Predicted metric with optional [instance] specifier.",
    },
    {   .name = "filter",
        .item = 2,
        .type = MMV_TYPE_STRING,
        .semantics = MMV_SEM_INSTANT,
        .dimension = MMV_UNITS(0,0,0,0,0,0),
        .shorttext = "Manual feature reduction metrics",
        .helptext = "Comma-separated list of metrics removed from training set.",
    },
    {   .name = "sampling.count",
        .item = 3,
        .type = MMV_TYPE_U32,
        .semantics = MMV_SEM_INSTANT,
        .dimension = MMV_UNITS(0,0,0,0,0,0),
        .shorttext = "Requested training set samples",
        .helptext = "Historical training data samples requested",
    },
    {   .name = "sampling.interval",
        .item = 4,
        .type = MMV_TYPE_FLOAT,
        .semantics = MMV_SEM_INSTANT,
        .dimension = MMV_UNITS(0,1,0,0,PM_TIME_SEC,0),
        .shorttext = "Requested training set sampling interval",
        .helptext = "Historical training set interval requested",
    },
    {   .name = "training.interval",
        .item = 5,
        .type = MMV_TYPE_FLOAT,
        .semantics = MMV_SEM_INSTANT,
        .dimension = MMV_UNITS(0,1,0,0,PM_TIME_SEC,0),
        .shorttext = "Requested training frequency",
        .helptext = "Requested frequency at which training occurs",
    },
    {   .name = "timestamp",
        .item = 6,
        .type = MMV_TYPE_STRING,
        .semantics = MMV_SEM_INSTANT,
        .dimension = MMV_UNITS(0,0,0,0,0,0),
        .shorttext = "Current prediction timestamp",
        .helptext = "Prediction time, training ends on prior sample",
    },
};

static const char* file = "treetop.client";
static const char* target = "disk.all.avactive";
static const char* notrain = "disk.all.aveq,disk.all.read,disk.all.blkread,disk.all.read_bytes,disk.all.total,disk.all.blktotal,disk.all.total_bytes,disk.all.write,disk.all.blkwrite,disk.all.write_bytes";
static const char* timestamp = "2012-05-10 08:47:47.462172";
static size_t sample_count = 720;
static float sample_interval = 10;
static float training_interval = 10;

static void* MMV_init(void) {
   // TODO: flags = MMV_FLAG_PROCESS (cull file at stop)
   mmv_registry_t* registry = mmv_stats_registry(file, 40, 0);
   void* map;

   if (!registry) {
      fprintf(stderr, "mmv_stats_registry: %s - %s\n", file, strerror(errno));
      return NULL;
   }
   for (size_t i = 0; i < sizeof(metrics) / sizeof(mmv_metric2_t); i++)
      mmv_stats_add_metric(registry,
			   metrics[i].name, metrics[i].item,
			   metrics[i].type, metrics[i].semantics,
			   metrics[i].dimension, metrics[i].indom,
			   metrics[i].shorttext, metrics[i].helptext);

   map = mmv_stats_start(registry);
   if (!map) {
      fprintf(stderr, "mmv_stats_start: %s - %s\n", file, strerror(errno));
      return NULL;
   }

   mmv_stats_set_string(map, "target", "", target);
   mmv_stats_set_string(map, "filter", "", notrain);
   mmv_stats_set_string(map, "timestamp", "", timestamp);

   mmv_stats_set(map, "sampling.count", "", sample_count);
   mmv_stats_set(map, "sampling.interval", "", sample_interval);
   mmv_stats_set(map, "training.interval", "", training_interval);

   return map;
}

static void MMV_done(void* map) {
   mmv_stats_stop(file, map);
}
