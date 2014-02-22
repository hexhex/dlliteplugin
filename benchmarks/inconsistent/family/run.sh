#!/bin/bash

runheader=$(which dlvhex_run_header.sh)
if [[ $runheader == "" ]] || [ $(cat $runheader | grep "dlvhex_run_header.sh Version 1." | wc -l) == 0 ]; then
        echo "Could not find dlvhex_run_header.sh (version 1.x); make sure that the benchmarks/script directory is in your PATH"
        exit 1
fi
source $runheader

# run instances
if [[ $all -eq 1 ]]; then
	# run all instances using the benchmark script run insts
	$bmscripts/runinsts.sh "instances/*.hex" "$mydir/run.sh" "$mydir/instances" "$to" "$mydir/aggregation.sh" "" "$req"
else
	# run single instance
	owlfile="${instance%%.hex}.owl"
	confstr="--supportsets;--supportsets --repair=$owlfile -n=1;--supportsets --repair=$owlfile --silent"

	$bmscripts/runconfigs.sh "dlvhex2 --heuristics=monolithic --liberalsafety --plugindir=../../../src/ --flpcheck=none --silent CONF INST" "$confstr" "$instance" "$to" "$bmscripts/ansctimeoutputbuilder.sh"
fi

