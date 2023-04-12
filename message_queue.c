#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// ������Ϣ�ṹ��
typedef struct msg_t {
    char* content;
    struct msg_t* next;
} msg_t;

// ������Ϣ���нṹ��
typedef struct queue_t {
    msg_t* head;
    msg_t* tail;
    int size;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    msg_t* pool;
} queue_t;

// ��ʼ����Ϣ����
queue_t* queue_init() {
    queue_t* queue = (queue_t*)malloc(sizeof(queue_t));
    queue->head = queue->tail = NULL;
    queue->size = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
    queue->pool = NULL;
    return queue;
}

// ������Ϣ����
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

    // ���ٶ����
    msg_t* pool = queue->pool;
    while (pool) {
        msg_t* next = pool->next;
        free(pool->content);
        free(pool);
        pool = next;
    }

    free(queue);
}

// ��������Ϣ
msg_t* msg_create(queue_t* queue, const char* content) {
    if (queue->pool) {
        // �Ӷ������ȡ����Ϣ
        msg_t* msg = queue->pool;
        queue->pool = msg->next;
        msg->content = (char*)realloc(msg->content, strlen(content) + 1);
        strcpy(msg->content, content);
        msg->next = NULL;
        return msg;
    } else {
        // ��������Ϣ
        msg_t* msg = (msg_t*)malloc(sizeof(msg_t));
        msg->content = (char*)malloc(strlen(content) + 1);
        strcpy(msg->content, content);
        msg->next = NULL;
        return msg;
    }
}

// ������Ϣ
void msg_destroy(queue_t* queue, msg_t* msg) {
    if (queue->pool) {
        // �����������ֱ��������Ϣ
        free(msg->content);
        free(msg);
    } else {
        // ����������
        msg->next = queue->pool;
        queue->pool = msg;
    }
}

// ��Ӳ���
void queue_push(queue_t* queue, const char* content) {
    pthread_mutex_lock(&queue->mutex);
    // ��������Ϣ
    msg_t* msg = msg_create(queue, content);

    // �������β��
    if (queue->tail == NULL) {
        queue->head = queue->tail = msg;
    } else {
        queue->tail->next = msg;
        queue->tail = msg;
    }
    queue->size++;
    pthread_mutex_unlock(&queue->mutex);

    // ֪ͨ������������Ϣ����
    pthread_cond_signal(&queue->cond);
}

// ���Ӳ���
msg_t* queue_pop_msg(queue_t* queue) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->head == NULL) {
        // ��Ϣ����Ϊ�գ��ȴ�֪ͨ
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    // ȡ����ͷ��Ϣ
    msg_t* msg = queue->head;
    queue->head = msg->next;
    if (queue->head == NULL) {
        queue->tail = NULL;
    }
    queue->size--;
    pthread_mutex_unlock(&queue->mutex);
    return msg;
}

// �������̺߳���
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
    // ������Ϣ����
    queue_t* queue = queue_init();

    // �����������߳�
    pthread_t tid;
    pthread_create(&tid, NULL, consumer, queue);

    // ��Ӳ���
    queue_push(queue, "Hello");
    queue_push(queue, "World");

    // �ȴ��������߳̽���
    pthread_join(tid, NULL);

    // ������Ϣ����
    queue_destroy(queue);

    return 0;
}
