  lb $0,$1,100
  lb $0,$2,101
  sub $1,$2,$3
  bgez $3,L1
  lb $0,$3,100
L1:
  sb $0,$3,102
  break
