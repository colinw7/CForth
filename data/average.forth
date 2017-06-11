: STACK? ." Stack> " PSTACK CR ;

: *ARRAY
  CREATE
    DUP ,
    0 ,
    0 DO
      0 ,
    LOOP
  DOES>
    DUP DUP @
    SWAP 2+
    OVER 0 SWAP
    0 DO
      OVER @ +
      SWAP 1+ SWAP
    LOOP
    SWAP DROP SWAP /
    OVER 1+ !
    1+ SWAP +
;

10 *ARRAY readings

10 1 readings !
20 2 readings !
1000 10 readings !

1  readings ? CR
2  readings ? CR
10 readings ? CR

0 readings ? CR

870 10 readings !
50 6 readings !

0 readings ? CR
