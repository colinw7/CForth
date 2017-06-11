: ?STACK ." Stack> " PSTACK CR ;

: STRING
  CREATE DUP ,
  0 ,
  ALLOT
DOES>
  2 +
  DUP 1 - @
;

: INPUT$
  DROP 1-
  DUP 1- @
  CR ." ? " QUERY
  1 WORD
  @ < IF
    ." String too big"
    DROP QUIT
  THEN
  HERE DUP @ 1+
  ROT SWAP MOVE
;

: PUT$
  DROP 1-
  DUP 1- @
  36 WORD
  @ < IF
    ." String too big"
    DROP QUIT
  THEN
  HERE DUP @ 1+
  ROT SWAP MOVE
;

( input: str1 len str2 )
: -MATCH
  OVER OVER ( dup len and str1 )
  + SWAP
  DO
    DROP 1+ DUP 1- @ ( get char from str1)
    I @ - DUP IF
      DUP ABS / LEAVE
    THEN
  LOOP
  SWAP DROP
;

( input: str1 len1 str2 len2)
: $=
  ROT OVER = IF ( compare len1 and len2)
    SWAP -MATCH NOT
  ELSE
    DROP DROP DROP 0
  THEN
;

( ------------ )

20 STRING A$

A$ INPUT$

A$ TYPE CR

A$ PUT$ another test$

A$ TYPE CR

( ------------ )

10 STRING ANSWER$

3 STRING YES$ YES$ PUT$ yes$
2 STRING NO$  NO$  PUT$ no$

YES$ TYPE CR
NO$  TYPE CR

YES$ YES$ DROP -MATCH NOT . CR

YES$ YES$ $= . CR
YES$ NO$  $= . CR

: CONTINUE?
  BEGIN
    ." Do you want to continue (yes/no) "
    ANSWER$ INPUT$
    ANSWER$ NO$ $= IF QUIT THEN
    ANSWER$ YES$ $=
  UNTIL
;

CONTINUE?

." Continued" CR
