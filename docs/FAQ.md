# Frequently Asked Questions
一些经常问的问题

如果在此文档没有解决你的问题，描述你的问题，截图软件控制台中打印的日志，一起发到QQ群里提问。

# adb问题
## ADB版本之间的冲突
```
adb server version (41) doesn't match this client (39); killing...
```
当你的电脑中运行不同版本的adb时，会发生此错误。你必须保证所有程序使用相同版本的adb。
现在你有两个办法解决这个问题：
1. 任务管理器找到adb进程并杀死
2. 配置QtScrcpy的config.ini中的AdbPath路径指向当前使用的adb

## 手机通过数据线连接电脑，刷新设备列表以后，没有任何设备出现
随便下载一个手机助手，尝试连接成功以后，再用QtScrcpy刷新设备列表连接

# 控制问题
## 可以看到画面，但无法控制
有些手机(小米等手机)需要额外打开控制权限，检查是否USB调试里打开了允许模拟点击

![image](image/USB调试(安全设置).jpg)

# WeChat batch registration (QtScrcpy batch dialog)

## Device and network requirements

- Batch registration expects **ADB over Tailscale** (`100.x.x.x:5555`). Public IPs in the sheet are rejected by design.
- The summary shows **“N of M connected”**; Install/Register only run on rows that passed the connection check. Unreachable devices receive no commands.

## SMS auto-read

- On **Redmi 220333QAG (MIUI/HyperOS V14, Android 13)** with USB debugging, `adb shell content query --uri content://sms/inbox --projection address:body:date --sort "date DESC"` **works** (no permission denial observed). WeChat codes in `[WeChat] (123456)` and `verification code (123456)` formats are parsed.
- If `content query` fails on your ROM, the registrar falls back to **`dumpsys notification`**. If neither yields a code, automatic SMS read is unsupported on that ROM — register manually or use a device where inbox query works.

## UI automation (uiautomator) on MIUI / XSpace

- **WeChat 8.0.66** on this hardware was installed in **MIUI XSpace (user 999)**. `uiautomator dump` from the owner profile often captures the **launcher**, not the XSpace window. For calibration, install the same APK on the owner profile without uninstalling XSpace: `adb shell cmd package install-existing --user 0 com.tencent.mm`, then `adb shell am force-stop --user 999 com.tencent.mm` and launch with `adb shell am start --user 0 -n com.tencent.mm/.ui.LauncherUI`.
- On the welcome splash, dumps sometimes fail with **“could not get idle state”** or return an empty `FrameLayout` until the sign-up sheet or form is open. Prefer capturing XML on **Sign up via Mobile**, the phone form, and SMS screens.
- English WeChat 8.0.66 uses **Sign Up → Sign up via Mobile → Accept and Continue** (not “Register” / “Next”). Captcha / slider screens are marked **Needs manual**; the batch continues.

## Limits verified in Task 10

- **Two-device sheet E2E** (one SIM + one deliberately unreachable Tailscale host) was **not run**: only one USB device (`9257142`) was available and no `100.x` Tailscale address was observed on it.
- Do not use **`pm clear`** on WeChat; logged-in devices are reported as **Already registered**.

# 其它
## 支持声音（软件不做支持）
[关于转发安卓声音到PC的讨论](https://github.com/Genymobile/scrcpy/issues/14#issuecomment-543204526)

## 画面不清晰
在Windows上，您可能需要配置缩放行为。

QtScrcpy.exe>属性>兼容性>更改高DPI设置>覆盖高DPI缩放行为>由以下人员执行缩放：应用程序。

如果视频窗口大小远远小于设备屏幕的大小，则画面会不清晰。这在文字上尤其明显

## 玩和平精英上下车操作会失效
这是由于游戏中上车会创建新的界面，导致鼠标触摸点失效，目前技术上没有好的解决办法

可以通过`连续按两次~键（数字键1左边）`来恢复，这是目前最好的办法。

## 无法输入中文
手机端安装搜狗输入法/QQ输入法就可以支持输入中文了

## 可以控制，但无法看到画面
控制台错误信息可能会包含 QOpenGLShaderProgram::attributeLocation(vertexIn): shader program is not linked

一般是由于显卡不支持当前的视频渲染方式，config.ini里修改下解码方式，改成1或者2试试

## 错误信息：AdbProcess::error:adb server version (40) doesnt match this client (41)
任务管理找到adb进程并杀死，重新操作即可

## 错误信息：Could not open video stream
导致这个错误的原因有很多，最简单的解决方法是在分辨率设置中，选择一个较低的分辨率

