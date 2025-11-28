/**************************************************************************
*
* Copyright (C) 2006 Steve Karg <skarg@users.sourceforge.net>
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*********************************************************************/
#ifndef BV_H
#define BV_H

#include <stdbool.h>
#include <stdint.h>
#include "bacdef.h"
#include "bacerror.h"
#include "rp.h"
#include "wp.h"
#if defined(INTRINSIC_REPORTING)
#include "nc.h"
#include "getevent.h"
#include "alarm_ack.h"
#include "get_alarm_sum.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef MAX_BINARY_VALUES
#define MAX_BINARY_VALUES 96
#endif

    typedef struct binary_value_descr {
        // char Object_Name[MAX_CHARACTER_STRING_BYTES];
        unsigned Event_State:3;
        BACNET_BINARY_PV Present_Value[BACNET_MAX_PRIORITY];
        BACNET_BINARY_PV Relinquish_Default;
        //BACNET_RELIABILITY Reliability;
        bool Out_Of_Service;
        //float Prior_Value;
        //bool Changed;
#if defined(INTRINSIC_REPORTING)
        uint32_t Time_Delay;
        uint32_t Notification_Class;
        BACNET_BINARY_PV Alarm_Value;
        unsigned Event_Enable:3;
        unsigned Notify_Type:1;
        ACKED_INFO Acked_Transitions[MAX_BACNET_EVENT_TRANSITION];
        BACNET_DATE_TIME Event_Time_Stamps[MAX_BACNET_EVENT_TRANSITION];
        /* time to generate event notification */
        uint32_t Remaining_Time_Delay;
        /* AckNotification informations */
        ACK_NOTIFICATION Ack_notify_data;
        bool Event_Detection_Enable;
#endif
    } BINARY_VALUE_DESCR;    

    void Binary_Value_Init(
        void);

    void Binary_Value_Property_Lists(
        uint32_t object_instance,
        const int **pRequired,
        const int **pOptional,
        const int **pProprietary);

    bool Binary_Value_Valid_Instance(
        uint32_t object_instance);
    unsigned Binary_Value_Count(
        void);
    uint32_t Binary_Value_Index_To_Instance(
        unsigned index);
    unsigned Binary_Value_Instance_To_Index(
        uint32_t object_instance);
    bool Binary_Value_Object_Instance_Add(
        uint32_t instance);

    bool Binary_Value_Object_Name(
        uint32_t object_instance,
        BACNET_CHARACTER_STRING * object_name);
    bool Binary_Value_Name_Set(
        uint32_t object_instance,
        char *new_name);

    char *Binary_Value_Description(
        uint32_t instance);
    bool Binary_Value_Description_Set(
        uint32_t instance,
        char *new_name);

    char *Binary_Value_Inactive_Text(
        uint32_t instance);
    bool Binary_Value_Inactive_Text_Set(
        uint32_t instance,
        char *new_name);
    char *Binary_Value_Active_Text(
        uint32_t instance);
    bool Binary_Value_Active_Text_Set(
        uint32_t instance,
        char *new_name);

    int Binary_Value_Read_Property(
        BACNET_READ_PROPERTY_DATA * rpdata,
        BACNET_LINK * bacLink);

    bool Binary_Value_Write_Property(
        BACNET_WRITE_PROPERTY_DATA * wp_data,
        BACNET_LINK * bacLink);

    bool Binary_Value_Encode_Value_List(
        uint32_t object_instance,
        BACNET_PROPERTY_VALUE * value_list);
    bool Binary_Value_Change_Of_Value(
        uint32_t instance);
    void Binary_Value_Change_Of_Value_Clear(
        uint32_t instance);

    BACNET_BINARY_PV Binary_Value_Present_Value(
        uint32_t instance);
    s8_t Binary_Value_Present_Value_Set(
        uint32_t instance,
        BACNET_BINARY_PV value,
        unsigned priority);

    bool Binary_Value_Out_Of_Service(
        uint32_t instance);
    void Binary_Value_Out_Of_Service_Set(
        uint32_t instance,
        bool value);

    uint8_t Binary_Value_Get_Status(
        uint32_t object_instance);

    char *Binary_Value_Description(
        uint32_t instance);
    bool Binary_Value_Description_Set(
        uint32_t object_instance,
        char *text_string);

    char *Binary_Value_Inactive_Text(
        uint32_t instance);
    bool Binary_Value_Inactive_Text_Set(
        uint32_t instance,
        char *new_name);
    char *Binary_Value_Active_Text(
        uint32_t instance);
    bool Binary_Value_Active_Text_Set(
        uint32_t instance,
        char *new_name);

    BACNET_POLARITY Binary_Value_Polarity(
        uint32_t instance);
    bool Binary_Value_Polarity_Set(
        uint32_t object_instance,
        BACNET_POLARITY polarity);

    /* note: header of Intrinsic_Reporting function is required
       even when INTRINSIC_REPORTING is not defined */
    void Binary_Value_Intrinsic_Reporting(
        uint32_t object_instance);


#if defined(INTRINSIC_REPORTING)
    int Binary_Value_Event_Information(
        unsigned index,
        BACNET_GET_EVENT_INFORMATION_DATA * getevent_data);

    int Binary_Value_Alarm_Ack(
        BACNET_ALARM_ACK_DATA * alarmack_data,
        BACNET_ERROR_CODE * error_code);

    int Binary_Value_Alarm_Summary(
        unsigned index,
        BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data);
#endif
    
    bool Binary_Value_Create(
        uint32_t object_instance);
    bool Binary_Value_Delete(
        uint32_t object_instance);
    void Binary_Value_Cleanup(
        void);

#ifdef TEST
#include "ctest.h"
    void testBinary_Value(
        Test * pTest);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
