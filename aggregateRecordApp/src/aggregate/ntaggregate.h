#ifndef NT_AGGREGATE_H
#define NT_AGGREGATE_H

#include <pvxs/nt.h>

struct NTAggregate {
    //! A TypeDef which can be appended
    PVXS_API
    pvxs::TypeDef build() const;
    //! Instantiate
    inline pvxs::Value create() const {
        return build().create();
    }
};

#endif // NT_AGGREGATE_H