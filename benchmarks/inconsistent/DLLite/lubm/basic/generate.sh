 # $1: probability of a fact



tas=( 113 93 130 59 17 67 34 114 27 94 43 52 142 46 110 7 96 15 2 1 144 5 26 109 76 143 14 58 90)
courses=( 32 59 34 40 21 8 27 47 20 37 14 4 55 53 0 18 9 12 35 25 36 42 29 28 17 2 30 41 11 )





prob=$((32768 * $1 / 100)) 



	for (( i=0; i<=28; i++ ))
	do

		if [[ $RANDOM -le $prob ]]; then
			
				echo "takesExam(\"http://www.Department0.University0.edu/GraduateStudent${tas[$i]}\",\"http://www.Department0.University0.edu/Course${courses[$i]}\")."
		fi
	done




