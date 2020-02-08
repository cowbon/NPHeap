CFLAGS := -g -O0 -I/usr/local/include
LDFLAGS := -lnpheap
SOURCE := benchmark.c validate.c benchmark_one_obj.c
PROGS := ${SOURCE:.c=}
all: $(PROGS)

$(PROGS): %:%.c
	$(CC) $< -o $@ $(CFLAGS) $(LDFLAGS)

	
clean:
	rm -f $(PROGS)
