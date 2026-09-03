#ifndef NT_AGGREGATE_H
#define NT_AGGREGATE_H

#include <pvxs/nt.h>
#include "aggregateRecordAPI.h"

struct NTAggregate {
    AGGREGATERECORD_API pvxs::TypeDef build() const;
    inline pvxs::Value create() const {
        return build().create();
    }
};

#endif // NT_AGGREGATE_H
