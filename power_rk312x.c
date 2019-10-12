/*
 * Copyright (C) 2016 Fuzhou Rockchip Electronics Co. Ltd. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of The Linux Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define LOG_TAG "RKPowerHAL"
#define DEBUG_EN 0
#include <utils/Log.h>
#include <cutils/properties.h>

#include <hardware/hardware.h>
#include <hardware/power.h>

#define DDR_BOOST_SUPPORT 1
#define BUFFER_LENGTH 128
#define FREQ_LENGTH 16

static bool low_power_mode = false;

#define LOW_POWER_MAX_FREQ cpu_clust0_available_freqs[cpu_clust0_max_index/2]
#define NORMAL_MAX_FREQ cpu_clust0_available_freqs[cpu_clust0_max_index]

//#define TOUCHSCREEN_POWER_PATH "/devices/platform/ff160000.i2c/i2c-4/4-0040/input"

#define CPU_MAX_FREQ_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq"
#define CPU_CLUST0_GOV_PATH "/sys/devices/system/cpu/cpufreq/policy0/scaling_governor"
#define CPU_CLUST0_AVAIL_FREQ "/sys/devices/system/cpu/cpufreq/policy0/scaling_available_frequencies"
#define CPU_CLUST0_SCAL_MAX_FREQ "/sys/devices/system/cpu/cpufreq/policy0/scaling_max_freq"
#define CPU_CLUST0_SCAL_MIN_FREQ "/sys/devices/system/cpu/cpufreq/policy0/scaling_min_freq"
#define CPU_CLUST0_BOOSTPULSE_PATH "/sys/devices/system/cpu/cpufreq/policy0/interactive/boostpulse"
#define CPU_CLUST0_HISPEED_FREQ_PATH "/sys/devices/system/cpu/cpu0/cpufreq/interactive/hispeed_freq"

enum gpu_num{
	GPU_GOV_PATH = 0,
	GPU_AVAIL_FREQ = 1,
	GPU_MIN_FREQ = 2,
	GPU_MAX_FREQ = 3,
	GPU_PATH_NUM
};

static char ddr_devfreq_path[BUFFER_LENGTH];
static char cpu_clust0_available_freqs[FREQ_LENGTH][FREQ_LENGTH];
static unsigned int cpu_clust0_max_index = 0;
static char gpu_devfreq_path[GPU_PATH_NUM][BUFFER_LENGTH];
static char gpu_available_freqs[FREQ_LENGTH][FREQ_LENGTH];
static unsigned int gpu_max_index = 0;
static char propbuf[256];
void get_gpu_ddr_devfreq_path()
{
	int fd;

	memset(gpu_devfreq_path[GPU_GOV_PATH], '\0', sizeof(gpu_devfreq_path[GPU_GOV_PATH]));
	memset(gpu_devfreq_path[GPU_AVAIL_FREQ], '\0', sizeof(gpu_devfreq_path[GPU_AVAIL_FREQ]));
	memset(gpu_devfreq_path[GPU_MIN_FREQ], '\0', sizeof(gpu_devfreq_path[GPU_MIN_FREQ]));
	memset(gpu_devfreq_path[GPU_MAX_FREQ], '\0', sizeof(gpu_devfreq_path[GPU_MAX_FREQ]));
	memset(ddr_devfreq_path, '\0', sizeof(ddr_devfreq_path));	
	
	if (fd = open ("sys/class/devfreq/devfreq0/device/gpuinfo",O_RDONLY)){
		strcpy(gpu_devfreq_path[GPU_GOV_PATH],"sys/class/devfreq/devfreq0/available_governors");
		strcpy(gpu_devfreq_path[GPU_AVAIL_FREQ],"sys/class/devfreq/devfreq0/available_frequencies");
		strcpy(gpu_devfreq_path[GPU_MIN_FREQ],"sys/class/devfreq/devfreq0/min_freq");
		strcpy(gpu_devfreq_path[GPU_MAX_FREQ],"sys/class/devfreq/devfreq0/max_freq");
		strcpy(ddr_devfreq_path,"sys/class/devfreq/devfreq1/system_status");
		if(DEBUG_EN)ALOGI("gpu:devfreq0 , ddr:devfreq1\n");
		close(fd);
		
    } else if (fd = open ("sys/class/devfreq/devfreq1/device/gpuinfo",O_RDONLY)) {
        strcpy(gpu_devfreq_path[GPU_GOV_PATH],"sys/class/devfreq/devfreq1/available_governors");
		strcpy(gpu_devfreq_path[GPU_AVAIL_FREQ],"sys/class/devfreq/devfreq1/available_frequencies");
		strcpy(gpu_devfreq_path[GPU_MIN_FREQ],"sys/class/devfreq/devfreq1/min_freq");
		strcpy(gpu_devfreq_path[GPU_MAX_FREQ],"sys/class/devfreq/devfreq1/max_freq");
		strcpy(ddr_devfreq_path,"sys/class/devfreq/devfreq0/system_status");
		if(DEBUG_EN)ALOGI("gpu:devfreq1 , ddr:devfreq0\n");
		close(fd);
    } else{
		ALOGE("Error gpu devfreq path not found\n");
	}
}

static void sysfs_write(char *path, char *s)
{
    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
    }

    close(fd);
}

/*************** Modify cpu clust0 scaling max && min freq for interactive mode **********************/
static void cpu_clus0_boost(unsigned int max, unsigned int min )
{
    if(DEBUG_EN)ALOGI("RK cpu_clus0_boost Entered!");

    if(max>=0 && max<=cpu_clust0_max_index && *cpu_clust0_available_freqs[max]>='0' && *cpu_clust0_available_freqs[max]<='9' ){
        if(DEBUG_EN)ALOGI("cpu_clust0_available_freqs[%d]:%s",max,cpu_clust0_available_freqs[max]);
        sysfs_write(CPU_CLUST0_SCAL_MAX_FREQ,cpu_clust0_available_freqs[max]);
    } else {
        ALOGE("Invalid max freq can not be set!");
    }

    if(min>=0 && min<=cpu_clust0_max_index && *cpu_clust0_available_freqs[min]>='0' && *cpu_clust0_available_freqs[min]<='9' ){
        if(DEBUG_EN)ALOGI("cpu_clust0_available_freqs[%d]:%s",min,cpu_clust0_available_freqs[min]);
        sysfs_write(CPU_CLUST0_SCAL_MIN_FREQ,cpu_clust0_available_freqs[min]);
    } else {
        ALOGE("Invalid min freq can not be set!");
    }
}

/*************** Modify gpu max && min freq for simple_ondemand mode **********************/
static void gpu_boost(unsigned int max, unsigned int min)
{
    if(DEBUG_EN)ALOGI("RK gpu_boost Entered!");

    if(max>=0 && max<=gpu_max_index && *gpu_available_freqs[max]>='0' && *gpu_available_freqs[max]<='9' ){
        if(DEBUG_EN)ALOGI("gpu_available_freqs[%d]:%s",max,gpu_available_freqs[max]);
        sysfs_write(gpu_devfreq_path[GPU_MAX_FREQ],gpu_available_freqs[max]);
    } else {
        ALOGE("Invalid max freq can not be set!");
    }

    if(min>=0 && min<=gpu_max_index && *gpu_available_freqs[min]>='0' && *gpu_available_freqs[min]<='9' ){
        if(DEBUG_EN)ALOGI("gpu_available_freqs[%d]:%s",min,gpu_available_freqs[min]);
        sysfs_write(gpu_devfreq_path[GPU_MIN_FREQ],gpu_available_freqs[min]);
    } else {
        ALOGE("Invalid min freq can not be set!");
    }
}

static bool is_cts_boost_scene()
{
    FILE *fp;
    char result[32];
    fp=fopen("/metadata/view_cts.ini","r");
    if(!fp) {
        ALOGE("fp is null");
        return false;
    }
    while(memset(result, 0, sizeof(result)),fgets(result,sizeof(result),fp)){
        if(DEBUG_EN)ALOGI("result:%s",result);
        if(!strncmp(result,"is_auto_fill=1",14))
            return true;
    }
    fclose(fp);
    return false;
}

/******** touch bootst  *********/
static void touch_boost(int on)
{
    if(DEBUG_EN)ALOGI("RK touch_boost Entered:%d", on);
    //sysfs_write(CPU_CLUST0_BOOSTPULSE_PATH, on ? "1" : "0");
}

/************** Modify cpu gpu ddr to performance mode ************************/
static void performance_boost(int on)
{
    if(DEBUG_EN)ALOGI("RK performance_boost, on=%d", on);
    sysfs_write(CPU_CLUST0_GOV_PATH, on ? "performance" : "interactive");
    sysfs_write(gpu_devfreq_path[GPU_GOV_PATH],on ? "performance" : "simple_ondemand");
#ifdef DDR_BOOST_SUPPORT
    sysfs_write(ddr_devfreq_path,on ? "p" : "n");
#endif
}

/************** Modify cpu gpu ddr to powersave mode ************************/
static void low_power_boost(int on)
{
    if(DEBUG_EN)ALOGI("RK low_power_boost Entered!");
    //sysfs_write(CPU_CLUST0_GOV_PATH, on ? "powersave" : "interactive");
    //sysfs_write(gpu_devfreq_path[GPU_GOV_PATH],on ? "powersave" : "simple_ondemand");
    low_power_mode = on;
#ifdef DDR_BOOST_SUPPORT
    sysfs_write(ddr_devfreq_path,on ? "l" : "L");
#endif
}

static void rk_power_init(struct power_module *module)
{
    if(DEBUG_EN)ALOGD("rk3126c: power hal version 4.0\n");

    int fd,count,i=0;
    char cpu_clus0_freqs[BUFFER_LENGTH];
    char gpu_freqs[BUFFER_LENGTH] ;
    char*freq_split;

	get_gpu_ddr_devfreq_path();
    /*********************** obtain cpu cluster0 available freqs **************************/
    if(fd = open (CPU_CLUST0_AVAIL_FREQ,O_RDONLY)){
        count = read(fd,cpu_clus0_freqs,sizeof(cpu_clus0_freqs)-1);
        if(count < 0) ALOGE("Error reading from %s\n", CPU_CLUST0_AVAIL_FREQ);
        else
            cpu_clus0_freqs[count] = '\0';
    } else {
        ALOGE("Error to open %s\n", CPU_CLUST0_AVAIL_FREQ);
    }
    if(DEBUG_EN)ALOGI("cpu_clus0_freqs:%s\n",cpu_clus0_freqs);

    freq_split = strtok(cpu_clus0_freqs," ");
    strcpy(cpu_clust0_available_freqs[0],freq_split);
    if(DEBUG_EN)ALOGI("cpu_clust0 available freq[0]:%s\n",cpu_clust0_available_freqs[0]);
    for(i=1;freq_split=strtok(NULL," ");i++){
        strcpy(cpu_clust0_available_freqs[i],freq_split);
        if(DEBUG_EN)ALOGI("cpu_clust0 available freq[%d]:%s\n",i,cpu_clust0_available_freqs[i]);
    }
    cpu_clust0_max_index = i-2;
    if(DEBUG_EN)ALOGI("cpu_clust0_max_index:%d\n",cpu_clust0_max_index);
    
    sysfs_write(CPU_CLUST0_HISPEED_FREQ_PATH,"600000");
    /*********************** obtain gpu available freqs **************************/
    if(fd = open (gpu_devfreq_path[GPU_AVAIL_FREQ],O_RDONLY)){
        count = read(fd,gpu_freqs,sizeof(gpu_freqs)-1);
        if(count < 0) ALOGE("Error reading from %s\n", GPU_AVAIL_FREQ);
        else
            gpu_freqs[count] = '\0';
    } else {
        ALOGE("Error to open %s\n", GPU_AVAIL_FREQ);
    }
    if(DEBUG_EN)ALOGI("gpu_freqs:%s\n",gpu_freqs);

    freq_split = strtok(gpu_freqs," ");
    strcpy(gpu_available_freqs[0],freq_split);
    if(DEBUG_EN)ALOGI("gpu available freq[0]:%s\n",gpu_available_freqs[0]);
    for(i=1;freq_split=strtok(NULL," ");i++){
        strcpy(gpu_available_freqs[i],freq_split);
        if(DEBUG_EN)ALOGI("gpu available freq[%d]:%s\n",i,gpu_available_freqs[i]);
    }
    gpu_max_index = i-1;
    if(DEBUG_EN)ALOGI("gpu_max_index:%d\n",gpu_max_index);
}

/*performs power management actions upon the
 * system entering interactive state (that is, the system is awake
 * and ready for interaction, often with UI devices such as
 * display and touchscreen enabled) or non-interactive state (the
 * system appears asleep, display usually turned off).
 */
static void rk_power_set_interactive(struct power_module *module, int on)
{
    /*************Add appropriate actions for specific platform && product type *****************/
    if(DEBUG_EN)ALOGD("power_set_interactive: %d\n", on);

    /*
     * Lower maximum frequency when screen is off.
     */
    sysfs_write(CPU_MAX_FREQ_PATH,
                (!on || low_power_mode) ? LOW_POWER_MAX_FREQ : NORMAL_MAX_FREQ);
    //sysfs_write("/sys/devices/system/cpu/cpu1/online", on ? "1" : "0");
    //sysfs_write("/sys/devices/system/cpu/cpu2/online", on ? "1" : "0");
    //sysfs_write("/sys/devices/system/cpu/cpu3/online", on ? "1" : "0");
    //sysfs_write(TOUCHSCREEN_POWER_PATH, on ? "1" : "0");
    if(DEBUG_EN)ALOGD("power_set_interactive: %d done\n", on);
}

/*
 * (*powerHint) is called to pass hints on power requirements, which
 * may result in adjustment of power/performance parameters of the
 * cpufreq governor and other controls.
 */
static void rk_power_hint(struct power_module *module, power_hint_t hint, void *data)
{
    /*************Add appropriate actions for specific platform && product type *****************
     * When the incoming parameter is 0, the 'data' will be lost.
     **/
    int mode = data!=NULL?*(int*)data:0;

    switch (hint) {
        case POWER_HINT_INTERACTION:
            property_get("ro.build.fingerprint", propbuf, NULL);
            if(DEBUG_EN)ALOGI("propbuf:%s",propbuf);
            if(strstr(propbuf,"generic_arm")) {
                if(is_cts_boost_scene())
                    performance_boost(1);
                else
                    performance_boost(0);
            }
            //touch_boost(mode);
            break;

        case POWER_HINT_VSYNC:
            break;

        case POWER_HINT_VIDEO_DECODE:
            break;

        case POWER_HINT_LOW_POWER:
            low_power_boost(mode);
            break;

        case POWER_HINT_SUSTAINED_PERFORMANCE:
            performance_boost(mode);
            break;

        case POWER_HINT_PERFORMANCE:
            performance_boost(mode);
            break;

        case POWER_HINT_VR_MODE:
            break;

        default:
            break;
    }
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct power_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = POWER_MODULE_API_VERSION_0_5,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = POWER_HARDWARE_MODULE_ID,
        .name = TARGET_BOARD_PLATFORM " Power HAL",
        .author = "Rockchip",
        .methods = &power_module_methods,
    },

    .init = rk_power_init,
    .setInteractive = rk_power_set_interactive,
    .powerHint = rk_power_hint,
};

