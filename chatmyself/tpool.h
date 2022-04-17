/* 线程池模板1   做法：有一个任务队列，先创建若干个线程，这些线程都去访问任务队列
当任务队列为空时，都睡眠等待，当任务队列中有任务时，去唤醒线程池里的线程去任务队列中取任务 */

#include<pthread.h>
#include<sqlite3.h>
#include<signal.h>
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>


/* 任务结点 ，链式队列实现*/
struct job
{
    void *(*func)(void *arg);    /* 任务函数，每次读此执行相应任务 */
    void *arg;                   /* 传入任务函数的参数，即套接口描述符 */
    struct job *next;
};

struct threadpool
{
    int thread_num;  //已开启线程池已工作线程
    pthread_t *pthread_ids;  // 薄脆线程池中线程id


    struct job *head;  //任务队列头指针
    struct job *tail;  // 任务队列的尾
    int queue_max_num;  //任务队列的最多放多少个
    int queue_cur_num;  //任务队列已有多少个任务

    pthread_mutex_t mutex;  //任务队列是对所有线程池中的线程共享访问的，访问一次就要上锁
    pthread_cond_t queue_empty;    //任务队列为空时，所有线程都要等待，等通知，通知之后再去访问任务队列
    pthread_cond_t queue_not_emtpy;  //任务队列不为空
    pthread_cond_t queue_not_full;  //任务队列不为满

    int pool_close;
};

/* 初始化线程池 */
struct threadpool * threadpool_init(int thread_num, int queue_max_num);

/* 向线程池中添加任务 */
void threadpool_add_job(struct threadpool *pool, void *(*func)(void *), void *arg);

/* 销毁线程池 */
void thread_destroy(struct threadpool *pool);