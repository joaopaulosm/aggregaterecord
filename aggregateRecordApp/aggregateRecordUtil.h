#ifndef AGGREGATERECORDUTIL_H
#define AGGREGATERECORDUTIL_H

#ifdef __cplusplus

#include <string>
#include <vector>

#include "aggregateRecordAPI.h"
#include "aggregateRecord.h"

struct AGGREGATERECORD_API AggregateRecordWrapper {

    // struct DataColumn {
    //     struct DataColumnConfig config;
    //     DBLINK *inp;
    //     void **val;
    //     epicsUInt32 *numrows;
    //     epicsUInt8  *chgd;
    //     char        *label;   /* live pointer to the CxxLABEL field (40 bytes) */

    //     DataColumn(const std::string & name, const std::string & label,
    //         epicsEnum16 type, DBLINK *inp, void **val, epicsUInt32 *numrows,
    //         epicsUInt8 *chgd, char *labelfld);
    // };


    aggregateRecord & rec;

    AggregateRecordWrapper(aggregateRecord & rec);
    AggregateRecordWrapper(struct dbCommon *prec);
    virtual ~AggregateRecordWrapper();

    template<typename T>
    void set_private(T* pvt) {
        rec.dpvt = (void*)pvt;
    }

    template<typename T>
    T* get_private() {
        return (T*)rec.dpvt;
    }

};

#endif // __cplusplus
#endif // AGGREGATERECORDUTIL_H
