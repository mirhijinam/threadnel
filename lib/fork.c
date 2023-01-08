/* implement fork from user space */

#include <inc/string.h>
#include <inc/lib.h>
#include <inc/x86.h>

/* User-level fork with copy-on-write.
 * Create a child.
 * Lazily copy our address space and page fault handler setup to the child.
 * Then mark the child as runnable and return.
 *
 * Returns: child's envid to the parent, 0 to the child, < 0 on error.
 * It is also OK to panic on error.
 *
 * Hint:
 *   Use sys_map_region, it can perform address space copying in one call
 *   Remember to fix "thisenv" in the child process.
 */
envid_t
fork(void) {
    // LAB 9: Your code here
    envid_t envid = sys_exofork();
    if (envid < 0)
        return envid;
    if (!envid) {
        thisenv = &envs[ENVX(sys_getenvid())];
        return 0;
    }
    if (sys_map_region(0, NULL, envid, NULL, MAX_USER_ADDRESS, PROT_ALL | PROT_LAZY | PROT_COMBINE)
        || sys_env_set_pgfault_upcall(envid, thisenv->env_pgfault_upcall)
        || sys_env_set_status(envid, ENV_RUNNABLE))
        return -1;
    //panic("fork() is not implemented");
    return envid;
}

volatile uintptr_t rip;

envid_t
thread_create(void (*func)()) {

	rip = (uintptr_t) func;

    envid_t id = sys_thread_create((uintptr_t)thread_main);

	return id;
}

// Individual
/* Функция добавляет environment в конец списка ожидающих потоков. */
void 
queue_append(envid_t envid, struct waiting_queue* queue) 
{
#ifdef TRACE
		cprintf("Appending an env (envid: %d)\n", envid);
#endif
	struct waiting_thread* wt = NULL;

	int r = sys_alloc_region(envid,(void*) wt, PAGE_SIZE, PTE_P | PTE_W | PTE_U);
	if (r < 0) {
		panic("%e\n", (double) r);
	}	

	wt->envid = envid;

	if (queue->first == NULL) {
		queue->first = wt;
		queue->last = wt;
		wt->next = NULL;
	} else {
		queue->last->next = wt;
		wt->next = NULL;
		queue->last = wt;
	}
}

/* Функция выбирает первого env из списка ожидания. 
 * Warning! Функция не должна вызываться, если список пустой. */
envid_t 
queue_pop(struct waiting_queue* queue) 
{

	if(queue->first == NULL) {
		panic("mutex waiting list empty!\n");
	}
	struct waiting_thread* popped = queue->first;
	queue->first = popped->next;
	envid_t envid = popped->envid;
#ifdef TRACE
	cprintf("Popping an env (envid: %d)\n", envid);
#endif
	return envid;
}

/* Функция блокирует мьютекс. Значение "Locked" изменяется атомарной операцией xchg.
 * Если возвращается 0 и никто не ждет мьютекс, мы устанавливаем владельца в текущий env
 * Иначе, env добавляется в конец списка и устанавливается в NOT RUNNABLE.
 */
void 
mutex_lock(struct Mutex* mtx)
{
	if ((xchg(&mtx->locked, 1) != 0)) {
		queue_append(sys_getenvid(), &mtx->queue);		
		int r = sys_env_set_status(sys_getenvid(), ENV_NOT_RUNNABLE);	

		if (r < 0) {
			panic("%e\n", (double) r);
		}
		sys_yield();
	} else {
		mtx->owner = sys_getenvid();
	}
}


/* Разблокировка мьютекса - изменение заблокированного значения на 0. 
 * Если лист ожидания не пустой, env запускается по порядку, 
 * он устанавливается как владелец потока и этот поток устанавливается как исполняемый. 
 */
void 
mutex_unlock(struct Mutex* mtx)
{
	while (xchg(&mtx->queueMutex, 1) == 1)
		;
	if (mtx->queue.first != NULL) {
		mtx->owner = queue_pop(&mtx->queue);
		xchg(&mtx->queueMutex, 0);
		int r = sys_env_set_status(mtx->owner, ENV_RUNNABLE);
		if (r < 0) {
			panic("%e\n", (double) r);
		}
	} else {
		xchg(&mtx->queueMutex, 0);
		xchg(&mtx->locked, 0);
	}
	
	sys_yield();
}

/* Инициализация мьютекса:
 * выделяется свободная страница и устанавливаются нулевые значения. */
void 
mutex_init(struct Mutex* mtx)
{	int r;
	if ((r = sys_alloc_region(sys_getenvid(), mtx, PAGE_SIZE, PTE_P | PTE_W | PTE_U)) < 0) {
		panic("panic at mutex init: %e\n", (double) r);
	}	
	mtx->locked = 0;
	mtx->queue.first = NULL;
	mtx->queue.last = NULL;
	mtx->owner = 0;
}

/* Удаление мьютекса:
 * все ожидающие потоки удаляются из очереди и устанавливаются в RUNNABLE. */ 
void 
mutex_destroy(struct Mutex* mtx)
{
	while (mtx->queue.first != NULL) {
		sys_env_set_status(queue_pop(&mtx->queue), ENV_RUNNABLE);
		mtx->queue.first = mtx->queue.first->next;
	}

	memset(mtx, 0, PAGE_SIZE);
	mtx = NULL;
}

envid_t
sfork() {
    panic("sfork() is not implemented");
}
