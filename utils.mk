CFLAGS += -I$(shell pwd)

-include ${patsubst %.o,%.d,$(DIR_OBJ_FILES)}

%.d: %.c
	$(CC) -MM $(CFLAGS) $< > $@.$$$$;	\
	sed 's,\($(notdir $*)\)\.o[ :]*,$(dir $*)\1.o $@ : ,g' < $@.$$$$ > $@;	\
	$(RM) -f $@.$$$$
