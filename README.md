目前该项目存在较多 Bug，完成度较低，不适用于生产环境。

本项目是内核机制研究与自签驱动加载的教学性工程，仅面向**隔离测试机 / 虚拟机**。
仓库**不提供一键安装程序**，也不会替使用者修改固件、启动策略或代码完整性设置。

---

## 1. 发布物说明

| 位置 | 内容 |
|---|---|
| **Releases** | `KMTools.zip`

---

## 2. 自签驱动的加载路线（二选一）

x64 上内核驱动必须有可验证的签名。自签驱动有两条互斥的加载路线：

| 路线 | 适用场景 | Secure Boot | 步骤 |
|---|---|---|---|
| **A. 测试签名模式** | 本机 / 虚拟机快速验证 | **关闭** | 见第 3 节 |
| **B. Custom Kernel Signers（CKS）** | 适用于自用/安全要求高的场景 | **开启** | ❌ 仅提供官方文档链接，见第 4 节 |

两条路线的安全设置方向相反，**不要同时配置**。

---

## 3. 路线 A：测试签名模式（完整安装步骤）

### 3.1 确认 Secure Boot 与 HVCI 已关闭

```powershell
# 管理员 PowerShell
Confirm-SecureBootUEFI          # True = 仍开启；提示「不支持」通常是 Legacy/CSM，可跳过
reg query "HKLM\SYSTEM\CurrentControlSet\Control\DeviceGuard\Scenarios\HypervisorEnforcedCodeIntegrity" /v Enabled
                                # 0x1 = 仍开启
```

也可在 `msinfo32` 查看「安全启动状态」，在「核心隔离」页面查看内存完整性开关。
Secure Boot 开启时 `bcdedit /set testsigning on` 会被启动策略拒绝；HVCI 开启时测试签名驱动不会通过代码完整性校验。

### 3.2 导入自签证书到本地计算机存储

自行下载发行版并安装压缩包中的公钥证书

### 3.3 生成并签名 catalog

INF 或 `.sys` 有**任何字节改动**都必须自行生成证书并签名然后重新生成 `.cat`，旧 catalog 会整包失效。

### 3.4 开启测试签名并安装

```cmd
bcdedit /set testsigning on
pnputil /add-driver Kernel_Tools.inf /install
shutdown /r /t 0
```

**重启不可省略**：它一次完成三件事 —— 让测试签名生效、让 `StartType = 2` 被 SCM 拉起、
清除 `STATUS_FLT_DO_NOT_DETACH` 赖着不走的旧内核镜像。

### 3.5 校验

```cmd
sc query KMTools
fltmc filters | findstr /i KMTools
fltmc instances -f KMTools          :: Num Instances 必须 > 0
powershell -ExecutionPolicy Bypass -File .\verify_device.ps1   :: \\.\Kernel_Tools 可打开
```

`Num Instances = 0` 但设备能打开，说明文件回调静默失效。

> 运行时 `sc stop` / `fltmc unload` 对本驱动无效（返回 `0x801f0010`，
> 即 `STATUS_FLT_DO_NOT_DETACH`）**任何卸载或覆盖 `.sys` 都要重启**。

---

## 4. 路线 B：Custom Kernel Signers（CKS）

CKS 允许组织以自有 PKI 签名的内核驱动在 **Secure Boot 保持开启**的情况下被信任，
其信任链涉及固件 PK / KEK、已签名的 App Control 策略与 UEFI 固件变量；
策略部署后若无同权威签名的替换策略就删除，会导致 Windows 无法启动，需进入 UEFI 恢复。

**本仓库不提供 CKS 的启用步骤，也不代管固件密钥、策略文件或签名授权。请直接以微软官方文档为准：**

- [Custom Kernel Signers for App Control for Business — Microsoft Learn（英文）](https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/design/custom-kernel-signers)
- [适用于企业的 App Control 的自定义内核签名者 — Microsoft Learn（中文）](https://learn.microsoft.com/zh-cn/windows/security/application-security/application-control/app-control-for-business/design/custom-kernel-signers)

走这条路线时，请勿套用第 3 节的 `bcdedit /set testsigning on`：CKS 不依赖测试签名模式，
且需要 Secure Boot 处于开启状态。

---

## 5. 卸载与恢复

```cmd
pnputil /enum-drivers                        :: 查出 oemNN.inf，或读 Services\KMTools\Owners
pnputil /delete-driver <oemNN.inf> /uninstall
sc delete KMTools
bcdedit /set testsigning off
certutil -delstore "Root" <证书指纹>          :: 可选，移除测试根证书
shutdown /r /t 0
```

重启后将 **Secure Boot** 与 **内存完整性** 重新打开。

---

## 6. 从源码构建

- Visual Studio 2026 + Windows Driver Kit（WDK），用户态 C++17 / C++20 均可。
- 驱动、CLI、GUI 共享 `KernCmdShared.h` 的 IOCTL 契约；设备对象名与 IOCTL 数值彼此独立，改设备名不需要动 IOCTL。
- 图标资源 DLL 由 `KernCmdDist/` 下的子集 TTF + `KernCmdFonts.rc` + 构建脚本生成，产物 `KernCmdFonts.dll` 不进仓库。
- 构建完成后按 3.3 生成并签名 catalog.

---

## 7. 已知边界

- PPL 保护目前**仅提供剥离**，驱动没有设置 / 附加接口；`BreakOnTermination` 为独立开关。
- 当前身份是 SYSTEM，**不带 TrustedInstaller 组**；HKCU 操作依赖 `--sid=` 补丁。
- 令牌操作会被 EDR / 火绒等安全产品记录；UAC 安全桌面无法覆盖是设计如此。
- 进程树 PPID 复用只保证不重复，父子归属仍可能错挂。
- 当前 minifilter 高度为合法低值，与 `FSFilter Activity Monitor` 加载顺序组不匹配,且优先级低于绝大多数驱动程序.
- 测试签名模式是整机安全降级；关闭 Secure Boot / HVCI 的代价由使用者自行承担。

---

## 8. 许可证与第三方声明

本项目源码采用 **MIT License**（见 `LICENSE`）。

图标资源来自 Fluent UI System Icons（MIT）。MIT 只覆盖本项目自行编写的代码，
第三方组件按各自许可证保留原始声明。

---

*仅供学术研究与合法、隔离的测试环境使用。作者不对误用、系统损坏或数据损失负责。*
