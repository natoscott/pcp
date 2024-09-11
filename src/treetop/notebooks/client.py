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

import sys
from datetime import datetime
from pcp import mmv, pmapi
from cpmapi import PM_COUNT_ONE, PM_TIME_SEC, PM_INDOM_NULL
from cmmv import (
        MMV_SEM_INSTANT, MMV_SEM_DISCRETE, MMV_INDOM_NULL,
        MMV_TYPE_U32, MMV_TYPE_FLOAT, MMV_TYPE_STRING
    )

class TreetopClient():
    """ MMV metrics for treetop client (user interface) utility """

    _exit_code = 1  # failure is the default
    _max_targets = 5  # number of prediction target variables
    _sample_count = 720  # number of samples in training set
    _sample_interval = 10  # metrics sampling interval (seconds)
    _training_interval = 10  # server training interval (seconds)

    def __init__(self):
        """ Worker which creates MMV instances, indoms, metrics
            and ties 'em all together via the MemoryMappedValues
            class.  Sampled by the treetop-server process for its
            dynamic configuration information.
        """

        metrics = [mmv.mmv_metric(name = "target",
                              item = 1,
                              typeof = MMV_TYPE_STRING,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              shorttext = "Prediction target metric",
                              helptext = "Predicted and explained metric and "
                                         "optional [instance] specifier."),
                   mmv.mmv_metric(name = "filter",
                              item = 2,
                              typeof = MMV_TYPE_STRING,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              shorttext = "Manual feature reduction metrics",
                              helptext = "Metrics removed from training set"
                                         " as a comma-separated list."),
                   mmv.mmv_metric(name = "sampling.count",
                              item = 3,
                              typeof = MMV_TYPE_U32,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              shorttext = "Number of training set samples",
                              helptext = "Historical training data samples"),
                   mmv.mmv_metric(name = "sampling.interval",
                              item = 4,
                              typeof = MMV_TYPE_FLOAT,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,1,0,0,PM_TIME_SEC,0),
                              shorttext = "Training set sampling interval",
                              helptext = "Historical training set interval"),
                   mmv.mmv_metric(name = "training.interval",
                              item = 5,
                              typeof = MMV_TYPE_FLOAT,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,1,0,0,PM_TIME_SEC,0),
                              shorttext = "Training frequency",
                              helptext = "Frequency at which training occurs"),
                   mmv.mmv_metric(name = "timestamp",
                              item = 6,
                              typeof = MMV_TYPE_STRING,
                              semantics = MMV_SEM_INSTANT,
                              dimension = pmapi.pmUnits(0,0,0,0,0,0),
                              shorttext = "Prediction timestamp",
                              helptext = "Predict time, training ends prior"),
                  ]

        values = mmv.MemoryMappedValues("treetop.client", cluster=40)
        values.add_metrics(metrics)
        values.start()
        if not values.started():
            self._exit_status = 2

        value = values.lookup_mapping("sampling.count", None)
        values.set(value, self._sample_count)
        value = values.lookup_mapping("sampling.interval", None)
        values.set(value, self._sample_interval)
        value = values.lookup_mapping("training.interval", None)
        values.set(value, self._training_interval)

        # example data
        target = 'disk.all.avactive'
        notrain = 'disk.all.aveq,disk.all.read,disk.all.blkread,disk.all.read_bytes,disk.all.total,disk.all.blktotal,disk.all.total_bytes,disk.all.write,disk.all.blkwrite,disk.all.write_bytes'
        timestamp = "2012-05-10 08:47:47.462172"

        value = values.lookup_mapping("target", None)
        values.set_string(value, target)
        value = values.lookup_mapping("filter", None)
        values.set_string(value, notrain)
        value = values.lookup_mapping("timestamp", None)
        values.set_string(value, timestamp)

        values.stop()
        if values.started():
            self._exit_code = 3
        else:
            self._exit_code = 0

    def status(self):
        return self._exit_code

if __name__ == '__main__':

    client = TreetopClient()
    sys.exit(client.status())

