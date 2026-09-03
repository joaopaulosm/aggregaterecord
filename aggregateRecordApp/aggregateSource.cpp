/* SPDX-License-Identifier: BSD-3-Clause
 * See file: COPYRIGHT
 * Author: Bruno Martins
 */
#include <cstring>
#include <stdexcept>
#include <memory>
#include <string>
#include <vector>
#include <map>

#include <dbAccess.h>
#include <dbChannel.h>
#include <dbEvent.h>
#include <dbFldTypes.h>
#include <dbLock.h>
#include <dbStaticLib.h>
#include <epicsString.h>
#include <epicsTime.h>

#include <pvxs/log.h>
#include <pvxs/nt.h>
#include <aggregate/ntaggregate.h>

#include "aggregateSource.h"
#include "aggregateRecord.h"


DEFINE_LOGGER(aglog, "pvxs.aggregate.source");

namespace aggregate {

/* Simple RAII record lock using the EPICS dbScanLock/dbScanUnlock API */
struct RecLock {
    dbCommon *prec_;
    explicit RecLock(dbCommon *p) : prec_(p) { dbScanLock(p); }
    ~RecLock() { dbScanUnlock(prec_); }
};

/* Map EPICS menuFtype → pvxs scalar TypeCode */
static pvxs::TypeCode ftype_to_typecode(epicsEnum16 type)
{
    switch (type) {
    case DBF_STRING: return pvxs::TypeCode::String;
    case DBF_CHAR:   return pvxs::TypeCode::Int8;
    case DBF_UCHAR:  return pvxs::TypeCode::UInt8;
    case DBF_SHORT:  return pvxs::TypeCode::Int16;
    case DBF_USHORT: return pvxs::TypeCode::UInt16;
    case DBF_LONG:   return pvxs::TypeCode::Int32;
    case DBF_ULONG:  return pvxs::TypeCode::UInt32;
    case DBF_INT64:  return pvxs::TypeCode::Int64;
    case DBF_UINT64: return pvxs::TypeCode::UInt64;
    case DBF_FLOAT:  return pvxs::TypeCode::Float32;
    case DBF_DOUBLE: return pvxs::TypeCode::Float64;
    default:         return pvxs::TypeCode::UInt8;
    }
}

// pvxs::Value AggregateSource::makeProto() const
// {
//     return NTAggregate().build().create();
// }

/* Fill a time_t sub-structure from an EPICS timestamp. */
static void fillTime(pvxs::Value v, const epicsTimeStamp &ts, epicsUInt64 utag = 0)
{
    v["secondsPastEpoch"] = int64_t(ts.secPastEpoch) + POSIX_TIME_AT_EPICS_EPOCH;
    v["nanoseconds"]      = int32_t(ts.nsec);
    v["userTag"]          = int32_t(utag);
}

/* Rebuild ctx.current from the record's current field values -- record fields
   only, never dpvt, since this also runs at construction before any process().
   Assigning a field also marks it, so the posted delta carries exactly what we
   set here.  Caller must hold the record lock. */
static void updateSnapshot(AggregateRecCtx &ctx, const aggregateRecord *prec)
{
    pvxs::Value v = ctx.proto.cloneEmpty();

    v["value"]      = prec->val;
    v["N"]          = int64_t(prec->idxn);
    v["dispersion"] = prec->dpsr;
    v["first"]      = prec->fval;
    v["last"]       = prec->lval;
    v["max"]        = prec->maxv;
    v["min"]        = prec->minv;
    v["descriptor"] = prec->desc;

    fillTime(v["firstTimeStamp"], prec->ftime);
    fillTime(v["lastTimeStamp"],  prec->ltime);
    fillTime(v["timeStamp"],      prec->time, prec->utag);

    v["alarm.severity"] = int32_t(prec->sevr);
    v["alarm.status"]   = int32_t(prec->stat);
    v["alarm.message"]  = prec->amsg;

    ctx.current = std::move(v);
}

/* ------------------------------------------------------------------ */

AggregateSource::AggregateSource()
{
    auto names = std::make_shared<std::set<std::string>>();
    DBENTRY dbe;
    dbInitEntry(pdbbase, &dbe);

    if (dbFindRecordType(&dbe, "aggregate") == 0) {
        for (long s = dbFirstRecord(&dbe); !s; s = dbNextRecord(&dbe)) {
            const char *rname = dbGetRecordName(&dbe);
            dbCommon *prec = (dbCommon *)dbe.precnode->precord;

            std::unique_ptr<AggregateRecCtx> ctx(new AggregateRecCtx());
            ctx->prec       = prec;
            ctx->src        = this;
            ctx->hdr.notify = &AggregateSource::onProcess;
            ctx->hdr.self   = ctx.get();

            try {
                RecLock lk(prec);
                ctx->proto = NTAggregate().build().create();
                /* Seed current from the record's initial field values, so a GET
                   on a record that has not processed yet still returns a marked
                   (non-empty) structure. */
                updateSnapshot(*ctx, (aggregateRecord *)prec);
                ((aggregateRecord *)prec)->rpvt = &ctx->hdr;
            } catch (std::exception &e) {
                log_err_printf(aglog, "NTAggregate().build().create() failed for '%s': %s\n",
                               rname, e.what());
                continue;
            }

            /* Claim both "REC" and "REC.VAL": over PVA the record *is* the
               NTAggregate, so its value field must not fall through to
               qsrvSingle as an NTScalar.  Other fields (REC.MINV, ...) do fall
               through and are served as ordinary NTScalars.  Only the bare
               name is advertised by onList(). */
            records_[rname] = ctx.get();
            records_[std::string(rname) + ".VAL"] = ctx.get();
            names->insert(rname);
            ctxs_.push_back(std::move(ctx));
        }
    }
    dbFinishEntry(&dbe);
    names_ = std::move(names);
}

AggregateSource::~AggregateSource()
{
    /* Stop process() from calling into us before our state is destroyed. */
    for (auto &ctx : ctxs_) {
        RecLock lk(ctx->prec);
        ((aggregateRecord *)ctx->prec)->rpvt = nullptr;
    }
}

void AggregateSource::onSearch(Search &op)
{
    for (auto &pv : op) {
        if (records_.count(pv.name()))
            pv.claim();
    }
}

void AggregateSource::onCreate(std::unique_ptr<pvxs::server::ChannelControl> &&chan)
{
    auto it = records_.find(chan->name());
    if (it == records_.end())
        return;

    AggregateRecCtx *ctx = it->second;

    /* GET / PUT */
    chan->onOp([ctx](std::unique_ptr<pvxs::server::ConnectOp> &&op) {
        op->connect(ctx->proto);

        op->onGet([ctx](std::unique_ptr<pvxs::server::ExecOp> &&get) {
            try {
                pvxs::Value v;
                {
                    RecLock lk(ctx->prec);
                    v = ctx->current.clone();
                }
                get->reply(v);
            } catch (std::exception &e) {
                get->error(e.what());
            }
        });

        op->onPut([ctx](std::unique_ptr<pvxs::server::ExecOp> &&put,
                       pvxs::Value &&val) {
            try {
                // {
                //     RecLock lk(ctx->prec);
                //     putValueTable(*ctx, val);
                //     dbProcess(ctx->prec);   /* process() drives the update via rpvt */
                // }
                put->reply();
            } catch (std::exception &e) {
                put->error(e.what());
            }
        });
    });

    /* MONITOR — register the subscription in the record context; updates are
       posted from process() via onProcess(). No dbEvent involved. */
    chan->onSubscribe([ctx](
                          std::unique_ptr<pvxs::server::MonitorSetupOp> &&sub) {
        try {
            auto sc   = std::make_shared<SubCtx>();
            sc->owner = ctx;
            sc->ctrl  = sub->connect(ctx->proto);

            sc->ctrl->onStart([ctx, sc](bool start) {
                if (start) {
                    /* Register and send the initial full snapshot while holding
                       both locks: the record lock keeps a concurrent process()
                       notify from interleaving, and ctx->mu makes the sub
                       visible to later notifies only once it has its baseline.
                       Lock order is record-lock -> ctx->mu, matching onProcess. */
                    try {
                        RecLock lk(ctx->prec);
                        std::lock_guard<std::mutex> g(ctx->mu);
                        ctx->subs.insert(sc);
                        sc->ctrl->post(ctx->current.clone());
                    } catch (std::exception &e) {
                        log_exc_printf(aglog, "initial snapshot: %s\n", e.what());
                    }
                } else {
                    std::lock_guard<std::mutex> g(ctx->mu);
                    ctx->subs.erase(sc);
                }
            });

            sub->onClose([ctx, sc](const std::string &) {
                std::lock_guard<std::mutex> g(ctx->mu);
                ctx->subs.erase(sc);
                sc->ctrl.reset();
            });
        } catch (std::exception &e) {
            sub->error(e.what());
        }
    });
}

/* Synchronous update hook -- installed in each record's rpvt->notify and called
   from aggregateRecord's process() with the record lock held, so the fields it
   reads are exactly those the cycle just produced.  Rebuilds the snapshot and
   posts it to every subscriber. */
void AggregateSource::onProcess(struct aggregateRecord *prect)
{
    if (!prect->rpvt)
        return;
    aggregateRecordPvt *hdr = static_cast<aggregateRecordPvt *>(prect->rpvt);
    AggregateRecCtx    *ctx = static_cast<AggregateRecCtx *>(hdr->self);
    if (!ctx)
        return;

    log_debug_printf(aglog, "onProcess: %s\n", prect->name);

    try {
        /* The record lock is held by process(), so ctx->current is ours to
           rewrite; ctx->mu only guards the subscriber set. */
        updateSnapshot(*ctx, prect);

        std::lock_guard<std::mutex> g(ctx->mu);
        for (auto &sub : ctx->subs) {
            if (sub->ctrl)
                sub->ctrl->post(ctx->current.clone());
        }
    } catch (std::exception &e) {
        log_exc_printf(aglog, "onProcess '%s': %s\n", prect->name, e.what());
    }
}

AggregateSource::List AggregateSource::onList()
{
    return List(names_);
}


} //namespace aggregate