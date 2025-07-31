<p align="center">
  <a href="./README.md"><img alt="README in English" src="https://img.shields.io/badge/English-d9d9d9"></a>
  <a href="./README_zh.md"><img alt="简体中文版自述文件" src="https://img.shields.io/badge/简体中文-d9d9d9"></a>
</p>

# EmbedMCP - 嵌入式MCP服务器库

一个轻量级的C语言库，用于创建基于纯业务函数的MCP（模型上下文协议）服务器。

## 项目状态

✅ **工具系统** - 完整实现，支持灵活的函数API
✅ **多会话支持** - 并发连接与会话管理
✅ **HAL架构** - 硬件抽象层，支持跨平台开发
✅ **跨平台就绪** - 通过HAL支持Linux、嵌入式Linux、RTOS
✅ **HTTP/STDIO传输** - 完整的MCP协议支持
🚧 **资源系统** - 即将推出
🚧 **提示系统** - 即将推出
🚧 **采样系统** - 即将推出

目前，EmbedMCP专注于MCP协议的**工具**部分，让您能够创建强大的自定义工具MCP服务器并支持多个并发客户端。该库具有硬件抽象层（HAL），使相同的应用程序代码能够在Linux、嵌入式Linux和各种RTOS平台上运行而无需修改！

## 特性

- **跨平台** - 相同代码通过HAL在Linux、嵌入式Linux、RTOS上运行
- **多会话支持** - 处理多个并发客户端，支持会话管理
- **易于集成** - 复制一个文件夹，包含一个头文件即可
- **多种传输** - HTTP和STDIO支持（RTOS支持UART/SPI/CAN）

## 跨平台架构

EmbedMCP具有**硬件抽象层（HAL）**，使相同的应用程序代码能够在多个平台上运行而无需修改：

### 支持的平台
- ✅ **嵌入式Linux** - 树莓派、嵌入式系统
- 🚧 **FreeRTOS** - 实时操作系统
- 🚧 **ROS2** - 机器人应用
- 🚧 **Zephyr** - 物联网和嵌入式应用

### 一次编写，到处运行
```c
// 这段代码在所有平台上完全相同
double add_numbers(double a, double b) {
    return a + b;  // 纯业务逻辑
}

int main() {
    embed_mcp_config_t config = {
        .name = "MyApp", .version = "1.0.0", .port = 8080
    };

    embed_mcp_server_t *server = embed_mcp_create(&config);

    // 注册加法函数
    const char* param_names[] = {"a", "b"};
    mcp_param_type_t param_types[] = {MCP_PARAM_DOUBLE, MCP_PARAM_DOUBLE};
    embed_mcp_add_tool(server, "add", "Add numbers", param_names, param_types, 2, MCP_RETURN_DOUBLE, add_numbers);

    embed_mcp_run(server, EMBED_MCP_TRANSPORT_HTTP);  // 在Linux、RTOS、ROS2等平台上都能工作
    embed_mcp_destroy(server);
    return 0;
}
```



## 快速开始

1. 复制`embed_mcp/`文件夹到您的项目
2. 包含`#include "embed_mcp/embed_mcp.h"`
3. 编译所有`.c`文件

完成！您有了一个可工作的MCP服务器。

**💡 提示：** 查看 `examples/` 文件夹获取完整的工作示例！

## 集成指南

**💡 快速开始：** 查看 `examples/` 文件夹中的完整示例！

### 步骤1：复制库文件

将 `embed_mcp/` 文件夹复制到您的项目：

```bash
# 将整个embed_mcp文件夹复制到您的项目
cp -r /path/to/EmbedMCP/embed_mcp/ your_project/
```

您的项目结构将如下所示：
```
your_project/
├── main.c                     # 您的应用程序代码
├── embed_mcp/                 # EmbedMCP库（已复制）
│   ├── embed_mcp.h           # 主API头文件
│   ├── embed_mcp.c           # 主API实现
│   ├── Makefile.inc          # Makefile配置
│   ├── application/          # 会话管理
│   ├── cjson/                # JSON依赖
│   ├── hal/                  # 硬件抽象层
│   ├── protocol/             # MCP协议实现
│   ├── tools/                # 工具系统
│   ├── transport/            # HTTP/STDIO传输
│   └── utils/                # 工具库
└── Makefile
```

### 步骤2：包含头文件

在您的源代码中，包含主头文件：

```c
#include "embed_mcp.h"

// 您的业务函数 - 无需处理JSON！
double add_numbers(double a, double b) {
    return a + b;
}

int main() {
    // 创建服务器配置
    embed_mcp_config_t config = {
        .name = "MyApp",
        .version = "1.0.0",
        .host = "0.0.0.0",      // HTTP绑定地址
        .port = 8080,           // HTTP端口
        .path = "/mcp",         // HTTP端点路径
        .max_tools = 100,       // 最大工具数量
        .debug = 0,             // 调试日志（0=关闭，1=开启）

        // 多会话配置
        .max_connections = 10,  // 最大并发连接数
        .session_timeout = 3600,// 会话超时时间（秒）
        .enable_sessions = 1,   // 启用会话管理
        .auto_cleanup = 1       // 自动清理过期会话
    };

    // 创建服务器
    embed_mcp_server_t *server = embed_mcp_create(&config);

    // 注册您的函数，指定参数名称和类型
    const char* param_names[] = {"a", "b"};
    mcp_param_type_t param_types[] = {MCP_PARAM_DOUBLE, MCP_PARAM_DOUBLE};

    embed_mcp_add_tool(server, "add", "两数相加",
                       param_names, param_types, 2,
                       MCP_RETURN_DOUBLE, add_numbers);

    // 运行服务器
    embed_mcp_run(server, EMBED_MCP_TRANSPORT_HTTP);

    // 清理
    embed_mcp_destroy(server);
    return 0;
}
```

### 步骤3：编译您的项目

**方式1：使用提供的Makefile配置**
```makefile
# 在您的Makefile中包含
include embed_mcp/Makefile.inc

my_app: main.c $(EMBED_MCP_SOURCES)
	$(CC) $(EMBED_MCP_INCLUDES) main.c $(EMBED_MCP_SOURCES) $(EMBED_MCP_LIBS) -o my_app
```

**方式2：直接编译**
```bash
gcc main.c embed_mcp/*.c embed_mcp/*/*.c embed_mcp/cjson/*.c \
    -Iembed_mcp -lpthread -lm -o my_app
```

## 核心数据结构

### 服务器配置 (`embed_mcp_config_t`)

```c
typedef struct {
    const char *name;           // 服务器名称（在MCP协议中显示）
    const char *version;        // 服务器版本（在MCP协议中显示）
    const char *host;           // HTTP绑定地址（默认："0.0.0.0"）
    int port;                   // HTTP端口号（默认：8080）
    const char *path;           // HTTP端点路径（默认："/mcp"）
    int max_tools;              // 允许的最大工具数量（默认：100）
    int debug;                  // 启用调试日志（0=关闭，1=开启，默认：0）
} embed_mcp_config_t;
```

**配置字段：**

| 字段 | 类型 | 描述 | 典型值 |
|------|------|------|--------|
| `name` | `const char*` | 服务器名称（在MCP协议中显示） | `"MyApp"` |
| `version` | `const char*` | 服务器版本（在MCP协议中显示） | `"1.0.0"` |
| `host` | `const char*` | HTTP绑定地址 | `"0.0.0.0"` |
| `port` | `int` | HTTP端口号 | `8080` |
| `path` | `const char*` | HTTP端点路径 | `"/mcp"` |
| `max_tools` | `int` | 允许的最大工具数量 | `100` |
| `debug` | `int` | 启用调试日志（0=关闭，1=开启） | `0` |

### 参数描述 (`mcp_param_desc_t`)

```c
typedef struct {
    const char *name;                   // 参数名称（在JSON中使用）
    const char *description;            // LLM可读的参数描述
    mcp_param_category_t category;      // 参数类别（单值/数组/对象）
    int required;                       // 1表示必需，0表示可选

    union {
        mcp_param_type_t single_type;   // 单值参数
        mcp_array_desc_t array_desc;    // 数组参数
        const char *object_schema;      // 复杂对象的JSON Schema字符串
    };
} mcp_param_desc_t;
```

### 参数访问器 (`mcp_param_accessor_t`)

参数访问器提供对工具参数的类型安全访问：

```c
struct mcp_param_accessor {
    // 基本类型的类型安全获取器
    int64_t (*get_int)(mcp_param_accessor_t* self, const char* name);
    double (*get_double)(mcp_param_accessor_t* self, const char* name);
    const char* (*get_string)(mcp_param_accessor_t* self, const char* name);
    int (*get_bool)(mcp_param_accessor_t* self, const char* name);

    // 常见MCP模式的数组获取器
    double* (*get_double_array)(mcp_param_accessor_t* self, const char* name, size_t* count);
    char** (*get_string_array)(mcp_param_accessor_t* self, const char* name, size_t* count);
    int64_t* (*get_int_array)(mcp_param_accessor_t* self, const char* name, size_t* count);

    // 工具函数
    int (*has_param)(mcp_param_accessor_t* self, const char* name);
    size_t (*get_param_count)(mcp_param_accessor_t* self);

    // 对于罕见的复杂情况：直接JSON访问
    const cJSON* (*get_json)(mcp_param_accessor_t* self, const char* name);
};
```

## API参考

### 核心函数

#### 服务器管理

```c
// 创建MCP服务器实例
embed_mcp_server_t *embed_mcp_create(const embed_mcp_config_t *config);

// 销毁服务器实例并释放资源
void embed_mcp_destroy(embed_mcp_server_t *server);

// 停止运行中的服务器（可从信号处理器调用）
void embed_mcp_stop(embed_mcp_server_t *server);
```

#### 服务器执行

```c
// 使用指定传输方式运行服务器
// transport: EMBED_MCP_TRANSPORT_STDIO 或 EMBED_MCP_TRANSPORT_HTTP
// 此函数会阻塞直到服务器停止
int embed_mcp_run(embed_mcp_server_t *server, embed_mcp_transport_t transport);
```

#### 工具注册

```c
// 注册工具函数，支持灵活的参数规范
int embed_mcp_add_tool(embed_mcp_server_t *server,
                       const char *name,
                       const char *description,
                       const char *param_names[],
                       mcp_param_type_t param_types[],
                       size_t param_count,
                       mcp_return_type_t return_type,
                       void *function_ptr);
```

**函数参数：**
- `server` - 使用 `embed_mcp_create()` 创建的服务器实例
- `name` - 唯一工具名称（在MCP协议中使用）
- `description` - 人类可读的工具描述
- `param_names` - 参数名称数组
- `param_types` - 参数类型数组
- `param_count` - 参数数量
- `return_type` - 返回类型（`MCP_RETURN_DOUBLE`、`MCP_RETURN_INT`、`MCP_RETURN_STRING`、`MCP_RETURN_VOID`）
- `function_ptr` - 指向您的C函数的指针

### 错误处理

```c
// 获取最后的错误消息（如果没有错误返回NULL）
const char *embed_mcp_get_error(void);
```

### 参数类型

注册工具时使用这些参数类型：

```c
typedef enum {
    MCP_PARAM_INT,        // 整数参数
    MCP_PARAM_DOUBLE,     // 双精度浮点数参数
    MCP_PARAM_STRING,     // 字符串参数
    MCP_PARAM_CHAR        // 字符参数
} mcp_param_type_t;
```

**使用示例：**
```c
// 对于函数: int add(int a, int b)
const char* param_names[] = {"a", "b"};
mcp_param_type_t param_types[] = {MCP_PARAM_INT, MCP_PARAM_INT};

embed_mcp_add_tool(server, "add", "两个整数相加",
                   param_names, param_types, 2, MCP_RETURN_INT, add_function);
```

### 参数定义宏

这些宏简化了参数定义：

```c
// 单值参数
MCP_PARAM_DOUBLE_DEF(name, description, required)   // 双精度参数
MCP_PARAM_INT_DEF(name, description, required)      // 整数参数
MCP_PARAM_STRING_DEF(name, description, required)   // 字符串参数
MCP_PARAM_BOOL_DEF(name, description, required)     // 布尔参数

// 数组参数
MCP_PARAM_ARRAY_DOUBLE_DEF(name, desc, elem_desc, required)  // 双精度数组
MCP_PARAM_ARRAY_STRING_DEF(name, desc, elem_desc, required)  // 字符串数组
MCP_PARAM_ARRAY_INT_DEF(name, desc, elem_desc, required)     // 整数数组

// 复杂对象参数
MCP_PARAM_OBJECT_DEF(name, description, json_schema, required)  // 自定义JSON对象
```

**参数：**
- `name` - 参数名称（字符串字面量）
- `description` - 人类可读描述（字符串字面量）
- `elem_desc` - 数组元素描述（字符串字面量）
- `json_schema` - 对象验证的JSON Schema字符串
- `required` - 1表示必需，0表示可选

### 返回类型

```c
typedef enum {
    MCP_RETURN_DOUBLE,    // 返回双精度值
    MCP_RETURN_INT,       // 返回整数值
    MCP_RETURN_STRING,    // 返回字符串值（调用者必须释放）
    MCP_RETURN_VOID       // 无返回值
} mcp_return_type_t;
```

### 传输类型

```c
typedef enum {
    EMBED_MCP_TRANSPORT_STDIO,    // 标准输入/输出传输
    EMBED_MCP_TRANSPORT_HTTP      // HTTP传输
} embed_mcp_transport_t;
```



## 测试与验证

✅ **MCP Inspector兼容** - 通过所有协议合规性测试
✅ **多会话测试** - 支持并发连接，会话隔离
✅ **生产测试** - HTTP/STDIO传输，多种参数类型
✅ **实际验证** - 我们使用自己的库（自用验证）




## 构建和运行

### 构建示例（开发）

对于开发和测试，您可以构建包含的示例：

```bash
make debug    # 带符号的调试构建
make clean    # 清理构建文件
```

### 运行示例服务器

```bash
# 构建并运行示例服务器（使用embed_mcp/库 - 自用验证！）
make
./bin/mcp_server -t http -p 8080

# 或使用STDIO传输
./bin/mcp_server -t stdio

# 启用调试日志
./bin/mcp_server -t http -p 8080 -d
```

**注意：** 我们的示例服务器是使用`embed_mcp/`库本身构建的，证明了库的正确性！

示例服务器包含三个演示工具（使用`embed_mcp_add_tool`注册）：
- `add(a, b)` - 两个数字相加（演示基础数学运算）
- `weather(city)` - 获取天气信息（演示字符串处理，支持济南）
- `calculate_score(base_points, grade, multiplier)` - 计算带等级奖励的分数（演示混合参数类型）

### 使用MCP Inspector测试

1. 打开MCP Inspector：访问 https://inspector.mcp.dev
2. 运行您的服务器：`./bin/mcp_server -t http -p 8080`
3. 在MCP Inspector中连接
4. 连接到：`http://localhost:8080/mcp`


## 重要注意事项

### 多客户端支持
**当前状态：** EmbedMCP目前支持单客户端场景。多客户端支持计划在未来版本中推出。

**当前限制：**
- 专为单客户端或顺序客户端访问设计
- 并发客户端可能会相互干扰
- 客户端之间没有会话隔离

**解决方案：**
- 使用反向代理/负载均衡器处理多个客户端
- 运行多个EmbedMCP服务器实例
- 确保一次只有一个客户端连接

### 线程安全
库安全地处理并发请求。如果您的工具函数访问共享资源，应该是无状态的或使用适当的同步。

### 内存管理
- **工具参数：** 由库自动管理
- **返回值：** 您的函数应该为字符串和复杂类型返回malloc分配的内存
- **简单类型：** 按值返回（double、int）或malloc分配的指针

### 错误处理
始终检查返回值并使用 `embed_mcp_get_error()` 获取详细错误信息

### 传输类型
- **HTTP传输：** 最适合Web集成，支持多个并发客户端
- **STDIO传输：** 最适合MCP客户端集成（Claude Desktop等）

### 性能提示
- 保持工具函数轻量和快速
- 使用适当的参数类型（尽可能避免复杂的嵌套对象）
- 考虑缓存昂贵的计算

## 路线图

- ✅ **v1.0** - 工具系统，MCP Inspector兼容，生产就绪
- ✅ **v1.1** - 多会话支持，并发连接，会话管理
- ✅ **v1.2** - HAL架构，跨平台支持，代码复用优化


## 贡献

我们欢迎贡献！请查看CONTRIBUTING.md了解如何为此项目做贡献的指南。

## 许可证

此项目采用MIT许可证 - 详情请参阅LICENSE文件。

---

**EmbedMCP** - 让MCP服务器像编写纯函数一样简单！🚀
```
