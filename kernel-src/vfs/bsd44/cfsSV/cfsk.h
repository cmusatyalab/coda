/* Macros to manipulate the queue */
#ifndef INIT_QUEUE
struct queue {
    struct queue *forw, *back;
};

#define INIT_QUEUE(head)                     \
do {                                         \
    (head).forw = (struct queue *)&(head);   \
    (head).back = (struct queue *)&(head);   \
} while (0)

#define GETNEXT(head) (head).forw

#define EMPTY(head) ((head).forw == &(head))

#define EOQ(el, head) ((struct queue *)(el) == (struct queue *)&(head))
		   
#define INSQUE(el, head)                             \
do {                                                 \
	(el).forw = ((head).back)->forw;             \
	(el).back = (head).back;                     \
	((head).back)->forw = (struct queue *)&(el); \
	(head).back = (struct queue *)&(el);         \
} while (0)

#define REMQUE(el)                         \
do {                                       \
	((el).forw)->back = (el).back;     \
	(el).back->forw = (el).forw;       \
}  while (0)

#endif INIT_QUEUE
