: ARRAY
  CREATE DUP ,
  ALLOT 
  DOES>
    SWAP 1 - SWAP
    OVER OVER
    @ U< NOT IF
      ." Array range error" CR
      QUIT
    THEN
    1 +
    SWAP +
;

20 ARRAY table

0 table

21 table

-1 5 table !

5 table ?
