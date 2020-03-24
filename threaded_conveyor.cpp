// threaded conveyor example to load Your cores hard
// just for fun
// (C) VVS (aka Vadim Sinolits), 2001-2020
// use it for free

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "threaded_conveyor.h"

// get free buffer to put data in it
thread_data_t *get_loopback(loopback_t *loopback) {
thread_data_t *data;
	pthread_mutex_lock (&loopback->lock);
	while ((loopback->writepos + 1) % loopback->n == loopback->readpos)
	{
		pthread_cond_wait (&loopback->notfull, &loopback->lock);
		}

	data = &loopback->data[loopback->writepos];
	
	loopback->writepos++;
	if (loopback->writepos >= loopback->n)
		loopback->writepos = 0;
	pthread_cond_signal (&loopback->notempty);
	pthread_mutex_unlock (&loopback->lock);

return data;
}

// release the buffer for input
void release_loopback(loopback_t *loopback) {

	pthread_mutex_lock (&loopback->lock);
	while (loopback->writepos == loopback->readpos)
	{
		pthread_cond_wait (&loopback->notempty, &loopback->lock);
		}
	// just switch the counter
	loopback->readpos++;
	if (loopback->readpos >= loopback->n)
		loopback->readpos = 0;
	pthread_cond_signal (&loopback->notfull);
	pthread_mutex_unlock (&loopback->lock);
}



void *manager_thread (void *p_){

	conveyor	*p = (conveyor*)p_;

	thread_data_t *data;
	
	pthread_mutex_lock (&p->startlock);

	printf("Start manager thread\n");

	if(p->next) {	// we have the next worker
	for(;;){
		data = p->Get();
		conveyor *next = (conveyor *)p->next;
		next->Put(data->x);
	}
	} else {
	for(;;){	// no next worker, just release the loopback buffer
		data = p->Get();
		printf("          GOT DATA!!!!!!!!!! %d\n", data->x);  fflush(stdout);
		// no next, so, release the loopback buff
		release_loopback(p->loopback_in);
		}
	}
return NULL;
}

void *worker_thread(void *arg) {
worker_t *w = (worker_t*)arg;
int	i;

	for(;;) {
	pthread_mutex_lock (&w->lock);

	while(w->state != C_STATE_START)
		pthread_cond_wait (&w->notempty, &w->lock);

	pthread_mutex_lock (&w->lock1);
	while (w->have_data)
		pthread_cond_wait (&w->havedata, &w->lock1);

	thread_data_t *t = (thread_data_t*)w->data;
	for(i= 0; i < t->len; i++)
		t->y[i] = rand();

	w->have_data = true;
	w->state = C_STATE_DATA;
	pthread_cond_signal (&w->havedata);
	pthread_mutex_unlock (&w->lock1);

	pthread_cond_signal (&w->notfull);
	pthread_mutex_unlock (&w->lock);
	}
return NULL;
}

conveyor::conveyor(loopback_t *lbin, loopback_t *lbout, int id_, int n_){
int	i;

printf("Start conveyor %d\n", id_);
	pthread_mutex_init (&lock, NULL);
	pthread_cond_init (&notempty, NULL);
	pthread_cond_init (&notfull, NULL);
	readpos = 0;
	writepos = 0;

	n = n_;
	id = id_;

	loopback_in = lbin;
	loopback_out = lbout;
	
	data = (thread_data_t **)malloc(n * sizeof(thread_data_t*));
	memset(data, 0, n * sizeof(thread_data_t*));
	workers = (worker_t*)malloc(n * sizeof(worker_t));
	char bb[32];
	for(i = 0; i < n; i++) {
		pthread_mutex_init (&workers[i].lock, NULL);
		pthread_mutex_init (&workers[i].lock1, NULL);
		pthread_cond_init (&workers[i].notempty, NULL);
		pthread_cond_init (&workers[i].notfull, NULL);
		pthread_cond_init (&workers[i].havedata, NULL);
		workers[i].state = C_STATE_WAIT;
		workers[i].have_data = false;
		workers[i].data = NULL;
		pthread_create(&workers[i].th, NULL, worker_thread, &workers[i]);
		sprintf(bb, "dread_worker_%d_%d", id, i);
		pthread_setname_np(workers[i].th, bb);
		}

	pthread_mutex_init (&startlock, NULL);
	pthread_mutex_lock (&startlock);
	pthread_create(&manager_th, NULL, manager_thread, this);
	sprintf(bb, "dread_manager_%d", id);
	pthread_setname_np(manager_th, bb);
	
}

void conveyor::Start(){
	pthread_mutex_unlock (&startlock);
}

conveyor::~conveyor(){
int i;
if(workers) {
	for(i = 0; i < n; i++) {
		pthread_cancel(workers[i].th);
		void *retv;
		pthread_join(workers[i].th, &retv);
	}
	free(workers);
	}
if(data)
	free(data);
}

void conveyor::Put(int x) {

	// get the place for data
	thread_data_t *t = get_loopback(loopback_in);

	// first, main conveyor
	pthread_mutex_lock (&lock);
	while ((writepos + 1) % n == readpos)
		pthread_cond_wait (&notfull, &lock);

	// wait for worker to start
	pthread_mutex_lock (&workers[writepos].lock);
	while (workers[writepos].state != C_STATE_WAIT)
		pthread_cond_wait (&workers[writepos].notfull, &workers[writepos].lock);
        
	t->x = x;
	data[writepos] = t;
	workers[writepos].data = t;
	
	// start worker
	workers[writepos].state = C_STATE_START;
	pthread_cond_signal (&workers[writepos].notempty);
	pthread_mutex_unlock (&workers[writepos].lock);
                
	writepos++;
	if (writepos >= n)
		writepos = 0;
	pthread_cond_signal (&notempty);
	pthread_mutex_unlock (&lock);
}


thread_data_t *conveyor::Get () {
	thread_data_t *d;
	pthread_mutex_lock (&lock);
	while (writepos == readpos)
		pthread_cond_wait (&notempty, &lock);

		pthread_mutex_lock (&workers[readpos].lock1);
		while(!workers[readpos].have_data)
			pthread_cond_wait (&workers[readpos].havedata, &workers[readpos].lock1);

		d = data[readpos];

		// release y
		if(loopback_out)
			release_loopback(loopback_out);

		workers[readpos].have_data = false;
		workers[readpos].state = C_STATE_WAIT;
		pthread_cond_signal (&workers[readpos].havedata);
		pthread_mutex_unlock (&workers[readpos].lock1);
		
	readpos++;
	if (readpos >= n)
		readpos = 0;
	pthread_cond_signal (&notfull);
	pthread_mutex_unlock (&lock);

return d;
}


main_conveyor::main_conveyor(int nc_, int nw_, int nb_, int len_) {
int	i, j;
	nc = nc_;
	nw = nw_;
	len = len_;
	nb = nb_;

loopback = (loopback_t *)malloc(nc * sizeof(loopback_t));
C = (conveyor **)malloc(nc * sizeof(conveyor*));

for(i = 0; i < nc; i++) {
	pthread_mutex_init (&loopback[i].lock, NULL);
	pthread_cond_init (&loopback[i].notempty, NULL);
	pthread_cond_init (&loopback[i].notfull, NULL);
	loopback[i].readpos = 0;
	loopback[i].writepos = 0;
	loopback[i].id = i;
	loopback[i].n = nb;

	// array for work
	loopback[i].data = (thread_data_t*)malloc(loopback[i].n * sizeof (thread_data_t));
	for(j = 0; j < loopback[i].n; j++) {
		loopback[i].data[j].len = len;
		loopback[i].data[j].y = (int*)malloc(loopback[i].data[j].len * sizeof (int));

		}

	}

// first conveyor has no output loopback control
// current scheme is i+1 worker loopback
for(i = 0; i < nc; i++) {
	if(i == 0)
		C[i] = new conveyor(&loopback[i], NULL, i, nw);
		else
		C[i] = new conveyor(&loopback[i], &loopback[i-1], i, nw * i);
	C[i]->next = NULL;
	}

// define loopback for each conv.
for(i = 0; i < nc - 1; i++)
	C[i]->next = C[i+1];		// where to call Put(); last one is empty (nc -1): output point

// so, everything is OK, startting conveyors
for(i = 0; i < nc; i++)
	C[i]->Start();
}

main_conveyor::~main_conveyor(){
int	i, j;
if(C) {
	for(i = 0; i < nc; i++)
		if(C[i]) delete C[i];
	free(C);
	}

if(loopback) {	
	for(i = 0; i < nc; i++) {
		for(j = 0; j < loopback[i].n; j++)
			if(loopback[i].data[j].y) free(loopback[i].data[j].y);
		if(loopback[i].data) free(loopback[i].data);
	}
	free(loopback);
	}

}



void *generator(void *p_){
main_conveyor *p = (main_conveyor*)p_;
int i = 0;
for(;;){
	p->C[0]->Put(i);
	i++;
	}


return NULL;
}



int main(){
pthread_t th, th1;
void *res;

main_conveyor *a = new main_conveyor(4, 15, 100, 500000);

pthread_create(&th, NULL, generator, a);
pthread_join(th, &res);
delete a;
return 0;
}
 