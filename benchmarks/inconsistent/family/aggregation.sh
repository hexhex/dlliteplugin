#!/bin/bash

# Call the default aggregation script as follows:
#    aggregate 300 0 0 "3,6,9" "2,4,5,7,8,9" "" ""
# Columns 3,6,9 will be averaged (contain the runtime for the 3 configurations)
# Columns 2,4,5,7,8,10,11 are added (2: number of instances, 4,7,10: timeouts for the 3 configurations, 5,8,11: number of answersets for the 3 configurations)
# (Last two parameters say that we don't compute minimum or maximum of any columns)
aggregateresults.sh 300 0 0 "3,6,9" "2,4,5,7,8,10,11" "" ""
