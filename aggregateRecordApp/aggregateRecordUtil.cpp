#include "aggregateRecordUtil.h"

#include <string.h>
#include <errlog.h>

AggregateRecordWrapper::AggregateRecordWrapper(aggregateRecord & rec)
: rec(rec)
{}

AggregateRecordWrapper::AggregateRecordWrapper(struct dbCommon *prec)
: AggregateRecordWrapper::AggregateRecordWrapper(*(aggregateRecord*)prec)
{}

AggregateRecordWrapper::~AggregateRecordWrapper()
{}

