srcdir = .


doofus:
	@echo ""
	@echo "Let's try this from the right directory..."
	@echo ""
	@cd ../../../ && make

static: ../mysql.o

modules: ../../../mysql.$(MOD_EXT)

../mysql.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) `mysql_config --cflags` -DMAKING_MODS -c $(srcdir)/mysql.c
	@rm -f ../mysql.o
	mv mysql.o ../

../../../mysql.$(MOD_EXT): ../mysql.o
	$(LD) -o ../../../mysql.$(MOD_EXT) ../mysql.o $(XLIBS) $(MODULE_XLIBS) `mysql_config --libs`
	$(STRIP) ../../../mysql.$(MOD_EXT)

depend:
	$(CC) $(CFLAGS) $(CPPFLAGS) -MM $(srcdir)/mysql.c > .depend

clean:
	@rm -f .depend *.o *.$(MOD_EXT) *~
distclean: clean

#safety hash
../mysql.o: .././mysql.mod/mysql.c ../../../src/mod/module.h \
 ../../../src/main.h ../../../src/lang.h ../../../src/eggdrop.h \
 ../../../src/flags.h ../../../src/proto.h ../../../lush.h \
 ../../../src/misc_file.h ../../../src/cmdt.h ../../../src/tclegg.h \
 ../../../src/tclhash.h ../../../src/chan.h ../../../src/users.h \
 ../../../src/compat/compat.h ../../../src/compat/inet_aton.h \
 ../../../src/compat/snprintf.h ../../../src/compat/memset.h \
 ../../../src/compat/memcpy.h ../../../src/compat/strcasecmp.h \
 ../../../src/mod/modvals.h ../../../src/tandem.h
