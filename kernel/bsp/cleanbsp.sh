find \( -name '*.[aios]' -o -name '*.ko' -o -name '.*.cmd' -o -name '*.ko.*' \
		-o -name '*.dtb' -o -name '*.dtbo' -o -name '*.dtb.S' -o -name '*.dt.yaml' -o -name '*.dwo' -o -name '*.lst' \
		-o -name '*.su' -o -name '*.mod' -o -name '.*.d' -o -name '.*.tmp' -o -name '*.mod.c' \
		-o -name '*.lex.c' -o -name '*.tab.[ch]' -o -name '*.asn1.[ch]' \
		-o -name '*.symtypes' -o -name 'modules.order' -o -name '.tmp_*.o.*' \
		-o -name '*.c.[012]*.*' -o -name '*.ll' -o -name '*.gcno' \
		-o -name '*.*.symversions' \) -type f -print | xargs rm -vf
