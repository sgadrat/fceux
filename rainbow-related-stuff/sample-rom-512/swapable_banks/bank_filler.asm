#echo
#echo bank total space:
#print $c000-$8000
#echo
#echo bank free space:
#print $c000-*

.dsb $c000-*, CURRENT_BANK_NUMBER
