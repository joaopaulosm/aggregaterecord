# aggregateRecord

Proposal to implement an EPICS record type that would allow the IOC to serve the *aggregate* Normative Type, which is, according to specification:

```
NTAggregate :=

structure
    double     value                 // The center point of the observations,
                                     // nominally the mean.
    long       N                     // Number of observations
    double     dispersion      :opt  // Dispersion of observations;
                                      // nominally the Standard Deviation or RMS
    double     first           :opt  // Initial observation value
    time_t     firstTimeStamp  :opt  // Time of initial observation
    double     last            :opt  // Final observation value
    time_t     lastTimeStamp   :opt  // Time of final observation
    double     max             :opt  // Highest value in the N observations
    double     min             :opt  // Lowest value in the N observations
    string     descriptor      :opt
    alarm_t    alarm           :opt
    time_t     timeStamp       :opt
```


