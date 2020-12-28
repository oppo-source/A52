/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#define DEBUG
#define LOG_FLAG	"sia8101_set_vdd"


#include <linux/slab.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <linux/time.h>
#include <linux/power_supply.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/gameport.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
//#include <linux/wakelock.h>
#include <linux/cdev.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/soc-dapm.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/cpufreq.h>
#include <linux/syscalls.h>
#include <linux/regmap.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/err.h>


#include "sia81xx_tuning_if.h"
#include "sia81xx_socket.h"
#include "sia81xx_set_vdd.h"

#define MAX_SET_VDD_INFO_NUM			(16)
#define RECV_WATI_TIME_MS				(10)

//#define V_OVER_S						(16 * 1000)
//#define S_MARGIN						(1 * 1000)

#define DEFAULT_MODULE_ID				(0x1000E900)
#define DEFAULT_PARAM_ID				(0x1000EA03)

#define CHANNEL_NUM						(2)
#define COMPONENT_ID					(50)

#define AUTO_SET_THREAD_CYCLE_TIME_MS	(500)//ms
#define AUTO_SET_INTERVAL_TIME_MS		(18 * 1000)//ms
#define AUTO_SET_FIRST_SET_SAMPLES		(3)
#define AUTO_SET_NORMAL_SET_SAMPLES		(3)

#define VDD_DEFAULT_VAL					(3300000)//3.3v
#define VDD_RIPPLE_INVALID_VAL			(0xFFFFFFFF);

static const uint32_t v_o_s[CHANNEL_NUM] = {
	[0] = (uint32_t)(10.6f * 1000.0f),
	[1] = (uint32_t)(10.6f * 1000.0f)
};

static const uint32_t s_margin[CHANNEL_NUM] = {
	[0] = 1 * 1000,
	[1] = 1 * 1000
};


typedef enum task_state_e {
	TASK_STATE_INIT = 0,
	TASK_STATE_RUN,
	TASK_STATE_CLOSING,
	TASK_STATE_CLOSED,
}task_state_t;


typedef struct sia81xx_set_vdd_info {
	unsigned long cal_handle;	//afe handle(qcom) or cal module unit(mtk)
	uint32_t cal_id; //afe port id(qcom) or task scene(mtk)
	atomic_t auto_set_flag;
	atomic_t task_state;
	struct task_struct *task;
	volatile struct timespec last_run_time;
	uint32_t vdd_val_pool[AUTO_SET_NORMAL_SET_SAMPLES];
	uint32_t vdd_val_pool_pos;
	uint32_t vdd_sample_cnt;
	uint32_t vdd_sample_send;

	uint32_t module_id;
	uint32_t param_id;
}SIA81XX_SET_VDD_INFO;

typedef struct sia81xx_vdd_msg {
	uint32_t vdd; //uv
	uint32_t v_o_s;//v_o_s / 1000
	uint32_t s_margin;//s_margin / 1000
	uint32_t ripple;//ripple / 1000
}SIA81XX_VDD_MSG;

typedef struct sia81xx_vdd_param {
	uint32_t proc_code;
	uint32_t id;
	uint32_t msg_len;
	SIA81XX_VDD_MSG msg;
}SIA81XX_VDD_PARAM;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4,11,12))
extern void release_task(struct task_struct * p);
extern struct timespec timespec_add_safe(const struct timespec lhs,
				 const struct timespec rhs);
#endif

static struct sia81xx_set_vdd_info info_table[MAX_SET_VDD_INFO_NUM];
//for test
static atomic_t open_flag;
static atomic_t switch_flag;
static struct mutex task_mutex;



static uint32_t sia81xx_read_cur_battery_voltage(void)
{
		union power_supply_propval val;
		struct power_supply *psy = power_supply_get_by_name("battery");
		uint32_t vdd_val;

		if(NULL == psy) {
			vdd_val = VDD_DEFAULT_VAL;
			goto end;
		}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0))
		if(NULL == psy->desc->get_property) {
			vdd_val = VDD_DEFAULT_VAL;
			goto end;
		}

		if(0 != psy->desc->get_property(
					psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val)) {
			vdd_val = VDD_DEFAULT_VAL;
			goto end;
		}
#else
		if(NULL == psy->get_property) {
			vdd_val = VDD_DEFAULT_VAL;
			goto end;
		}

		if(0 != psy->get_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &val)) {
			vdd_val = VDD_DEFAULT_VAL;
			goto end;
		}
#endif

	vdd_val = val.intval;

end :
	pr_debug("[debug][%s] %s: record current voltage = %u, open flag = %d \r\n",
		LOG_FLAG, __func__, (unsigned int)vdd_val, (int)atomic_read(&open_flag));

	return vdd_val;
}

static void sia81xx_record_cur_battery_voltage(
	struct sia81xx_set_vdd_info *info)
{
	info->vdd_val_pool_pos ++;
	if(info->vdd_val_pool_pos >= ARRAY_SIZE(info->vdd_val_pool))
		info->vdd_val_pool_pos = 0;

	info->vdd_val_pool[info->vdd_val_pool_pos] =
		sia81xx_read_cur_battery_voltage();

	return ;
}

static uint32_t sia81xx_get_battery_voltage(
	struct sia81xx_set_vdd_info *info, uint32_t samples)
{
	uint64_t ave_val = 0;
	uint32_t n = 0, pos = 0;
	int i = 0;

	if(NULL == info)
		return VDD_DEFAULT_VAL;

	n = samples < ARRAY_SIZE(info->vdd_val_pool) ?
		samples : ARRAY_SIZE(info->vdd_val_pool);
	pos = info->vdd_val_pool_pos;

	for(i = 0; i < n; i++) {
		ave_val += info->vdd_val_pool[pos];

		if(0 == pos)
			pos = ARRAY_SIZE(info->vdd_val_pool) - 1;
		else
			pos --;
	}

	ave_val = ave_val / n;

	pr_debug("[debug][%s] %s: average vdd = %llu, n = %u \r\n",
		LOG_FLAG, __func__, ave_val, (unsigned int)n);

	return (uint32_t)ave_val;
}

static struct sia81xx_set_vdd_info *is_cal_id_exist(
	uint32_t cal_id)
{
	int i = 0;

	for(i = 0; i < MAX_SET_VDD_INFO_NUM; i++) {
		if((cal_id == info_table[i].cal_id) &&
			(0 != info_table[i].cal_handle))
			return &info_table[i];
	}

	return NULL;
}

static struct sia81xx_set_vdd_info *get_one_can_use_info(
	uint32_t cal_id)
{
	struct sia81xx_set_vdd_info *info = NULL;
	int i = 0;

	if(NULL != (info = is_cal_id_exist(cal_id))) {
		return info;
	}

	for(i = 0; i < MAX_SET_VDD_INFO_NUM; i++) {
		if(0 == info_table[i].cal_handle)
			return &info_table[i];
	}

	return NULL;
}

static struct sia81xx_set_vdd_info *record_info(
	unsigned long cal_handle,
	uint32_t cal_id)
{
	struct sia81xx_set_vdd_info *info = NULL;

	info = get_one_can_use_info(cal_id);
	if(NULL == info)
		return NULL;

	info->cal_handle = cal_handle;
	info->cal_id = cal_id;

	return info;
}

static void delete_all_info(void)
{
	int i = 0;

	for(i = 0; i < MAX_SET_VDD_INFO_NUM; i++) {

		atomic_set(&info_table[i].auto_set_flag, 0);
		atomic_set(&info_table[i].task_state, TASK_STATE_INIT);

		if(0 != info_table[i].cal_handle) {
			if(0 == tuning_if_opt.close(info_table[i].cal_handle)) {
				info_table[i].cal_handle = 0;
				info_table[i].cal_id = 0;
			} else {
				pr_err("[  err][%s] %s: tuning_if_opt.close err, "
					"id = %d \r\n",
					LOG_FLAG, __func__, info_table[i].cal_id);
				continue;
			}
		}
	}
}

static void send_set_vdd_msg(
	struct sia81xx_set_vdd_info *info, uint32_t vdd)
{
	int ret = 0, i = 0;
	SIA81XX_VDD_PARAM param;

	if(NULL == tuning_if_opt.write) {
		pr_err("[  err][%s] %s: NULL == tuning_if_opt.opt.write \r\n",
			LOG_FLAG, __func__);
		return;
	}

	//param.msg.v_o_s = V_OVER_S;
	//param.msg.s_margin = S_MARGIN;
	param.msg.vdd = vdd;
	/* don't set ripple in driver, it should be setted at acdb and it's fixed */
	param.msg.ripple = VDD_RIPPLE_INVALID_VAL;

	param.id = COMPONENT_ID;//ID_VDD
	param.msg_len = sizeof(SIA81XX_VDD_PARAM);

	for(i = 0; i < CHANNEL_NUM; i++) {

		param.proc_code = i;// ch sn
		param.msg.v_o_s = v_o_s[i];
		param.msg.s_margin = s_margin[i];

		ret = tuning_if_opt.write(
			info->cal_handle,
			info->module_id,
			info->param_id,
			(uint32_t)sizeof(param),
			(uint8_t *)&param);

		if(0 > ret) {
			pr_err("[debug][%s] %s: tuning_if_opt.write failed "
				"ret = %d \r\n",
				LOG_FLAG, __func__, ret);
			return ;
		}
	}
}

static int sia81xx_auto_set_task(
	void *data)
{
	struct sia81xx_set_vdd_info *info = NULL;
	volatile int i = 0;
	//for test
	static volatile int entry_cnt = 0, entry_max = 0;

	if(NULL == data) {
		pr_err("[  err][%s] %s: NULL == data \r\n",
			LOG_FLAG, __func__);
		return -ECHILD;
	}

	info = data;
	atomic_set(&info->task_state, TASK_STATE_RUN);

	//for test
	entry_cnt ++;
	entry_max ++;

	while(TASK_STATE_RUN == atomic_read(&info->task_state)) {

		info->last_run_time = current_kernel_time();

		//for test
		mutex_lock(&task_mutex);
		if(0 == atomic_read(&open_flag)) {
			//for test
			entry_cnt --;
			atomic_set(&info->task_state, TASK_STATE_CLOSED);
			mutex_unlock(&task_mutex);
			return 0;
			//msleep(AUTO_SET_THREAD_CYCLE_TIME_MS);//check audio start per second
			//continue;
		}
		mutex_unlock(&task_mutex);

		if(0 == atomic_read(&info->auto_set_flag)) {
			msleep(AUTO_SET_THREAD_CYCLE_TIME_MS);//check audio start per second
			continue;
		}

		pr_debug("[debug][%s] %s: run33, cur time : %lld, entry_cnt = %d, max = %d \r\n",
			LOG_FLAG, __func__, (long long)info->last_run_time.tv_sec, entry_cnt, entry_max);

#if 0
		//send vdd value per second
		for (i = 0; i < AUTO_SET_NORMAL_SET_SAMPLES; i++) {
			sia81xx_record_cur_battery_voltage(info);

			msleep(AUTO_SET_THREAD_CYCLE_TIME_MS);

			info->vdd_sample_cnt ++;
			if(info->vdd_sample_cnt < info->vdd_sample_send)
				continue;

			/* warning : if a interruption occurrence by funtion
			 * "sia81xx_enable_auto_set_vdd()" running the option :
			 * "pInfo->vdd_sample_send = AUTO_SET_FIRST_SET_SAMPLES;",
			 * then this option will be lost because of the next option :
			 * "info->vdd_sample_send = AUTO_SET_NORMAL_SET_SAMPLES".
			 * In consideration of the problem will not lead to a serious
			 * consequences, so that's that */
			send_set_vdd_msg(
				info,
				sia81xx_get_battery_voltage(info, info->vdd_sample_cnt));

			info->vdd_sample_cnt = 0;
		}
#else
		atomic_set(&switch_flag, 0);

		for (i = 0; i < AUTO_SET_NORMAL_SET_SAMPLES; i++) {
			sia81xx_record_cur_battery_voltage(info);
			msleep(AUTO_SET_THREAD_CYCLE_TIME_MS);
		}

		if(1 == atomic_read(&switch_flag)) {
			atomic_set(&switch_flag, 0);
			continue;
		}

		if(1 == atomic_read(&open_flag)) {
			send_set_vdd_msg(
				info,
				sia81xx_get_battery_voltage(info, AUTO_SET_NORMAL_SET_SAMPLES));
		}
#endif

		//send vdd value per second
		//msleep(AUTO_SET_INTERVAL_TIME_MS); //AUTO_SET_THREAD_CYCLE_TIME_MS,17s
		for(i = 0;
			i < (AUTO_SET_INTERVAL_TIME_MS / AUTO_SET_THREAD_CYCLE_TIME_MS);
			i++) {

			atomic_set(&switch_flag, 0);

			msleep(AUTO_SET_THREAD_CYCLE_TIME_MS);

			if(1 == atomic_read(&switch_flag)) {
				atomic_set(&switch_flag, 0);
				break;
			}
		}

	}

	//for test
	entry_cnt --;

	atomic_set(&info->task_state, TASK_STATE_CLOSED);

	return 0;
}

static int sia81xx_open_auto_set_task(
	struct sia81xx_set_vdd_info *info)
{
	pr_debug("[debug][%s] %s: run \r\n", LOG_FLAG, __func__);

	if(NULL != info->task) {
		pr_err("[  err][%s] %s: NULL != info->task \r\n",
			LOG_FLAG, __func__);
		return -EFAULT;
	}
#if 0
	if((TASK_STATE_RUN == atomic_read(&info->task_state)) ||
		(TASK_STATE_CLOSING == atomic_read(&info->task_state)))
		return -EINVAL;
#endif
	info->task = kthread_create(
		sia81xx_auto_set_task, info, "sia81xx_auto_set_vdd");
	if(IS_ERR(info->task)) {
		pr_err("[  err][%s] %s: kthread_create fail, err code : %d \r\n",
			LOG_FLAG, __func__, PTR_ERR(info->task));
		return -ECHILD;
	}

	if(wake_up_process(info->task) < 0) {
		return -EFAULT;
	}

	return 0;
}

#if 0
static int sia81xx_close_auto_set_task(
	struct sia81xx_set_vdd_info *info)
{
	int max_wait_time_ms = RECV_WATI_TIME_MS * 10;

	pr_debug("[debug][%s] %s: \r\n", LOG_FLAG, __func__);

	if(TASK_STATE_RUN != atomic_read(&info->task_state))
		goto end;

	atomic_set(&info->task_state, TASK_STATE_CLOSING);

	while(TASK_STATE_CLOSED != atomic_read(&info->task_state)) {

		if(0 >= max_wait_time_ms) {
			pr_err("[  err][%s] %s: sia81xx_wait_auto_set_task_close "
				"time out\r\n",
				LOG_FLAG, __func__);
			return -EBUSY;
		}

		msleep(1);
		max_wait_time_ms --;
	}

end :

	info->task = NULL;

	pr_debug("[debug][%s] %s: use wait time %d ms \r\n",
		LOG_FLAG, __func__, (RECV_WATI_TIME_MS * 10) - max_wait_time_ms);

	return 0;
}
#endif

int sia81xx_open_set_vdd_server(
	uint32_t cal_id)
{

	unsigned long cal_handle = 0;
	struct sia81xx_set_vdd_info *info = NULL;

	if(NULL == tuning_if_opt.open) {
		pr_err("[  err][%s] %s: NULL == tuning_if_opt.opt.open \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	cal_handle = tuning_if_opt.open(cal_id);
	if(0 == cal_handle) {
		pr_err("[  err][%s] %s: NULL == cal_handle \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	if(NULL == (info = record_info(cal_handle, cal_id))) {
		pr_err("[  err][%s] %s: 0 != record_info \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	atomic_set(&info->auto_set_flag, 0);
	atomic_set(&info->task_state, TASK_STATE_INIT);
	info->module_id = DEFAULT_MODULE_ID;
	info->param_id = DEFAULT_PARAM_ID;
	info->last_run_time = current_kernel_time();

	return sia81xx_open_auto_set_task(info);
}

#if 0
static int sia81xx_close_set_vdd_server(
	uint32_t cal_id)
{
	struct sia81xx_set_vdd_info *info = is_cal_id_exist(cal_id);
	if(NULL == info) {
		pr_info("[ info][%s] %s: NULL == map, id = %d \r\n",
			LOG_FLAG, __func__, cal_id);
		return 0;
	}

	if(NULL == tuning_if_opt.close) {
		pr_err("[  err][%s] %s: NULL == tuning_if_opt.opt.close \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	if(0 != tuning_if_opt.close(info->cal_handle)) {
		pr_err("[  err][%s] %s: 0 != tuning_if_opt.close \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	sia81xx_close_auto_set_task(info);

	info->cal_handle = 0;
	info->cal_id = 0;
	atomic_set(&info->auto_set_flag, 0);

	return 0;
}
#endif

static void keep_thread_alive(
	struct sia81xx_set_vdd_info *pInfo)
{
	struct timespec cur_time, gitter_range;
	gitter_range.tv_sec =
		((AUTO_SET_INTERVAL_TIME_MS + MSEC_PER_SEC - 1) / MSEC_PER_SEC) * 3;
	gitter_range.tv_nsec = 0;

	if(NULL == pInfo)
		return ;

	if(NULL == pInfo->task)
		goto reboot_thread;

	mutex_lock(&task_mutex);
	if(TASK_STATE_RUN != atomic_read(&pInfo->task_state)) {
		mutex_unlock(&task_mutex);
		pInfo->task = NULL;
		goto reboot_thread;
	}
	mutex_unlock(&task_mutex);


	/* need not consider the "add" & "compare" opt overflow */
	gitter_range = timespec_add_safe(pInfo->last_run_time, gitter_range);
	cur_time = current_kernel_time();
#if 0
	if(timespec_compare(&cur_time, &gitter_range) > 0) {
		pr_err("[  err][%s] %s: time out, cur : %lld, last + 3 : %lld \r\n",
			LOG_FLAG, __func__, (long long)cur_time.tv_sec,
			(long long)gitter_range.tv_sec);
		/* detroy the thread */
		/* modify for customer */
		#if 0
		release_task(pInfo->task);
		#else
		send_sig_info(SIGKILL, SEND_SIG_FORCED, pInfo->task);
		wake_up_process(pInfo->task);
		#endif
		pInfo->task = NULL;
		goto reboot_thread;
	}
#endif

	return ;

reboot_thread :

	atomic_set(&pInfo->auto_set_flag, 0);
	atomic_set(&pInfo->task_state, TASK_STATE_INIT);
	pInfo->last_run_time = current_kernel_time();

	sia81xx_open_auto_set_task(pInfo);

	return ;
}

int sia81xx_enable_auto_set_vdd(
	uint32_t cal_id)
{
	int ret = 0, i = 0;

	struct sia81xx_set_vdd_info *pInfo = is_cal_id_exist(cal_id);
	if(NULL == pInfo) {
		ret = sia81xx_open_set_vdd_server(cal_id);
		if(0 != ret) {
			pr_err("[  err][%s] %s: sia81xx_close_set_vdd_server ret : %d \r\n",
				LOG_FLAG, __func__, ret);
			return -EINVAL;
		}

		pInfo = is_cal_id_exist(cal_id);
		if(NULL == pInfo) {
			pr_err("[  err][%s] %s: NULL == pInfo \r\n",
				LOG_FLAG, __func__);
			return -EINVAL;
		}
	}

	//for test
	if(0 == atomic_read(&open_flag))
		atomic_set(&switch_flag, 1);

	mutex_lock(&task_mutex);
	atomic_set(&open_flag, 1);
	mutex_unlock(&task_mutex);

	/* clear vdd sample pool state */
	pInfo->vdd_sample_send = AUTO_SET_NORMAL_SET_SAMPLES;
	pInfo->vdd_sample_cnt = 0;
	pInfo->vdd_val_pool_pos = 0;
	for(i = 0; i < ARRAY_SIZE(pInfo->vdd_val_pool); i++) {
		pInfo->vdd_val_pool[i] = VDD_DEFAULT_VAL;
	}

	keep_thread_alive(pInfo);

	atomic_set(&pInfo->auto_set_flag, 1);

	return 0;
}

int sia81xx_disable_auto_set_vdd(
	uint32_t cal_id)
{

	struct sia81xx_set_vdd_info *pInfo = is_cal_id_exist(cal_id);

	if(1 == atomic_read(&open_flag))
		atomic_set(&switch_flag, 1);

	//for test
	atomic_set(&open_flag, 0);

	if(NULL == pInfo)
		return -EINVAL;

	atomic_set(&pInfo->auto_set_flag, 0);

	return 0;
}

int sia81xx_set_vdd_init(void) {

	int ret = 0;

	mutex_init(&task_mutex);
	atomic_set(&switch_flag, 0);

	delete_all_info();

	pr_info("[ info][%s] %s: run !! ",
		LOG_FLAG, __func__);

	return ret;
}

void sia81xx_set_vdd_exit(void) {

	delete_all_info();

	pr_info("[ info][%s] %s: run !! ",
		LOG_FLAG, __func__);
}


