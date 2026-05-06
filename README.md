# KEY_MQTT_FreeRTOS_W25Q16_new

基于 **STM32F407VET6** 的小型物联网演示工程：FreeRTOS + ESP32 AT（WiFi + OneNET/MQTT）+ LCD 显示 + AHT20 温湿度 + W25Q16 外部 Flash 资源（字体/图片）+ 按键控制。

## 功能概述

- **上电流程**：板级初始化 → LCD/UI 欢迎页 → WiFi 连接页 → 等待连接 → 主页面 → 周期任务（WiFi/MQTT/按键/时间/传感器/天气）
- **云端通信**：通过 ESP32 AT 连接 OneNET（MQTT），订阅云端下行并按需/周期上报设备状态
- **本地交互**：两路按键控制两路 LED，状态变化会触发立即上报
- **传感器**：AHT20 采集室内温湿度，并在主页面刷新显示
- **时间同步**：SNTP 同步一次后写入 RTC，后续主要依赖 RTC，降低频繁网络查询
- **外部 Flash**：W25Q16 用于存储字体/图片资源，并支持烧录资源到 Flash

## 开发环境

- **IDE**：Keil uVision5（工程文件：`mdk/stm32f407temp.uvprojx`）
- **MCU**：`STM32F407VETx`
- **库/中间件**：
  - 标准外设库（StdPeriph）
  - FreeRTOS（`third_lib/FreeRTOS`）
  - ESP32 AT 驱动（`driver/esp32_c3/espat.*`）
  - OneNET/MQTT（`driver/NET/onenet`、`driver/NET/MQTT`）
  - cJSON（`driver/NET/CJSON`）

## 目录结构（关键）

- `app/`
  - `main.c`：系统入口（创建 init 任务并启动调度器）
  - `board.c/.h`：板级资源描述与初始化（GPIO/USART/SPI/I2C/LCD/RTC/W25Q16/AHT20/KEY/LED）
  - `loop.c/.h`：周期任务调度（软定时器 + workqueue），包含 MQTT/WiFi/按键/时间/传感器/天气逻辑
  - `ui.c/.h`：UI 绘制封装（文字/图片）
  - `page/`：页面（欢迎页、WiFi 页、主页面、错误页）
  - `font/`、`img/`：字体/图片资源（也可烧录至 W25Q16）
- `driver/`
  - `esp32_c3/`：ESP-AT 通信
  - `NET/onenet/`：OneNET 连接、订阅、上报、下行处理
  - `NET/MQTT/`：MQTT 工具封装（按工程实现为准）
  - `LCD/`：LCD 驱动（SPI）
  - `w25q16/`：W25Q16 驱动与资源烧录（字体/图片）
  - `aht20/`：AHT20 驱动
  - `key/`、`led/`、`rtc/`、`usart/`：外设驱动
- `firmware/`、`firmware/driver/`：STM32F4 标准外设库相关文件
- `third_lib/FreeRTOS/`：FreeRTOS 内核与移植

## 硬件资源映射（默认）

以下来自 `app/board.c` 的默认配置（如你的硬件不同，请对应修改）：

- **LED**
  - LED1：PA6
  - LED2：PA7
- **KEY（上拉输入，低电平按下）**
  - KEY1：PE4
  - KEY2：PE3
- **USART**
  - USART1：PA9(TX) / PA10(RX)
  - USART2：PA2(TX) / PA3(RX) 对应esp32的IO06(RX), IO07(TX)
- **AHT20（I2C1）**
  - PB6(SCL) / PB7(SDA)
- **LCD（SPI2）**
  - RST：PB9
  - DC：PB10
  - CS：PB12
  - BL：PB0
  - SCK：PB13
  - MOSI：PB15
- **W25Q16（SPI1）**
  - CS：PB0
  - SCK：PB3
  - MISO：PB4
  - MOSI：PB5

> 注意：`board.c` 中 LCD 的 `BLPin` 与 W25Q16 的 `CS_Pin` 都使用了 **PB0**。如果你的实际硬件确实存在共用/复用，请确认不会冲突；如果是误配置，建议调整其中一个引脚映射。

## 编译与烧录（Keil）

1. 用 Keil 打开 `mdk/stm32f407temp.uvprojx`
2. 选择 Target：`stm32f407`
3. 编译（Build）
4. 使用 ST-Link/J-Link 按你环境配置下载到板子

（可选）如果使用 EIDE：

- 工程配置文件在 `.eide/eide.yml`（工具链 AC5、包含路径、上传器等）

## 运行说明（系统启动流程）

`app/main.c` 中的 init 任务大致流程：

1. `board_init()`：初始化外设、LCD、W25Q16、AHT20、RTC 等
2. `ui_init()` + 欢迎页显示
3. `wireless_init()` → WiFi 页面显示 → `wireless_wait_connect()` 等待联网
4. `workqueue_init()`：初始化工作队列（用于运行较重的周期任务，避免阻塞定时器回调）
5. `main_loop_init()`：创建并启动多个 FreeRTOS Timer，周期执行：
   - WiFi 状态刷新
   - MQTT 处理（连接/订阅/接收/上报）
   - 按键扫描与 LED 控制
   - 室内温湿度采集
   - 室外天气查询与显示
   - RTC 时间显示与 SNTP 同步

## 配置项

### WiFi 账号密码

在 `app/loop.h`：

- `WIFI_SSID`
- `WIFI_PASSWD`

> 建议：WiFi 明文账号密码不适合长期放在仓库里；实际项目可改为编译宏/本地私有配置文件（不提交）或通过串口交互配置。

### OneNET / MQTT

工程在 `app/loop.c` 中通过 `driver/NET/onenet` 提供的接口完成：

- 连接：`OneNet_DevLink()`
- 订阅：`OneNET_Subscribe()`
- 下行处理：`OneNet_RevPro(...)`
- 上报：`OneNet_SendData()`

具体产品 ID、设备名、鉴权信息等通常在 `driver/NET/onenet/src/onenet.c` 或相关头文件中配置（以你的实际实现为准）。

### 天气 API

`app/loop.c` 的 `outdoor_update()` 里使用 `seniverse.com` 的 HTTP API，URL 中包含 `key` 与 `location` 参数。

> 提示：这类 API Key 也不建议直接提交到仓库；可以改为本地配置或编译宏注入。

### W25Q16 存储分区

见 `driver/w25q16/w25q16_burn.h`，默认包含：

- 索引表：`0x000000`，4KB
- 字体区：`FONT16/FONT20/.../FONT76`（多个字号）
- 图片区：起始 `0x048000`
- 用户区：`0x1E0000`（128KB）

资源烧录接口：

- `w25q16_burn_font(w25q16)`
- `w25q16_burn_img(w25q16)`

在 `app/main.c` 中已有相关测试代码（默认注释）。

## 常见问题与排查

- **网页端控制偶尔丢指令/需要多次点击**
  - 参考：`信号丢失问题分析.md`
  - 该文档总结了缓冲区过小、溢出处理不当、发送无重试、确认延迟不足、清空缓存时机不当等原因与优化方案。

- **网页端控制响应不灵敏（延迟明显）**
  - 参考：`优化说明.md`
  - 文档给出了网页端轮询/锁定时间与嵌入式端接收超时/循环间隔的匹配优化建议。

## License

未声明（如需开源协议，请补充 `LICENSE` 文件）。

