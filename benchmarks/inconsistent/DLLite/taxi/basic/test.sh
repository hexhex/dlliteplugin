if [[ $2 -eq 1 ]]; then 
	dlvhex2 --plugindir=../../../../../src --supportsets --liberalsafety --heuristics=monolithic --repair=$1.owl $1.hex --verbose=1 2>out
 
else 
	dlvhex2 --plugindir=../../../../../src --supportsets --liberalsafety --heuristics=monolithic $1.hex --verbose=1 2>out
 
fi
