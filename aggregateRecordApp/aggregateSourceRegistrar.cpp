/* SPDX-License-Identifier: BSD-3-Clause
 * See file: COPYRIGHT
 * Author: Bruno Martins
 */
#include <memory>
#include <stdexcept>

#include <initHooks.h>
#include <epicsExport.h>

#include <pvxs/iochooks.h>

#include "aggregateSource.h"

static void addAggregateSource(void)
{
    try {
        /* Use priority -1 so aggregateSrc is checked first in the onCreate
           dispatch loop (pvxs iterates sources from lowest to highest
           priority and stops at the first accepting source). */
        pvxs::ioc::server().addSource(
            "aggregateSrc",
            std::make_shared<aggregate::AggregateSource>(),
            -1);
    } catch (std::exception& e) {
        fprintf(stderr, "addAggregateSource: %s\n", e.what());
    }
}

/* Register the aggregate NTTable source automatically during iocInit. By
   initHookAfterIocBuilt the database is fully initialized (record and device
   support init done) and the QSRV server exists but is not yet serving, which
   is the pvxs-documented point to add a custom source (see pvxs/iochooks.h). */
static void aggregateSourceInitHook(initHookState state)
{
    if (state == initHookAfterIocBuilt)
        addAggregateSource();
}

static void registerAggregateSourceCmds(void)
{
    initHookRegister(&aggregateSourceInitHook);
}

extern "C" {
    epicsExportRegistrar(registerAggregateSourceCmds);
}
