#!/bin/bash

# Call the default aggregation script as follows:
#    aggregate 300 0 0 "3,5,6,8,9,11" "2,4,7,10" "" ""
# Columns 3,5,6,8,9,11 will be averaged (3,6,9: runtime for the 3 configurations, 5,8,11: number of answersets for the 3 configurations)
# Columns 2,4,7,10 are added (2: number of instances, 4,7,10: timeouts for the 3 configurations)
# (Last two parameters say that we don't compute minimum or maximum of any columns)
aggregateresults.sh 300 0 0 "3,5,6,8,9,11" "2,4,7,10" "" ""
