/* SPDX-FileCopyrightText: 2024-2026 HALA Academy */
/* SPDX-License-Identifier: Apache-2.0 */
/**
 * @file    Dio.c
 * @brief   Digital Input/Output (DIO) Driver Source File
 * @details File này chứa phần hiện thực các API của DIO Driver
 *          theo chuẩn AUTOSAR dành cho STM32F103. Driver này
 *          cung cấp khả năng đọc và ghi các mức logic của
 *          các chân (GPIO pin) và các cổng (GPIO port) trong
 *          vi điều khiển STM32F103.
 *
 * @version 1.0.0
 * @date    2024-09-30
 * @author  HALA Academy
 * @see     https://hala.edu.vn
 */

/* ==========================================================================
 *                                 INCLUDES
 * ========================================================================== */
#include "Dio.h"

/* ==========================================================================
 *                          FUNCTION DEFINITIONS
 * ========================================================================== */

/**
 * @func    Dio_ReadChannel
 * @brief   Đọc trạng thái logic của một kênh DIO.
 * @details Hàm này đọc trạng thái logic của một chân GPIO (DIO Channel)
 *          trên vi điều khiển STM32F103. Hàm sẽ xác định cổng và chân GPIO
 *          dựa trên ChannelId và sau đó trả về mức logic (HIGH hoặc LOW).
 *
 * @param[in]  ChannelId ID của kênh DIO cần đọc (xác định bởi macro
 * DIO_CHANNEL)
 *
 * @return  Dio_LevelType
 *          - STD_HIGH: Nếu chân có mức logic cao (Vcc)
 *          - STD_LOW:  Nếu chân có mức logic thấp (GND)
 */
Dio_LevelType Dio_ReadChannel(Dio_ChannelType ChannelId) {
  GPIO_TypeDef *GPIO_Port; /* Con trỏ đến cấu trúc định nghĩa cổng GPIO */
  uint16_t GPIO_Pin;       /* Số chân GPIO tương ứng */

  /* Bước 1: Xác định cổng GPIO bằng macro DIO_GET_PORT */
  GPIO_Port = DIO_GET_PORT(ChannelId);

  /* Kiểm tra xem cổng GPIO có hợp lệ không */
  if (GPIO_Port == NULL) {
    return STD_LOW; /* Trả về STD_LOW nếu cổng không hợp lệ */
  }

  /* Bước 2: Xác định chân GPIO bằng macro DIO_GET_PIN */
  GPIO_Pin = DIO_GET_PIN(ChannelId);

  /* Bước 3: Đọc trạng thái logic của chân GPIO tương ứng */
  if (GPIO_ReadInputDataBit(GPIO_Port, GPIO_Pin) == Bit_SET) {
    return STD_HIGH; /* Nếu chân có mức logic cao, trả về STD_HIGH */
  } else {
    return STD_LOW; /* Nếu chân có mức logic thấp, trả về STD_LOW */
  }
}

/**
 * @func    Dio_WriteChannel
 * @brief   Ghi trạng thái logic cho một kênh DIO.
 * @details Hàm này ghi trạng thái logic (HIGH hoặc LOW) cho một chân GPIO
 *          (DIO Channel) trên vi điều khiển STM32F103. Hàm xác định cổng
 *          và chân GPIO dựa trên ChannelId và sau đó ghi mức logic được chỉ
 * định.
 *
 * @param[in]  ChannelId ID của kênh DIO cần ghi (xác định bởi macro
 * DIO_CHANNEL)
 * @param[in]  Level     Trạng thái logic cần ghi vào kênh DIO (STD_HIGH hoặc
 * STD_LOW)
 *
 * @return  void
 */
void Dio_WriteChannel(Dio_ChannelType ChannelId, Dio_LevelType Level) {
  GPIO_TypeDef *GPIO_Port; /* Con trỏ đến cấu trúc định nghĩa cổng GPIO */
  uint16_t GPIO_Pin;       /* Số chân GPIO tương ứng */

  /* Bước 1: Xác định cổng GPIO bằng macro DIO_GET_PORT */
  GPIO_Port = DIO_GET_PORT(ChannelId);

  /* Kiểm tra xem cổng GPIO có hợp lệ không */
  if (GPIO_Port == NULL) {
    return; /* Không xác định được cổng, thoát khỏi hàm */
  }

  /* Bước 2: Xác định chân GPIO bằng macro DIO_GET_PIN */
  GPIO_Pin = DIO_GET_PIN(ChannelId);

  /* Bước 3: Ghi trạng thái logic cho chân GPIO tương ứng */
  if (Level == STD_HIGH) {
    GPIO_SetBits(GPIO_Port, GPIO_Pin); /* Đặt chân GPIO ở mức HIGH */
  } else {
    GPIO_ResetBits(GPIO_Port, GPIO_Pin); /* Đặt chân GPIO ở mức LOW */
  }
}

/**
 * @func    Dio_ReadPort
 * @brief   Đọc trạng thái logic của toàn bộ cổng DIO.
 * @details Hàm này đọc trạng thái logic của tất cả các chân GPIO
 *          trong một cổng (GPIO port) trên vi điều khiển STM32F103.
 *          Cổng GPIO được xác định bởi PortId và toàn bộ trạng thái
 *          của cổng được trả về dưới dạng một giá trị 16-bit.
 *
 * @param[in]  PortId ID của cổng DIO cần đọc (DIO_PORT_A, DIO_PORT_B, ...)
 *
 * @return  Dio_PortLevelType
 *          Trạng thái logic của toàn bộ cổng DIO (bao gồm nhiều chân GPIO)
 */
Dio_PortLevelType Dio_ReadPort(Dio_PortType PortId) {
  /* Bước 1: Sử dụng macro DIO_GET_PORT để xác định cổng GPIO */
  GPIO_TypeDef *GPIO_Port = DIO_GET_PORT(PortId);

  /* Kiểm tra xem cổng GPIO có hợp lệ không */
  if (GPIO_Port == NULL) {
    return 0; /* Trả về 0 nếu PortId không hợp lệ */
  }

  /* Bước 2: Đọc và trả về trạng thái của toàn bộ cổng GPIO */
  return (Dio_PortLevelType)GPIO_ReadInputData(GPIO_Port);
}

/**
 * @func    Dio_WritePort
 * @brief   Ghi trạng thái logic cho toàn bộ cổng DIO.
 * @details Hàm này ghi một giá trị trạng thái logic (16-bit) cho
 *          toàn bộ cổng GPIO trên vi điều khiển STM32F103. Cổng GPIO
 *          được xác định bởi PortId và giá trị trạng thái logic cần
 *          ghi sẽ được truyền vào dưới dạng tham số Level.
 *
 * @param[in]  PortId ID của cổng DIO cần ghi (DIO_PORT_A, DIO_PORT_B, ...)
 * @param[in]  Level  Giá trị trạng thái logic cần ghi cho toàn bộ cổng GPIO
 *
 * @return  void
 */
void Dio_WritePort(Dio_PortType PortId, Dio_PortLevelType Level) {
  /* Sử dụng macro DIO_GET_PORT để xác định cổng GPIO */
  GPIO_TypeDef *GPIO_Port = DIO_GET_PORT(PortId);

  /* Kiểm tra xem cổng GPIO có hợp lệ không */
  if (GPIO_Port == NULL) {
    return; /* Nếu cổng không hợp lệ, thoát hàm */
  }

  /* Ghi trạng thái logic cho toàn bộ cổng GPIO */
  GPIO_Write(GPIO_Port, Level);
}

/**
 * @func    Dio_ReadChannelGroup
 * @brief   Đọc trạng thái logic của một nhóm kênh DIO.
 * @details Hàm này đọc trạng thái logic của một nhóm chân GPIO
 *          (DIO Channel Group) trên một cổng GPIO được xác định
 *          thông qua cấu trúc **GroupIdPtr**. Kết quả đọc sẽ được
 *          dịch và trả về dựa trên mask và offset của nhóm.
 *
 * @param[in]  GroupIdPtr Con trỏ đến cấu trúc nhóm DIO (chứa thông tin cổng,
 * mask và offset)
 *
 * @return  Dio_PortLevelType
 *          Trạng thái logic của nhóm kênh DIO
 */
Dio_PortLevelType Dio_ReadChannelGroup(const Dio_ChannelGroupType *GroupIdPtr) {
  /* Sử dụng macro DIO_GET_PORT để xác định cổng GPIO */
  GPIO_TypeDef *GPIO_Port = DIO_GET_PORT(GroupIdPtr->port);

  /* Kiểm tra xem cổng GPIO có hợp lệ không */
  if (GPIO_Port == NULL) {
    return 0; /* Trả về 0 nếu port không hợp lệ */
  }

  /* Đọc trạng thái logic của toàn bộ cổng GPIO */
  uint16_t portData = GPIO_ReadInputData(GPIO_Port);

  /* Áp dụng mask và offset để lấy giá trị của nhóm kênh */
  return (Dio_PortLevelType)((portData & GroupIdPtr->mask) >>
                             GroupIdPtr->offset);
}

/**
 * @func    Dio_WriteChannelGroup
 * @brief   Ghi trạng thái logic cho một nhóm kênh DIO.
 * @details Hàm này ghi một giá trị trạng thái logic vào một nhóm chân GPIO
 *          (DIO Channel Group) trên một cổng GPIO được xác định thông qua
 *          cấu trúc **GroupIdPtr**. Giá trị sẽ được ghi vào nhóm các chân
 *          xác định bởi mask và offset.
 *
 * @param[in]  GroupIdPtr Con trỏ đến cấu trúc nhóm DIO (chứa thông tin cổng,
 * mask và offset).
 * @param[in]  Level      Giá trị trạng thái logic cần ghi cho nhóm kênh DIO.
 *
 * @return  void
 */
void Dio_WriteChannelGroup(const Dio_ChannelGroupType *GroupIdPtr,
                           Dio_PortLevelType Level) {
  /* Sử dụng macro DIO_GET_PORT để xác định cổng GPIO */
  GPIO_TypeDef *GPIO_Port = DIO_GET_PORT(GroupIdPtr->port);

  /* Kiểm tra xem cổng GPIO có hợp lệ không */
  if (GPIO_Port == NULL) {
    return; /* Nếu không xác định được cổng, thoát hàm */
  }

  /* Đọc dữ liệu từ cổng GPIO */
  uint16_t portData = GPIO_ReadInputData(GPIO_Port);

  /* Xóa các bit trong nhóm, sau đó ghi giá trị mới cho nhóm */
  portData &= ~(GroupIdPtr->mask); /* Xóa các bit tương ứng với mask */
  portData |= ((Level << GroupIdPtr->offset) &
               GroupIdPtr->mask); /* Ghi giá trị mới vào nhóm chân */

  /* Ghi lại dữ liệu đã chỉnh sửa vào cổng GPIO */
  GPIO_Write(GPIO_Port, portData);
}

/**
 * @func    Dio_GetVersionInfo
 * @brief   Hàm lấy thông tin phiên bản của DIO Driver.
 * @details Hàm này trả về thông tin về phiên bản của DIO driver.
 *
 * @param[out] VersionInfo Con trỏ đến cấu trúc Std_VersionInfoType để nhận
 * thông tin phiên bản
 *
 * @return  void
 */
void Dio_GetVersionInfo(Std_VersionInfoType *VersionInfo) {
  /* Gán các thông tin phiên bản */
  VersionInfo->vendorID = 0x1234; /* Ví dụ ID nhà cung cấp */
  VersionInfo->moduleID = 0x5678; /* Ví dụ ID module DIO */
  VersionInfo->sw_major_version = 1;
  VersionInfo->sw_minor_version = 0;
  VersionInfo->sw_patch_version = 0;
}

/**
 * @func    Dio_FlipChannel
 * @brief   Hàm lật giá trị logic của một kênh DIO.
 * @details Hàm này lật trạng thái logic của một chân GPIO (HIGH thành LOW hoặc
 * ngược lại).
 *
 * @param[in]  ChannelId ID của kênh DIO cần lật giá trị
 *
 * @return  Dio_LevelType
 *          Trạng thái logic mới của kênh sau khi lật
 */
Dio_LevelType Dio_FlipChannel(Dio_ChannelType ChannelId) {
  Dio_LevelType currentLevel = Dio_ReadChannel(ChannelId);

  /* Lật giá trị logic của kênh */
  if (currentLevel == STD_HIGH) {
    Dio_WriteChannel(ChannelId, STD_LOW);
    return STD_LOW;
  } else {
    Dio_WriteChannel(ChannelId, STD_HIGH);
    return STD_HIGH;
  }
}
