#include "mcs.h"

#define MUT1 m_u.m_m1.m1i1
#define COND1 m_u.m_m1.m1i2

int mcs_lock(mutex_id)
int mutex_id;
{
	int sysc_res;
	message m;
	m.MUT1 = mutex_id;
	sysc_res = _syscall(MCS_PROC_NR, MCS_LOCK, &m);
	if(sysc_res == -1) {
		errno = EINTR;
	}
	return sysc_res;
}

int mcs_unlock(mutex_id)
int mutex_id;
{
	int sysc_res;
	message m;
	m.MUT1 = mutex_id;
	sysc_res = _syscall(MCS_PROC_NR, MCS_UNLOCK, &m);
	if(sysc_res == -1) {
		errno = EPERM;
	}
	return sysc_res;	
}

int mcs_wait(con_var_id, mutex_id)
int con_var_id;
int mutex_id;
{
	int sysc_res;
	message m;
	m.MUT1 = mutex_id;
	m.COND1 = con_var_id;
	sysc_res = _syscall(MCS_PROC_NR, MCS_WAIT, &m);
	if(sysc_res == -1) {
		errno = EINVAL;
	}
	return sysc_res;
}

int mcs_broadcast(con_var_id)
int con_var_id;
{
	message m;
	m.COND1 = con_var_id;
	return _syscall(MCS_PROC_NR, MCS_BROADCAST, &m);
}