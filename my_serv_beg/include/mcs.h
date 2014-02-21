#ifndef _MCS_H
#define _MCS_H

#include <lib.h>

/* Function Prototypes. */
_PROTOTYPE( int mcs_lock, (int mutex_id)				);
_PROTOTYPE( int mcs_unlock, (int mutex_id)				);
_PROTOTYPE( int mcs_wait, (int con_var_id, int mutex_id));
_PROTOTYPE( int mcs_broadcast, (int con_var_id)			);

#endif