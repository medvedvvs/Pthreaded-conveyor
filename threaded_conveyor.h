#ifndef T_CONVEYOR_H
#include <pthread.h>



#define C_STATE_WAIT	0
#define C_STATE_START	1
#define C_STATE_DATA	2

typedef struct thread_data_t {
	int	x;			// just for sequence control
	int	*y;			// stupid data for work
	int	len;			// y buffer len


}thread_data_t;

// look at classical produce/consumer example
typedef struct loopback_t {
	int		id;			// lb  id
	pthread_mutex_t	lock;			// mutex ensuring exclusive access to lb
	int		readpos, writepos;	// positions for reading and writing
	pthread_cond_t	notempty;		// signaled when buffer is not empty
	pthread_cond_t	notfull;		// signaled when buffer is not full

	int		n;			// number of buffers to switch
	thread_data_t	*data;			// actual data[n]

} loopback_t;

typedef struct worker_t {
	thread_data_t   *data;                  // reference to the data for work
	pthread_t	th;			// worker thread
	pthread_mutex_t	lock;			// lock for input
	pthread_mutex_t	lock1;			// lock for output
	pthread_cond_t	notempty;		// signaled when buffer is not empty
	pthread_cond_t	notfull;		// signaled when buffer is not empty
	pthread_cond_t	havedata;		// signaled when buffer is not full
	bool		have_data;
	int		state;			// C_STATE_WAIT | C_STATE_START | C_STATE_DATA
} worker_t;


class conveyor {
public:
	conveyor(loopback_t *lbint, loopback_t *lbout, int id, int n_);
	~conveyor();

	int		n;			// number of workers/threads
	pthread_mutex_t	lock;
	int		readpos, writepos;
	pthread_cond_t	notempty;
	pthread_cond_t	notfull;
	
	pthread_mutex_t	startlock;		// just to sync

	thread_data_t	**data;			// array of pointers for work from loopback

	void		Put(int c);		// lounch data to conveyor
	thread_data_t	*Get();			// get ready data
	void		Start();		// start work

	loopback_t	*loopback_in;		// reference to input loopback buffer switch
	loopback_t	*loopback_out;		// output lb; NULL for the first buffer

	pthread_t	manager_th;		// managing thread of the buffer (get data and send it to the next conveyor)
	
	int		id;			// just id
	
	void		*next;			// next  conveyor
	
	worker_t	*workers;		// working threads


};


// just a plant with conveyors
class main_conveyor {
public:
	main_conveyor(int nc, int nw, int nb, int len);	// n - number of conveyors with nw workers in each
							// nb - number of buffers in loopback switch (data[nb]);
							// len - y buffer len in thread_data_t
	~main_conveyor();
	
	int	nc, nw, nb, len;


	loopback_t	*loopback;	// loopbacks
	conveyor	**C;		// conveyors

	
};

#endif // T_CONVEYOR_H
