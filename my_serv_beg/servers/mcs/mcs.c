#include "mcs.h"

/* Set debugging level to 0, 1, or 2 to see no, some, all debug output. */
#define DEBUG_LEVEL 1
#define DPRINTF   if (DEBUG_LEVEL > 0) printf

#define MUT1 m_u.m_m1.m1i1
#define COND1 m_u.m_m1.m1i2

struct cond_var
{
  int id;
  int process;
  int mutex_id;
};

struct mutex
{
  int id;
  int process;
};

/* Allocate space for the global variables. */
message m_in;   /* the input message itself */
message m_out;    /* the output message used for reply */
int who;    /* caller's proc number */
int callnr;   /* system call number */

int locked_mutexes_nmb;
int cur_mut;
struct mutex locked_mutexes[MAX_MUT];

int con_queue_size;
struct cond_var con_queue[NR_PROCS];

struct mutex mut_queue[NR_PROCS];
int mut_queue_size;

extern int errno; /* error number set by system library */

/* Declare some local functions. */
FORWARD _PROTOTYPE(void init_server, (int argc, char **argv)    );
FORWARD _PROTOTYPE(void exit_server, (void)       );
FORWARD _PROTOTYPE(void get_work, (void)        );
FORWARD _PROTOTYPE(void reply, (int whom, int result)     );
FORWARD _PROTOTYPE(void lock, (int mutex_id, int proc));
FORWARD _PROTOTYPE(void deal_with_unlock_sig, (int mutex_id, int who));
FORWARD _PROTOTYPE(void deal_with_wait_sig, (int who, int mutex_id, int cond_var_id));
FORWARD _PROTOTYPE(void deal_with_unpause_sig, (int who));
FORWARD _PROTOTYPE(void deal_with_broadcast_sig, (int who, int cond_var_id));
FORWARD _PROTOTYPE(void deal_with_exit_sig, (int who));


/*===========================================================================*
 *        main                                         *
 *===========================================================================*/
PUBLIC int main(int argc, char **argv)
{
/* This is the main routine of this service. The main loop consists of 
 * three major activities: getting new work, processing the work, and
 * sending the reply. The loop never terminates, unless a panic occurs.
 */
  int result;                 
  sigset_t sigset;

  /* Initialize the server, then go to work. */
  init_server(argc, argv);
  mut_queue_size = locked_mutexes_nmb = con_queue_size =  0;

  /* Main loop - get work and do it, forever. */         
  while (TRUE) {              

      /* Wait for incoming message, sets 'callnr' and 'who'. */
      get_work();

      switch (callnr) {
      case SYS_SIG:
        sigset = (sigset_t) m_in.NOTIFY_ARG;
        if (sigismember(&sigset,SIGTERM) || sigismember(&sigset,SIGKSTOP)) {
          exit_server();
        }
        continue;
      case MCS_LOCK:
          lock(m_in.MUT1, who);
          break;
      case MCS_UNLOCK:
          deal_with_unlock_sig(m_in.MUT1, who);
          break;
      case MCS_WAIT:
          deal_with_wait_sig(who, m_in.MUT1, m_in.COND1);
          break;
      case MCS_BROADCAST:
          deal_with_broadcast_sig(who, m_in.COND1);
          break;
      case MCS_UNPAUSE:
          reply(who,0);
          deal_with_unpause_sig(who);
          break;
      case MCS_EXIT:
          reply(who,0);
          deal_with_exit_sig(who);
          break;
      default: 
          report("MCS","warning, got illegal request ", callnr);
          result = EINVAL;
      }
  }
  return(OK);       /* shouldn't come here */
}

/*===========================================================================*
 *         init_server                                 *
 *===========================================================================*/
PRIVATE void init_server(int argc, char **argv)
{
/* Initialize the information service. */
  int fkeys, sfkeys;
  int i, s;
#if DEAD_CODE
  struct sigaction sigact;

  /* Install signal handler. Ask PM to transform signal into message. */
  sigact.sa_handler = SIG_MESS;
  sigact.sa_mask = ~0;      /* block all other signals */
  sigact.sa_flags = 0;      /* default behaviour */
  if (sigaction(SIGTERM, &sigact, NULL) < 0) 
      report("MCS","warning, sigaction() failed", errno);
#endif
}

/*===========================================================================*
 *        exit_server                                  *
 *===========================================================================*/
PRIVATE void exit_server()
{
  exit(0);
}

/*===========================================================================*
 *        get_work                                     *
 *===========================================================================*/
PRIVATE void get_work()
{
    int status = 0;
    status = receive(ANY, &m_in);   /* this blocks until message arrives */
    if (OK != status)
        panic("MCS","failed to receive message!", status);
    who = m_in.m_source;        /* message arrived! set sender */
    callnr = m_in.m_type;       /* set function call number */
    cur_mut = m_in.MUT1;
}

/*===========================================================================*
 *        reply              *
 *===========================================================================*/
PRIVATE void reply(who, result)
int who;                            /* destination */
int result;                             /* report result to replyee */
{
    int send_status;
    m_out.m_type = result;      /* build reply message */
    send_status = send(who, &m_out);    /* send the message */
    if (OK != send_status)
        panic("MCS", "unable to send reply!", send_status);
}


/*===========================================================================*
 *        pomocnicze funkcje do lockowanych mutexow           *
 *===========================================================================*/
/*1 if locked, 0 otherwise*/
PRIVATE int is_locked(int mutex_id)
{
  int i;
  for (i = 0; i < locked_mutexes_nmb; ++i)
  {
    if(locked_mutexes[i].id == mutex_id) {
      return 1;
    }
  }
  return 0;
}

/*returns index in locked_mutexes*/
PRIVATE int locked_nmb(int mutex_id)
{
  int i;
  for (i = 0; i < locked_mutexes_nmb; ++i)
  {
    if(locked_mutexes[i].id == mutex_id) {
      return i;
    }
  }
  return -1;
}

/*===========================================================================*
 *        lock              *
 *===========================================================================*/
PRIVATE void lock(int mutex_id,int proc)
{
  struct mutex m;
  int index;
  m.id = mutex_id;
  m.process = proc;

  index = locked_nmb(mutex_id);
  if(index != -1) 
  {
    if (locked_mutexes[index].process == proc)
    {
      reply(proc, 0);
      return;
    }
    /* dodaj na kolejke*/
    mut_queue[mut_queue_size] = m;
    mut_queue_size++;
  }
  else
  {
    /* lockuj*/
    locked_mutexes[locked_mutexes_nmb] = m;
    locked_mutexes_nmb++;
    reply(proc, 0);
  }
}

/*===========================================================================*
 *        deal_with_unpause_sig             *
 *===========================================================================*/
PRIVATE void deal_with_unpause_sig(int who)
{
  int i;

  for (i = 0; i < locked_mutexes_nmb; ++i)
  {
    if(mut_queue[i].process == who) 
    {
      move_mut_queue(i);
      reply(who, 0);
      return;
    }
  }

  for (i = 0; i < con_queue_size; ++i)
  {
    if (con_queue[i].process == who)
    {
      rmv_from_con_queue(i);
      reply(who, 0);
      return;
    }
  }
}

/*===========================================================================*
 *        deal_with_wait_sig             *
 *===========================================================================*/
PRIVATE void deal_with_wait_sig(int who, int mutex_id, int cond_var_id)
{
  int index;
  int i;
  struct cond_var cv;
  index = locked_nmb(mutex_id);

  if(index == -1 || locked_mutexes[index].process != who)
  {
    reply(who, EINVAL);
    return;
  }

  deal_with_unlock_sig(mutex_id, who);

  cv.mutex_id = mutex_id;
  cv.id = cond_var_id;
  cv.process = who;
  con_queue[con_queue_size] = cv;
  con_queue_size++;
}

/*===========================================================================*
 *        deal_broadcast_wait_sig             *
 *===========================================================================*/
PRIVATE void rmv_from_con_queue(int ind) {
  int i;

  for (i = ind; i < con_queue_size-1; ++i)
  {
    con_queue[i] = con_queue[i+1];
  }
  con_queue_size--;
}

PRIVATE void deal_with_broadcast_sig(int who, int cond_var_id)
{
  int i;
  i=0;
  while(i < con_queue_size)
  {
    if(con_queue[i].id == cond_var_id) {
      deal_with_unlock_sig(con_queue[i].mutex_id, who);
      rmv_from_con_queue(i);
    } else {
      i++;
    }
  }
}

/*===========================================================================*
 *        deal_with_unlock_sig             *
 *===========================================================================*/

PRIVATE void move_mut_queue(int index) {
  int i;
  for (i = index; i < mut_queue_size-1; ++i)
  {
    mut_queue[i] = mut_queue[i+1];
  }
  mut_queue_size--;
}

PRIVATE void unlock(int index, int mutex_id) 
{
  int i;
  locked_mutexes[index] = locked_mutexes[locked_mutexes_nmb-1];
  locked_mutexes_nmb--;
  /*sprawdz czy cos oczekuje na wlasnie zwolniony mutex*/
  for(i = 0; i < mut_queue_size; i++)
  {
    if(mut_queue[i].id == mutex_id) {
      lock(mut_queue[i].id, mut_queue[i].process);
      reply(mut_queue[i].process, 0);
      move_mut_queue(i);
      return;
    }
  }
}

PRIVATE void deal_with_unlock_sig(int mutex_id, int who)
{
  int index;
  struct mutex m;
  index = locked_nmb(mutex_id);
  if(index == -1)
  {
    reply(who, -1);
    return;
  }
  if(locked_mutexes[index].process != who)
  {
    reply(who, -1);
    return;
  }
  m = locked_mutexes[index];
  unlock(index, mutex_id);
  reply(who, 0);
}


/*===========================================================================*
 *        deal_with_exit_sig             *
 *===========================================================================*/

PRIVATE void deal_with_exit_sig(int who)
{
  int index;
  int i;
  struct mutex m;

  /* odblokuj mutexy tego procesu */
  i = 0;
  while (i < locked_mutexes_nmb)
  {
    if (locked_mutexes[i].process == who)
    {
      unlock(i, locked_mutexes[i].id);
    }
    else i++;
  }

  /* usun mutexy z kolejki*/
  i = 0;
  while (i < mut_queue_size)
  {
    if (mut_queue[i].process == who)
    {
      move_mut_queue(i);
    }
    else i++;
  }

  /* ten proces juz nie czeka na nic */
  i = 0;
  while (i < con_queue_size)
  {
    if (con_queue[i].process == who)
    {
      rmv_from_con_queue(i);
    }
    else i++;
  }
}

