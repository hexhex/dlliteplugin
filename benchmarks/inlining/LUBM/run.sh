#!/bin/bash

runheader=$(which run_header.sh)
if [[ $runheader == "" ]] || [ $(cat $runheader | grep "run_header.sh Version 1." | wc -l) == 0 ]; then
        echo "Could not find run_header.sh (version 1.x); make sure that the benchmark scripts directory is in your PATH"
        exit 1
fi
source $runheader

# HERE THE BENCHMARK-SPECIFIC PART STARTS
if [[ $all -eq 1 ]]; then
	# ============================================================
	# Replace "instances/*.hex" in (1) by the loop condition
	# to be used for iterating over the instances
	# ============================================================

	# run all instances using the benchmark script run insts
	$bmscripts/runinsts.sh "instances/*.hex" "$mydir/run.sh" "$mydir" "$to" ""
	
	#$bmscripts/runinsts.sh "instances/*.hex" "$mydir/run.sh" "$mydir" "$to"	# (1)
else
	# ============================================================
	# Define the variable "confstr" in (2) as a semicolon-
	# separated list of configurations to compare.
	# In (3) replace "dlvhex2 --plugindir=../../src INST CONF"
	# by an appropriate call of dlvhex, where INST will be
	# substituted by the instance file and CONF by the current
	# configuration from variable "confstr".
	# ============================================================

	confstr="--extlearn=iobehavior,neg;--supportsets;--extinlining"

	$bmscripts/runconfigs.sh "dlvhex2 --plugindir=../../../src --heuristics=monolithic --silent INST CONF" "$confstr" "$instance" "$to" "ansctimeoutputbuilder.sh"
fi
