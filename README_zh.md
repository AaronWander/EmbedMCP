# EmbedMCP - 嵌入式MCP服务器库

一个轻量级的C语言库，用于创建MCP（模型上下文协议）服务器，将您现有的C函数转换为AI可访问的工具，只需最少的代码修改。

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![C Standard](https://img.shields.io/badge/C-99-blue.svg)](https://en.wikipedia.org/wiki/C99)
[![Platform](https://img.shields.io/badge/Platform-Cross--Platform-green.svg)](#平台支持)
[![MCP](https://img.shields.io/badge/MCP-2025--11--25-orange.svg)](https://modelcontextprotocol.io/)

[English](./README.md) • [简体中文](./README_zh.md)

## 为什么选择 EmbedMCP？

EmbedMCP 在您现有的C代码库和现代AI系统之间架起了桥梁。无需重写您久经考验的C函数，EmbedMCP让您通过标准化的模型上下文协议（MCP）将它们暴露给AI模型，只需最少的代码修改。

## 核心特性

- **🚀 简单集成**：复制一个文件夹，包含一个头文件
- **⚡ 高性能**：直接C函数调用，开销最小
- **🔧 跨平台**：通过通用HAL在15+个平台上运行
- **📦 零依赖**：自包含库，无外部依赖
- **🎯 两种注册方法**：简单函数用魔法宏，复杂函数完全控制
- **🌐 多种传输**：Streamable HTTP和STDIO支持不同用例
- **🧠 智能内存管理**：自动清理，明确所有权规则
- **📊 数组支持**：处理简单参数和复杂数据结构

## 快速开始

### 安装

1. **下载 EmbedMCP**
   ```bash
   git clone https://github.com/AaronWander/EmbedMCP.git
   cd EmbedMCP
   ```

2. **复制到您的项目**
   ```bash
   cp -r embed_mcp/ your_project/
   ```

### 基本用法

```c
#include "embed_mcp/embed_mcp.h"

// 您的业务函数
double add_numbers(double a, double b) {
    return a + b;
}

// 使用宏生成包装器
EMBED_MCP_WRAPPER(add_wrapper, add_numbers, DOUBLE, DOUBLE, a, DOUBLE, b)

int main() {
    embed_mcp_config_t config = {
        .name = "MathServer",
        .version = "1.0.0",
        .instructions = "简单的数学运算服务器",
        .port = 8080
    };

    embed_mcp_server_t *server = embed_mcp_create(&config);

    // 注册函数
    const char* names[] = {"a", "b"};
    const char* descs[] = {"第一个数字", "第二个数字"};
    mcp_param_type_t types[] = {MCP_PARAM_DOUBLE, MCP_PARAM_DOUBLE};

    embed_mcp_add_tool(server, "add", "两个数字相加",
                       names, descs, types, 2, MCP_RETURN_DOUBLE, add_wrapper, NULL);

    embed_mcp_run(server, EMBED_MCP_TRANSPORT_HTTP);
    embed_mcp_destroy(server);
    return 0;
}
```

### 构建和运行（例程）

```bash
# 构建
make

# 运行Streamable HTTP服务器
./bin/mcp_server --transport http --port 8080

# 或运行STDIO服务器
./bin/mcp_server --transport stdio
```

## 函数注册

EmbedMCP支持两种注册方式：

### 简单函数（推荐）

对于基本参数类型（int、double、string、bool），使用宏一键构成：

```c
// 业务函数
double add_numbers(double a, double b) {
    return a + b;
}

// 一行生成包装器
EMBED_MCP_WRAPPER(add_wrapper, add_numbers, DOUBLE, DOUBLE, a, DOUBLE, b)

// 注册
const char* names[] = {"a", "b"};
const char* descs[] = {"第一个数字", "第二个数字"};
mcp_param_type_t types[] = {MCP_PARAM_DOUBLE, MCP_PARAM_DOUBLE};

embed_mcp_add_tool(server, "add", "两个数字相加",
                   names, descs, types, 2, MCP_RETURN_DOUBLE, add_wrapper, NULL);
```

### 数组函数（高级）

对于包含数组参数的函数，需要手动构建：

```c
// 业务函数
double sum_numbers(double* numbers, size_t count) {
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        sum += numbers[i];
    }
    return sum;
}

// 手动包装器（处理内存管理）
void* sum_wrapper(mcp_param_accessor_t* params, void* user_data) {
    size_t count;
    double* numbers = params->get_double_array(params, "numbers", &count);

    double result_val = sum_numbers(numbers, count);
    free(numbers); // 清理

    double* result = malloc(sizeof(double));
    *result = result_val;
    return result;
}

// 使用数组参数注册
mcp_param_desc_t params[] = {
    MCP_PARAM_ARRAY_DOUBLE_DEF("numbers", "数字数组", "一个数字", 1)
};

embed_mcp_add_tool(server, "sum", "数字求和", params, NULL, NULL, 1,
                   MCP_RETURN_DOUBLE, sum_wrapper, NULL);
```

## 内存管理

EmbedMCP自动处理大部分内存管理：

- **参数**：所有输入参数在函数返回后自动释放
- **JSON处理**：请求/响应解析和清理由内部处理
- **数组**：动态数组自动分配和释放
- **错误处理**：即使发生错误也会正确清理内存

**您的责任**：字符串返回值必须使用 `malloc()`：

```c
char* get_weather(const char* city) {
    char* result = malloc(200);  // ✅ EmbedMCP会调用free()
    sprintf(result, "Weather for %s: Sunny", city);
    return result;
}
```

## 服务器模式

### Streamable HTTP传输（样例）

```bash
./my_server --transport http --port 8080
```
- 多个并发客户端
- 通过`MCP-Session-Id`头部进行会话管理
- 通过`MCP-Protocol-Version`头部进行协议版本协商
- Web应用后端
- 开发和测试

### STDIO传输  
用于MCP客户端如Claude Desktop：
```bash
./my_server --transport stdio
```
- Claude Desktop集成
- AI助手工具
- 命令行工作流
- 单客户端通信

## 参数定义宏

对于数组参数，使用这些便利宏：

```c
// 数组参数宏
MCP_PARAM_ARRAY_DOUBLE_DEF(name, desc, elem_desc, required)  // 双精度数组
MCP_PARAM_ARRAY_STRING_DEF(name, desc, elem_desc, required)  // 字符串数组
MCP_PARAM_ARRAY_INT_DEF(name, desc, elem_desc, required)     // 整数数组

// 单一参数宏
MCP_PARAM_DOUBLE_DEF(name, description, required)   // 双精度参数
MCP_PARAM_STRING_DEF(name, description, required)   // 字符串参数
```

## 示例服务器

包含的示例演示了所有EmbedMCP功能：

```bash
# 构建并运行示例
make && ./bin/mcp_server --transport stdio
```

### 可用的演示工具

| 工具 | 参数 | 描述 | 示例 |
|------|------|------|------|
| `add` | `a: number, b: number` | 两个数字相加 | `add(10, 20)` → `30` |
| `sum_numbers` | `numbers: number[]` | 数字数组求和 | `sum_numbers([1,2,3])` → `6` |
| `join_strings` | `strings: string[], separator: string` | 连接字符串数组 | `join_strings(["a","b"], ",")` → `"a,b"` |
| `weather` | `city: string` | 获取天气信息 | `weather("济南")` → 天气报告 |
| `calculate_score` | `base_points: int, grade: string, multiplier: number` | 计算带奖励的分数 | `calculate_score(80, "A", 1.2)` → `120` |

### 使用MCP Inspector测试

1. 启动服务器：`./bin/mcp_server --transport http --port 8080`
2. 打开 [MCP Inspector](https://inspector.mcp.dev)
3. 连接到：`http://localhost:8080/mcp`
4. 测试可用工具

## 平台支持

EmbedMCP设计为在嵌入式设备上最大程度的可移植性：

### 嵌入式系统
- **RTOS**：FreeRTOS、Zephyr、ThreadX、embOS
- **MCU**：STM32、ESP32、Nordic nRF系列
- **SBC**：Raspberry Pi、BeagleBone、Orange Pi


### 要求
- **最低**：C99编译器，64KB RAM，100KB flash
- **推荐**：512KB RAM用于复杂应用
- **依赖**：无（自包含）

## 应用场景

### 工业物联网
- **传感器数据处理**：将C传感器驱动暴露给AI模型
- **设备监控**：机器数据的实时分析
- **预测性维护**：AI驱动的故障预测

### 嵌入式AI
- **边缘计算**：在嵌入式设备上运行AI推理
- **智能设备**：语音助手、智能摄像头、物联网中枢
- **机器人技术**：AI控制的机器人系统


## 故障排除

### 常见问题

**构建错误：**
```bash
# 缺少依赖
make deps

# 清理构建
make clean && make
```

**运行时错误：**
```bash
# 启用调试日志
./bin/mcp_server --transport stdio --debug

# 检查内存使用
valgrind ./bin/mcp_server --transport stdio
```

**连接问题：**
- 确保正确的传输模式（Streamable HTTP vs STDIO）
- 检查Streamable HTTP模式的防火墙设置
- 验证MCP客户端配置和协议版本头部

## 贡献

当前我们暂不接受外部代码贡献（PR）。

欢迎通过 Issue 提交 Bug 报告和功能建议。

### 开发设置

```bash
# 克隆仓库
git clone https://github.com/AaronWander/EmbedMCP.git
cd EmbedMCP

# 构建调试版本
make debug

# 运行测试
make test
```

## 许可证

本项目采用MIT许可证 - 详见 [LICENSE](LICENSE) 文件。

## 👥 Community & Support

### Community Resources

- Contributing: At this time, we’re not accepting external code contributions (PRs).
- Issues are welcome for bug reports and feature requests.

- 🐛 通过 [GitHub Issues](https://github.com/AaronWander/EmbedMCP/issues) 报告 Bug
- 💡 在 [Discussions](https://github.com/AaronWander/EmbedMCP/discussions) 提交功能建议
- 💬 加入 [Discord](https://discord.gg/D3D8Ngm9PB) 获取实时社区支持

### Stay Connected

- 🐦 Twitter: [@Tiyuzhao](https://x.com/Tiyuzhao)
