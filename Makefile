CC =		gcc
CFLAGS =	-g -O0
LDFLAGS =	-lpthread -lm
OBJSR = performance.o dictionary.o iniparser.o cost_model_history.o sockio.o scheduler_main.o scheduler_SFQD.o logging.o dump.o llist.o proxy2.o utility.o heap.o signals.o message.o config.o socket_pool.o options.o scheduler_DSFQ.o scheduler_vDSFQ.o scheduler_SFQD2.o scheduler_SFQD3.o scheduler_vDSFQ2.o scheduler_2LSFQD.o scheduler_SFQD_Full.o

all:	proxy2

proxy2: ${OBJSR}
	${CC} ${CFLAGS} ${LDFLAGS} ${OBJSR} -o proxy2
	
cost_model_history.o: cost_model_history.c cost_model_history.h	
	$(CC) $(CFLAGS) -c cost_model_history.c

sockio.o: sockio.c sockio.h
	$(CC) $(CFLAGS) -c sockio.c
	
signals.o: signals.c signals.h system.h
	$(CC) $(CFLAGS) -c signals.c
	
logging.o: logging.c logging.h
	$(CC) $(CFLAGS) -c logging.c
		
dump.o: dump.c dump.h
	$(CC) $(CFLAGS) -c dump.c

llist.o: llist.c llist.h
	$(CC) $(CFLAGS) -c llist.c
	
scheduler_SFQD.o: scheduler_SFQD.c scheduler_SFQD.h proxy2.h
	$(CC) $(CFLAGS) -c scheduler_SFQD.c

scheduler_2LSFQD.o: scheduler_2LSFQD.c scheduler_2LSFQD.h proxy2.h
	$(CC) $(CFLAGS) -c scheduler_2LSFQD.c


scheduler_vDSFQ.o: scheduler_vDSFQ.c scheduler_vDSFQ.h proxy2.h
	$(CC) $(CFLAGS) -c scheduler_vDSFQ.c

scheduler_SFQD2.o: scheduler_SFQD2.c scheduler_SFQD2.h proxy2.h
	$(CC) $(CFLAGS) -c scheduler_SFQD2.c

scheduler_SFQD3.o: scheduler_SFQD3.c scheduler_SFQD3.h proxy2.h
	$(CC) $(CFLAGS) -c scheduler_SFQD3.c

scheduler_vDSFQ2.o: scheduler_vDSFQ2.c scheduler_vDSFQ2.h proxy2.h
	$(CC) $(CFLAGS) -c scheduler_vDSFQ2.c

scheduler_SFQD_Full.o: scheduler_SFQD_Full.c scheduler_SFQD_Full.h proxy2.h
	$(CC) $(CFLAGS) -c scheduler_SFQD_Full.c

options.o: options.c
	$(CC) $(CFLAGS) -c options.c
	
utility.o: utility.c
	$(CC) $(CFLAGS) -c utility.c
	
proxy2.o: proxy2.c proxy2.h
	$(CC) $(CFLAGS) -c proxy2.c
	
message.o: message.c message.h
	$(CC) $(CFLAGS) -c message.c

socket_pool.o: socket_pool.c socket_pool.h
	$(CC) $(CFLAGS) -c socket_pool.c
	
heap.o: heap.c heap.h
	$(CC) $(CFLAGS) -c heap.c

iniparser.o: iniparser.c iniparser.h
	$(CC) $(CFLAGS) -c iniparser.c

dictionary.o: dictionary.c dictionary.h 
	$(CC) $(CFLAGS) -c dictionary.c
	
performance.o: performance.c performance.h
	$(CC) $(CFLAGS) -c performance.c
	
config.o: config.c config.h
	$(CC) $(CFLAGS) -c config.c

scheduler_main.o: scheduler_main.c scheduler_main.h
	$(CC) $(CFLAGS) -c scheduler_main.c

scheduler_DSFQ.o: scheduler_DSFQ.c scheduler_DSFQ.h
	$(CC) $(CFLAGS) -c scheduler_DSFQ.c

clean:
	rm -f proxy2 *.o core core.* *.core *.gch
