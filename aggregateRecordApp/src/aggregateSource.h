// Author: Bruno Martins

#ifndef AGGREGATE_SOURCE_H
#define AGGREGATE_SOURCE_H

#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <dbCommon.h>

#include <pvxs/source.h>
#include <pvxs/server.h>
#include <pvxs/nt.h>

#include "aggregateRecordHook.h"

namespace aggregate {

struct AggregateRecCtx;

/* Per-subscription state: just the pvxs monitor control and a back-pointer to
   the owning record context (so onClose can deregister). */
struct SubCtx {
    std::unique_ptr<pvxs::server::MonitorControlOp> ctrl;
    AggregateRecCtx *owner;

    SubCtx() : owner(nullptr) {}
};

/* Per-record context, owned by AggregateSource and pointed to by prec->rpvt.
   aggregateRecordPvt MUST be the first member so the record can recover the full
   context from prec->rpvt. The NTTable prototype is built once and cloned for
   every update and GET. */
struct AggregateRecCtx {
    aggregateRecordPvt                 hdr;    /* offset 0: notify() */
    dbCommon                           *prec;
    pvxs::Value                        proto;  /* built once, cloned per update */
    std::mutex                         mu;     /* guards subs */
    std::set<std::shared_ptr<SubCtx>>  subs;
    class AggregateSource              *src;

    AggregateRecCtx() : prec(nullptr), src(nullptr) { hdr.notify = nullptr; }
};

/**
 * Custom pvxs Source that publishes table records as NTTable PVs.
 *
 * Registered at priority -1, it intercepts channels for table records before
 * the default qsrvSingle source (priority 0).
 *
 * Updates are driven synchronously from aggregateRecord's process() via the RPVT
 * hook (see onProcess), so the per-column CHGD flags are read in the same locked
 * cycle that set them — no asynchronous dbEvent, no race.
 */
class AggregateSource final : public pvxs::server::Source {
public:
    AggregateSource();
    ~AggregateSource();

    void onSearch(Search& op) override;
    void onCreate(std::unique_ptr<pvxs::server::ChannelControl>&& op) override;
    List onList() override;

    /* Installed into each record's rpvt->notify; called from process() under
       the record lock. Recovers its AggregateRecCtx from prec->rpvt. */
    static void onProcess(struct aggregateRecord* prec);

private:
    std::vector<std::unique_ptr<AggregateRecCtx>>    ctxs_;
    std::map<std::string, AggregateRecCtx*>          records_;
    std::shared_ptr<const std::set<std::string>> names_;

    pvxs::Value makeProto() const;
};

} /* namespace table */

#endif /* AGGREGATE_SOURCE_H */
