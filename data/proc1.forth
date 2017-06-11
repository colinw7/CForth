: % * 100 / ;
150 12 % . CR

VARIABLE account

: invest account @ 12 % account +! ;

200 account !

invest invest invest

account @ . CR

: Squared ." = " DUP * . ;

4 Squared CR
