/**
  ******************************************************************************
  * @file    debug_log.h
  * @brief   USART1（PA9/PA10，115200 8N1）上的最小调试输出。
  *
  *          仅用于台架验证：上电横幅 + 1 Hz 状态行。
  *          全部为阻塞发送，行长受控在 ~120 字符（约 10 ms @115200）。
  *          不需要日志时把 APP_DEBUG_LOG_ENABLE 置 0 即可整体裁剪。
  ******************************************************************************
  */
#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/** 打印上电横幅（时钟、节点 ID、摇杆握手结果） */
void DebugLog_Init(void);

/** 主循环周期调用；内部按 1 Hz 输出状态行 */
void DebugLog_Update(void);

/** 格式化输出一行（自动追加 \r\n），超长内容会被截断 */
void DebugLog_Printf(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_LOG_H */
