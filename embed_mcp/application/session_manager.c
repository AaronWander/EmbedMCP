#include "session_manager.h"
#include "utils/logging.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// 生成会话ID
char *mcp_session_generate_id(void) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    char *session_id = malloc(33); // 32字符 + null terminator
    if (!session_id) return NULL;
    
    srand((unsigned int)time(NULL) + (unsigned int)getpid());
    
    for (int i = 0; i < 32; i++) {
        session_id[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    session_id[32] = '\0';
    
    return session_id;
}

// 验证会话ID格式
bool mcp_session_validate_id(const char *session_id) {
    if (!session_id) return false;
    
    size_t len = strlen(session_id);
    if (len != 32) return false;
    
    for (size_t i = 0; i < len; i++) {
        char c = session_id[i];
        if (!((c >= 'a' && c <= 'z') || 
              (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9'))) {
            return false;
        }
    }
    
    return true;
}

// 创建默认配置
mcp_session_manager_config_t *mcp_session_manager_config_create_default(void) {
    mcp_session_manager_config_t *config = malloc(sizeof(mcp_session_manager_config_t));
    if (!config) return NULL;
    
    config->max_sessions = 10;
    config->default_session_timeout = 3600; // 1小时
    config->cleanup_interval = 300; // 5分钟
    config->auto_cleanup = true;
    config->strict_session_validation = true;
    
    return config;
}

void mcp_session_manager_config_destroy(mcp_session_manager_config_t *config) {
    free(config);
}

// 创建会话管理器
mcp_session_manager_t *mcp_session_manager_create(const mcp_session_manager_config_t *config) {
    if (!config) return NULL;
    
    mcp_session_manager_t *manager = malloc(sizeof(mcp_session_manager_t));
    if (!manager) return NULL;
    
    // 复制配置
    manager->config = *config;
    
    // 初始化会话存储
    manager->sessions = calloc(config->max_sessions, sizeof(mcp_session_t*));
    if (!manager->sessions) {
        free(manager);
        return NULL;
    }
    
    manager->session_count = 0;
    manager->session_capacity = config->max_sessions;
    
    // 初始化线程安全
    if (pthread_rwlock_init(&manager->sessions_lock, NULL) != 0) {
        free(manager->sessions);
        free(manager);
        return NULL;
    }
    
    if (pthread_mutex_init(&manager->manager_mutex, NULL) != 0) {
        pthread_rwlock_destroy(&manager->sessions_lock);
        free(manager->sessions);
        free(manager);
        return NULL;
    }
    
    // 初始化统计信息
    manager->total_sessions_created = 0;
    manager->sessions_expired = 0;
    manager->sessions_terminated = 0;
    manager->cleanup_running = false;
    
    mcp_log_info("Session manager created with max_sessions=%zu", config->max_sessions);
    
    return manager;
}

// 销毁会话管理器
void mcp_session_manager_destroy(mcp_session_manager_t *manager) {
    if (!manager) return;
    
    // 停止清理线程
    if (manager->cleanup_running) {
        mcp_session_manager_stop(manager);
    }
    
    // 清理所有会话
    pthread_rwlock_wrlock(&manager->sessions_lock);
    for (size_t i = 0; i < manager->session_capacity; i++) {
        if (manager->sessions[i]) {
            mcp_session_terminate(manager->sessions[i]);
            mcp_session_unref(manager->sessions[i]);
        }
    }
    pthread_rwlock_unlock(&manager->sessions_lock);
    
    // 销毁同步原语
    pthread_rwlock_destroy(&manager->sessions_lock);
    pthread_mutex_destroy(&manager->manager_mutex);
    
    // 释放内存
    free(manager->sessions);
    free(manager);
    
    mcp_log_info("Session manager destroyed");
}

// 清理线程函数
static void *session_cleanup_thread(void *arg) {
    mcp_session_manager_t *manager = (mcp_session_manager_t*)arg;
    
    mcp_log_info("Session cleanup thread started");
    
    while (manager->cleanup_running) {
        sleep(manager->config.cleanup_interval);
        
        if (!manager->cleanup_running) break;
        
        mcp_session_manager_cleanup_expired_sessions(manager);
    }
    
    mcp_log_info("Session cleanup thread stopped");
    return NULL;
}

// 启动会话管理器
int mcp_session_manager_start(mcp_session_manager_t *manager) {
    if (!manager) return -1;
    
    pthread_mutex_lock(&manager->manager_mutex);
    
    if (manager->cleanup_running) {
        pthread_mutex_unlock(&manager->manager_mutex);
        return 0; // 已经启动
    }
    
    if (manager->config.auto_cleanup) {
        manager->cleanup_running = true;
        
        if (pthread_create(&manager->cleanup_thread, NULL, session_cleanup_thread, manager) != 0) {
            manager->cleanup_running = false;
            pthread_mutex_unlock(&manager->manager_mutex);
            mcp_log_error("Failed to create session cleanup thread");
            return -1;
        }
    }
    
    pthread_mutex_unlock(&manager->manager_mutex);
    
    mcp_log_info("Session manager started");
    return 0;
}

// 停止会话管理器
int mcp_session_manager_stop(mcp_session_manager_t *manager) {
    if (!manager) return -1;
    
    pthread_mutex_lock(&manager->manager_mutex);
    
    if (!manager->cleanup_running) {
        pthread_mutex_unlock(&manager->manager_mutex);
        return 0; // 已经停止
    }
    
    manager->cleanup_running = false;
    pthread_mutex_unlock(&manager->manager_mutex);
    
    // 等待清理线程结束
    if (pthread_join(manager->cleanup_thread, NULL) != 0) {
        mcp_log_warn("Failed to join session cleanup thread");
    }
    
    mcp_log_info("Session manager stopped");
    return 0;
}

// 创建会话
mcp_session_t *mcp_session_manager_create_session(mcp_session_manager_t *manager,
                                                 const char *session_id) {
    if (!manager) return NULL;
    
    // 生成会话ID（如果未提供）
    char *id = NULL;
    if (session_id) {
        if (!mcp_session_validate_id(session_id)) {
            mcp_log_error("Invalid session ID format: %s", session_id);
            return NULL;
        }
        id = strdup(session_id);
    } else {
        id = mcp_session_generate_id();
    }
    
    if (!id) return NULL;
    
    // 检查会话是否已存在
    pthread_rwlock_rdlock(&manager->sessions_lock);
    for (size_t i = 0; i < manager->session_capacity; i++) {
        if (manager->sessions[i] && 
            strcmp(manager->sessions[i]->session_id, id) == 0) {
            pthread_rwlock_unlock(&manager->sessions_lock);
            free(id);
            mcp_log_warn("Session already exists: %s", id);
            return NULL;
        }
    }
    pthread_rwlock_unlock(&manager->sessions_lock);
    
    // 创建新会话
    mcp_session_t *session = malloc(sizeof(mcp_session_t));
    if (!session) {
        free(id);
        return NULL;
    }
    
    memset(session, 0, sizeof(mcp_session_t));
    
    session->session_id = id;
    session->state = MCP_SESSION_STATE_CREATED;
    session->created_time = time(NULL);
    session->last_activity = session->created_time;
    session->expires_at = session->created_time + manager->config.default_session_timeout;
    session->ref_count = 1;
    
    // 初始化互斥锁
    if (pthread_mutex_init(&session->mutex, NULL) != 0) {
        free(session->session_id);
        free(session);
        return NULL;
    }
    
    // 添加到管理器
    pthread_rwlock_wrlock(&manager->sessions_lock);
    
    // 查找空位
    bool added = false;
    for (size_t i = 0; i < manager->session_capacity; i++) {
        if (!manager->sessions[i]) {
            manager->sessions[i] = session;
            manager->session_count++;
            manager->total_sessions_created++;
            added = true;
            break;
        }
    }
    
    pthread_rwlock_unlock(&manager->sessions_lock);
    
    if (!added) {
        pthread_mutex_destroy(&session->mutex);
        free(session->session_id);
        free(session);
        mcp_log_error("Session manager is full, cannot create new session");
        return NULL;
    }
    
    mcp_log_info("Session created: %s", session->session_id);
    return session;
}

// 查找会话
mcp_session_t *mcp_session_manager_find_session(mcp_session_manager_t *manager,
                                               const char *session_id) {
    if (!manager || !session_id) return NULL;

    pthread_rwlock_rdlock(&manager->sessions_lock);

    for (size_t i = 0; i < manager->session_capacity; i++) {
        if (manager->sessions[i] &&
            strcmp(manager->sessions[i]->session_id, session_id) == 0) {
            mcp_session_t *session = mcp_session_ref(manager->sessions[i]);
            pthread_rwlock_unlock(&manager->sessions_lock);
            return session;
        }
    }

    pthread_rwlock_unlock(&manager->sessions_lock);
    return NULL;
}

// 移除会话
int mcp_session_manager_remove_session(mcp_session_manager_t *manager,
                                      const char *session_id) {
    if (!manager || !session_id) return -1;

    pthread_rwlock_wrlock(&manager->sessions_lock);

    for (size_t i = 0; i < manager->session_capacity; i++) {
        if (manager->sessions[i] &&
            strcmp(manager->sessions[i]->session_id, session_id) == 0) {
            mcp_session_t *session = manager->sessions[i];
            manager->sessions[i] = NULL;
            manager->session_count--;

            pthread_rwlock_unlock(&manager->sessions_lock);

            mcp_session_terminate(session);
            mcp_session_unref(session);

            mcp_log_info("Session removed: %s", session_id);
            return 0;
        }
    }

    pthread_rwlock_unlock(&manager->sessions_lock);
    return -1;
}

// 会话引用计数
mcp_session_t *mcp_session_ref(mcp_session_t *session) {
    if (!session) return NULL;

    pthread_mutex_lock(&session->mutex);
    session->ref_count++;
    pthread_mutex_unlock(&session->mutex);

    return session;
}

void mcp_session_unref(mcp_session_t *session) {
    if (!session) return;

    pthread_mutex_lock(&session->mutex);
    session->ref_count--;
    bool should_destroy = (session->ref_count == 0);
    pthread_mutex_unlock(&session->mutex);

    if (should_destroy) {
        // 清理会话资源
        if (session->protocol_state) {
            // TODO: 清理协议状态
        }

        pthread_mutex_destroy(&session->mutex);
        free(session->session_id);
        free(session->client_name);
        free(session->client_version);
        free(session->protocol_version);
        free(session);
    }
}

// 会话状态管理
mcp_session_state_t mcp_session_get_state(const mcp_session_t *session) {
    if (!session) return MCP_SESSION_STATE_TERMINATED;
    return session->state;
}

bool mcp_session_is_active(const mcp_session_t *session) {
    if (!session) return false;
    return session->state == MCP_SESSION_STATE_ACTIVE;
}

bool mcp_session_is_expired(const mcp_session_t *session) {
    if (!session) return true;
    return time(NULL) > session->expires_at;
}

int mcp_session_update_activity(mcp_session_t *session) {
    if (!session) return -1;

    pthread_mutex_lock(&session->mutex);
    session->last_activity = time(NULL);
    pthread_mutex_unlock(&session->mutex);

    return 0;
}

int mcp_session_extend_expiry(mcp_session_t *session, time_t additional_time) {
    if (!session) return -1;

    pthread_mutex_lock(&session->mutex);
    session->expires_at += additional_time;
    pthread_mutex_unlock(&session->mutex);

    return 0;
}

// 会话信息获取
const char *mcp_session_get_id(const mcp_session_t *session) {
    return session ? session->session_id : NULL;
}

const char *mcp_session_get_client_name(const mcp_session_t *session) {
    return session ? session->client_name : NULL;
}

const char *mcp_session_get_protocol_version(const mcp_session_t *session) {
    return session ? session->protocol_version : NULL;
}

time_t mcp_session_get_created_time(const mcp_session_t *session) {
    return session ? session->created_time : 0;
}

time_t mcp_session_get_last_activity(const mcp_session_t *session) {
    return session ? session->last_activity : 0;
}

// 清理过期会话
int mcp_session_manager_cleanup_expired_sessions(mcp_session_manager_t *manager) {
    if (!manager) return -1;

    time_t now = time(NULL);
    int cleaned = 0;

    pthread_rwlock_wrlock(&manager->sessions_lock);

    for (size_t i = 0; i < manager->session_capacity; i++) {
        if (manager->sessions[i] && now > manager->sessions[i]->expires_at) {
            mcp_session_t *session = manager->sessions[i];
            manager->sessions[i] = NULL;
            manager->session_count--;
            manager->sessions_expired++;
            cleaned++;

            mcp_log_info("Session expired and cleaned: %s", session->session_id);

            // 在锁外清理
            pthread_rwlock_unlock(&manager->sessions_lock);
            mcp_session_terminate(session);
            mcp_session_unref(session);
            pthread_rwlock_wrlock(&manager->sessions_lock);
        }
    }

    pthread_rwlock_unlock(&manager->sessions_lock);

    if (cleaned > 0) {
        mcp_log_info("Cleaned %d expired sessions", cleaned);
    }

    return cleaned;
}

// 获取会话统计
size_t mcp_session_manager_get_session_count(const mcp_session_manager_t *manager) {
    return manager ? manager->session_count : 0;
}

size_t mcp_session_manager_get_active_session_count(const mcp_session_manager_t *manager) {
    if (!manager) return 0;

    size_t active_count = 0;

    // Cast away const for pthread functions
    mcp_session_manager_t *non_const_manager = (mcp_session_manager_t*)manager;
    pthread_rwlock_rdlock(&non_const_manager->sessions_lock);
    for (size_t i = 0; i < manager->session_capacity; i++) {
        if (manager->sessions[i] && mcp_session_is_active(manager->sessions[i])) {
            active_count++;
        }
    }
    pthread_rwlock_unlock(&non_const_manager->sessions_lock);

    return active_count;
}

// 会话生命周期管理
int mcp_session_initialize(mcp_session_t *session,
                          const char *protocol_version,
                          const cJSON *client_capabilities,
                          const cJSON *client_info) {
    if (!session) return -1;

    pthread_mutex_lock(&session->mutex);

    if (session->state != MCP_SESSION_STATE_CREATED) {
        pthread_mutex_unlock(&session->mutex);
        return -1;
    }

    session->state = MCP_SESSION_STATE_INITIALIZING;

    if (protocol_version) {
        free(session->protocol_version);
        session->protocol_version = strdup(protocol_version);
    }

    // 处理客户端信息
    if (client_info) {
        const cJSON *name = cJSON_GetObjectItem(client_info, "name");
        if (cJSON_IsString(name)) {
            free(session->client_name);
            session->client_name = strdup(name->valuestring);
        }

        const cJSON *version = cJSON_GetObjectItem(client_info, "version");
        if (cJSON_IsString(version)) {
            free(session->client_version);
            session->client_version = strdup(version->valuestring);
        }
    }

    session->state = MCP_SESSION_STATE_ACTIVE;
    session->last_activity = time(NULL);

    pthread_mutex_unlock(&session->mutex);

    mcp_log_info("Session initialized: %s", session->session_id);
    return 0;
}

int mcp_session_activate(mcp_session_t *session) {
    if (!session) return -1;

    pthread_mutex_lock(&session->mutex);
    session->state = MCP_SESSION_STATE_ACTIVE;
    session->last_activity = time(NULL);
    pthread_mutex_unlock(&session->mutex);

    return 0;
}

int mcp_session_deactivate(mcp_session_t *session) {
    if (!session) return -1;

    pthread_mutex_lock(&session->mutex);
    session->state = MCP_SESSION_STATE_INACTIVE;
    pthread_mutex_unlock(&session->mutex);

    return 0;
}

int mcp_session_terminate(mcp_session_t *session) {
    if (!session) return -1;

    pthread_mutex_lock(&session->mutex);
    session->state = MCP_SESSION_STATE_TERMINATED;
    pthread_mutex_unlock(&session->mutex);

    mcp_log_info("Session terminated: %s", session->session_id);
    return 0;
}

// 状态转换字符串
const char *mcp_session_state_to_string(mcp_session_state_t state) {
    switch (state) {
        case MCP_SESSION_STATE_CREATED: return "CREATED";
        case MCP_SESSION_STATE_INITIALIZING: return "INITIALIZING";
        case MCP_SESSION_STATE_ACTIVE: return "ACTIVE";
        case MCP_SESSION_STATE_INACTIVE: return "INACTIVE";
        case MCP_SESSION_STATE_EXPIRED: return "EXPIRED";
        case MCP_SESSION_STATE_TERMINATED: return "TERMINATED";
        default: return "UNKNOWN";
    }
}
