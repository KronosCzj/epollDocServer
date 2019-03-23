#include"condition.h"

typedef struct task{		//线程池中的对线需要执行的任务 
	void (*run)(void *args);	//函数指针，需要执行的函数
	void *arg;				//参数
	struct task *next;		//任务队列中的下一个任务 
}task_t;


typedef struct threadpool{		//线程池结构体 
	condition_t ready;			//状态量 （多个线程抢夺一个任务，要保证只有一个线程夺得）对任务，idle,counter进行操作时都要上锁 
	task_t *first;				//任务队列中第一个任务 
	task_t *last;				//任务队列中最后一个任务 
	int counter;				//线程池中已有线程数目 （正在工作的线程数） 
	int idle;					//线程池中空闲线程数 
	int max_threads;			//线程池最大线程数 
	int quit;					//是否关闭线程池，0不关闭，1关闭 
}threadpool_t; 

void threadpool_init(threadpool_t *pool,int threads);

void threadpool_add_task(threadpool_t *pool,void (*run)(void *arg),void *arg);

void threadpool_destroy(threadpool_t *pool);



 
