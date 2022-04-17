// #include<pthread.h>
// #include<sqlite3.h>
// #include<signal.h>
#include"tpool.h"
// #include<stdio.h>


// /* 任务结点 ，链式队列实现*/
// struct job
// {
//     void *(*func)(void *arg);    /* 任务函数，每次读此执行相应任务 */
//     void *arg;                   /* 传入任务函数的参数，即套接口描述符 */
//     struct job *next;
// };

// struct threadpool
// {
//     int thread_num;  //已开启线程池已工作线程
//     pthread_t *pthread_ids;  // 薄脆线程池中线程id


//     struct job *head;  //任务队列头指针
//     struct job *tail;  // 任务队列的尾
//     int queue_max_num;  //任务队列的最多放多少个
//     int queue_cur_num;  //任务队列已有多少个任务

//     pthread_mutex_t mutex;  //任务队列是对所有线程池中的线程共享访问的，访问一次就要上锁
//     pthread_cond_t queue_empty;    //任务队列为空时，所有线程都要等待，等通知，通知之后再去访问任务队列
//     pthread_cond_t queue_not_emtpy;  //任务队列不为空
//     pthread_cond_t queue_not_full;  //任务队列不为满

//     int pool_close;
// };

/* 工作者线程函数，从任务链表中取出任务并执行 */
void * threadpool_function(void *arg)
{
    struct threadpool *pool = (struct threadpool *)arg;
    struct job *pjob = NULL;

    while (1)
    {
        pthread_mutex_lock(&(pool->mutex));//先上锁

        while(pool->queue_cur_num == 0)//判断任务队列是否为空，为空就等待
        {
            pthread_cond_wait(&(pool->queue_not_emtpy), &(pool->mutex));

            if (pool->pool_close == 1)
            {
                pthread_exit(NULL);
            }
        }


       //不为空就去任务队列中取任务并去运行该任务所指向的函数
        pjob = pool->head;
        pool->queue_cur_num--;

        if (pool->queue_cur_num != pool->queue_max_num)
        {
            pthread_cond_broadcast(&(pool->queue_not_full));
        }
        
        if (pool->queue_cur_num == 0)
        {
            pool->head = pool->tail = NULL;
            pthread_cond_broadcast(&(pool->queue_empty));
        }
        else
        {
            pool->head = pjob->next;
        }
        
        pthread_mutex_unlock(&(pool->mutex));

        (*(pjob->func))(pjob->arg);
        free(pjob);
        pjob = NULL;
    }
}

/* 初始化线程池 */
struct threadpool * threadpool_init(int thread_num, int queue_max_num)
{
    struct threadpool *pool = (struct threadpool *)malloc(sizeof(struct threadpool));
    // malloc

    pool->queue_max_num = queue_max_num;
    pool->queue_cur_num = 0;
    pool->pool_close = 0;
    pool->head = NULL;
    pool->tail = NULL;

    pthread_mutex_init(&(pool->mutex), NULL);
    pthread_cond_init(&(pool->queue_empty), NULL);
    pthread_cond_init(&(pool->queue_not_emtpy), NULL);
    pthread_cond_init(&(pool->queue_not_full), NULL);

    pool->thread_num = thread_num;
    pool->pthread_ids = (pthread_t *)malloc(sizeof(pthread_t) * thread_num);
    // malloc

    for (int i = 0; i < pool->thread_num; i++)
    {
        pthread_create(&pool->pthread_ids[i], NULL, (void *)threadpool_function, (void *)pool);
    }

    return pool;
}

/* 向线程池中添加任务 */
void threadpool_add_job(struct threadpool *pool, void *(*func)(void *), void *arg)
{
    pthread_mutex_lock(&(pool->mutex));//先上锁
    while (pool->queue_cur_num == pool->queue_max_num)
    {
        pthread_cond_wait(&pool->queue_not_full, &(pool->mutex));
    }
    
    
    struct job *pjob = (struct job *)malloc(sizeof(struct job));
    //malloc
    
    pjob->func = func;
    pjob->arg = arg;
    pjob->next = NULL;
    
    // pjob->func(pjob->arg);
    if (pool->head == NULL)
    {
        pool->head = pool->tail = pjob;
        pthread_cond_broadcast(&(pool->queue_not_emtpy));
    }
    else
    {
        pool->tail ->next = pjob;
        pool->tail = pjob;
    }

    pool->queue_cur_num++;
    pthread_mutex_unlock(&(pool->mutex));
}

/* 销毁线程池 */
void thread_destroy(struct threadpool *pool)
{
    pthread_mutex_lock(&(pool->mutex));

    while (pool->queue_cur_num != 0)
    {
         pthread_cond_wait(&(pool->queue_empty),&(pool->mutex));
    }

    pthread_mutex_unlock(&(pool->mutex));

    pthread_cond_broadcast(&(pool->queue_not_full));

    pool->pool_close = 1;

    for (int i = 0; i < pool->thread_num; i++)
    {
        pthread_cond_broadcast(&(pool->queue_not_emtpy));
        // pthread_cancel(pool->pthread_ids[i]); //有系统调用，才能销毁掉；有bug
        printf("thread exit!\n");
        pthread_join(pool->pthread_ids[i], NULL);
    }

    pthread_mutex_destroy(&(pool->mutex));
    pthread_cond_destroy(&(pool->queue_empty));
    pthread_cond_destroy(&(pool->queue_not_emtpy));
    pthread_cond_destroy(&(pool->queue_not_full));

    free(pool->pthread_ids);

    struct job *temp;
    while(pool->head != NULL)
    {
        temp = pool->head;
        pool->head = temp->next;
        free(temp);
    }

    free(pool);

    printf("destroy finish!\n");
}

