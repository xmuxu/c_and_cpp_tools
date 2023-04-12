#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// 定义消息结构体
typedef struct msg_t {
    char* content;
    struct msg_t* next;
} msg_t;

// 定义消息队列结构体
typedef struct queue_t {
    msg_t* head;
    msg_t* tail;
    int size;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    msg_t* pool;
} queue_t;

// 初始化消息队列
queue_t* queue_init() {
    queue_t* queue = (queue_t*)malloc(sizeof(queue_t));
    queue->head = queue->tail = NULL;
    queue->size = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
    queue->pool = NULL;
    return queue;
}

// 销毁消息队列
void queue_destroy(queue_t* queue) {
    msg_t* msg = queue->head;
    while (msg) {
        msg_t* next = msg->next;
        free(msg->content);
        free(msg);
        msg = next;
    }
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->cond);

    // 销毁对象池
    msg_t* pool = queue->pool;
    while (pool) {
        msg_t* next = pool->next;
        free(pool->content);
        free(pool);
        pool = next;
    }

    free(queue);
}

// 创建新消息
msg_t* msg_create(queue_t* queue, const char* content) {
    if (queue->pool) {
        // 从对象池中取出消息
        msg_t* msg = queue->pool;
        queue->pool = msg->next;
        msg->content = (char*)realloc(msg->content, strlen(content) + 1);
        strcpy(msg->content, content);
        msg->next = NULL;
        return msg;
    } else {
        // 创建新消息
        msg_t* msg = (msg_t*)malloc(sizeof(msg_t));
        msg->content = (char*)malloc(strlen(content) + 1);
        strcpy(msg->content, content);
        msg->next = NULL;
        return msg;
    }
}

// 销毁消息
void msg_destroy(queue_t* queue, msg_t* msg) {
    if (queue->pool) {
        // 对象池已满，直接销毁消息
        free(msg->content);
        free(msg);
    } else {
        // 放入对象池中
        msg->next = queue->pool;
        queue->pool = msg;
    }
}

// 入队操作
void queue_push(queue_t* queue, const char* content) {
    pthread_mutex_lock(&queue->mutex);
    // 创建新消息
    msg_t* msg = msg_create(queue, content);

    // 加入队列尾部
    if (queue->tail == NULL) {
        queue->head = queue->tail = msg;
    } else {
        queue->tail->next = msg;
        queue->tail = msg;
    }
    queue->size++;
    pthread_mutex_unlock(&queue->mutex);

    // 通知消费者有新消息到达
    pthread_cond_signal(&queue->cond);
}

// 出队操作
msg_t* queue_pop_msg(queue_t* queue) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->head == NULL) {
        // 消息队列为空，等待通知
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    // 取出队头消息
    msg_t* msg = queue->head;
    queue->head = msg->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    queue->size--;
    pthread_mutex_unlock(&queue->mutex);
    return msg;
}

// 消费者线程函数
void* consumer(void* arg) {
    queue_t* queue = (queue_t*)arg;
    while (1) {
        msg_t* msg = queue_pop_msg(queue);
        printf("Received message: %s\n", msg->content);
        msg_destroy(queue, msg);
    }
    return NULL;
}

int main() {
    // 创建消息队列
    queue_t* queue = queue_init();

    // 创建消费者线程
    pthread_t tid;
    pthread_create(&tid, NULL, consumer, queue);

    // 入队操作
    queue_push(queue, "Hello");
    queue_push(queue, "World");

    // 等待消费者线程结束
    pthread_join(tid, NULL);

    // 销毁消息队列
    queue_destroy(queue);

    return 0;
}
