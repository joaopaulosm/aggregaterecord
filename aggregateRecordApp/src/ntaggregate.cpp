#include <aggregate/ntaggregate.h>

namespace pvxs {
namespace nt {

TypeDef TimeStamp::build()
{
    using namespace pvxs::members;

    TypeDef def(TypeCode::Struct, "time_t", {
                    Int64("secondsPastEpoch"),
                    Int32("nanoseconds"),
                    Int32("userTag"),
                });
    return def;
}

TypeDef Alarm::build()
{
    using namespace pvxs::members;

    TypeDef def(TypeCode::Struct, "alarm_t", {
                    Int32("severity"),
                    Int32("status"),
                    String("message"),
                });
    return def;
}


TypeDef NTAggregate::build() const
{
    using namespace pvxs::members;

    auto time_t(TimeStamp{}.build());
    auto alarm_t = {
        Int32("severity"),
        Int32("status"),
        String("message"),
    };

    TypeDef def(TypeCode::Struct, "epics:nt/NTAggregate:1.0", {
        Float64("value"),
        Int64("N"),
        Float64("dispersion"),
        Float64("first"),
        time_t.as("firstTimeStamp"),
        Float64("last"),
        time_t.as("lastTimeStamp"),
        Float64("max"),
        Float64("min"),
        String("descriptor"),
        Struct("alarm", "alarm_t", alarm_t),
        time_t.as("timeStamp"),
    });
    return def;
}

}} // namespace pvxs::nt
