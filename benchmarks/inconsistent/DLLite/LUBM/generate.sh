# $1: probability of a person taking exam in the course

prop=$((32768 * $1 / 100))

ta=( 94 7 5 46 15 109 90 142 58 93 67 52 2 27 14 113 144 110 34 96 59 76 26 17 143 43 1 114 130 )
c[94]=37
c[7]=18
c[5]=42
c[46]=53
c[15]=12
c[109]=28
c[90]=11
c[142]=55
c[58]=41
c[93]=59
c[67]=8
c[52]=4
c[2]=35
c[27]=20
c[14]=30
c[113]=32
c[144]=36
c[110]=0
c[34]=27
c[96]=9
c[59]=40
c[76]=17
c[26]=29
c[17]=21
c[143]=2
c[43]=14
c[1]=25
c[114]=47
c[130]=34

 
#18 42 53 12 28 11 55 41 59 8 4 35 20 30 32 36 0 27 9 40 17 29 21 2 14 25 47 34 )

for element in ${ta[@]}
do
	if [[ $RANDOM -le $prop ]]; then echo  "takesExam(\"http://www.Department0.University0.edu/GraduateStudent$element\",\"http://www.Department0.University0.edu/Course${c[$element]}\")."
	fi
done

