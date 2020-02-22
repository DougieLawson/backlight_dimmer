src	=	$(wildcard	*.c)
obj	=	$(src:.c=.o)

LDFLAGS	=	-lm	-lpcre

timeout:	$(obj)
				$(CC)	-o	$@	$^	$(LDFLAGS)

.PHONY:	clean
clean:
				rm	-f	$(obj)	timeout
