CREATE 1reply 10 ALLOT
CREATE 2reply 10 ALLOT
CREATE 3reply 10 ALLOT

: GETREPLIES
  1reply 10 32 FILL
  2reply 10 32 FILL
  3reply 10 32 FILL
  CR ." ?"
  QUERY
  44 WORD
  COUNT 1reply SWAP MOVE
  44 WORD
  COUNT 2reply SWAP MOVE
  1 WORD
  COUNT 3reply SWAP MOVE
;

GETREPLIES

1reply 10 TYPE
2reply 10 TYPE
3reply 10 TYPE