#!/usr/bin/env pmpython
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#

import numpy as np
import pandas as pd
import xgboost as xgb

import copy, os, re, sys, signal, time

from datetime import datetime
from collections import OrderedDict

from sklearn.ensemble import IsolationForest
from sklearn.model_selection import TimeSeriesSplit, cross_validate
from sklearn.feature_selection import VarianceThreshold, mutual_info_regression

from pcp import mmv, pmapi, pmconfig
from cpmapi import ( pmSetContextOptions, PM_COUNT_ONE, PM_TIME_SEC,
                     PM_CONTEXT_HOST, PM_MODE_INTERP, PM_SEM_COUNTER,
                     PM_TYPE_32, PM_TYPE_U32, PM_TYPE_64, PM_TYPE_U64,
                     PM_TYPE_FLOAT, PM_TYPE_DOUBLE, PM_TYPE_STRING,
                     PM_INDOM_NULL )
from cmmv import ( MMV_SEM_INSTANT, MMV_SEM_DISCRETE, MMV_SEM_COUNTER,
                   MMV_TYPE_U32, MMV_TYPE_FLOAT, MMV_TYPE_DOUBLE,
                   MMV_TYPE_STRING, MMV_FLAG_SENTINEL )

try:
    import shap
    shap_explanations = True
except ImportError:
    shap_explanations = False

# signal handling for IPC
(refresh, finished) = (False, False)

def signal_refresh(sig, frame):
    global refresh
    refresh = True
    raise StopIteration('Server refresh signal')

def signal_finished(sig, frame):
    global finished
    finished = True
    raise EOFError('Server finished via signal')

def metricspec(s):
    # change an xgboost-encoded name back into PCP metricspec
    # xgboost requires special handling of [, ] - we use (, ).
    start = s.find('(')
    if start == -1: return s
    return s[:start] + '[' + s[start+1: -1] + ']'

# DIFFI algorithm implementation does not exist as a module
# Source code:  https://github.com/mattiacarletti/DIFFI.git
#  -- Copyright (c) 2020 Mattia Carletti -- MIT License --

def local_diffi(iforest, x):
    # start time
    start = time.time()
    # initialization 
    estimators = iforest.estimators_
    cfi = np.zeros(len(x)).astype('float')
    counter = np.zeros(len(x)).astype('int')
    max_depth = int(np.ceil(np.log2(iforest.max_samples_)))
    # for every iTree in the iForest
    for estimator in estimators:
        n_nodes = estimator.tree_.node_count
        children_left = estimator.tree_.children_left
        children_right = estimator.tree_.children_right
        feature = estimator.tree_.feature
        node_depth = np.zeros(shape=n_nodes, dtype=np.int64)
        is_leaves = np.zeros(shape=n_nodes, dtype=bool)
        # compute node depths
        stack = [(0, -1)]  
        while len(stack) > 0:
            node_id, parent_depth = stack.pop()
            node_depth[node_id] = parent_depth + 1
            # if test node
            if (children_left[node_id] != children_right[node_id]):
                stack.append((children_left[node_id], parent_depth + 1))
                stack.append((children_right[node_id], parent_depth + 1))
            else:
                is_leaves[node_id] = True
        # update cumulative importance and counter
        x = x.reshape(1,-1)
        node_indicator = estimator.decision_path(x)
        node_indicator_array = node_indicator.toarray()
        path = list(np.where(node_indicator_array == 1)[1])
        leaf_depth = node_depth[path[-1]]
        for node in path:
            if not is_leaves[node]:
                current_feature = feature[node] 
                cfi[current_feature] += (1 / leaf_depth) - (1 / max_depth)
                counter[current_feature] += 1
    # compute FI
    fi = np.zeros(len(cfi))
    for i in range(len(cfi)):
        if counter[i] != 0:
            fi[i] = cfi[i] / counter[i]
    end = time.time()
    exec_time = end - start
    return fi, exec_time

def local_diffi_batch(iforest, X):
    fi = []
    ord_idx = []
    exec_time = []
    for i in range(X.shape[0]):
        x_curr = X[i, :]
        fi_curr, exec_time_curr = local_diffi(iforest, x_curr)
        fi.append(fi_curr)
        ord_idx_curr = np.argsort(fi_curr)[::-1]
        ord_idx.append(ord_idx_curr)
        exec_time.append(exec_time_curr)
    fi = np.vstack(fi)
    ord_idx = np.vstack(ord_idx)
    return fi, ord_idx, exec_time 


class TreetopServer():
    """ MMV metrics for treetop server (predict+explain) utility """

    _exit_code = 1  # failure is the default
    _max_features = 16  # number of feature importances reported
    _sample_count = 720  # number of samples in training set
    _sample_interval = 15  # metrics sampling interval (seconds)
    _sample_valueset = 128  # count of raw sample values exported
    _training_count = 0  # number of times training was performed
    _training_interval = 10  # server training interval (seconds)
    _importance_type = 'gain'  # default feature importance measure
    _variance_threshold = 0.125  # minimum level of feature variance
    _mutualinfo_threshold = 0.125  # minimum target mutual information
    _max_anomaly_features = 25  # maximum anomaly features engineered
    _max_mutualinfo_features = 1001 # maximum features post MI reduction

    def __init__(self):
        self.client = None
        self.source = None
        self.datasets = None
        self.timestamp = None
        self.timestamp_s = None
        self.start_time = None
        self.end_time = None
        self.mutualinfo = None
        self.NaN = float("nan")

        # pmconfig state
        self.pmconfig = pmconfig.pmConfig(self)
        self.metrics = OrderedDict()
        self.instances = []
        self.interpol = True
        self.ignore_incompat = True
        self.derived = None
        self.globals = 1

        # command line
        self.opts = self.options()
        self.opts.pmSetOptionInterval(str(self._sample_interval))
        self.opts.pmSetOptionSamples(str(self._sample_count))

        # mmap'd metrics
        self.values = self.mapping()

        # dataset
        self.df = None
        self.columns = []

    def options(self):
        """ Setup default command line argument option handling """
        opts = pmapi.pmOptions()
        opts.pmSetShortOptions("a:h:D:V?A:S:T:O:s:t:Z:z")
        opts.pmSetShortUsage("[option...]")
        opts.pmSetLongOptionArchive()      # -a/--archive
        opts.pmSetLongOptionArchiveFolio() # --archive-folio
        opts.pmSetLongOptionHost()         # -h/--host
        opts.pmSetLongOptionDebug()        # -D/--debug
        opts.pmSetLongOptionVersion()      # -V/--version
        opts.pmSetLongOptionHelp()         # -?/--help
        opts.pmSetLongOptionAlign()        # -A/--align
        opts.pmSetLongOptionStart()        # -S/--start
        opts.pmSetLongOptionFinish()       # -T/--finish
        opts.pmSetLongOptionOrigin()       # -O/--origin
        opts.pmSetLongOptionSamples()      # -s/--samples
        opts.pmSetLongOptionInterval()     # -t/--interval
        opts.pmSetLongOptionTimeZone()     # -Z/--timezone
        opts.pmSetLongOptionHostZone()     # -z/--hostzone
        return opts

    def configure(self):
        os.putenv('PCP_DERIVED_CONFIG', '')
        self.config = self.pmconfig.set_config_path([])
        self.pmconfig.read_cmd_line()
        self.datasets = self.opts.pmGetOptionArchives()
        if not self.datasets:
            raise pmapi.pmUsageErr("no dataset provided (-a/--archive)\n")

    def mapping(self):
        """ Helper which creates MMV instances, indoms, metrics
            and ties 'em all together via the MemoryMappedValues
            class.  Sampled by the treetop-client process for the
            servers results for training, predicting & explaining.
        """
        model_features, sample_valueset = [], []
        optim_features, local_features = [], []
        for instid in range(self._sample_valueset):
            instance = str(instid)
            sample_valueset.append(mmv.mmv_instance(instid, instance))
        for feature in range(self._max_features):
            instance = str(feature)
            model_features.append(mmv.mmv_instance(feature, instance))
            local_features.append(mmv.mmv_instance(feature, instance))
            optim_features.append(mmv.mmv_instance(feature, instance))
        indoms = [mmv.mmv_indom(serial = 1,
                    shorttext = "Model-based feature importance",
                    helptext = "Set of important features found by modeling"),
                  mmv.mmv_indom(serial = 2,
                    shorttext = "Sample-based feature importance",
                    helptext = "Set of important features found for a sample (local SHAP)"),
                  mmv.mmv_indom(serial = 3,
                    shorttext = "Perturbation-based optimisation feature importance",
                    helptext = "Set of important optimisation features found by min/max perturbation"),
                  mmv.mmv_indom(serial = 4,
                    shorttext = "Target metric recent sampled values"),
                 ]
        indoms[0].set_instances(model_features)
        indoms[1].set_instances(local_features)
        indoms[2].set_instances(optim_features)
        indoms[3].set_instances(sample_valueset)

        metrics = [mmv.mmv_metric(name = "target.metric",
                              item = 0,
                              typeof = MMV_TYPE_STRING,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              shorttext = "Prediction targets",
                              helptext = "Predicted and explained metrics"),
                   mmv.mmv_metric(name = "target.timestamp",
                              item = 1,
                              typeof = MMV_TYPE_DOUBLE,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,1,0,0,PM_TIME_SEC,0),
                              shorttext = "Timestamp in seconds for explanatory models",
                              helptext = "Timestamp providing the target time "
                                  "for inference and explanations."),
                   mmv.mmv_metric(name = "target.timestamp_s",
                              item = 2,
                              typeof = MMV_TYPE_STRING,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              shorttext = "Timestamp string for explanatory models",
                              helptext = "Timestamp string providing the target time "
                                  "for inference and explanations."),
                   mmv.mmv_metric(name = "target.valueset",
                              item = 3,
                              typeof = MMV_TYPE_FLOAT,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              indom = 4,
                              shorttext = "Target values preceding timestamp",
                              helptext = "Comma-separated values for target "
                                  "up to and including the current sample."),
                   mmv.mmv_metric(name = "processing.state",
                              item = 20,
                              typeof = MMV_TYPE_STRING,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              shorttext = "Current state of the server",
                              helptext = "Current server processing state - "
                                  "waiting, sampling, training, or explaining."),
                   mmv.mmv_metric(name = "sampling.count",
                              item = 30,
                              typeof = MMV_TYPE_U32,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,1,0,0,PM_COUNT_ONE),
                              shorttext = "Count of samples in the training set"),
                   mmv.mmv_metric(name = "sampling.interval",
                              item = 31,
                              typeof = MMV_TYPE_DOUBLE,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,1,0,0,PM_TIME_SEC,0),
                              shorttext = "Training data sampling interval"),
                   mmv.mmv_metric(name = "sampling.elapsed_time",
                              item = 32,
                              typeof = MMV_TYPE_DOUBLE,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,1,0,0,PM_TIME_SEC,0),
                              shorttext = "Time to fetch metric values",
                              helptext = "Sampling time during dataset preparation"),
                   mmv.mmv_metric(name = "training.count",
                              item = 60,
                              typeof = MMV_TYPE_U32,
                              semantics = MMV_SEM_COUNTER,
                              dimension = pmapi.pmUnits(0,0,1,0,0,PM_COUNT_ONE),
                              shorttext = "Count of training iterations completed"),
                   mmv.mmv_metric(name = "training.interval",
                              item = 61,
                              typeof = MMV_TYPE_DOUBLE,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,1,0,0,PM_TIME_SEC,0),
                              shorttext = "Training interval in seconds"),
                   mmv.mmv_metric(name = "training.window",
                              item = 61,
                              typeof = MMV_TYPE_DOUBLE,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,1,0,0,PM_TIME_SEC,0),
                              shorttext = "Training window size in seconds"),
                   mmv.mmv_metric(name = "training.boosted_rounds",
                              item = 63,
                              typeof = MMV_TYPE_U32,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,1,0,0,PM_COUNT_ONE),
                              shorttext = "Number of XGBoost iterations of boosting"),
                   mmv.mmv_metric(name = "training.elapsed_time",
                              item = 64,
                              typeof = MMV_TYPE_DOUBLE,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,1,0,0,PM_TIME_SEC,0),
                              shorttext = "Time to train models",
                              helptext = "Elapsed time during model training"),
                   mmv.mmv_metric(name = "features.anomalies",
                              item = 100,
                              typeof = MMV_TYPE_U32,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,1,0,0,PM_COUNT_ONE),
                              shorttext = "Count of total anomaly detection features"),
                   mmv.mmv_metric(name = "features.missing_values",
                              item = 101,
                              typeof = MMV_TYPE_U32,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,1,0,0,PM_COUNT_ONE),
                              shorttext = "Count of features with missing values"),
                   mmv.mmv_metric(name = "features.mutual_information",
                              item = 102,
                              typeof = MMV_TYPE_U32,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,1,0,0,PM_COUNT_ONE),
                              shorttext = "Count of features with sufficient mutual information"),
                   mmv.mmv_metric(name = "features.variance",
                              item = 103,
                              typeof = MMV_TYPE_U32,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,1,0,0,PM_COUNT_ONE),
                              shorttext = "Count of features with sufficient variance"),
                   mmv.mmv_metric(name = "features.total",
                              item = 104,
                              typeof = MMV_TYPE_U32,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,1,0,0,PM_COUNT_ONE),
                              shorttext = "Count of features in the training set"),
                   mmv.mmv_metric(name = "explaining.model.confidence",
                              item = 150,
                              typeof = MMV_TYPE_FLOAT,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,1,0,0,PM_TIME_SEC,0),
                              shorttext = "Model confidence from prediction accuracy"),
                   mmv.mmv_metric(name = "explaining.model.features",
                              item = 151,
                              typeof = MMV_TYPE_STRING,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              indom = 1,
                              shorttext = "Names of most important features",
                              helptext = "Most important features "
                                  "(metric instance name pairs, anomalies)"),
                   mmv.mmv_metric(name = "explaining.model.importance",
                              item = 152,
                              typeof = MMV_TYPE_FLOAT,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              indom = 1,
                              shorttext = "Most important feature rankings."),
                   mmv.mmv_metric(name = "explaining.model.importance_type",
                              item = 153,
                              typeof = MMV_TYPE_STRING,
                              semantics = MMV_SEM_DISCRETE,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              shorttext = "Feature importance type",
                              helptext = "XGBoost feature importance type "
                                  "(one of 'weight', 'gain' or 'cover')"),
                   mmv.mmv_metric(name = "explaining.model.mutual_information",
                              item = 154,
                              typeof = MMV_TYPE_FLOAT,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              indom = 1,
                              shorttext = "Mutual information for features",
                              helptext = "Mutual information between each "
                                  "important feature and target variables)"),
                   mmv.mmv_metric(name = "explaining.model.elapsed_time",
                              item = 155,
                              typeof = MMV_TYPE_DOUBLE,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,1,0,0,PM_TIME_SEC,0),
                              shorttext = "Time for model-based explanations"),
                   mmv.mmv_metric(name = "explaining.local.features",
                              item = 170,
                              typeof = MMV_TYPE_STRING,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              indom = 2,
                              shorttext = "Sample (local SHAP) important feature names"),
                   mmv.mmv_metric(name = "explaining.local.importance",
                              item = 171,
                              typeof = MMV_TYPE_FLOAT,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              indom = 2,
                              shorttext = "Local SHAP value, feature importance"),
                   mmv.mmv_metric(name = "explaining.local.mutual_information",
                              item = 172,
                              typeof = MMV_TYPE_FLOAT,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              indom = 2,
                              shorttext = "Mutual information for SHAP importance",
                              helptext = "Mutual information for SHAP-based feature"
                                " importance explanations (compared to target)."),
                   mmv.mmv_metric(name = "explaining.local.elapsed_time",
                              item = 173,
                              typeof = MMV_TYPE_DOUBLE,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,1,0,0,PM_TIME_SEC,0),
                              shorttext = "Time for sample-based (local SHAP) explanations"),
                   mmv.mmv_metric(name = "optimising.difference",
                              item = 190,
                              typeof = MMV_TYPE_FLOAT,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              indom = 3,
                              shorttext = "Perturbation optimisation value difference"),
                   mmv.mmv_metric(name = "optimising.min_max",
                              item = 191,
                              typeof = MMV_TYPE_STRING,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              indom = 3,
                              shorttext = "Perturbation optimisation method (min/max)"),
                   mmv.mmv_metric(name = "optimising.features",
                              item = 192,
                              typeof = MMV_TYPE_STRING,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              indom = 3,
                              shorttext = "Perturbation optimisation feature names"),
                   mmv.mmv_metric(name = "optimising.mutual_information",
                              item = 193,
                              typeof = MMV_TYPE_FLOAT,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              indom = 3,
                              shorttext = "Mutual information for optimisation features",
                              helptext = "Mutual information for optimisation feature"
                                " importance explanations (compared to target)."),
                   mmv.mmv_metric(name = "optimising.elapsed_time",
                              item = 194,
                              typeof = MMV_TYPE_DOUBLE,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,1,0,0,PM_TIME_SEC,0),
                              shorttext = "Time for permutation optimisation explanations"),
                  ]

        values = mmv.MemoryMappedValues("treetop.server",
                                        flags=MMV_FLAG_SENTINEL,
                                        cluster=41)
        values.add_indoms(indoms)
        values.add_metrics(metrics)
        values.start()
        if not values.started():
            self._exit_status = 2
            raise RuntimeError("Failed to start memory mapping for metrics")
        value = values.lookup_mapping("processing.state", None)
        values.set_string(value, "waiting");
        return values

    def settings(self):
        """ Prepare all default settings with input from client program """
        self._training_count += 1

        reset = 'partial'
        if self.sample_count() != self._sample_count:
            self._sample_count = self.sample_count()
            reset = 'full'
        if self.sample_interval() != self._sample_interval:
            self._sample_interval = self.sample_interval()
            reset = 'full'
        if self.training_interval() != self._training_interval:
            self._training_interval = self.training_interval()
        timewindow = self._sample_count * self._sample_interval
        if self.sample_time() <= 0.0 or self.timestamp is None:
            self.timestamp = min(self.start_time + timewindow, self.end_time)
            reset = 'full'
        #elif self.sample_time() != self.timestamp:
        #    self.timestamp = self.sample_time()
        #    print('RESET NE:', str(datetime.fromtimestamp(self.sample_time())), 'vs', str(datetime.fromtimestamp(self.timestamp)))
        #    reset = 'full'
        else:
            self.timestamp += self._sample_interval
            print('STEP FWD:', str(datetime.fromtimestamp(self.timestamp)))
        self.timestamp_s = str(datetime.fromtimestamp(self.timestamp))

        values = self.values
        value = values.lookup_mapping("target.metric", None)
        values.set_string(value, self.target())
        value = values.lookup_mapping("target.timestamp", None)
        values.set(value, self.timestamp)
        value = values.lookup_mapping("target.timestamp_s", None)
        values.set_string(value, self.timestamp_s)
        value = values.lookup_mapping("sampling.count", None)
        values.set(value, self._sample_count)
        value = values.lookup_mapping("sampling.interval", None)
        values.set(value, self._sample_interval)
        self._refresh = values.lookup_mapping("training.count", None)
        values.set(self._refresh, self._training_count)
        value = values.lookup_mapping("training.interval", None)
        values.set(value, self._training_interval)
        value = values.lookup_mapping("training.window", None)
        values.set(value, timewindow)

        print("Training interval:", self._training_interval)
        print("Sample interval:", self._sample_interval)
        print("Sample count:", self._sample_count)
        print("Timestamp:", self.timestamp_s, '-', self.timestamp)
        print("Target metric:", self.target())
        print("Filter metrics:", self.filter().split())
        print("Start time:", datetime.fromtimestamp(self.start_time), '-', self.start_time)
        print("End time:", datetime.fromtimestamp(self.end_time), '-', self.end_time)
        print("Total time: %.5f seconds" % (self.end_time - self.start_time))

        if self._training_count == 1: # first time
            reset = 'full'
        return reset

    def finish(self):
        self.values.stop()
        if self.values.started():
            self._exit_code = 3
        else:
            self._exit_code = 0
        return self._exit_code

    def client_connect(self):
        try:
            top = 'mmv.treetop.client.'
            # TODO: local context to private MMV PMDA?
            mmv = pmapi.fetchgroup(PM_CONTEXT_HOST, "local:")
            self.training_interval = mmv.extend_item(top + 'training.interval',
                                                     mtype = MMV_TYPE_FLOAT)
            self.sample_interval = mmv.extend_item(top + 'sampling.interval',
                                                   mtype = MMV_TYPE_FLOAT)
            self.sample_count = mmv.extend_item(top + 'sampling.count',
                                                mtype = MMV_TYPE_U32)
            self.sample_time = mmv.extend_item(top + 'timestamp',
                                             mtype = MMV_TYPE_FLOAT)
            self.target = mmv.extend_item(top + 'target',
                                          mtype = MMV_TYPE_STRING)
            self.filter = mmv.extend_item(top + 'filter',
                                          mtype = MMV_TYPE_STRING)
            self.client = mmv
        except pmapi.pmErr as error:
            sys.stderr.write('%s: %s\n' % (error.progname(), error.message()))
            sys.stderr.write('Failed "local:" connection setup\n')
            self.client = None
            return False
        return True

    def source_connect(self):
        try:
            log = self.datasets[0]
            ctx, src = pmapi.pmContext.set_connect_options(self.opts, log, None)
            self.pmfg = pmapi.fetchgroup(ctx, src)
            self.pmfg_ts = self.pmfg.extend_timestamp()
            self.context = self.pmfg.get_context()
            ctx = self.context
            if pmSetContextOptions(ctx.ctx, self.opts.mode, self.opts.delta):
                raise pmapi.pmUsageErr()
            self.end_time = float(self.opts.pmGetOptionFinish())
            self.start_time = float(self.opts.pmGetOptionStart())
            self.hostname = ctx.pmGetContextHostName()
            self.pmconfig.prepare_metrics(pmns=True)
            self.pmconfig.validate_metrics()
            self.pmconfig.finalize_options()
            self.source = src
        except pmapi.pmErr as error:
            sys.stderr.write('%s: %s\n' % (error.progname(), error.message()))
            sys.stderr.write('Failed source connection setup\n')
            self.source = None
            return False
        return True

    def connect(self):
        """ Connect both MMV and metric source contexts """
        if not self.client and not self.client_connect():
            return False # cannot proceed further, error reported
        if not self.source and not self.source_connect():
            return False # cannot proceed further, error reported
        return True

    def sleep(self):
        print('Sleeping at timestep', self.timestamp_s, '-', self.timestamp)
        value = self.values.lookup_mapping("processing.state", None)
        self.values.set_string(value, "waiting");
        time.sleep(self._training_interval)

    def elapsed(self, start_time, metric_name):
        elapsed_time = time.time() - start_time
        value = self.values.lookup_mapping(metric_name, None)
        self.values.set(value, elapsed_time)
        return elapsed_time

    def refresh(self):
        if not self.connect():
            sys.stderr.write('Cannot refresh\n')
            return
        print('Server refreshing')
        # refresh information from the client
        self.client.fetch()
        reset = self.settings()

        value = self.values.lookup_mapping("processing.state", None)
        self.values.set_string(value, "sampling");

        # ensure we have a pandas dataframe covering the time window
        timer = time.time()
        self.prepare_dataset(reset)
        timer = self.elapsed(timer, "sampling.elapsed_time")
        print('Dataset preparation complete in %.5fsec' % (timer))

        value = self.values.lookup_mapping("processing.state", None)
        self.values.set_string(value, "training");

        # use self.{target,filter} to start training at self.sample_time
        timer = time.time()
        ensemble, train_X, train_y, test_X, test_y = self.prepare_models()
        timer = self.elapsed(timer, "training.elapsed_time")
        print('Model training complete in %.5fsec' % (timer))

        value = self.values.lookup_mapping("processing.state", None)
        self.values.set_string(value, "explaining");

        # finally, generate explanations that client tools can display
        self.explain_models(ensemble, train_X, train_y, test_X, test_y)

    def prepare_dataset(self, reset='full'):
        # refresh from the metrics source to form a pandas dataframe

        #if reset == 'full' and self.df is not None:
        #    self.df = None
        self.df = None

        origin = pmapi.timespec(self.timestamp)
        delta = pmapi.timespec(self.sample_interval())
        count = self.sample_count()
        print('Origin:', origin)
        print('Delta:', delta)
        print('Count:', count)
        self.context.pmSetModeHighRes(PM_MODE_INTERP, origin, delta)

        while count > 0:
            count = count - 1
            refresh_metrics = self.pmconfig.fetch()
            if refresh_metrics < 0:
                break
            #print('Sampling at:', self.pmfg_ts())
            result = self.pmconfig.get_ranked_results(valid_only=True)
            self.append_sample(count, result)

        self.df = self.df.replace(b'', None)  # from .loc
        self.df = self.df.astype(self.dtypes, copy=False)
        self.df = self.df.reindex(columns=self.dtypes.keys())
        #print('Reindexed dataframe, shape:', self.df.shape)
        #print('Columns', self.df.columns[:10])
        #print('Dtypes', list(self.df.dtypes)[:10])
        #print('Values', self.df[:10])

    def lookup_dtype(self, desc):
        """ Map the appropriate pandas type for a metric descriptor """
        if desc.sem == PM_SEM_COUNTER: # reduce as rate converted
            return 'float32'
        # floating point to represent missing values for numerics
        if desc.type in [PM_TYPE_32, PM_TYPE_U32, PM_TYPE_FLOAT]:
            return 'float32'
        if desc.type in [PM_TYPE_64, PM_TYPE_U64, PM_TYPE_DOUBLE]:
            return 'float64'
        return None

    def update_dataset(self, index, names, dtypes, values):
        if self.df is None:
            self.df = pd.DataFrame(columns=names)
            self.dtypes = dtypes  # the 1st dictionary seen
        else:
            self.dtypes.update(dtypes)  # add to dictionary
        # insert this sample at the specified offset (index)
        self.df.loc[index, names] = values

    def append_sample(self, index, result):
        names, values, dtypes = ['timestamp'], {}, {}
        values['timestamp'] = self.pmfg_ts()
        dtypes['timestamp'] = 'datetime64[ns]'

        for i, metric in enumerate(self.metrics):
            if metric not in result:
                continue
            desc = self.pmconfig.descs[i]
            dtype = self.lookup_dtype(desc)
            if dtype is None:
                continue

            # Handle the simpler no-instance-domain case first
            if desc.indom == PM_INDOM_NULL:
                names.append(metric)  # column name
                dtypes[metric] = dtype
                values[metric] = result[metric][0][2]
                continue

            # Iterate values for each instance for this metric
            for instid, instname, value in result[metric]:
                # ensure we meet xgboost column name rules
                instname = re.sub(r'\[|\]|<', '', instname) 
                metricspec = metric + '(' + instname + ')'
                names.append(metricspec)  # column name
                dtypes[metricspec] = dtype
                values[metricspec] = value

        self.update_dataset(index, names, dtypes, values)

    def export_values(self, target, window):
        """ export dataset metrics, including the valueset metric """
        """ (used in the treetop recent-values lag graph display) """
        valueset = window[target].tolist()[-self._sample_valueset:]
        values = self.values
        count = 0
        while count < self._sample_valueset or count < len(valueset):
            inst = str(count)
            value = values.lookup_mapping("target.valueset", inst)
            if count < len(valueset):
                values.set(value, valueset[count])
            else:
                values.set(value, self.NaN)
            count = count + 1
        value = values.lookup_mapping("features.total", None)
        values.set(value, window.shape[1])
        value = values.lookup_mapping("features.missing_values", None)
        values.set(value, sum(len(window) - window.count()))

    def timestamp_features(self, timestamps):
        """ time features - into each row (sample) add time-based features """
        (sec_in_min, min_in_hour, hour_in_day, day_of_week) = ([], [], [], [])

        # timestamp is removed from training datasets and replaced with these
        # new representations adding meaning for cyclic analysis scenarios
        for timestamp in timestamps:
            sec_in_min.append(timestamp.second)
            min_in_hour.append(timestamp.minute)
            hour_in_day.append(timestamp.hour)
            day_of_week.append(timestamp.dayofweek)
        return pd.DataFrame(data={
            'timestamp-second-in-minute': sec_in_min,
            'timestamp-minute-in-hour': min_in_hour,
            'timestamp-hour-in-day': hour_in_day,
            'timestamp-day-of-week': day_of_week,
        })

    def top_anomaly_features(self, iso, y_pred_diffi, df, N):
        """ pick top-most anomalous features to add to the raw dataset """
        fit = df.to_numpy()[np.where(y_pred_diffi == 1)]
        diffi, ord_idx_diffi, exec_time_diffi = local_diffi_batch(iso, fit)
        #print('Average time Local-DIFFI:', round(np.mean(exec_time_diffi), 5))
        #print('Total time Local-DIFFI:', round(np.sum(exec_time_diffi), 5))

        # use DIFFI anomaly values to find the features contributing most
        rank_df = pd.DataFrame(diffi).sum().nlargest(N, keep='all')

        # dictionary of keys: anomalies-feature_name and values: array of DIFFI scores
        frame = {}
        for i in rank_df.index:   # column index (original features), from ranking
            key = 'anomalies-' + df.columns[i]
            value = [0] * df.shape[0]   # zero-filled array
            # fill in just the anomaly values now (replacing zeroes)
            for diffi_index, value_index in enumerate(np.where(y_pred_diffi == 1)[0]):
                value[value_index] = diffi[diffi_index][i]
            #print('Anomaly:', key, 'score:', value)
            frame[key] = value
        return pd.DataFrame(data=frame, dtype='float64')

    def anomaly_features(self, df):
        """ anomaly feature engineering - add up to a limit of new features """
        t0 = time.time()
        df0 = df.fillna(0)
        iso = IsolationForest(n_jobs=-1).fit(df0)
        y_pred_diffi = np.array(iso.decision_function(df0) < 0).astype('int')
        anomalies_df = self.top_anomaly_features(iso, y_pred_diffi, df0,
                                                 self._max_anomaly_features)
        t1 = time.time()
        print('Anomaly time:', t1 - t0)
        print('Anomaly features:', len(anomalies_df.columns))
        value = self.values.lookup_mapping("features.anomalies", None)
        self.values.set(value,  len(anomalies_df))
        return anomalies_df

    def reduce_with_variance(self, train_X):
        """ Automated dimensionality reduction using variance """
        t0 = time.time()
        try:
           cull = VarianceThreshold(threshold=self._variance_threshold)
           cull.fit(train_X)
        except ValueError:
            return train_X # no columns met criteria, training will struggle
        t1 = time.time()
        print('Variance time:', t1 - t0)
        keep = cull.get_feature_names_out()
        print('Keeping', len(keep), 'of', train_X.shape[1], 'columns with variance')
        value = self.values.lookup_mapping("features.variance", None)
        self.values.set(value, len(keep))
        return train_X[keep]

    def reduce_with_mutual_info(self, train_y, train_X):
        """ Automated dimensionality reduction using mutual information """
        clean_X = train_X.fillna(0)
        clean_y = train_y.values.flatten()

        # calculate all features mutual information with the target variable
        t0 = time.time()
        # TODO: scikit-learn 1.5+ adds optional n_jobs[=-1] parameter here:
        mi = mutual_info_regression(clean_X, clean_y, discrete_features=False)
        mi /= np.max(mi)  # normalise based on largest value observed
        t1 = time.time()
        print('MutualInformation time:', t1 - t0)

        results = {}
        for i, column in enumerate(clean_X.columns):
            results[column] = list([mi[i]])
        self.mutualinfo = pd.DataFrame(data=results, dtype='float64')

        cull = mi <= self._mutualinfo_threshold
        indices = np.where(cull)
        drop_columns = clean_X.columns[indices]

        keeping = clean_X.shape[1] - len(drop_columns)
        print('Keeping', keeping, 'columns with MutualInformation')
        if len(drop_columns) > self._max_mutualinfo_features:
            # still too many so reduce using argsort to prioritize -
            # caps the number of features after mutual information;
            # argsort arranges values from smallest to largest MI.
            upuntil = mi.shape[0] - self._max_mutualinfo_features
            indices = mi.argsort()[0:upuntil]
            print('Limit', upuntil, 'columns with MutualInformation')

        clean_X = clean_X.drop(clean_X.columns[indices], axis=1)
        train_X = train_X[clean_X.columns]  # undo NaN->0
        print('MutualInformation shape:', train_X.shape)

        value = self.values.lookup_mapping("features.mutual_information", None)
        self.values.set(value, train_X.shape[1])
        return train_X

    def prepare_split(self, target, notrain, splits=5, verbose=0):
        targets = [target]
        window = self.df
        if verbose:
            print('Dimensionality reduction for training dataset')
            print('Initial sample @', window.iloc[0]['timestamp'])
            print('Initial shape:', window.shape)
            print('Final sample @', window.iloc[-1]['timestamp'])
    
        # ensure target metrics (y) values are valid
        window = window.dropna(subset=targets, ignore_index=True)

        # export most recent target values for client to display
        self.export_values(target, window)

        # remove columns (performance metrics) requested by user
        columns = copy.deepcopy(notrain)
        if verbose: print('Dropping notrain', len(columns), 'columns:', columns)
        clean_X = window.drop(columns=columns)
    
        # automated feature engineering based on time
        times_X = self.timestamp_features(window['timestamp'])

        # extract sample (prediction) timestamp
        clean_y = clean_X.loc[:, targets]
        timestr = clean_X.iloc[-1]['timestamp']
        targets.append('timestamp')

        # remove the original timestamp feature
        clean_X = clean_X.drop(columns=targets)
    
        # automated anomaly-based feature engineering
        quirk_X = self.anomaly_features(clean_X)
        if verbose: print('quirk_X shape:', quirk_X.shape)
    
        # merge reduced set with new features
        clean_X = pd.merge(times_X, clean_X, left_index=True, right_index=True)
        clean_X = pd.merge(clean_X, quirk_X, left_index=True, right_index=True)

        # automated feature reduction based on variance
        clean_X = self.reduce_with_variance(clean_X)
    
        # automated feature reduction based on mutual information
        clean_X = self.reduce_with_mutual_info(clean_y, clean_X)
    
        # prepare for cross-validation over the training window
        final_X = clean_X
        finish = final_X.shape[0] - 1
        time_cv = TimeSeriesSplit(gap=0, max_train_size=finish,
                                  n_splits=splits, test_size=1)
    
        # Finally, perform the actual test/train splitting.
        # Next target metric value (y) forms the test set,
        # prediction of this drives explainable AI usage.
    
        test_X = final_X.iloc[[finish]]     # latest value
        train_X = final_X.iloc[0:finish, :]     # the rest
        test_y = clean_y.iloc[[finish]]
        train_y = clean_y.iloc[0:finish, :]
    
        if verbose > 1:
            print('Final training y shape:', train_y.shape, 'type:', type(train_y))
            print('Final training X shape:', train_X.shape, 'type:', type(train_X))
            print('Final test set y shape:', test_y.shape, 'type:', type(test_y))
            print('Final test set X shape:', test_X.shape, 'type:', type(test_X))
    
        return (train_X, train_y, test_X, test_y, time_cv, timestr)
 
    def prepare_models(self):
        # given a specific self.timestamp identifying the target record
        # train ensemble model and update metrics
        # - prepare IsolationForest anomaly detection model and use it.
        # - perform DIFFI calculations & append columns to the dataset.
        # - perform time-based feature engineering & append to dataset.
        # - perform feature reduction via variance, mutual information.
        # - prepare separate training, validation and testing datasets.
        # - train XGBoost regression model for prediction/explanations.
        print("Target metric:", self.target())
        filtered = self.filter().split(',')
        print("Filter metrics:", filtered)

        # TODO - drop time series CV - proving ineffective, costly
        (train_X, train_y, test_X, test_y, time_cv, timestr) = \
            self.prepare_split(self.target(), filtered, verbose=1)

        early = xgb.callback.EarlyStopping(10, metric_name='rmse', save_best=True)
        model = xgb.XGBRegressor(
            tree_method="hist",
            booster="gbtree",
            eta=0.075,
            max_depth=5,
            min_child_weight=1,
            subsample=1.0,
            colsample_bytree=1.0,
            callbacks=[early],
            n_jobs=-1,
            #seed=1,
        )
        model.fit(train_X, train_y, eval_set=[(test_X, test_y)], verbose=0)

        return model, train_X, train_y, test_X, test_y

    def optimise_update(self, count, df, column, i):
        feature = df.iloc[i]['feature']
        difference = df.iloc[i][column]
        if difference == 0:
            return False
        v = self.values
        inst = str(count)
        value = v.lookup_mapping("optimising.features", inst)
        v.set_string(value, metricspec(feature))
        value = v.lookup_mapping("optimising.difference", inst)
        v.set(value, difference)
        value = v.lookup_mapping("optimising.min_max", inst)
        v.set_string(value, column)
        value = v.lookup_mapping("optimising.mutual_information", inst)
        v.set(value, self.mutualinfo[feature].item())
        return True

    def optimise_nodata(self, count):
        v = self.values
        inst = str(count)
        value = v.lookup_mapping("optimising.features", inst)
        v.set_string(value, '')
        value = v.lookup_mapping("optimising.difference", inst)
        v.set(value, self.NaN)
        value = v.lookup_mapping("optimising.min_max", inst)
        v.set_string(value, '')
        value = v.lookup_mapping("optimising.mutual_information", inst)
        v.set(value, self.NaN)

    def optimise_export(self, max_inc, max_dec, min_inc, min_dec, test_X):
        quarter = int(self._max_features / 4)
        count = 0
        # increasing changes in max-based prediction
        for i in max_inc['maximum'].argsort()[::-1]:
            if not self.optimise_update(count, max_inc, 'maximum', i):
                break
            if count >= quarter: # up to 1st quarter: max-based, increasing
                break
            count += 1
        # decreasing changes in max-based prediction
        for i in max_dec['maximum'].argsort():
            if not self.optimise_update(count, max_dec, 'maximum', i):
                break
            if count >= quarter * 2: # up to half: max-based, decreasing
                break
            count += 1
        # increasing changes in min-based prediction
        for i in min_inc['minimum'].argsort()[::-1]:
            if not self.optimise_update(count, min_inc, 'minimum', i):
                break
            if count >= quarter * 3: # up to 3rd quarter: min-based, increasing
                break
            count += 1
        # decreasing changes in min-based prediction
        for i in min_dec['minimum'].argsort():
            if not self.optimise_update(count, min_dec, 'minimum', i):
                break
            if count >= self._max_features: # up to end: min-based, decreasing
                break
            count += 1
        # clear out any remaining instance slots
        while count < self._max_features:
            self.optimise_nodata(count)
            count += 1

    def optimise(self, model, train_X, train_y, test_X, test_y):
        print('Calculating optimisation importance')
        timer = time.time()

        count = test_X.shape[1]
        maxdf = test_X.loc[test_X.index.repeat(count)].reset_index(drop=True)
        mindf = maxdf.copy()  # min/max contents the same before perturbation

        # perturb one feature in each row
        maxima = train_X.max().to_frame().T
        minima = train_X.min().to_frame().T
        for i, column in enumerate(test_X):
            maxdf.at[i, column] = maxima[column][0]
            mindf.at[i, column] = minima[column][0]

        perturbed = pd.DataFrame({
            'feature': test_X.columns.values,
            'minimum': model.predict(maxdf),
            'maximum': model.predict(mindf),
        })
        predict = model.predict(test_X)[0]
        max_inc = perturbed[perturbed.maximum > predict]
        max_dec = perturbed[perturbed.maximum < predict]
        min_inc = perturbed[perturbed.minimum > predict]
        min_dec = perturbed[perturbed.minimum < predict]

        self.optimise_export(max_inc, max_dec, min_inc, min_dec, test_X)

        timer = self.elapsed(timer, "optimising.elapsed_time")
        print('Finished optimisation importance in %.5f seconds' % (timer))

    def confidence_level(self, model, target, test_X, test_y):
        """ Make prediction using latest values and compare to ground truth """
        goal = test_y.iloc[-1][target]
        pred = model.predict(test_X)[0]
        diff = abs(pred - goal)
        if diff == 0:
            ratio = 1  # avoid divide-by-zero, highly confident
        else:
            ratio = 1.0 - (diff / goal)
        print('Confidence: %.5f' % (ratio * 100.0))
        value = self.values.lookup_mapping("explaining.model.confidence", None)
        self.values.set(value, ratio * 100.0)

    def model_importance(self, model):
        """ Details: https://mljar.com/blog/feature-importance-xgboost/ """
        v = self.values
        value = v.lookup_mapping("explaining.model.importance_type", None)
        v.set_string(value, self._importance_type)

        booster = model.get_booster()   # non-scikit-learn XGBoost model
        value = v.lookup_mapping("explaining.model.boosted_rounds", None)
        v.set(value, booster.num_boosted_rounds())

        # Importance from XGBoost model measures
        # supported importances: gain, total_gain, weight, cover, total_cover
        feature_map = booster.get_score(importance_type=self._importance_type)
        top_features = sorted(feature_map.items(), key=lambda item: item[1])
        top_features = top_features[::-1][:self._max_features]
        print('Metrics', [item[0] for item in top_features])
        print('Importance', [item[1] for item in top_features])
        timer = time.time()
        for i, feature in enumerate(top_features):
            inst = str(i)
            topfn = feature[0]  # feature name
            topfv = feature[1]  # feature value
            topmi = self.mutualinfo[topfn].item()
            value = v.lookup_mapping("explaining.model.features", inst)
            v.set_string(value, metricspec(topfn))
            value = v.lookup_mapping("explaining.model.importance", inst)
            v.set(value, topfv)
            value = v.lookup_mapping("explaining.model.mutual_information", inst)
            v.set(value, topmi)
        timer = self.elapsed(timer, "explaining.model.elapsed_time")
        print('Finished model importance in %.5f seconds [%d]' % (timer, i+1))
        while i < self._max_features:  # clear any remaining instances
            inst = str(i + 1)
            value = v.lookup_mapping("explaining.model.features", inst)
            v.set_string(value, '')
            value = v.lookup_mapping("explaining.model.importance", inst)
            v.set(value, self.NaN)
            value = v.lookup_mapping("explaining.model.mutual_information", inst)
            v.set(value, self.NaN)
            i = i + 1

    def local_importance(self, model, train_X, test_X):
        if not shap_explanations:
            print('Skipping SHAP importance')
            return
        print('Calculating SHAP importance')
        v = self.values
        timer = time.time()
        explainer = shap.TreeExplainer(model)
        explanation = explainer.shap_values(test_X)
        count = 0
        for i in explanation.argsort()[0, ::-1]:
            inst = str(count)
            name = train_X.columns[i]  # feature name
            shapv = explanation[0, i]  # SHAP value
            if shapv == 0:
                iname = ''
                shapv = self.NaN
                mutual = self.NaN
            else:
                mutual = self.mutualinfo[name].item()
                iname = metricspec(name)
            value = v.lookup_mapping("explaining.local.features", inst)
            v.set_string(value, iname)
            value = v.lookup_mapping("explaining.local.importance", inst)
            v.set(value, shapv)
            value = v.lookup_mapping("explaining.local.mutual_information", inst)
            v.set(value, mutual)
            if count >= self._max_features:
                break
            count += 1
        timer = self.elapsed(timer, "explaining.local.elapsed_time")
        print('Finished SHAP importance in %.5f seconds [%d]' % (timer, count))
        while count < self._max_features:  # clear any remaining instances
            inst = str(count)
            value = v.lookup_mapping("explaining.local.features", inst)
            v.set_string(value, '')
            value = v.lookup_mapping("explaining.local.importance", inst)
            v.set(value, self.NaN)
            value = v.lookup_mapping("explaining.local.mutual_information", inst)
            v.set(value, self.NaN)
            count = count + 1

    def explain_models(self, model, train_X, train_y, test_X, test_y):
        """ Generate global and local feature importance measures,  """
        """ permutation optimisation assessment and update metrics. """

        # Firstly, calculate and export a confidence level for the model
        self.confidence_level(model, self.target(), test_X, test_y)

        # Model feature importance measures
        self.model_importance(model)

        # Importance from local SHAP values
        self.local_importance(model, train_X, test_X)

        # Importance from optimisation measures
        self.optimise(model, train_X, train_y, test_X, test_y)

if __name__ == '__main__':

    signal.signal(signal.SIGHUP, signal_refresh)
    signal.signal(signal.SIGINT, signal_finished)
    signal.signal(signal.SIGQUIT, signal_finished)
    signal.signal(signal.SIGTERM, signal_finished)

    server = TreetopServer()
    try:
        server.configure()
    except pmapi.pmErr as error:
        sys.stderr.write("%s: %s" % (error.progname(), error.message()))
        sys.stderr.write("\n")
        sys.exit(1)
    except pmapi.pmUsageErr as usage:
        usage.message()
        sys.exit(1)

    try:
        server.refresh()
        while not finished:
            try:
                server.sleep()
            except StopIteration as err:
                pass
            server.refresh()
    except (EOFError, KeyboardInterrupt) as err:
        pass

    sys.exit(server.finish())
