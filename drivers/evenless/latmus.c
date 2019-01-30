/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * Derived from Xenomai Cobalt's autotune driver
 * (http://git.xenomai.org/xenomai-3.git/)
 * Copyright (C) 2014, 2018 Philippe Gerum  <rpm@xenomai.org>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sort.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <evenless/file.h>
#include <evenless/flag.h>
#include <evenless/clock.h>
#include <evenless/thread.h>
#include <evenless/xbuf.h>
#include <uapi/evenless/devices/latmus.h>
#include <trace/events/evenless.h>

#define TUNER_SAMPLING_TIME	500000000UL
#define TUNER_WARMUP_STEPS	10
#define TUNER_RESULT_STEPS	40

#define progress(__runner, __fmt, __args...)				\
	do {								\
		if ((__runner)->verbosity > 1)				\
			printk(EVL_INFO "latmus(%s) " __fmt "\n",	\
			       (__runner)->name, ##__args);		\
	} while (0)

struct tuning_score {
	int pmean;
	int stddev;
	int minlat;
	unsigned int step;
	unsigned int gravity;
};

struct runner_state {
	ktime_t ideal;
	int min_lat;
	int max_lat;
	int allmax_lat;
	int prev_mean;
	int prev_sqs;
	int cur_sqs;
	unsigned int sum;
	unsigned int overruns;
	unsigned int cur_samples;
	unsigned int max_samples;
};

struct latmus_runner {
	const char *name;
	unsigned int (*get_gravity)(struct latmus_runner *runner);
	void (*set_gravity)(struct latmus_runner *runner, unsigned int gravity);
	unsigned int (*adjust_gravity)(struct latmus_runner *runner, int adjust);
	int (*start)(struct latmus_runner *runner, ktime_t start_time);
	void (*destroy)(struct latmus_runner *runner);
	int (*add_sample)(struct latmus_runner *runner, ktime_t timestamp);
	int (*run)(struct latmus_runner *runner, struct latmus_result *result);
	void (*cleanup)(struct latmus_runner *runner);
	struct runner_state state;
	struct evl_flag done;
	int status;
	int verbosity;
	ktime_t period;
	union {
		struct {
			struct tuning_score scores[TUNER_RESULT_STEPS];
			int nscores;
		};
		struct {
			unsigned int warmup_samples;
			unsigned int warmup_limit;
			int xfd;
			struct evl_xbuf *xbuf;
			u32 hcells;
			s32 *histogram;
		};
	};
};

struct irq_runner {
	struct evl_timer timer;
	struct latmus_runner runner;
};

struct kthread_runner {
	struct evl_kthread kthread;
	struct evl_flag barrier;
	ktime_t start_time;
	struct latmus_runner runner;
};

struct uthread_runner {
	struct evl_timer timer;
	struct evl_flag pulse;
	struct latmus_runner runner;
};

struct latmus_state {
	struct evl_file efile;
	struct latmus_runner *runner;
};

static inline void init_runner_base(struct latmus_runner *runner)
{
	evl_init_flag(&runner->done);
	runner->status = 0;
}

static inline void destroy_runner_base(struct latmus_runner *runner)
{
	evl_destroy_flag(&runner->done);
	if (runner->cleanup)
		runner->cleanup(runner);
}

static inline void done_sampling(struct latmus_runner *runner,
				 int status)
{
	runner->status = status;
	evl_raise_flag(&runner->done);
}

static void send_measurement(struct latmus_runner *runner)
{
	struct runner_state *state = &runner->state;
	struct latmus_measurement meas;

	meas.min_lat = state->min_lat;
	meas.max_lat = state->max_lat;
	meas.sum_lat = state->sum;
	meas.overruns = state->overruns;
	meas.samples = state->cur_samples;
	evl_write_xbuf(runner->xbuf, &meas, sizeof(meas), O_NONBLOCK);

	/* Reset counters for next round. */
	state->min_lat = ktime_to_ns(runner->period);
	state->max_lat = 0;
	state->sum = 0;
	state->overruns = 0;
	state->cur_samples = 0;
}

static int add_measurement_sample(struct latmus_runner *runner,
				  ktime_t timestamp)
{
	struct runner_state *state = &runner->state;
	ktime_t period = runner->period;
	int delta, cell;

	/* Skip samples in warmup time. */
	if (runner->warmup_samples < runner->warmup_limit) {
		runner->warmup_samples++;
		state->ideal = ktime_add(state->ideal, period);
		return 0;
	}

	delta = (int)ktime_to_ns(ktime_sub(timestamp, state->ideal));
	if (delta < state->min_lat)
		state->min_lat = delta;
	if (delta > state->max_lat) {
		state->max_lat = delta;
		if (delta > state->allmax_lat) {
			state->allmax_lat = delta;
			trace_evl_latspot(delta);
			trace_evl_trigger("latmus");
		}
	}

	if (runner->histogram) {
		cell = (delta < 0 ? -delta : delta) / 1000; /* us */
		if (cell >= runner->hcells)
			cell = runner->hcells - 1;
		runner->histogram[cell]++;
	}

	state->sum += delta;
	state->ideal = ktime_add(state->ideal, period);

	while (delta > ktime_to_ns(period)) { /* period > 0 */
		state->overruns++;
		state->ideal = ktime_add(state->ideal, period);
		delta -= ktime_to_ns(period);
	}

	if (++state->cur_samples >= state->max_samples)
		send_measurement(runner);

	return 0;	/* Always keep going. */
}

static int add_tuning_sample(struct latmus_runner *runner,
			     ktime_t timestamp)
{
	struct runner_state *state = &runner->state;
	int n, delta, cur_mean;

	delta = (int)ktime_to_ns(ktime_sub(timestamp, state->ideal));
	if (delta < state->min_lat)
		state->min_lat = delta;
	if (delta > state->max_lat)
		state->max_lat = delta;
	if (delta < 0)
		delta = 0;

	state->sum += delta;
	state->ideal = ktime_add(state->ideal, runner->period);
	n = ++state->cur_samples;

	/* TAOCP (Vol 2), single-pass computation of variance. */
	if (n == 1)
		state->prev_mean = delta;
	else {
		cur_mean = state->prev_mean + (delta - state->prev_mean) / n;
                state->cur_sqs = state->prev_sqs + (delta - state->prev_mean)
			* (delta - cur_mean);
                state->prev_mean = cur_mean;
                state->prev_sqs = state->cur_sqs;
	}

	if (n >= state->max_samples) {
		done_sampling(runner, 0);
		return 1;	/* Finished. */
	}

	return 0;	/* Keep going. */
}

static void latmus_irq_handler(struct evl_timer *timer) /* hard irqs off */
{
	struct irq_runner *irq_runner;
	ktime_t now;

	irq_runner = container_of(timer, struct irq_runner, timer);
	now = evl_read_clock(&evl_mono_clock);

	xnlock_get(&nklock);

	if (irq_runner->runner.add_sample(&irq_runner->runner, now))
		evl_stop_timer(timer);

	xnlock_put(&nklock);
}

static void destroy_irq_runner(struct latmus_runner *runner)
{
	struct irq_runner *irq_runner;

	irq_runner = container_of(runner, struct irq_runner, runner);
	evl_destroy_timer(&irq_runner->timer);
	destroy_runner_base(runner);
	kfree(irq_runner);
}

static unsigned int get_irq_gravity(struct latmus_runner *runner)
{
	return evl_mono_clock.gravity.irq;
}

static void set_irq_gravity(struct latmus_runner *runner, unsigned int gravity)
{
	evl_mono_clock.gravity.irq = gravity;
}

static unsigned int adjust_irq_gravity(struct latmus_runner *runner, int adjust)
{
	return evl_mono_clock.gravity.irq += adjust;
}

static int start_irq_runner(struct latmus_runner *runner,
			    ktime_t start_time)
{
	struct irq_runner *irq_runner;

	irq_runner = container_of(runner, struct irq_runner, runner);

	evl_start_timer(&irq_runner->timer, start_time, runner->period);

	return 0;
}

static struct latmus_runner *create_irq_runner(int cpu)
{
	struct irq_runner *irq_runner;

	irq_runner = kzalloc(sizeof(*irq_runner), GFP_KERNEL);
	if (irq_runner == NULL)
		return NULL;

	irq_runner->runner = (struct latmus_runner){
		.name = "irqhand",
		.destroy = destroy_irq_runner,
		.get_gravity = get_irq_gravity,
		.set_gravity = set_irq_gravity,
		.adjust_gravity = adjust_irq_gravity,
		.start = start_irq_runner,
	};

	init_runner_base(&irq_runner->runner);
	evl_init_timer_on_cpu(&irq_runner->timer, cpu, latmus_irq_handler);

	return &irq_runner->runner;
}

void kthread_handler(struct evl_kthread *kthread)
{
	struct kthread_runner *k_runner;
	ktime_t now;
	int ret = 0;

	k_runner = container_of(kthread, struct kthread_runner, kthread);

	for (;;) {
		if (evl_kthread_should_stop())
			break;

		ret = evl_wait_flag(&k_runner->barrier);
		if (ret)
			break;

		ret = evl_set_thread_period(&evl_mono_clock,
					    k_runner->start_time,
					    k_runner->runner.period);
		if (ret)
			break;

		for (;;) {
			ret = evl_wait_thread_period(NULL);
			if (ret && ret != -ETIMEDOUT)
				goto out;

			now = evl_read_clock(&evl_mono_clock);
			if (k_runner->runner.add_sample(&k_runner->runner, now)) {
				evl_set_thread_period(NULL, 0, 0);
				break;
			}
		}
	}
out:
	done_sampling(&k_runner->runner, ret);
	evl_cancel_kthread(&k_runner->kthread);
}

static void destroy_kthread_runner(struct latmus_runner *runner)
{
	struct kthread_runner *k_runner;

	k_runner = container_of(runner, struct kthread_runner, runner);
	evl_cancel_kthread(&k_runner->kthread);
	evl_destroy_flag(&k_runner->barrier);
	destroy_runner_base(runner);
	kfree(k_runner);
}

static unsigned int get_kthread_gravity(struct latmus_runner *runner)
{
	return evl_mono_clock.gravity.kernel;
}

static void
set_kthread_gravity(struct latmus_runner *runner, unsigned int gravity)
{
	evl_mono_clock.gravity.kernel = gravity;
}

static unsigned int
adjust_kthread_gravity(struct latmus_runner *runner, int adjust)
{
	return evl_mono_clock.gravity.kernel += adjust;
}

static int start_kthread_runner(struct latmus_runner *runner,
				ktime_t start_time)
{
	struct kthread_runner *k_runner;

	k_runner = container_of(runner, struct kthread_runner, runner);

	k_runner->start_time = start_time;
	evl_raise_flag(&k_runner->barrier);

	return 0;
}

static struct latmus_runner *
create_kthread_runner(int priority, int cpu)
{
	struct kthread_runner *k_runner;
	int ret;

	k_runner = kzalloc(sizeof(*k_runner), GFP_KERNEL);
	if (k_runner == NULL)
		return NULL;

	k_runner->runner = (struct latmus_runner){
		.name = "kthread",
		.destroy = destroy_kthread_runner,
		.get_gravity = get_kthread_gravity,
		.set_gravity = set_kthread_gravity,
		.adjust_gravity = adjust_kthread_gravity,
		.start = start_kthread_runner,
	};

	init_runner_base(&k_runner->runner);
	evl_init_flag(&k_runner->barrier);

	ret = evl_run_kthread_on_cpu(&k_runner->kthread, cpu,
				     kthread_handler,
				     priority, "latmus-klat:%d",
				     task_pid_nr(current));
	if (ret) {
		kfree(k_runner);
		return ERR_PTR(ret);
	}

	return &k_runner->runner;
}

static void latmus_pulse_handler(struct evl_timer *timer)
{
	struct uthread_runner *u_runner;

	u_runner = container_of(timer, struct uthread_runner, timer);
	evl_raise_flag(&u_runner->pulse);
}

static void destroy_uthread_runner(struct latmus_runner *runner)
{
	struct uthread_runner *u_runner;

	u_runner = container_of(runner, struct uthread_runner, runner);
	evl_destroy_timer(&u_runner->timer);
	evl_destroy_flag(&u_runner->pulse);
	destroy_runner_base(runner);
	kfree(u_runner);
}

static unsigned int get_uthread_gravity(struct latmus_runner *runner)
{
	return evl_mono_clock.gravity.user;
}

static void set_uthread_gravity(struct latmus_runner *runner,
				unsigned int gravity)
{
	evl_mono_clock.gravity.user = gravity;
}

static unsigned int
adjust_uthread_gravity(struct latmus_runner *runner, int adjust)
{
	return evl_mono_clock.gravity.user += adjust;
}

static int start_uthread_runner(struct latmus_runner *runner,
				ktime_t start_time)
{
	struct uthread_runner *u_runner;

	u_runner = container_of(runner, struct uthread_runner, runner);

	evl_start_timer(&u_runner->timer, start_time, runner->period);

	return 0;
}

static int add_uthread_sample(struct latmus_runner *runner,
			      ktime_t user_timestamp)
{
	struct uthread_runner *u_runner;
	int ret;

	u_runner = container_of(runner, struct uthread_runner, runner);

	if (user_timestamp &&
	    u_runner->runner.add_sample(runner, user_timestamp)) {
		evl_stop_timer(&u_runner->timer);
		/* Tell the caller to park until next round. */
		ret = -EPIPE;
	} else
		ret = evl_wait_flag(&u_runner->pulse);

	return ret;
}

static struct latmus_runner *create_uthread_runner(int cpu)
{
	struct uthread_runner *u_runner;

	u_runner = kzalloc(sizeof(*u_runner), GFP_KERNEL);
	if (u_runner == NULL)
		return NULL;

	u_runner->runner = (struct latmus_runner){
		.name = "uthread",
		.destroy = destroy_uthread_runner,
		.get_gravity = get_uthread_gravity,
		.set_gravity = set_uthread_gravity,
		.adjust_gravity = adjust_uthread_gravity,
		.start = start_uthread_runner,
	};

	init_runner_base(&u_runner->runner);
	evl_init_timer_on_cpu(&u_runner->timer, cpu, latmus_pulse_handler);
	evl_set_timer_gravity(&u_runner->timer, EVL_TIMER_UGRAVITY);
	evl_init_flag(&u_runner->pulse);

	return &u_runner->runner;
}

static inline void build_score(struct latmus_runner *runner, int step)
{
	struct runner_state *state = &runner->state;
	unsigned int variance, n;

	n = state->cur_samples;
	runner->scores[step].pmean = state->sum / n;
	variance = n > 1 ? state->cur_sqs / (n - 1) : 0;
	runner->scores[step].stddev = int_sqrt(variance);
	runner->scores[step].minlat = state->min_lat;
	runner->scores[step].gravity = runner->get_gravity(runner);
	runner->scores[step].step = step;
	runner->nscores++;
}

static int cmp_score_mean(const void *c, const void *r)
{
	const struct tuning_score *sc = c, *sr = r;
	return sc->pmean - sr->pmean;
}

static int cmp_score_stddev(const void *c, const void *r)
{
	const struct tuning_score *sc = c, *sr = r;
	return sc->stddev - sr->stddev;
}

static int cmp_score_minlat(const void *c, const void *r)
{
	const struct tuning_score *sc = c, *sr = r;
	return sc->minlat - sr->minlat;
}

static int cmp_score_gravity(const void *c, const void *r)
{
	const struct tuning_score *sc = c, *sr = r;
	return sc->gravity - sr->gravity;
}

static int filter_mean(struct latmus_runner *runner)
{
	sort(runner->scores, runner->nscores,
	     sizeof(struct tuning_score),
	     cmp_score_mean, NULL);

	/* Top half of the best pondered means. */

	return (runner->nscores + 1) / 2;
}

static int filter_stddev(struct latmus_runner *runner)
{
	sort(runner->scores, runner->nscores,
	     sizeof(struct tuning_score),
	     cmp_score_stddev, NULL);

	/* Top half of the best standard deviations. */

	return (runner->nscores + 1) / 2;
}

static int filter_minlat(struct latmus_runner *runner)
{
	sort(runner->scores, runner->nscores,
	     sizeof(struct tuning_score),
	     cmp_score_minlat, NULL);

	/* Top half of the minimum latencies. */

	return (runner->nscores + 1) / 2;
}

static int filter_gravity(struct latmus_runner *runner)
{
	sort(runner->scores, runner->nscores,
	     sizeof(struct tuning_score),
	     cmp_score_gravity, NULL);

	/* Smallest gravity required among the shortest latencies. */

	return runner->nscores;
}

static void dump_scores(struct latmus_runner *runner)
{
	int n;

	if (runner->verbosity < 2)
		return;

	for (n = 0; n < runner->nscores; n++)
		printk(EVL_INFO
		       ".. S%.2d pmean=%d stddev=%u minlat=%u gravity=%u\n",
		       runner->scores[n].step,
		       runner->scores[n].pmean,
		       runner->scores[n].stddev,
		       runner->scores[n].minlat,
		       runner->scores[n].gravity);
}

static inline void filter_score(struct latmus_runner *runner,
				int (*filter)(struct latmus_runner *runner))
{
	runner->nscores = filter(runner);
	dump_scores(runner);
}

static int measure_continously(struct latmus_runner *runner)
{
	struct runner_state *state = &runner->state;
	ktime_t period = runner->period;
	struct evl_file *sfilp;
	struct evl_xbuf *xbuf;
	int ret;

	/*
	 * Get a reference on the cross-buffer we should use to send
	 * interval results to userland. This may delay the disposal
	 * of such element when the last in-band file reference is
	 * dropped until we are done with OOB operations
	 * (evl_put_xbuf).
	 */
	xbuf = evl_get_xbuf(runner->xfd, &sfilp);
	if (xbuf == NULL)
		return -EBADF;	/* muhh? */

	state->max_samples = ONE_BILLION / (int)ktime_to_ns(period);
	runner->add_sample = add_measurement_sample;
	runner->xbuf = xbuf;
	state->min_lat = ktime_to_ns(period);
	state->max_lat = 0;
	state->allmax_lat = 0;
	state->sum = 0;
	state->overruns = 0;
	state->cur_samples = 0;
	state->ideal = ktime_add(evl_read_clock(&evl_mono_clock), period);

	ret = runner->start(runner, state->ideal);
	if (ret)
		goto out;

	ret = evl_wait_flag(&runner->done) ?: runner->status;
out:
	evl_put_xbuf(sfilp);

	return ret;
}

static int tune_gravity(struct latmus_runner *runner)
{
	struct runner_state *state = &runner->state;
	int ret, step, gravity_limit, adjust;
	ktime_t period = runner->period;
	unsigned int orig_gravity;

	state->max_samples = TUNER_SAMPLING_TIME / (int)ktime_to_ns(period);
	orig_gravity = runner->get_gravity(runner);
	runner->add_sample = add_tuning_sample;
	runner->set_gravity(runner, 0);
	runner->nscores = 0;
	adjust = 500; /* Gravity adjustment step */
	gravity_limit = 0;
	progress(runner, "warming up...");

	for (step = 0; step < TUNER_WARMUP_STEPS + TUNER_RESULT_STEPS; step++) {
		state->ideal = ktime_add_ns(evl_read_clock(&evl_mono_clock),
			    ktime_to_ns(period) * TUNER_WARMUP_STEPS);
		state->min_lat = ktime_to_ns(period);
		state->max_lat = 0;
		state->prev_mean = 0;
		state->prev_sqs = 0;
		state->cur_sqs = 0;
		state->sum = 0;
		state->cur_samples = 0;

		ret = runner->start(runner, state->ideal);
		if (ret)
			goto fail;

		/* Runner stops when posting. */
		ret = evl_wait_flag(&runner->done);
		if (ret)
			goto fail;

		ret = runner->status;
		if (ret)
			goto fail;

		if (step < TUNER_WARMUP_STEPS) {
			if (state->min_lat > gravity_limit) {
				gravity_limit = state->min_lat;
				progress(runner, "gravity limit set to %u ns (%d)",
					 gravity_limit, state->min_lat);
			}
			continue;
		}

		if (state->min_lat < 0) {
			if (runner->get_gravity(runner) < -state->min_lat) {
				printk(EVL_WARNING
				       "latmus(%s) failed with early shot (%d ns)\n",
				       runner->name,
				       -(runner->get_gravity(runner) + state->min_lat));
				ret = -EAGAIN;
				goto fail;
			}
			break;
		}

		if (((step - TUNER_WARMUP_STEPS) % 5) == 0)
			progress(runner, "calibrating... (slice %d)",
				 (step - TUNER_WARMUP_STEPS) / 5 + 1);

		build_score(runner, step - TUNER_WARMUP_STEPS);

		/*
		 * Anticipating by more than the minimum latency
		 * detected at warmup would make no sense: cap the
		 * gravity we may try.
		 */
		if (runner->adjust_gravity(runner, adjust) > gravity_limit) {
			progress(runner, "beyond gravity limit at %u ns",
				 runner->get_gravity(runner));
			break;
		}
	}

	progress(runner, "calibration scores");
	dump_scores(runner);
	progress(runner, "pondered mean filter");
	filter_score(runner, filter_mean);
	progress(runner, "standard deviation filter");
	filter_score(runner, filter_stddev);
	progress(runner, "minimum latency filter");
	filter_score(runner, filter_minlat);
	progress(runner, "gravity filter");
	filter_score(runner, filter_gravity);
	runner->set_gravity(runner, runner->scores[0].gravity);

	return 0;
fail:
	runner->set_gravity(runner, orig_gravity);

	return ret;
}

static int setup_tuning(struct latmus_runner *runner,
			struct latmus_setup *setup)
{
	runner->verbosity = setup->u.tune.verbosity;
	runner->period = setup->period;

	return 0;
}

static int run_tuning(struct latmus_runner *runner,
		      struct latmus_result *result)
{
	__u32 gravity;
	int ret;

	ret = tune_gravity(runner);
	if (ret)
		return ret;

	gravity = runner->get_gravity(runner);

	if (raw_copy_to_user(result->data, &gravity, sizeof(gravity)))
		return -EFAULT;

	return 0;
}

static int setup_measurement(struct latmus_runner *runner,
			     struct latmus_setup *setup)
{
	runner->period = setup->period;
	runner->warmup_limit = ONE_BILLION / (int)ktime_to_ns(setup->period); /* 1s warmup */
	runner->xfd = setup->u.measure.xfd;
	runner->histogram = NULL;
	runner->hcells = setup->u.measure.hcells;
	if (runner->hcells == 0)
		return 0;

	if (runner->hcells > 1000) /* LART */
		return -EINVAL;

	runner->histogram = kzalloc(runner->hcells * sizeof(s32),
				    GFP_KERNEL);

	return runner->histogram ? 0 : -ENOMEM;
}

static int run_measurement(struct latmus_runner *runner,
			   struct latmus_result *result)
{
	size_t len;
	int ret;

	ret = measure_continously(runner);
	if (ret != -EINTR)
		return ret;

	/* Copy distribution data back to userland. */
	if (runner->histogram) {
		len = runner->hcells * sizeof(s32);
		if (len > result->len)
			len = result->len;
		if (len > 0 &&
		    raw_copy_to_user(result->data, runner->histogram, len))
			return -EFAULT;
	}

	return 0;
}

static void cleanup_measurement(struct latmus_runner *runner)
{
	if (runner->histogram)
		kfree(runner->histogram);
}

static long latmus_ioctl(struct file *filp, unsigned int cmd,
			 unsigned long arg)
{
	struct latmus_state *ls = filp->private_data;
	int (*setup)(struct latmus_runner *runner,
		     struct latmus_setup *setup_data);
	int (*run)(struct latmus_runner *runner,
		   struct latmus_result *result);
	void (*cleanup)(struct latmus_runner *runner);
	struct latmus_setup setup_data;
	struct latmus_runner *runner;
	int ret;

	switch (cmd) {
	case EVL_LATIOC_RESET:
		evl_reset_clock_gravity(&evl_mono_clock);
		return 0;
	case EVL_LATIOC_TUNE:
		setup = setup_tuning;
		run = run_tuning;
		cleanup = NULL;
		break;
	case EVL_LATIOC_MEASURE:
		setup = setup_measurement;
		run = run_measurement;
		cleanup = cleanup_measurement;
		break;
	default:
		return -ENOTTY;
	}

	if (copy_from_user(&setup_data, (struct latmus_setup __user *)arg,
			   sizeof(setup_data)))
		return -EFAULT;

	if (setup_data.period <= 0 ||
	    setup_data.period > ONE_BILLION)
		return -EINVAL;

	if (setup_data.priority < 1 ||
	    setup_data.priority > EVL_FIFO_MAX_PRIO)
		return -EINVAL;

	/* Clear previous runner. */
	runner = ls->runner;
	if (runner) {
		runner->destroy(runner);
		ls->runner = NULL;
	}

	switch (setup_data.type) {
	case EVL_LAT_IRQ:
		runner = create_irq_runner(setup_data.cpu);
		break;
	case EVL_LAT_KERN:
		runner = create_kthread_runner(setup_data.priority,
					       setup_data.cpu);
		break;
	case EVL_LAT_USER:
		runner = create_uthread_runner(setup_data.cpu);
		break;
	default:
		return -EINVAL;
	}

	if (IS_ERR(runner))
		return PTR_ERR(runner);

	ret = setup(runner, &setup_data);
	if (ret) {
		runner->destroy(runner);
		return ret;
	}

	runner->run = run;
	runner->cleanup = cleanup;
	ls->runner = runner;

	return 0;
}

static long latmus_oob_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	struct latmus_state *ls = filp->private_data;
	struct latmus_runner *runner;
	struct latmus_result result;
	__u64 timestamp;
	int ret;

	runner = ls->runner;
	if (runner == NULL)
		return -EAGAIN;

	switch (cmd) {
	case EVL_LATIOC_RUN:
		ret = raw_copy_from_user(&result,
				 (struct latmus_result __user *)arg,
				 sizeof(result));
		if (ret)
			return -EFAULT;
		ret = runner->run(runner, &result);
		break;
	case EVL_LATIOC_PULSE:
		if (runner->start != start_uthread_runner)
			return -EINVAL;
		ret = raw_copy_from_user(&timestamp, (__u64 __user *)arg,
					 sizeof(timestamp));
		if (ret)
			return -EFAULT;
		ret = add_uthread_sample(runner, ns_to_ktime(timestamp));
		break;
	default:
		ret = -ENOTTY;
	}

	return ret;
}

static int latmus_open(struct inode *inode, struct file *filp)
{
	struct latmus_state *ls;
	int ret;

	ls = kzalloc(sizeof(*ls), GFP_KERNEL);
	if (ls == NULL)
		return -ENOMEM;

	ret = evl_open_file(&ls->efile, filp);
	if (ret)
		kfree(ls);

	filp->private_data = ls;

	return ret;
}

static int latmus_release(struct inode *inode, struct file *filp)
{
	struct latmus_state *ls = filp->private_data;
	struct latmus_runner *runner;

	runner = ls->runner;
	if (runner)
		runner->destroy(runner);

	evl_release_file(&ls->efile);
	kfree(ls);

	return 0;
}

static struct class latmus_class = {
	.name = "latmus",
	.owner = THIS_MODULE,
};

static const struct file_operations latmus_fops = {
	.open		= latmus_open,
	.release	= latmus_release,
	.unlocked_ioctl	= latmus_ioctl,
	.oob_ioctl	= latmus_oob_ioctl,
};

static dev_t latmus_devt;

static struct cdev latmus_cdev;

static int __init latmus_init(void)
{
	struct device *dev;
	int ret;

	ret = class_register(&latmus_class);
	if (ret)
		return ret;

	ret = alloc_chrdev_region(&latmus_devt, 0, 1, "latmus");
	if (ret)
		goto fail_region;

	cdev_init(&latmus_cdev, &latmus_fops);
	ret = cdev_add(&latmus_cdev, latmus_devt, 1);
	if (ret)
		goto fail_add;

	dev = device_create(&latmus_class, NULL, latmus_devt, NULL, "latmus");
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		goto fail_dev;
	}

	return 0;

fail_dev:
	cdev_del(&latmus_cdev);
fail_add:
	unregister_chrdev_region(latmus_devt, 1);
fail_region:
	class_unregister(&latmus_class);

	return ret;
}
module_init(latmus_init);

static void __exit latmus_exit(void)
{
	device_destroy(&latmus_class, MKDEV(MAJOR(latmus_devt), 0));
	cdev_del(&latmus_cdev);
	class_unregister(&latmus_class);
}
module_exit(latmus_exit);

MODULE_LICENSE("GPL");
