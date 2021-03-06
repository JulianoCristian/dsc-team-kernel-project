/* arch/arm/mach-msm/clock.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2007-2011, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/pm_qos_params.h>
#include <linux/clk.h>

#include <asm/clkdev.h>

#include <mach/socinfo.h>

#include "clock.h"

static DEFINE_MUTEX(clocks_mutex);
static DEFINE_SPINLOCK(ebi1_vote_lock);
static LIST_HEAD(clocks);

static struct notifier_block axi_freq_notifier_block;

/*
 * Standard clock functions defined in include/linux/clk.h
 */
int clk_enable(struct clk *clk)
{
	return clk->ops->enable(clk->id);
}
EXPORT_SYMBOL(clk_enable);

void clk_disable(struct clk *clk)
{
	clk->ops->disable(clk->id);
}
EXPORT_SYMBOL(clk_disable);

int clk_reset(struct clk *clk, enum clk_reset_action action)
{
	int ret = -EPERM;

	/* Try clk->ops->reset() and fallback to a remote reset if it fails. */
	if (clk->ops->reset != NULL)
		ret = clk->ops->reset(clk->id, action);
	if (ret == -EPERM && clk_ops_remote.reset != NULL)
		ret = clk_ops_remote.reset(clk->remote_id, action);

	return ret;
}
EXPORT_SYMBOL(clk_reset);

unsigned long clk_get_rate(struct clk *clk)
{
	return clk->ops->get_rate(clk->id);
}
EXPORT_SYMBOL(clk_get_rate);

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	return clk->ops->set_rate(clk->id, rate);
}
EXPORT_SYMBOL(clk_set_rate);

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	return clk->ops->round_rate(clk->id, rate);
}
EXPORT_SYMBOL(clk_round_rate);

int clk_set_min_rate(struct clk *clk, unsigned long rate)
{
	return clk->ops->set_min_rate(clk->id, rate);
}
EXPORT_SYMBOL(clk_set_min_rate);

int clk_set_max_rate(struct clk *clk, unsigned long rate)
{
	return clk->ops->set_max_rate(clk->id, rate);
}
EXPORT_SYMBOL(clk_set_max_rate);

int clk_set_parent(struct clk *clk, struct clk *parent)
{
	if (clk->ops->set_parent)
		return clk->ops->set_parent(clk->id, parent);
	return -ENOSYS;
}
EXPORT_SYMBOL(clk_set_parent);

struct clk *clk_get_parent(struct clk *clk)
{
	return ERR_PTR(-ENOSYS);
}
EXPORT_SYMBOL(clk_get_parent);

int clk_set_flags(struct clk *clk, unsigned long flags)
{
	if (clk == NULL || IS_ERR(clk))
		return -EINVAL;
	return clk->ops->set_flags(clk->id, flags);
}
EXPORT_SYMBOL(clk_set_flags);

/* EBI1 is the only shared clock that several clients want to vote on as of
 * this commit. If this changes in the future, then it might be better to
 * make clk_min_rate handle the voting or make ebi1_clk_set_min_rate more
 * generic to support different clocks.
 */
static unsigned long ebi1_min_rate[CLKVOTE_MAX];
static struct clk *ebi1_clk;

/* Rate is in Hz to be consistent with the other clk APIs. */
int ebi1_clk_set_min_rate(enum clkvote_client client, unsigned long rate)
{
	static unsigned long last_set_val = -1;
	unsigned long new_val;
	unsigned long flags;
	int ret = 0, i;

	spin_lock_irqsave(&ebi1_vote_lock, flags);

	ebi1_min_rate[client] = (rate == MSM_AXI_MAX_FREQ) ?
				(clk_get_max_axi_khz() * 1000) : rate;

	new_val = ebi1_min_rate[0];
	for (i = 1; i < CLKVOTE_MAX; i++)
		if (ebi1_min_rate[i] > new_val)
			new_val = ebi1_min_rate[i];

	/* This check is to save a proc_comm call. */
	if (last_set_val != new_val) {
		ret = clk_set_min_rate(ebi1_clk, new_val);
		if (ret < 0) {
			pr_err("Setting EBI1 min rate to %lu Hz failed!\n",
				new_val);
			pr_err("Last successful value was %lu Hz.\n",
				last_set_val);
		} else {
			last_set_val = new_val;
		}
	}

	spin_unlock_irqrestore(&ebi1_vote_lock, flags);

	return ret;
}

static int axi_freq_notifier_handler(struct notifier_block *block,
				unsigned long min_freq, void *v)
{
	/* convert min_freq from KHz to Hz, unless it's a magic value */
	if (min_freq != MSM_AXI_MAX_FREQ)
		min_freq *= 1000;

	switch (socinfo_get_msm_cpu()) {
	case MSM_CPU_7X30:
	case MSM_CPU_8X55:
		/* On 7x30/8x55, ebi1_clk votes are dropped during power
		 * collapse, but pbus_clk votes are not. Use pbus_clk to
		 * implicitly request ebi1 and AXI rates. */
		return clk_set_min_rate(ebi1_clk, min_freq);
	case MSM_CPU_8X60:
		/* The bus driver handles ebi1_clk requests on 8x60. */
		return 0;
	default:
		/* Update pm_qos vote for ebi1_clk. */
		return ebi1_clk_set_min_rate(CLKVOTE_PMQOS, min_freq);
	}
}
//patthan+
/*
 * Find the clock matching the given id and copy its name to the
 * provided buffer.
 *
 * Return value:
 * -ENODEV: there is no clock matching the given id
 *       0: success
 */
int msm_clock_get_name(uint32_t id, char *name, uint32_t size)
{
        struct clk *c_clk;
        int ret = -ENODEV;

        mutex_lock(&clocks_mutex);
        list_for_each_entry(c_clk, &clocks, list) {
                if (id == c_clk->id) {
                        strlcpy(name, c_clk->dbg_name, size);
                        ret = 0;
                        break;
                }
        }
        mutex_unlock(&clocks_mutex);

        return ret;
}
//patthan-

void __init msm_clock_init(struct clk_lookup *clock_tbl, unsigned num_clocks)
{
	unsigned n;
	struct clk *clk;

	/* Do SoC-speficic clock init operations. */
	msm_clk_soc_init();

	mutex_lock(&clocks_mutex);
	for (n = 0; n < num_clocks; n++) {
		msm_clk_soc_set_ops(clock_tbl[n].clk);
		clkdev_add(&clock_tbl[n]);
		list_add_tail(&clock_tbl[n].clk->list, &clocks);
	}
	mutex_unlock(&clocks_mutex);

	for (n = 0; n < num_clocks; n++) {
		clk = clock_tbl[n].clk;
		if (clk->flags & CLKFLAG_VOTER) {
			struct clk *agg_clk = clk_get(NULL, clk->aggregator);
			BUG_ON(IS_ERR(agg_clk));

			clk_set_parent(clk, agg_clk);
		}
	}

	ebi1_clk = clk_get(NULL, "ebi1_pm_qos_clk");
	if (!cpu_is_msm8x60()) {
		BUG_ON(IS_ERR(ebi1_clk));
		clk_enable(ebi1_clk);
	}

	axi_freq_notifier_block.notifier_call = axi_freq_notifier_handler;
	pm_qos_add_notifier(PM_QOS_SYSTEM_BUS_FREQ, &axi_freq_notifier_block);
}

/* The bootloader and/or AMSS may have left various clocks enabled.
 * Disable any clocks that belong to us (CLKFLAG_AUTO_OFF) but have
 * not been explicitly enabled by a clk_enable() call.
 */
static int __init clock_late_init(void)
{
	struct clk *clk;

	clock_debug_init(&clocks);
	mutex_lock(&clocks_mutex);
	list_for_each_entry(clk, &clocks, list) {
		clock_debug_add(clk);
		if (clk->flags & CLKFLAG_AUTO_OFF)
			clk->ops->auto_off(clk->id);
	}
	mutex_unlock(&clocks_mutex);
	return 0;
}
late_initcall(clock_late_init);
