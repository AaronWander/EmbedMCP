# EmbedMCP - 嵌入式MCP服务器库

一个轻量级的C语言库，用于创建基于纯业务函数的MCP（模型上下文协议）服务器。

## 项目状态

✅ **工具系统** - 完整实现，支持纯函数API  
🚧 **资源系统** - 即将推出  
🚧 **提示系统** - 即将推出  
🚧 **采样系统** - 即将推出  

目前，EmbedMCP专注于MCP协议的**工具**部分，让您能够创建强大的自定义工具MCP服务器。其他MCP功能将在未来版本中添加。

## 特性

- **纯函数API** - 编写业务逻辑无需处理JSON
- **通用参数访问** - 处理任意参数类型组合  
- **自动Schema生成** - 无需手动编写JSON Schema
- **多种传输方式** - 支持STDIO和HTTP传输
- **类型安全** - 编译时参数验证
- **最小依赖** - 仅需要cJSON（已包含）
- **极其简单** - 只需学习6个核心API函数
- **易于集成** - 复制一个文件夹，包含一个头文件
- **多客户端就绪** - 自动支持并发客户端

## 快速集成摘要

**3个简单步骤：**
1. 复制 `embed_mcp/` 文件夹到您的项目
2. 在代码中包含 `#include "embed_mcp/embed_mcp.h"`  
3. 一起编译所有 `.c` 文件

**就这样！** 您现在拥有了一个支持多客户端的完整MCP服务器。

## 集成指南

### 步骤1：复制库文件

将 `embed_mcp/` 文件夹复制到您的项目：

```bash
# 将整个embed_mcp文件夹复制到您的项目
cp -r /path/to/EmbedMCP/embed_mcp/ your_project/
```

您的项目结构将如下所示：
```
your_project/
├── src/
│   └── main.c                 # 您的应用程序代码
├── embed_mcp/                 # EmbedMCP库（已复制）
│   ├── embed_mcp.h           # 主API头文件
│   ├── embed_mcp.c           # 主API实现
│   ├── cjson/                # JSON依赖
│   │   ├── cJSON.h           # JSON解析器头文件
│   │   └── cJSON.c           # JSON解析器实现
│   ├── protocol/             # MCP协议实现
│   ├── transport/            # HTTP/STDIO传输
│   ├── tools/                # 工具系统
│   ├── application/          # 多客户端支持
│   └── utils/                # 工具库
└── Makefile
```

### 步骤2：包含头文件

在您的源代码中，包含主头文件：

```c
#include "embed_mcp/embed_mcp.h"

// 您的业务函数 - 无需处理JSON！
void* add_numbers(mcp_param_accessor_t* params) {
    double a = params->get_double(params, "a");
    double b = params->get_double(params, "b");
    
    double* result = malloc(sizeof(double));
    *result = a + b;
    return result;
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
        .debug = 0              // 调试日志（0=关闭，1=开启）
    };
    
    // 创建服务器
    embed_mcp_server_t *server = embed_mcp_create(&config);
    
    // 定义参数
    mcp_param_desc_t params[] = {
        MCP_PARAM_DOUBLE_DEF("a", "第一个数字", 1),
        MCP_PARAM_DOUBLE_DEF("b", "第二个数字", 1)
    };
    
    // 注册您的纯函数
    embed_mcp_add_pure_function(server, "add", "两数相加",
                                params, 2, MCP_RETURN_DOUBLE, add_numbers);
    
    // 运行服务器
    embed_mcp_run(server, EMBED_MCP_TRANSPORT_HTTP);
    
    // 清理
    embed_mcp_destroy(server);
    return 0;
}
```

### 步骤3：编译您的项目

#### 选项1：简单编译（所有源文件）

```bash
# 一起编译所有源文件
gcc src/main.c \
    embed_mcp/embed_mcp.c \
    embed_mcp/cjson/cJSON.c \
    embed_mcp/protocol/*.c \
    embed_mcp/transport/*.c \
    embed_mcp/tools/*.c \
    embed_mcp/utils/*.c \
    -I embed_mcp \
    -o my_app
```

#### 选项2：先创建静态库

```bash
# 创建目标文件
gcc -c embed_mcp/embed_mcp.c -I embed_mcp -o embed_mcp.o
gcc -c embed_mcp/cjson/cJSON.c -I embed_mcp -o cJSON.o
gcc -c embed_mcp/protocol/*.c -I embed_mcp
gcc -c embed_mcp/transport/*.c -I embed_mcp  
gcc -c embed_mcp/tools/*.c -I embed_mcp
gcc -c embed_mcp/utils/*.c -I embed_mcp

# 创建静态库
ar rcs libembed_mcp.a *.o

# 编译您的应用程序
gcc src/main.c libembed_mcp.a -I embed_mcp -o my_app
```

#### 选项3：使用Makefile

创建一个简单的Makefile：

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I embed_mcp
SRCDIR = embed_mcp
SOURCES = $(wildcard $(SRCDIR)/*.c $(SRCDIR)/*/*.c)
OBJECTS = $(SOURCES:.c=.o)

my_app: src/main.c $(OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) my_app

.PHONY: clean
```

然后简单运行：
```bash
make
```

## 库文件概览

`embed_mcp/` 文件夹包含将EmbedMCP集成到您项目所需的所有文件：

### 核心文件
- **`embed_mcp.h`** - 主API头文件（这是您需要包含的）
- **`embed_mcp.c`** - 主API实现
- **`cjson/`** - JSON解析依赖（已捆绑）
  - `cJSON.h` - JSON解析器头文件
  - `cJSON.c` - JSON解析器实现

### 内部实现
- **`protocol/`** - MCP协议实现
  - `mcp_protocol.h/.c` - 核心协议处理
  - `message.h/.c` - 消息解析和格式化
  - `jsonrpc.h/.c` - JSON-RPC实现
  - `protocol_state.h/.c` - 协议状态管理

- **`transport/`** - 传输层（HTTP/STDIO）
  - `transport_interface.h` - 传输抽象
  - `http_transport.h/.c` - HTTP服务器实现
  - `stdio_transport.h/.c` - MCP客户端的STDIO传输
  - `sse_transport.h` - 服务器发送事件支持

- **`tools/`** - 工具系统
  - `tool_interface.h/.c` - 工具接口和执行
  - `tool_registry.h/.c` - 工具注册和管理

- **`application/`** - 多客户端支持
  - `client_manager.h` - 多客户端连接管理
  - `session_manager.h` - 会话隔离和管理
  - `request_router.h` - 请求路由到正确会话
  - `mcp_server.h` - 高级服务器应用层

- **`utils/`** - 工具库
  - `logging.h/.c` - 日志系统
  - `memory.h/.c` - 内存管理工具

### 您需要了解的

**对于用户：** 您只需要了解 `embed_mcp.h` - 这就是您的整个API！

**对于集成：** 复制整个 `embed_mcp/` 文件夹并一起编译所有 `.c` 文件。

**多客户端支持：** 库通过应用层自动处理多个并发客户端，但您的工具函数保持简单，无需担心客户端管理。

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
    const char *description;            // 人类可读的参数描述
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

### 核心函数（只有6个函数！）

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
// 注册纯函数工具（处理所有MCP场景）
int embed_mcp_add_pure_function(embed_mcp_server_t *server,
                                const char *name,
                                const char *description,
                                mcp_param_desc_t *params,
                                size_t param_count,
                                mcp_return_type_t return_type,
                                mcp_universal_func_t function_ptr);
```

**函数参数：**
- `server` - 使用 `embed_mcp_create()` 创建的服务器实例
- `name` - 唯一工具名称（在MCP协议中使用）
- `description` - 人类可读的工具描述
- `params` - 参数描述数组
- `param_count` - 数组中的参数数量
- `return_type` - 返回类型（`MCP_RETURN_DOUBLE`、`MCP_RETURN_INT`、`MCP_RETURN_STRING`、`MCP_RETURN_VOID`）
- `function_ptr` - 指向您的纯业务函数的指针

#### 错误处理

```c
// 获取最后的错误消息（如果没有错误返回NULL）
const char *embed_mcp_get_error(void);
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

## 完整示例

### 1. 基础数学工具

```c
#include "embed_mcp/embed_mcp.h"

// 纯业务函数 - 无需处理JSON！
void* add_numbers(mcp_param_accessor_t* params) {
    double a = params->get_double(params, "a");
    double b = params->get_double(params, "b");

    double* result = malloc(sizeof(double));
    *result = a + b;
    return result;
}

int main() {
    // 创建服务器配置
    embed_mcp_config_t config = {
        .name = "数学服务器",
        .version = "1.0.0",
        .host = "0.0.0.0",
        .port = 8080,
        .path = "/mcp",
        .max_tools = 100,
        .debug = 1
    };

    // 创建服务器
    embed_mcp_server_t *server = embed_mcp_create(&config);
    if (!server) {
        printf("错误：%s\n", embed_mcp_get_error());
        return -1;
    }

    // 定义参数
    mcp_param_desc_t params[] = {
        MCP_PARAM_DOUBLE_DEF("a", "要相加的第一个数字", 1),
        MCP_PARAM_DOUBLE_DEF("b", "要相加的第二个数字", 1)
    };

    // 注册工具
    if (embed_mcp_add_pure_function(server, "add", "两个数字相加",
                                    params, 2, MCP_RETURN_DOUBLE, add_numbers) != 0) {
        printf("错误：%s\n", embed_mcp_get_error());
        embed_mcp_destroy(server);
        return -1;
    }

    // 运行服务器
    printf("在 http://localhost:8080/mcp 启动MCP服务器\n");
    int result = embed_mcp_run(server, EMBED_MCP_TRANSPORT_HTTP);

    // 清理
    embed_mcp_destroy(server);
    return result;
}
```

### 2. 字符串处理工具

```c
void* process_text(mcp_param_accessor_t* params) {
    const char* input = params->get_string(params, "text");
    const char* operation = params->get_string(params, "operation");

    size_t len = strlen(input);
    char* result = malloc(len + 1);

    if (strcmp(operation, "upper") == 0) {
        for (size_t i = 0; i < len; i++) {
            result[i] = toupper(input[i]);
        }
    } else if (strcmp(operation, "lower") == 0) {
        for (size_t i = 0; i < len; i++) {
            result[i] = tolower(input[i]);
        }
    } else {
        strcpy(result, input);  // 无变化
    }
    result[len] = '\0';

    return result;
}

// 注册工具
mcp_param_desc_t text_params[] = {
    MCP_PARAM_STRING_DEF("text", "要处理的输入文本", 1),
    MCP_PARAM_STRING_DEF("operation", "操作：'upper' 或 'lower'", 1)
};
embed_mcp_add_pure_function(server, "process_text", "使用各种操作处理文本",
                            text_params, 2, MCP_RETURN_STRING, process_text);
```

### 3. 数组处理工具

```c
void* sum_array(mcp_param_accessor_t* params) {
    size_t count;
    double* numbers = params->get_double_array(params, "numbers", &count);

    double total = 0.0;
    for (size_t i = 0; i < count; i++) {
        total += numbers[i];
    }

    double* result = malloc(sizeof(double));
    *result = total;
    return result;
}

// 注册工具
mcp_param_desc_t array_params[] = {
    MCP_PARAM_ARRAY_DOUBLE_DEF("numbers", "要求和的数字数组", "一个数字", 1)
};
embed_mcp_add_pure_function(server, "sum_array", "计算数字数组的和",
                            array_params, 1, MCP_RETURN_DOUBLE, sum_array);
```

### 4. 复杂参数（直接JSON访问）

```c
void* complex_handler(mcp_param_accessor_t* params) {
    // 对简单参数使用类型安全访问器
    const char* operation = params->get_string(params, "operation");

    // 对复杂嵌套结构使用直接JSON访问
    const cJSON* config = params->get_json(params, "config");
    if (config) {
        cJSON* database = cJSON_GetObjectItem(config, "database");
        if (database) {
            cJSON* host = cJSON_GetObjectItem(database, "host");
            cJSON* port = cJSON_GetObjectItem(database, "port");

            printf("连接到 %s:%d\n",
                   cJSON_GetStringValue(host),
                   cJSON_GetNumberValue(port));
        }
    }

    char* result = malloc(256);
    snprintf(result, 256, "已处理 %s 操作", operation);
    return result;
}
```

## 构建和运行

### 构建示例（开发）

对于开发和测试，您可以构建包含的示例：

```bash
make debug    # 带符号的调试构建
make release  # 优化的发布构建
make clean    # 清理构建文件
```

### 运行示例服务器

```bash
# 构建并运行示例服务器
make debug
./bin/mcp_server -t http -p 8080

# 或使用STDIO传输
./bin/mcp_server -t stdio
```

示例服务器包含三个演示工具：
- `add(a, b)` - 两数相加
- `sum_array(numbers[])` - 数组求和
- `weather(city)` - 获取天气信息（支持济南）

### 使用MCP Inspector测试

1. 安装MCP Inspector：`npm install -g @modelcontextprotocol/inspector`
2. 运行您的服务器：`./bin/mcp_server -t http -p 8080`
3. 打开MCP Inspector：`mcp-inspector`
4. 连接到：`http://localhost:8080/mcp`

### 使用curl测试

```bash
# 列出可用工具
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'

# 调用add工具
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"add","arguments":{"a":10,"b":5}}}'

# 调用weather工具
curl -X POST http://localhost:8080/mcp \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"weather","arguments":{"city":"济南"}}}'
```

## 重要注意事项

### 多客户端支持
EmbedMCP自动支持多个并发客户端。每个客户端都有自己的会话，工具调用被正确隔离。您无需在工具函数中担心客户端管理。

### 线程安全
库安全地处理并发请求。如果您的工具函数访问共享资源，应该是无状态的或使用适当的同步。

### 内存管理
- **工具参数：** 由库自动管理
- **返回值：** 您的函数应该为字符串和复杂类型返回malloc分配的内存
- **简单类型：** 按值返回（double、int）或malloc分配的指针

### 错误处理
始终检查返回值并使用 `embed_mcp_get_error()` 获取详细错误信息：

```c
if (embed_mcp_add_pure_function(...) != 0) {
    printf("错误：%s\n", embed_mcp_get_error());
    // 适当处理错误
}
```

### 传输类型
- **HTTP传输：** 最适合Web集成，支持多个并发客户端
- **STDIO传输：** 最适合MCP客户端集成（Claude Desktop等）

### 性能提示
- 保持工具函数轻量和快速
- 使用适当的参数类型（尽可能避免复杂的嵌套对象）
- 考虑缓存昂贵的计算

## 路线图

- ✅ **v1.0** - 工具系统与纯函数API
- 🚧 **v1.1** - 资源系统（文件访问、数据源）
- 🚧 **v1.2** - 提示系统（提示模板、补全）
- 🚧 **v1.3** - 采样系统（LLM采样控制）
- 🚧 **v2.0** - 高级功能（日志、指标、认证）

## 贡献

我们欢迎贡献！请查看CONTRIBUTING.md了解如何为此项目做贡献的指南。

## 许可证

此项目采用MIT许可证 - 详情请参阅LICENSE文件。

---

**EmbedMCP** - 让MCP服务器像编写纯函数一样简单！🚀
```
