: SingleLoop 11 1 DO ." I" I . CR LOOP ;
: DoubleLoop 11 1 DO ." I" I . CR 11 1 DO 2 SPACES ." I" I . ." J" J . CR LOOP LOOP ;

: Timestable CR 11 1 DO 11 1 DO J I * . LOOP CR LOOP ;

( SingleLoop )
( DoubleLoop )

Timestable
