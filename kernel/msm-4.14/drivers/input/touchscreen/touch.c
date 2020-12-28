/***************************************************
 * File:touch.c
 * VENDOR_EDIT
 * Copyright (c)  2008- 2030  Oppo Mobile communication Corp.ltd.
 * Description:
 *             tp dev
 * Version:1.0:
 * Date created:2016/09/02
 * Author: hao.wang@Bsp.Driver
 * TAG: BSP.TP.Init
*/

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/serio.h>
#include "oppo_touchscreen/tp_devices.h"
#include "oppo_touchscreen/touchpanel_common.h"
#include <soc/oppo/oppo_project.h>
#include <soc/oppo/device_info.h>
#include "touch.h"

#define MAX_LIMIT_DATA_LENGTH         100

extern char *saved_command_line;
int g_tp_dev_vendor = TP_UNKNOWN;
int g_tp_prj_id = 0;

/*if can not compile success, please update vendor/oppo_touchsreen*/
struct tp_dev_name tp_dev_names[] = {
    {TP_OFILM, "OFILM"},
    {TP_BIEL, "BIEL"},
    {TP_TRULY, "TRULY"},
    {TP_BOE, "BOE"},
    {TP_G2Y, "G2Y"},
    {TP_TPK, "TPK"},
    {TP_JDI, "JDI"},
    {TP_TIANMA, "TIANMA"},
    {TP_SAMSUNG, "SAMSUNG"},
    {TP_DSJM, "DSJM"},
    {TP_BOE_B8, "BOEB8"},
    {TP_HUAXING, "HUAXING"},
    {TP_HLT, "HLT"},
    {TP_UNKNOWN, "UNKNOWN"},
};

#ifdef VENDOR_EDIT
//Kui.Feng@BSP.kernel.Touch, 2019/12/19, Add for touch firmware headfile update && firmware info
typedef enum {
    TP_INDEX_NULL,
    hx83112a_boe,
    hx83112a_jdi,
    hx83112a_huaxing,
    nt36672a_tianma,
    hx83112a_hlt,
} TP_USED_INDEX;
TP_USED_INDEX tp_used_index  = TP_INDEX_NULL;
#endif

#define GET_TP_DEV_NAME(tp_type) ((tp_dev_names[tp_type].type == (tp_type))?tp_dev_names[tp_type].name:"UNMATCH")

bool __init tp_judge_ic_match(char *tp_ic_name)
{
    pr_err("[TP] tp_ic_name = %s \n", tp_ic_name);
    //pr_err("[TP] boot_command_line = %s \n", saved_command_line);

    switch(get_project()) {
    case 19101:
    case 19102:
    case 19501:
        g_tp_prj_id = 19101;
        if (strstr(tp_ic_name, "synaptics-s3908") && (strstr(saved_command_line, "mdss_dsi_oppo19101boe_nt37800_1080_2400_cmd")
            || strstr(saved_command_line, "mdss_dsi_oppo19101boe_nt37800_1080_2340_cmd"))) {
            g_tp_dev_vendor = TP_BOE;
            return true;
        }
        if (strstr(tp_ic_name, "synaptics-s3706") && (strstr(saved_command_line, "mdss_dsi_oppo19125samsung_s6e3fc2x01_1080_2340_cmd")
            || strstr(saved_command_line, "mdss_dsi_oppo19125samsung_ams641rw01_1080_2340_cmd"))) {
            g_tp_dev_vendor = TP_SAMSUNG;
            return true;
        }
        if (strstr(tp_ic_name, "sec-s6sy771") && (strstr(saved_command_line, "mdss_dsi_oppo19101samsung_sofx01f_a_1080_2400_cmd")
            || strstr(saved_command_line, "mdss_dsi_oppo19101samsung_amb655uv01_1080_2400_cmd"))) {
            g_tp_dev_vendor = TP_SAMSUNG;
            return true;
        }
        pr_err("[TP] Driver does not match the project\n");
        break;

    case 19125:
    case 19126:
    case 19127:
    case 19128:
    case 19521:
    case 19522:
        g_tp_prj_id = 19125;
    case 19375:
        g_tp_prj_id = 19375;
        if (strstr(tp_ic_name, "synaptics-s3706") && (strstr(saved_command_line, "mdss_dsi_oppo19125samsung_s6e3fc2x01_1080_2340_cmd")
            || strstr(saved_command_line, "mdss_dsi_oppo19125samsung_ams641rw01_1080_2340_cmd")
            || strstr(saved_command_line, "mdss_dsi_oppo19101samsung_sofx01f_a_1080_2400_cmd"))) {
            g_tp_dev_vendor = TP_SAMSUNG;
            return true;
        }
        if (strstr(tp_ic_name, "Goodix-gt9886") && (strstr(saved_command_line, "mdss_dsi_oppo19125samsung_ams644va04_1080_2400_video")
			|| strstr(saved_command_line, "dsi_s6e8fc1x01_samsumg_vid_display"))) {
            g_tp_dev_vendor = TP_SAMSUNG;
            return true;
        }
        pr_err("[TP] Driver does not match the project\n");
        break;
//#ifdef VENDOR_EDIT
//fengkui@BSP.kernel.drvier, 2019/11/27, Modify for oppo touch bring up (19571 T0 branch)
    case 19171:
    case 19175:
    case 19176:
    case 19570:
    case 19571:
    case 19572:
    case 19573:
    case 19575:
    case 19576:
    case 19577:
    case 19578:
    case 19579:
        g_tp_prj_id = 19571;
        if (strstr(tp_ic_name, "nf_hx83112a") && strstr(saved_command_line, "dsi_hx83112a_boe_vid_display")) {
            g_tp_dev_vendor = TP_BOE;
#ifdef VENDOR_EDIT
//Kui.Feng@BSP.kernel.Touch, 2019/12/19, Add for touch firmware headfile update && firmware info
	    tp_used_index = hx83112a_boe;
#endif
            return true;
        }
        if (strstr(tp_ic_name, "nf_hx83112a") && strstr(saved_command_line, "dsi_hx83112a_dongshan_vid_display")) {
	    g_tp_dev_vendor =TP_JDI;
#ifdef VENDOR_EDIT
//Kui.Feng@BSP.kernel.Touch, 2019/12/19, Add for touch firmware headfile update && firmware info
	    tp_used_index = hx83112a_jdi;
#endif
            return true;
        }
        if (strstr(tp_ic_name, "nf_hx83112a") && strstr(saved_command_line, "dsi_hx83112a_huaxing_vid_display")) {
            g_tp_dev_vendor =TP_HUAXING;
#ifdef VENDOR_EDIT
//Kui.Feng@BSP.kernel.Touch, 2019/12/19, Add for touch firmware headfile update && firmware info
            tp_used_index = hx83112a_huaxing;
#endif
            return true;
        }
        if (strstr(tp_ic_name, "nf_nt36672a") && strstr(saved_command_line, "dsi_nt36672a_tm_vid_display")) {
            g_tp_dev_vendor = TP_TIANMA;
#ifdef VENDOR_EDIT
//Kui.Feng@BSP.kernel.Touch, 2019/12/19, Add for touch firmware headfile update && firmware info
	    tp_used_index = nt36672a_tianma;
#endif
            return true;
        }
//#endif
        if (strstr(tp_ic_name, "nf_hx83112a") && strstr(saved_command_line, "dsi_hx83112a_hlt_vid_display")) {
            g_tp_dev_vendor =TP_HLT;
#ifdef VENDOR_EDIT
//Kui.Feng@BSP.kernel.Touch, 2019/12/19, Add for touch firmware headfile update && firmware info
            tp_used_index = hx83112a_hlt;
#endif
            return true;
        }

        pr_err("[TP] Driver does not match the project\n");
        break;

    default:
        pr_err("Invalid project\n");
        break;
    }

    pr_err("Lcd module not found\n");

    return false;
}

int tp_util_get_vendor(struct hw_resource *hw_res, struct panel_info *panel_data)
{
    char* vendor;

    panel_data->test_limit_name = kzalloc(MAX_LIMIT_DATA_LENGTH, GFP_KERNEL);
    if (panel_data->test_limit_name == NULL) {
        pr_err("[TP]panel_data.test_limit_name kzalloc error\n");
    }
    switch(get_project()) {
    case 19375:
        g_tp_prj_id = 19375;
        if ( strstr(saved_command_line, "dsi_s6e8fc1x01_samsumg_vid_display")) {
            g_tp_dev_vendor = TP_SAMSUNG;
        }
        if (strstr(saved_command_line, "mdss_dsi_oppo19125samsung_ams644va04_1080_2400_video")) {
            g_tp_dev_vendor = TP_SAMSUNG;
       }
       break;
    }
   panel_data->tp_type = g_tp_dev_vendor;

    if (panel_data->tp_type == TP_UNKNOWN) {
        pr_err("[TP]%s type is unknown\n", __func__);
        return 0;
    }

    vendor = GET_TP_DEV_NAME(panel_data->tp_type);

    strcpy(panel_data->manufacture_info.manufacture, vendor);

    snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH,
            "tp/%d/FW_%s_%s.img",
            g_tp_prj_id, panel_data->chip_name, vendor);

    if (panel_data->test_limit_name) {
        snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH,
            "tp/%d/LIMIT_%s_%s.img",
            g_tp_prj_id, panel_data->chip_name, vendor);
    }


    switch(get_project()) {
	case 19375:
	memcpy(panel_data->manufacture_info.version, "0xbd2830000", 11);
	break;
#ifdef VENDOR_EDIT
//Kui.Feng@BSP.kernel.Touch, 2019/12/19, Add for touch firmware headfile update && firmware info
	case 19171:
	case 19175:
	case 19176:
	case 19570:
	case 19571:
	case 19572:
	case 19573:
	case 19575:
	case 19576:
	case 19577:
	case 19578:
	case 19579:
	memcpy(panel_data->manufacture_info.version, "0000000", 7);
	panel_data->vid_len = 7;
	if (tp_used_index == hx83112a_boe) {
		memcpy(panel_data->manufacture_info.version, "FA135BH", 7);
		panel_data->firmware_headfile.firmware_data = FW_19571_NF_HX83112A_BOE;
		panel_data->firmware_headfile.firmware_size = sizeof(FW_19571_NF_HX83112A_BOE);
	} else if (tp_used_index == hx83112a_jdi) {
		memcpy(panel_data->manufacture_info.version, "FA135MH", 7);
		panel_data->firmware_headfile.firmware_data = FW_19571_NF_HX83112A_JDI;
		panel_data->firmware_headfile.firmware_size = sizeof(FW_19571_NF_HX83112A_JDI);
	} else if (tp_used_index == hx83112a_huaxing) {
		memcpy(panel_data->manufacture_info.version, "FA135CH", 7);
		panel_data->firmware_headfile.firmware_data = FW_19571_NF_HX83112A_HUAXING;
		panel_data->firmware_headfile.firmware_size = sizeof(FW_19571_NF_HX83112A_HUAXING);
	} else if (tp_used_index == hx83112a_hlt) {
		memcpy(panel_data->manufacture_info.version, "FA135HH", 7);
		panel_data->firmware_headfile.firmware_data = FW_19571_NF_HX83112A_HLT;
		panel_data->firmware_headfile.firmware_size = sizeof(FW_19571_NF_HX83112A_HLT);
	} else if (tp_used_index == nt36672a_tianma) {
		memcpy(panel_data->manufacture_info.version, "FA135TN", 7);
		panel_data->firmware_headfile.firmware_data = FW_19571_NF_NT36672A_TIANMA;
		panel_data->firmware_headfile.firmware_size = sizeof(FW_19571_NF_NT36672A_TIANMA);
	} else {
		panel_data->firmware_headfile.firmware_data = NULL;
		panel_data->firmware_headfile.firmware_size = 0;
	}
	break;
	default:
	panel_data->firmware_headfile.firmware_data = NULL;
	panel_data->firmware_headfile.firmware_size = 0;
	break;
#endif
    }
    panel_data->manufacture_info.fw_path = panel_data->fw_name;

    pr_info("[TP]vendor:%s fw:%s limit:%s\n",
        vendor,
        panel_data->fw_name,
        panel_data->test_limit_name == NULL ? "NO Limit" : panel_data->test_limit_name);

    return 0;
}

