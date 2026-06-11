#ifndef APP_CLOUD_BIND_CMD_H
#define APP_CLOUD_BIND_CMD_H

#include <stdint.h>

/** 解析 property/set JSON，处理 bind_pair_code。 @return 1=已识别并处理绑定类消息 */
uint8_t app_cloud_bind_handle_json(const char *json, uint16_t len);

/** 访客临时密码已使用：通知后端作废 App 列表 */
void app_cloud_publish_temp_password_used(const char *guest_account);
/** 延后到 cloud poll 发送，避免阻塞开锁 GUI */
void app_cloud_queue_temp_password_used(const char *guest_account);
void app_cloud_flush_deferred_actions(void);
void app_cloud_bind_flush_pending_user_list(void);

/** 上行完整用户表（门锁为真源，供 App/后端覆盖缓存） */
void app_cloud_publish_user_list_snapshot(void);

#endif
