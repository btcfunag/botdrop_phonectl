# BotDrop → openclaw 手机控制 — 完整配置指南

## 概览

本项目让 BotDrop App 中的 AI (openclaw) 能直接控制 Android 手机，绕过 MIUI 的安全限制。

**核心原理**：通过 `/dev/uinput` 创建虚拟触摸屏设备（设置 `INPUT_PROP_DIRECT` + `BUS_VIRTUAL`），使 Android 将其识别为直接触摸屏（`SOURCE_TOUCHSCREEN`），从而绕过 MIUI 对 `input` 命令和 `INJECT_EVENTS` 权限的封锁。

**工作链路**：
```
BotDrop (SSH终端) → adb shell (localhost:5555) → /data/local/tmp/uinput_touch → /dev/uinput → 内核触摸事件
```

## 前提条件

### 手机端
- 设备：小米 MI CC 9（或其他支持 uinput 的 Android 设备）
- Android 11 (SDK 30), MIUI 12.5.5
- 已安装 BotDrop App（基于 Termux）
- 开启 USB 调试

### Mac 端
- Homebrew 已安装
- USB 数据线连接手机

---

## 第一步：Mac 安装依赖

```bash
# SSH 连接工具
brew install sshpass

# Android 工具链
brew install --cask android-commandlinetools

# NDK (用于交叉编译 C 程序)
sdkmanager "ndk;27.2.12479018"

# Build tools (如需 dex 工具)
sdkmanager "build-tools;35.0.0"

# scrcpy (可选，实时投屏)
brew install scrcpy
```

NDK 编译器路径：
```
/opt/homebrew/share/android-commandlinetools/ndk/27.2.12479018/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android30-clang
```

---

## 第二步：ADB 连接与授权

### 2.1 USB 连接
```bash
# 确认设备已连接
adb devices
# 输出应类似: a8fb8d95  device
```

### 2.2 开启 ADB TCP 模式
```bash
# 让 ADB 同时监听 TCP 5555 端口（BotDrop 需要通过此端口连接）
adb tcpip 5555
```

> **注意**：每次手机重启后需重新执行此命令。

---

## 第三步：BotDrop 内安装 ADB

BotDrop 自带的 Termux 环境没有 ADB，需要手动安装。

### 3.1 SSH 连接到 BotDrop
```bash
# BotDrop SSH 信息（在 App 内查看）
sshpass -p 'passw0rd' ssh -o StrictHostKeyChecking=no -p 8022 u0_a240@192.168.88.38
```

> **注意**：IP、端口、用户名、密码根据你的 BotDrop 实际配置修改。

### 3.2 下载并安装 ADB 二进制
```bash
# 在 BotDrop SSH 终端内执行：

# 下载 termux 版 adb
curl -L -o /tmp/adb.deb https://packages.termux.dev/apt/termux-main/pool/main/a/android-tools/android-tools_35.0.2-3_aarch64.deb

# 解压（Termux 环境用 dpkg-deb 或手动解压）
mkdir /tmp/adb_extract
dpkg-deb -x /tmp/adb.deb /tmp/adb_extract 2>/dev/null || \
  (cd /tmp/adb_extract && ar x /tmp/adb.deb && tar xf data.tar.xz 2>/dev/null || tar xf data.tar.gz)

# 复制 adb 到 PATH 目录
cp /tmp/adb_extract/data/data/com.termux/files/usr/bin/adb /data/data/app.botdrop/files/usr/bin/adb
chmod +x /data/data/app.botdrop/files/usr/bin/adb
```

### 3.3 配置环境变量

在 BotDrop 的 `~/.bashrc`（或 `~/.profile`、`/data/data/app.botdrop/files/usr/etc/profile`）中添加：
```bash
export LD_LIBRARY_PATH=/data/data/app.botdrop/files/usr/lib
export PATH=/data/data/app.botdrop/files/usr/bin:$PATH
```

### 3.4 复制 ADB 授权密钥

BotDrop 的 ADB 需要已授权的密钥才能连接。从 Mac 复制：
```bash
# 在 Mac 上执行：
sshpass -p 'passw0rd' scp -o StrictHostKeyChecking=no -P 8022 \
  ~/.android/adbkey ~/.android/adbkey.pub \
  u0_a240@192.168.88.38:/data/data/app.botdrop/files/home/.android/
```

### 3.5 验证 BotDrop ADB 连接
```bash
# 在 BotDrop SSH 内执行：
adb -s localhost:5555 shell whoami
# 应输出: shell
```

---

## 第四步：编译并部署 uinput_touch

### 4.1 交叉编译

在 Mac 上执行：
```bash
NDK_CC=/opt/homebrew/share/android-commandlinetools/ndk/27.2.12479018/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android30-clang

$NDK_CC -static -O2 -o /tmp/uinput_touch uinput_touch.c
```

> **关键编译选项**：`-static` 是必须的，因为手机上的 ADB shell 环境没有完整的动态链接库。

### 4.2 推送到手机
```bash
adb push /tmp/uinput_touch /data/local/tmp/uinput_touch
adb shell chmod 755 /data/local/tmp/uinput_touch
```

### 4.3 验证
```bash
adb shell /data/local/tmp/uinput_touch tap 540 1170
# 应输出: OK（约 2.5 秒后），并且手机屏幕上对应位置被点击
```

---

## 第五步：部署 phonectl 脚本

### 5.1 复制到 BotDrop
```bash
# 在 Mac 上执行：
sshpass -p 'passw0rd' scp -o StrictHostKeyChecking=no -P 8022 \
  phonectl.sh \
  u0_a240@192.168.88.38:/data/data/app.botdrop/files/usr/bin/phonectl
```

### 5.2 设置权限
```bash
sshpass -p 'passw0rd' ssh -o StrictHostKeyChecking=no -p 8022 u0_a240@192.168.88.38 \
  'chmod +x /data/data/app.botdrop/files/usr/bin/phonectl'
```

### 5.3 验证
```bash
# 在 BotDrop SSH 内执行：
phonectl current_app
# 应显示当前前台应用

phonectl uidump_text
# 应显示屏幕上的所有文字

phonectl tap 668 1423
# 应点击屏幕上的对应位置
```

---

## 第六步：适配不同设备

如果在不同手机上使用，需要修改以下参数：

### 6.1 屏幕分辨率
修改 `uinput_touch.c` 中的：
```c
#define SCREEN_W 1080
#define SCREEN_H 2340
```
改为目标设备分辨率，然后重新编译。

查询分辨率：
```bash
adb shell wm size
```

### 6.2 BotDrop 路径
如果 BotDrop 包名不同，修改 `phonectl.sh` 中的：
```bash
ADB_CMD="/data/data/app.botdrop/files/usr/bin/adb -s localhost:5555"
```

### 6.3 验证 uinput 可用性
```bash
adb shell ls -la /dev/uinput
# 需要输出类似: crw-rw---- uhid uhid ... /dev/uinput
# shell 用户需要在 uhid 组中（uid 2000 通常有 gid 3011）
```

如果 `/dev/uinput` 权限不够，此方案不可用。

---

## 故障排查

### 问题：`input tap/keyevent` 报 SecurityException
这是 MIUI 的正常行为。MIUI 要求开启"USB调试（安全设置）"才允许 `INJECT_EVENTS`，而此选项需要插入 SIM 卡。这也是为什么我们使用 uinput 方案绕过它。

### 问题：uinput_touch 返回 OK 但没有效果
1. 检查设备是否被正确注册为触摸屏：
```bash
adb shell dumpsys input | grep -A5 "virtual_touchscreen"
# 应看到 sources=0x00001002 (TOUCHSCREEN), mode 1 (DIRECT)
```
2. 如果 `sources=0x2002`（MOUSE）或有 EXTERNAL 标记，说明设备被错误分类：
   - 确保代码中有 `UI_SET_PROPBIT, INPUT_PROP_DIRECT`
   - 确保 `bustype = BUS_VIRTUAL`（不是 `BUS_USB`）

### 问题：BotDrop ADB 连接被拒
```bash
# 重新开启 TCP 模式（在 Mac 上用 USB ADB）
adb tcpip 5555

# 重新复制密钥
sshpass -p 'passw0rd' scp -P 8022 ~/.android/adbkey* u0_a240@192.168.88.38:~/.android/
```

### 问题：每次操作有 2 秒延迟
这是 uinput 设备注册的固有开销。每次调用 `uinput_touch` 都会创建一个新的虚拟设备，等待 Android InputManager 注册后才发送事件。可以通过改为 daemon 模式（后台驻留，复用设备）来优化，但当前单次执行模式更稳定。

### 问题：home 键无效
MIUI 拦截了虚拟设备的 HOME 键。phonectl 中 `home` 命令已改为使用 `am start` 方式。`back` 键正常工作。

---

## 文件清单

```
hackbotdrop/
├── uinput_touch.c      # 虚拟触摸屏注入工具 C 源码
├── phonectl.sh         # openclaw 控制脚本（部署到 BotDrop）
├── instruction.md      # phonectl 命令使用说明
└── setup.md            # 本文件 — 完整配置指南
```

### 手机上的文件位置
```
/data/local/tmp/uinput_touch                        # 编译后的触摸工具
/data/data/app.botdrop/files/usr/bin/phonectl       # 控制脚本
/data/data/app.botdrop/files/usr/bin/adb            # ADB 客户端
/data/data/app.botdrop/files/home/.android/adbkey   # ADB 授权密钥
```

---

## 快速恢复命令

手机重启后，需要重新执行以下步骤：

```bash
# 1. Mac: 重新开启 ADB TCP
adb tcpip 5555

# 2. 验证 BotDrop 可以连接
sshpass -p 'passw0rd' ssh -p 8022 u0_a240@192.168.88.38 \
  'export LD_LIBRARY_PATH=/data/data/app.botdrop/files/usr/lib; adb -s localhost:5555 shell whoami'

# 3. 验证触摸工具
sshpass -p 'passw0rd' ssh -p 8022 u0_a240@192.168.88.38 \
  'export LD_LIBRARY_PATH=/data/data/app.botdrop/files/usr/lib; phonectl current_app'
```

> `uinput_touch` 和 `phonectl` 已持久化在 `/data/local/tmp/` 和 BotDrop 目录中，重启不丢失。
> 唯一需要重新做的是 `adb tcpip 5555`（ADB TCP 模式在重启后会重置为 USB only）。
