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
#ifndef BI_H
#define BI_H

#include <stdbool.h>
#include <stdint.h>
#include "bacdef.h"
#include "cov.h"
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

#ifndef MAX_BINARY_INPUTS
#define MAX_BINARY_INPUTS (TOTALDIGITALINPUTS + TOTALUIO)
#endif

    typedef struct binary_input_descr {
        char Object_Name[MAX_CHARACTER_STRING_BYTES];
        unsigned Event_State:3;
        // BACNET_BINARY_PV Present_Value;
        //BACNET_BINARY_PV Relinquish_Default;
        //BACNET_RELIABILITY Reliability;
        // bool Out_Of_Service;
        //float Prior_Value;
        // bool Change_Of_Value;
        // BACNET_POLARITY Polarity;
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
    } BINARY_INPUT_DESCR;  

    void Binary_Input_Init(
        void); 

    void Binary_Input_Property_Lists(
        uint32_t object_instance,
        const int **pRequired,
        const int **pOptional,
        const int **pProprietary);

    bool Binary_Input_Valid_Instance(
        uint32_t object_instance);
    unsigned Binary_Input_Count(
        void);
    uint32_t Binary_Input_Index_To_Instance(
        unsigned index);
    unsigned Binary_Input_Instance_To_Index(
        uint32_t instance);
    bool Binary_Input_Object_Instance_Add(
        uint32_t instance);

    bool Binary_Input_Object_Name(
        uint32_t object_instance,
        BACNET_CHARACTER_STRING * object_name);
    bool Binary_Input_Name_Set(
        uint32_t object_instance,
        char *new_name);

    BACNET_BINARY_PV Binary_Input_Present_Value(
        uint32_t object_instance);
    bool Binary_Input_Present_Value_Set(
        uint32_t object_instance,
        BACNET_BINARY_PV value);

    char *Binary_Input_Description(
        uint32_t instance);
    bool Binary_Input_Description_Set(
        uint32_t instance,
        char *new_name);

    char *Binary_Input_Inactive_Text(
        uint32_t instance);
    bool Binary_Input_Inactive_Text_Set(
        uint32_t instance,
        char *new_name);
    char *Binary_Input_Active_Text(
        uint32_t instance);
    bool Binary_Input_Active_Text_Set(
        uint32_t instance,
        char *new_name);

    BACNET_POLARITY Binary_Input_Polarity(
        uint32_t object_instance);
    bool Binary_Input_Polarity_Set(
        uint32_t object_instance,
        BACNET_POLARITY polarity);

    bool Binary_Input_Out_Of_Service(
        uint32_t object_instance);
    void Binary_Input_Out_Of_Service_Set(
        uint32_t object_instance,
        bool value);

    uint8_t Binary_Input_Get_Status(
        uint32_t object_instance);

    bool Binary_Input_Encode_Value_List(
        uint32_t object_instance,
        BACNET_PROPERTY_VALUE * value_list);
    bool Binary_Input_Change_Of_Value(
        uint32_t instance);
    void Binary_Input_Change_Of_Value_Clear(
        uint32_t instance);

    int Binary_Input_Read_Property(
        BACNET_READ_PROPERTY_DATA * rpdata,
        BACNET_LINK * bacLink);

    bool Binary_Input_Write_Property(
        BACNET_WRITE_PROPERTY_DATA * wp_data,
        BACNET_LINK * bacLink);

    /* note: header of Intrinsic_Reporting function is required
       even when INTRINSIC_REPORTING is not defined */
    void Binary_Input_Intrinsic_Reporting(
        uint32_t object_instance);


#if defined(INTRINSIC_REPORTING)
    int Binary_Input_Event_Information(
        unsigned index,
        BACNET_GET_EVENT_INFORMATION_DATA * getevent_data);

    int Binary_Input_Alarm_Ack(
        BACNET_ALARM_ACK_DATA * alarmack_data,
        BACNET_ERROR_CODE * error_code);

    int Binary_Input_Alarm_Summary(
        unsigned index,
        BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data);
#endif

    bool Binary_Input_Create(
        uint32_t object_instance);
    bool Binary_Input_Delete(
        uint32_t object_instance);
    void Binary_Input_Cleanup(
        void);
    void Binary_Input_Init(
        void);

#ifdef TEST
#include "ctest.h"
    void testBinaryInput(
        Test * pTest);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
