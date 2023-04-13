#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

#define QUEUE_POOL_SIZE 1000

typedef struct node {
    void *data;
    struct node *next;
} node_t;

typedef struct queue {
    node_t *head;
    node_t *tail;
    int size;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    node_t *pool;
    int pool_size;
} queue_t;

node_t *node_pool_init(int pool_size) {
    node_t *pool = malloc(pool_size * sizeof(node_t));

    for (int i = 0; i < pool_size - 1; i++) {
        pool[i].next = &(pool[i + 1]);
    }

    pool[pool_size - 1].next = NULL;

    return pool;
}

void queue_init(queue_t *queue, int pool_size) {
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    pthread_mutex_init(&(queue->lock), NULL);
    pthread_cond_init(&(queue->cond), NULL);
    queue->pool = node_pool_init(pool_size);
    queue->pool_size = pool_size;
}

void queue_destroy(queue_t *queue) {
    pthread_mutex_destroy(&(queue->lock));
    pthread_cond_destroy(&(queue->cond));
    free(queue->pool);
}

bool is_empty(queue_t *queue) {
    return queue->head == NULL;
}

int size(queue_t *queue) {
    return queue->size;
}

void enqueue(queue_t *queue, void *data) {
    pthread_mutex_lock(&(queue->lock));

    if (queue->pool == NULL) {
        pthread_mutex_unlock(&(queue->lock));
        return;
    }

    node_t *new_node = queue->pool;
    queue->pool = queue->pool->next;

    new_node->data = data;
    new_node->next = NULL;

    if (is_empty(queue)) {
        queue->head = new_node;
    } else {
        queue->tail->next = new_node;
    }

    queue->tail = new_node;
    queue->size++;

    pthread_cond_signal(&(queue->cond));
    pthread_mutex_unlock(&(queue->lock));
}

void *dequeue(queue_t *queue) {
    pthread_mutex_lock(&(queue->lock));

    while (is_empty(queue)) {
        pthread_cond_wait(&(queue->cond), &(queue->lock));
    }

    void *data = queue->head->data;
    node_t *temp = queue->head;
    queue->head = queue->head->next;
    queue->size--;

    temp->next = queue->pool;
    queue->pool = temp;

    pthread_mutex_unlock(&(queue->lock));
    return data;
}

queue_t queue;
pthread_t producer, consumer;

int producer_data[] = {1, 2, 3, 4, 5};
const int producer_data_size = sizeof(producer_data) / sizeof(producer_data[0]);

void *producer_func(void *arg) {
	for (int i = 0; i < producer_data_size; i++) {
		enqueue(&queue, &(producer_data[i]));
		printf("Producer: enqueued %d\n", producer_data[i]);
	}

	return NULL;
}

void *consumer_func(void *arg) {
	for (int i = 0; i < producer_data_size; i++) {
		int *data = dequeue(&queue);
		printf("Consumer: dequeued %d\n", *data);
	}

	return NULL;
}

int main() {
    queue_init(&queue, QUEUE_POOL_SIZE);

    pthread_create(&producer, NULL, &producer_func, NULL);
    pthread_create(&consumer, NULL, &consumer_func, NULL);

    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);

    queue_destroy(&queue);

    return 0;
}
