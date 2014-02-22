#!/bin/bash

source dlvhex_run_header.sh

# run instances
if [[ $all -eq 1 ]]; then
	# run all instances using the benchmark script run insts
	$bmscripts/runinsts.sh "instances/*.hex" "$mydir/run.sh" "$mydir" "$to" "$mydir/aggregation.sh" "" "$req"
else
	# run single instance
	owlfile="${instance%%.hex}.owl"
	confstr="--supportsets;--supportsets --repair=$owlfile -n=1;--supportsets --repair=$owlfile --silent"

	$bmscripts/runconfigs.sh "dlvhex2 --heuristics=monolithic --liberalsafety --plugindir=../../../src/ --flpcheck=none --silent CONF INST" "$confstr" "$instance" "$to" "$bmscripts/ansctimeoutputbuilder.sh"
fi

