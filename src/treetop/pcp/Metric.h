#ifndef HEADER_Metric
#define HEADER_Metric
/*
htop - Metric.h
(C) 2020-2021 htop dev team
(C) 2020-2021 Red Hat, Inc.
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include <ctype.h>
#include <stdbool.h>
#include <pcp/pmapi.h>
#include <sys/time.h>

/* use htop config.h values for these macros, not pcp values */
#undef PACKAGE_URL
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_BUGREPORT


typedef enum Metric_ {
   PCP_TARGET_METRIC,		/* treetop.server.target.metric */
   PCP_TARGET_TIMESTAMP,	/* treetop.server.target.timestamp */
   PCP_TARGET_VALUESET,		/* treetop.server.target.valueset */
   PCP_SAMPLING_COUNT,		/* treetop.server.sampling.count */
   PCP_SAMPLING_INTERVAL,	/* treetop.server.sampling.interval */
   PCP_SAMPLING_ELAPSED,	/* treetop.server.sampling.elapsed_time */
   PCP_TRAINING_COUNT,		/* treetop.server.training.count */
   PCP_TRAINING_INTERVAL,	/* treetop.server.training.interval */
   PCP_TRAINING_WINDOW,		/* treetop.server.training.window */
   PCP_TRAINING_BOOSTED,	/* treetop.server.training.boosted_rounds */
   PCP_TRAINING_ELAPSED,	/* treetop.server.training.elapsed_time */
   PCP_FEATURES_ANOMALIES,	/* treetop.server.features.anomalies */
   PCP_FEATURES_MISSING,	/* treetop.server.features.missing_values */
   PCP_FEATURES_MUTUALINFO,	/* treetop.server.features.mutual_information */
   PCP_FEATURES_VARIANCE,	/* treetop.server.features.variance */
   PCP_FEATURES_TOTAL,		/* treetop.server.features.total */
   PCP_MODEL_CONFIDENCE,	/* treetop.server.explaining.model.confidence */
   PCP_MODEL_FEATURES,		/* treetop.server.explaining.model.features */
   PCP_MODEL_IMPORTANCE,	/* treetop.server.explaining.model.importance */
   PCP_IMPORTANCE_TYPE,		/* treetop.server.explaining.model.importance_type */
   PCP_MODEL_MUTUALINFO,	/* treetop.server.explaining.model.mutual_information */
   PCP_MODEL_ELAPSED,		/* treetop.server.explaining.model.elapsed_time */
   PCP_SHAP_FEATURES,		/* treetop.server.explaining.shap.features */
   PCP_SHAP_VALUES,		/* treetop.server.explaining.shap.values */
   PCP_SHAP_MUTUALINFO,		/* treetop.server.explaining.shap.mutual_information */
   PCP_SHAP_ELAPSED,		/* treetop.server.explaining.shap.elapsed_time */
   PCP_OPTMAX_CHANGE,		/* treetop.server.optimising.maxima.change */
   PCP_OPTMAX_DIRECTION,	/* treetop.server.optimising.maxima.direction */
   PCP_OPTMAX_FEATURES,		/* treetop.server.optimising.maxima.features */
   PCP_OPTMIN_CHANGE,		/* treetop.server.optimising.minima.change */
   PCP_OPTMIN_DIRECTION,	/* treetop.server.optimising.minima.direction */
   PCP_OPTMIN_FEATURES,		/* treetop.server.optimising.minima.features */
   PCP_OPTIMA_ELAPSED,		/* treetop.server.optimising.elapsed_time */

   PCP_METRIC_COUNT             /* total metric count */
} Metric;

void Metric_enable(Metric metric, bool enable);

bool Metric_enabled(Metric metric);

bool Metric_fetch(struct timeval* timestamp);

bool Metric_iterate(Metric metric, int* instp, int* offsetp);

pmAtomValue* Metric_values(Metric metric, pmAtomValue* atom, int count, int type);

const pmDesc* Metric_desc(Metric metric);

int Metric_type(Metric metric);

int Metric_instanceCount(Metric metric);

int Metric_instanceOffset(Metric metric, int inst);

pmAtomValue* Metric_instance(Metric metric, int inst, int offset, pmAtomValue* atom, int type);

void Metric_externalName(Metric metric, int inst, char** externalName);

int Metric_lookupText(const char* metric, char** desc);

#endif
