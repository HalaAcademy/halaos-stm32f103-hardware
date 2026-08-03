/* SPDX-FileCopyrightText: 2024-2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file    Dio.h
 * @brief   Digital Input/Output (DIO) Driver Header File
 * @details File này chứa các định nghĩa về kiểu dữ liệu và
 *          khai báo các API của DIO Driver tuân theo chuẩn AUTOSAR.
 *          Driver này được thiết kế để điều khiển GPIO của STM32F103.
 *
 * @version 1.0.0
 * @date    2024-09-30
 * @author  HALA Academy
 * @see     https://hala.edu.vn
 */

#ifndef DIO_H
#define DIO_H


/* ==========================================================================
 *                                 INCLUDES
 * ========================================================================== */
#include "Std_Types.h"      /* Bao gồm các kiểu dữ liệu chuẩn của AUTOSAR */
#include "stm32f10x_gpio.h" /* Thư viện chuẩn hướng đối tượng cho STM32F103 */

/* ==========================================================================
 *                             DEFINES AND MACROS
 * ========================================================================== */

/**
 * @brief Định nghĩa các ID cho cổng GPIO (Dio Port ID)
 */
#define DIO_PORT_A 0 /**< Ánh xạ cho cổng GPIOA */
#define DIO_PORT_B 1 /**< Ánh xạ cho cổng GPIOB */
#define DIO_PORT_C 2 /**< Ánh xạ cho cổng GPIOC */
#define DIO_PORT_D 3 /**< Ánh xạ cho cổng GPIOD */

/**
 * @brief Macro xác định cổng GPIO dựa trên ChannelId
 */
#define DIO_GET_PORT(ChannelId)                                                \
  (((ChannelId) < 16)   ? GPIOA                                                \
   : ((ChannelId) < 32) ? GPIOB                                                \
   : ((ChannelId) < 48) ? GPIOC                                                \
   : ((ChannelId) < 64) ? GPIOD                                                \
                        : NULL)

/**
 * @brief Macro xác định chân GPIO (bit mask) dựa trên ChannelId
 */
#define DIO_GET_PIN(ChannelId) (1 << ((ChannelId) % 16))

/**
 * @brief Macro tạo ChannelId từ GPIO Port và Pin Index
 * @param[in] GPIOx Giá trị đại diện Port (0, 1, 2, 3...)
 * @param[in] Pin   Số thứ tự chân (0-15)
 */
#define DIO_CHANNEL(GPIOx, Pin) (((GPIOx) << 4) + (Pin))

/* --------------------------------------------------------------------------
 *  Định nghĩa Channel ID cho tất cả các chân
 * -------------------------------------------------------------------------- */

/* GPIOA Channels */
#define DIO_CHANNEL_A0 DIO_CHANNEL(DIO_PORT_A, 0)   /**< GPIOA Pin 0  */
#define DIO_CHANNEL_A1 DIO_CHANNEL(DIO_PORT_A, 1)   /**< GPIOA Pin 1  */
#define DIO_CHANNEL_A2 DIO_CHANNEL(DIO_PORT_A, 2)   /**< GPIOA Pin 2  */
#define DIO_CHANNEL_A3 DIO_CHANNEL(DIO_PORT_A, 3)   /**< GPIOA Pin 3  */
#define DIO_CHANNEL_A4 DIO_CHANNEL(DIO_PORT_A, 4)   /**< GPIOA Pin 4  */
#define DIO_CHANNEL_A5 DIO_CHANNEL(DIO_PORT_A, 5)   /**< GPIOA Pin 5  */
#define DIO_CHANNEL_A6 DIO_CHANNEL(DIO_PORT_A, 6)   /**< GPIOA Pin 6  */
#define DIO_CHANNEL_A7 DIO_CHANNEL(DIO_PORT_A, 7)   /**< GPIOA Pin 7  */
#define DIO_CHANNEL_A8 DIO_CHANNEL(DIO_PORT_A, 8)   /**< GPIOA Pin 8  */
#define DIO_CHANNEL_A9 DIO_CHANNEL(DIO_PORT_A, 9)   /**< GPIOA Pin 9  */
#define DIO_CHANNEL_A10 DIO_CHANNEL(DIO_PORT_A, 10) /**< GPIOA Pin 10 */
#define DIO_CHANNEL_A11 DIO_CHANNEL(DIO_PORT_A, 11) /**< GPIOA Pin 11 */
#define DIO_CHANNEL_A12 DIO_CHANNEL(DIO_PORT_A, 12) /**< GPIOA Pin 12 */
#define DIO_CHANNEL_A13 DIO_CHANNEL(DIO_PORT_A, 13) /**< GPIOA Pin 13 */
#define DIO_CHANNEL_A14 DIO_CHANNEL(DIO_PORT_A, 14) /**< GPIOA Pin 14 */
#define DIO_CHANNEL_A15 DIO_CHANNEL(DIO_PORT_A, 15) /**< GPIOA Pin 15 */

/* GPIOB Channels */
#define DIO_CHANNEL_B0 DIO_CHANNEL(DIO_PORT_B, 0)   /**< GPIOB Pin 0  */
#define DIO_CHANNEL_B1 DIO_CHANNEL(DIO_PORT_B, 1)   /**< GPIOB Pin 1  */
#define DIO_CHANNEL_B2 DIO_CHANNEL(DIO_PORT_B, 2)   /**< GPIOB Pin 2  */
#define DIO_CHANNEL_B3 DIO_CHANNEL(DIO_PORT_B, 3)   /**< GPIOB Pin 3  */
#define DIO_CHANNEL_B4 DIO_CHANNEL(DIO_PORT_B, 4)   /**< GPIOB Pin 4  */
#define DIO_CHANNEL_B5 DIO_CHANNEL(DIO_PORT_B, 5)   /**< GPIOB Pin 5  */
#define DIO_CHANNEL_B6 DIO_CHANNEL(DIO_PORT_B, 6)   /**< GPIOB Pin 6  */
#define DIO_CHANNEL_B7 DIO_CHANNEL(DIO_PORT_B, 7)   /**< GPIOB Pin 7  */
#define DIO_CHANNEL_B8 DIO_CHANNEL(DIO_PORT_B, 8)   /**< GPIOB Pin 8  */
#define DIO_CHANNEL_B9 DIO_CHANNEL(DIO_PORT_B, 9)   /**< GPIOB Pin 9  */
#define DIO_CHANNEL_B10 DIO_CHANNEL(DIO_PORT_B, 10) /**< GPIOB Pin 10 */
#define DIO_CHANNEL_B11 DIO_CHANNEL(DIO_PORT_B, 11) /**< GPIOB Pin 11 */
#define DIO_CHANNEL_B12 DIO_CHANNEL(DIO_PORT_B, 12) /**< GPIOB Pin 12 */
#define DIO_CHANNEL_B13 DIO_CHANNEL(DIO_PORT_B, 13) /**< GPIOB Pin 13 */
#define DIO_CHANNEL_B14 DIO_CHANNEL(DIO_PORT_B, 14) /**< GPIOB Pin 14 */
#define DIO_CHANNEL_B15 DIO_CHANNEL(DIO_PORT_B, 15) /**< GPIOB Pin 15 */

/* GPIOC Channels */
#define DIO_CHANNEL_C0 DIO_CHANNEL(DIO_PORT_C, 0)   /**< GPIOC Pin 0  */
#define DIO_CHANNEL_C1 DIO_CHANNEL(DIO_PORT_C, 1)   /**< GPIOC Pin 1  */
#define DIO_CHANNEL_C2 DIO_CHANNEL(DIO_PORT_C, 2)   /**< GPIOC Pin 2  */
#define DIO_CHANNEL_C3 DIO_CHANNEL(DIO_PORT_C, 3)   /**< GPIOC Pin 3  */
#define DIO_CHANNEL_C4 DIO_CHANNEL(DIO_PORT_C, 4)   /**< GPIOC Pin 4  */
#define DIO_CHANNEL_C5 DIO_CHANNEL(DIO_PORT_C, 5)   /**< GPIOC Pin 5  */
#define DIO_CHANNEL_C6 DIO_CHANNEL(DIO_PORT_C, 6)   /**< GPIOC Pin 6  */
#define DIO_CHANNEL_C7 DIO_CHANNEL(DIO_PORT_C, 7)   /**< GPIOC Pin 7  */
#define DIO_CHANNEL_C8 DIO_CHANNEL(DIO_PORT_C, 8)   /**< GPIOC Pin 8  */
#define DIO_CHANNEL_C9 DIO_CHANNEL(DIO_PORT_C, 9)   /**< GPIOC Pin 9  */
#define DIO_CHANNEL_C10 DIO_CHANNEL(DIO_PORT_C, 10) /**< GPIOC Pin 10 */
#define DIO_CHANNEL_C11 DIO_CHANNEL(DIO_PORT_C, 11) /**< GPIOC Pin 11 */
#define DIO_CHANNEL_C12 DIO_CHANNEL(DIO_PORT_C, 12) /**< GPIOC Pin 12 */
#define DIO_CHANNEL_C13 DIO_CHANNEL(DIO_PORT_C, 13) /**< GPIOC Pin 13 */
#define DIO_CHANNEL_C14 DIO_CHANNEL(DIO_PORT_C, 14) /**< GPIOC Pin 14 */
#define DIO_CHANNEL_C15 DIO_CHANNEL(DIO_PORT_C, 15) /**< GPIOC Pin 15 */

/* GPIOD Channels */
#define DIO_CHANNEL_D0 DIO_CHANNEL(DIO_PORT_D, 0)   /**< GPIOD Pin 0  */
#define DIO_CHANNEL_D1 DIO_CHANNEL(DIO_PORT_D, 1)   /**< GPIOD Pin 1  */
#define DIO_CHANNEL_D2 DIO_CHANNEL(DIO_PORT_D, 2)   /**< GPIOD Pin 2  */
#define DIO_CHANNEL_D3 DIO_CHANNEL(DIO_PORT_D, 3)   /**< GPIOD Pin 3  */
#define DIO_CHANNEL_D4 DIO_CHANNEL(DIO_PORT_D, 4)   /**< GPIOD Pin 4  */
#define DIO_CHANNEL_D5 DIO_CHANNEL(DIO_PORT_D, 5)   /**< GPIOD Pin 5  */
#define DIO_CHANNEL_D6 DIO_CHANNEL(DIO_PORT_D, 6)   /**< GPIOD Pin 6  */
#define DIO_CHANNEL_D7 DIO_CHANNEL(DIO_PORT_D, 7)   /**< GPIOD Pin 7  */
#define DIO_CHANNEL_D8 DIO_CHANNEL(DIO_PORT_D, 8)   /**< GPIOD Pin 8  */
#define DIO_CHANNEL_D9 DIO_CHANNEL(DIO_PORT_D, 9)   /**< GPIOD Pin 9  */
#define DIO_CHANNEL_D10 DIO_CHANNEL(DIO_PORT_D, 10) /**< GPIOD Pin 10 */
#define DIO_CHANNEL_D11 DIO_CHANNEL(DIO_PORT_D, 11) /**< GPIOD Pin 11 */
#define DIO_CHANNEL_D12 DIO_CHANNEL(DIO_PORT_D, 12) /**< GPIOD Pin 12 */
#define DIO_CHANNEL_D13 DIO_CHANNEL(DIO_PORT_D, 13) /**< GPIOD Pin 13 */
#define DIO_CHANNEL_D14 DIO_CHANNEL(DIO_PORT_D, 14) /**< GPIOD Pin 14 */
#define DIO_CHANNEL_D15 DIO_CHANNEL(DIO_PORT_D, 15) /**< GPIOD Pin 15 */

/* ==========================================================================
 *                            TYPE DEFINITIONS
 * ========================================================================== */

/**
 * @brief   Kiểu dữ liệu cho một kênh DIO (Channel).
 * @details Định danh cho một chân (pin) cụ thể.
 */
typedef uint8 Dio_ChannelType;

/**
 * @brief   Kiểu dữ liệu cho một cổng DIO (Port).
 * @details Định danh cho một cổng (port) cụ thể.
 */
typedef uint16 Dio_PortType;

/**
 * @brief   Kiểu dữ liệu cho mức logic của một kênh DIO.
 * @details Các mức logic này sẽ là STD_HIGH (1) hoặc STD_LOW (0).
 */
typedef uint8 Dio_LevelType;

/**
 * @brief   Kiểu dữ liệu cho mức logic của một cổng DIO.
 * @details Mỗi cổng có thể chứa nhiều kênh, do đó mức logic của cổng
 *          là tập hợp trạng thái của các chân đó.
 */
typedef uint16 Dio_PortLevelType;

/**
 * @brief   Cấu trúc định nghĩa một nhóm các kênh DIO (Channel Group).
 * @details Dùng để thao tác với tập hợp con các chân trong cùng một Port.
 */
typedef struct {
  Dio_PortType port; /**< Cổng DIO của nhóm */
  uint8 offset;      /**< Vị trí bắt đầu (độ dịch) của nhóm bit */
  uint8 mask;        /**< Mặt nạ (mask) xác định các bit thuộc nhóm */
} Dio_ChannelGroupType;

/* ==========================================================================
 *                          FUNCTION PROTOTYPES
 * ========================================================================== */

/**
 * @func    Dio_ReadChannel
 * @brief   Đọc trạng thái của một kênh DIO.
 *
 * @param[in] ChannelId ID của kênh DIO cần đọc.
 *
 * @return  Dio_LevelType
 *          - STD_HIGH: Mức logic cao.
 *          - STD_LOW:  Mức logic thấp.
 */
Dio_LevelType Dio_ReadChannel(Dio_ChannelType ChannelId);

/**
 * @func    Dio_WriteChannel
 * @brief   Ghi trạng thái logic cho một kênh DIO.
 *
 * @param[in] ChannelId ID của kênh DIO cần ghi.
 * @param[in] Level     Trạng thái cần ghi (STD_HIGH hoặc STD_LOW).
 *
 * @return  void
 */
void Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level);

/**
 * @func    Dio_ReadPort
 * @brief   Đọc trạng thái của toàn bộ một cổng DIO.
 *
 * @param[in] PortId ID của cổng DIO cần đọc.
 *
 * @return  Dio_PortLevelType Trạng thái logic của toàn bộ cổng.
 */
Dio_PortLevelType Dio_ReadPort(Dio_PortType PortId);

/**
 * @func    Dio_WritePort
 * @brief   Ghi trạng thái logic cho toàn bộ một cổng DIO.
 *
 * @param[in] PortId ID của cổng DIO cần ghi.
 * @param[in] Level  Giá trị trạng thái logic cần ghi.
 *
 * @return  void
 */
void Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level);

/**
 * @func    Dio_ReadChannelGroup
 * @brief   Đọc mức logic của một nhóm kênh DIO.
 *
 * @param[in] GroupIdPtr Con trỏ đến cấu hình nhóm DIO.
 *
 * @return  Dio_PortLevelType Trạng thái logic của nhóm kênh (đã dịch bit).
 */
Dio_PortLevelType Dio_ReadChannelGroup(const Dio_ChannelGroupType *GroupIdPtr);

/**
 * @func    Dio_WriteChannelGroup
 * @brief   Ghi mức logic cho một nhóm kênh DIO.
 *
 * @param[in] GroupIdPtr Con trỏ đến cấu hình nhóm DIO.
 * @param[in] Level      Mức logic cần ghi cho nhóm (chưa dịch bit).
 *
 * @return  void
 */
void Dio_WriteChannelGroup(const Dio_ChannelGroupType *GroupIdPtr,
                           Dio_PortLevelType Level);

/**
 * @func    Dio_GetVersionInfo
 * @brief   Lấy thông tin phiên bản của DIO Driver.
 *
 * @param[out] VersionInfo Con trỏ để lưu thông tin phiên bản.
 *
 * @return  void
 */
void Dio_GetVersionInfo(Std_VersionInfoType *VersionInfo);

/**
 * @func    Dio_FlipChannel
 * @brief   Lật trạng thái logic của một kênh DIO.
 *
 * @param[in] ChannelId ID của kênh DIO cần lật.
 *
 * @return  Dio_LevelType Trạng thái mới của kênh sau khi lật.
 */
Dio_LevelType Dio_FlipChannel(Dio_ChannelType ChannelId);

#endif /* DIO_H */
