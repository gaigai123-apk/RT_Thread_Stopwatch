### 参数持久化设计说明（Stopwatch 项目）

本说明定义“参数持久化”的范围、数据结构、存储布局、读写流程、命令、测试与风险。目标：掉电/重启后自动恢复用户偏好，改动小、可靠、可扩展。

---

## 1. 目标与范围

- 开机自动加载上次保存的偏好；提供保存/恢复命令
- 涵盖当前可配置项：
  - sw_beep（蜂鸣器开关，bool）
  - sw_light（光敏联动开关，bool）
  - sw_light_invert（光敏极性反转，bool）
  - sw_oled_rate（OLED 刷新周期，uint16，单位 ms）
  - sw_page（OLED 页面：0=main，1=laps，uint8）
  - sw_timefmt（CSV 时间格式：0=ms，1=human，uint8）
  - sw_csv_header（CSV 打印表头：0/1，uint8）

> 可扩展项（后续可加入）：对比度、圈速页偏移、光敏阈值/去抖参数等。

---

## 2. 默认值（工厂设置）

- sw_beep = on（1）
- sw_light = off（0）
- sw_light_invert = off（0）
- sw_oled_rate = 100（ms）
- sw_page = 0（main）
- sw_timefmt = 0（ms）
- sw_csv_header = 0（不自动打印）

---

## 3. 存储布局（STM32F103C8T6）

- 片内 Flash：起始 0x08000000，容量 64KB，页大小 1KB
- 建议使用最后一页（0x0800FC00~0x0800FFFF）作为“参数页”
- 单页存放一个配置块（含 MAGIC/版本/长度/CRC）

地址示意：

```
0x0800_0000 ... [程序区] ... 0x0800_FC00 [参数页 1KB]
```

---

## 4. 配置结构（C 结构体示例）

```c
/* 对齐 4 字节，末尾预留扩展空间 */
typedef struct __attribute__((packed, aligned(4)))
{
    uint32_t magic;        // 固定 0x53575031 ('SWP1')
    uint16_t length;       // 结构体有效长度（不含 CRC）
    uint16_t version;      // 版本（从 1 开始）

    uint8_t  sw_beep;          // 0/1
    uint8_t  sw_light;         // 0/1
    uint8_t  sw_light_invert;  // 0/1
    uint8_t  sw_page;          // 0/1

    uint16_t sw_oled_rate_ms;  // 建议 50~200
    uint8_t  sw_timefmt;       // 0:ms 1:human
    uint8_t  sw_csv_header;    // 0/1

    uint32_t rsv0;         // 预留（对齐/扩展）
    uint32_t rsv1;         // 预留（对齐/扩展）

    uint32_t crc32;        // 末尾 CRC32（多项式 0x04C11DB7，初始 0xFFFF_FFFF）
} sw_params_t;
```

约定：
- MAGIC 不匹配、长度/版本非法或 CRC 错时，使用默认值并提示“参数恢复默认”。
- 仅使用单页、单拷贝；如需更可靠可后续升级为“双缓冲+序列号”的 wear-leveling。

---

## 5. 读写流程

- 开机加载（app 启动时）
  1) 从参数页读取 `sw_params_t`
  2) 校验 MAGIC/版本/长度/CRC
  3) 失败则加载默认，并可延迟写入（或等待用户 `sw_save`）
  4) 成功则应用到运行时：调用对应 setter（如 `notifier_beep_enable`、`ui_oled_set_refresh_ms`、`ui_oled_set_page`、`sensor_light_enable` 等）

- 保存（命令 sw_save）
  1) 收集当前运行时参数填充 `sw_params_t`
  2) 计算 CRC32
  3) 解锁 Flash → 擦除参数页 → 编程写入 → 上锁
  4) 打印保存结果/错误码

- 恢复出厂/重载（命令 sw_load）
  - 若携带 `default` 参数：使用默认值并调用 `sw_save` 覆盖参数页
  - 否则：重新从参数页读取并应用（相当于 reboot 后的加载）

---

## 6. 命令定义（msh）

- `sw_save`
  - 作用：将当前参数保存到 Flash
  - 返回：保存成功/失败（包含擦写错误码）

- `sw_load`
  - 作用：从 Flash 读取并应用
  - 可选：`sw_load default` → 恢复默认并保存

---

## 7. 错误处理与健壮性

- Flash 操作失败：保留 RAM 中参数不变，提示错误码
- CRC 错误：提示并加载默认值；用户可 `sw_save` 修复
- 参数约束：写入前 clamp 合法范围（如 `sw_oled_rate_ms` 50~2000）
- 并发：保存时可临时屏蔽中断或加全局锁，避免并发修改参数

---

## 8. 版本与迁移

- `version` 字段自增；加载时按版本兼容：
  - 旧版：未包含新字段时用默认
  - 新版：多余字段忽略
- 结构体末尾保留 `rsv*` 便于平滑演进

---

## 9. 实施步骤（开发计划）

1) 新增 `applications/params_store.{c,h}`：封装 load/save/CRC/默认
2) 在 `main` 或 `stopwatch_init` 早期调用 `params_load_and_apply()`
3) 在 `stopwatch_cli.c` 注册 `sw_save`、`sw_load` 命令
4) 接入各子系统 setter：
   - 蜂鸣器：`notifier_beep_enable`
   - 光敏：`sensor_light_enable`、`sensor_light_set_invert`
   - OLED：`ui_oled_set_refresh_ms`、`ui_oled_set_page`
   - CSV：`sw_timefmt`、`sw_csv_header` 的内部变量
5) 编写单元/集成测试（见下）
6) 文档更新：`PROJECT_STOPWATCH.md` 增加使用说明与注意事项

---

## 10. 测试用例

- 上电默认：不写入 Flash 的首次启动应按默认值生效
- 保存/重启：修改→`sw_save`→复位/断电→检查是否恢复
- CRC 损坏模拟：手工篡改参数页几个字节→应自动回默认
- 边界：`sw_oled_rate_ms` 设置为 10/5000→加载后应 clamp 到合法范围
- 压力：多次保存（数百次）→仍能正确读取与应用

---

## 11. 资源与限制

- 占用 1KB Flash（最后一页）
- 运行期 RAM 占用 < 64B（结构体常驻 + 临时缓冲）
- 写入寿命：F1 Flash 典型 1万次擦写；建议减少频繁保存（由用户触发 `sw_save`）

---

## 12. 风险与规避

- 掉电在“已擦除未写入”阶段：下次启动加载默认（可提示用户重新保存）
- 并发修改：保存前统一从运行时获取一次快照
- 未来扩展字段：通过 `version/length` 兼容

---

## 13. 用户使用说明（摘要）

- 修改偏好（如 `sw_beep off`、`sw_light on`、`sw_oled_rate 80` 等）后，输入 `sw_save` 保存
- 下次上电/复位将自动恢复这些设置
- 使用 `sw_load` 可重载；`sw_load default` 恢复出厂并保存










