/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SE2707__H
#define __SE2707__H
/* Includes ------------------------------------------------------------------*/

/* Private includes ----------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/
/*  Set Param Tempory or Permanent */
typedef enum {
    Set_Param_Tempory = 0x00,
    Set_Param_Permanent = 0x08,
} eSE2707_Set_Param_status;

/*
 * Image Capture Preferences Parameter Defaults (continued)
 * ----------------------------------------------------------------------------------------------
 * Parameter                                Parameter   SSI         Default                 Page
 * ----------------------------------------------------------------------------------------------
 * Operational Modes                        N/A         N/A         N/A                     7-4
 * Aim Brightness                           668         F1h 9Ch     2 (High)                7-5
 * Illumination Brightness                  669         F1h 9Dh     7                       7-6
 * Decoding Autoexposure                    297         F0h 29h     Enable                  7-6
 * Decoding Illumination                    298         F0h 2Ah     Enable                  7-7
 * Decode Aiming Pattern                    306         F0h 32h     Enable                  7-7
 * Image Capture Illumination               361         F0h 69h     Enable                  7-8
 * Image Capture Autoexposure               360         F0h 68h     Enable                  7-8
 * Exposure Time                            567         F4h F1h     37h 100 (10 ms)         7-9
 * Analog Gain                              1232        F4h D0h     Analog Gain 1           7-10
 * Snapshot Mode Timeout                    323         F0h 43h     0 (30 seconds)          7-11
 * Snapshot Aiming Pattern                  300         F0h 2Ch     Enable                  7-12
 * Image Size (Number of Pixels)            302         F0h 2Eh     Full                    7-13
 * Image Brightness (Target White)          390         F0h 86h     180                     7-14
 * Image File Format Selection              304         F0h 30h     JPEG                    7-15
 * Image Rotation                           665         F1h 99h     0                       7-16
 * Video View Finder                        324         F0h 44h     Disable                 7-17
 * Target Video Frame Size                  328         F0h 48h     2200 bytes              7-18
 * Video View Finder Image Size             329         F0h 49h     1700 bytes              7-18
 * Video Resolution                         667         F1h 9Bh     1/4 resolution          7-19
 * ----------------------------------------------------------------------------------------------
 *
 *
 */

typedef enum {
    Operational_Modes,
    Aim_Brightness = 0xF19C,
    Illumination_Brightness = 0xF19D,
    Decoding_Autoexposure = 0xF029,
    Decoding_Illumination = 0xF02A,
    Decode_Aiming_Pattern = 0xF032,
    Image_Capture_Illumination = 0xF069,
    Image_Capture_Autoexposure = 0xF068,
    Exposure_Time = 0xF4F1,
    Analog_Gain = 0xF4D0,
    Snapshot_Mode_Timeout = 0xF043,
    Snapshot_Aiming_Pattern = 0xF02C,
    Image_Size = 0xF02E,
    Image_Brightness = 0xF086,
    Image_File_Format_Selection = 0xF030,
    Image_Rotation = 0xF199,
    Video_View_Finder = 0xF044,
    Target_Video_Frame_Size = 0xF048,
    Video_View_Finder_Image_Size = 0xF049,
    Video_Resolution = 0xF19B,
    Continuous_Bar_Code_Read = 0xF189,
} eSE2707_Image_Capture_Param;

typedef struct {
    eSE2707_Image_Capture_Param param;
    uint8_t data;
} sSE2707_Image_Capture_Param;

/* Exported constants --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
uint16_t se2707_checksum_gen(uint8_t * pData, uint16_t length);
uint8_t se2707_checksum_check(uint8_t * pData, uint16_t length);

uint16_t se2707_build_pack(uint8_t cmd, eSE2707_Set_Param_status status, uint8_t * pPayload, uint8_t payload_length, uint8_t * pResult);
uint16_t se2707_build_pack_ack(uint8_t * pResult);
uint16_t se2707_build_pack_beep_conf(uint8_t beep_code, eSE2707_Set_Param_status status, uint8_t * pResult);
uint16_t se2707_build_pack_param_read(uint16_t param_type, uint8_t * pResult);
uint16_t se2707_build_pack_param_write(uint16_t param_type, eSE2707_Set_Param_status status, uint8_t conf, uint8_t * pResult);
uint16_t se2707_build_pack_reset_Default(uint8_t * pResult);

uint16_t se2707_send_pack(UART_HandleTypeDef * puart, uint8_t * pData, uint16_t length);
void se2707_clear_recv(UART_HandleTypeDef * puart);
uint16_t se2707_recv_pack(UART_HandleTypeDef * puart, uint8_t * pData, uint16_t length, uint32_t timeout);
uint8_t se2707_check_recv_ack(uint8_t * pdata, uint8_t length);
uint8_t se2707_decode_param(uint8_t * pData, uint8_t length, sSE2707_Image_Capture_Param * pResult);

uint8_t se2707_conf_param(UART_HandleTypeDef * puart, sSE2707_Image_Capture_Param * pICP, uint32_t timeout, uint8_t retry);
uint8_t se2707_check_param(UART_HandleTypeDef * puart, sSE2707_Image_Capture_Param conf, uint32_t timeout, uint8_t retry);
uint8_t se2707_reset_param(UART_HandleTypeDef * puart, uint32_t timeout, uint8_t retry);
/* Private defines -----------------------------------------------------------*/

#endif
