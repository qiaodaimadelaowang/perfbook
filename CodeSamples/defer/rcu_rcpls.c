/*
 * rcu_rcpls.c: simple user-level implementation of RCU based on per-thread
 * pairs of global reference counters, but that is also capable of sharing
 * grace periods between multiple updates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (c) 2008 Paul E. McKenney, IBM Corporation.
 */

#include "../api.h"
#include "rcu_rcpls.h"

static void flip_counter_and_wait(int ctr)
{
	int i;
	int t;

	ACCESS_ONCE(rcu_idx) = ctr + 1;
	i = ctr & 0x1;
	smp_mb();
	for_each_thread(t) {
		while (per_thread(rcu_refcnt, t)[i] != 0) {
			/* @@@ poll(NULL, 0, 10); */
			barrier();
		}
	}
	smp_mb();
}

void synchronize_rcu(void)
{
	int ctr;
	int oldctr;

	smp_mb();
	oldctr = ACCESS_ONCE(rcu_idx);
	smp_mb();
	spin_lock(&rcu_gp_lock);
	ctr = ACCESS_ONCE(rcu_idx);
	if (ctr - oldctr >= 3) {

		/*
		 * There have been at least two full cycles, so
		 * all pre-existing RCU read-side critical sections
		 * must have completed.  Our work is done!
		 */

		spin_unlock(&rcu_gp_lock);
		smp_mb();
		return;
	}

	/*
	 * Flip counter once and wait for old counts to go away,
	 * but someone might have been preempted while we waited, so
	 * we must flip and wait twice.  Unless a pair of flips happened
	 * while we were acquiring the lock...
	 */

	flip_counter_and_wait(ctr);
	if (ctr - oldctr < 2)
		flip_counter_and_wait(ctr + 1);

	spin_unlock(&rcu_gp_lock);
	smp_mb();
}

#ifdef TEST
#include "rcutorture.h"
#endif /* #ifdef TEST */
