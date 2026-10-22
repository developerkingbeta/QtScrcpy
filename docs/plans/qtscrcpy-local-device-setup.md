# QtScrcpy local device setup (macOS)

## Goal
Install QtScrcpy on this Mac, verify ADB sees the user's Android device, launch QtScrcpy, and confirm screen mirror works.

## Global Constraints
- Prefer official release DMG over building from source
- Do not push/commit unless asked
- Stop only for irreversible/security actions or physical device steps

## Tasks
### Task 1: Install QtScrcpy
- Download macOS release matching arch (Intel=x64, Apple Silicon=arm64)
- Install to ~/Applications, clear quarantine
- Verify binary launches

### Task 2: Verify ADB
- Ensure adb on PATH (SDK platform-tools or bundled)
- `adb devices -l` shows authorized device

### Task 3: Launch mirror test
- Open QtScrcpy against connected device
- Confirm mirror window / process alive
- Wireless: USB once → Get device IP → Start adbd → Wireless connect → refresh → Start service
- Remote later: prefer Tailscale/VPN host in Wireless field; do not expose ADB :5555 on the public internet

### Task 4: Remote via Tailscale (cellular / off-LAN)
1. Mac on Tailscale (`tailscale ip -4` → e.g. `dragon`).
2. Install Tailscale on phone (Play Store or official APK from pkgs.tailscale.com/stable). Sign in same Tailscale account; enable VPN.
3. Note phone Tailscale IP (`100.x`) from `tailscale status` or phone app.
4. USB once: `adb -s <serial> tcpip 5555`, then `adb connect <phone-tailscale-ip>:5555`.
5. QtScrcpy Wireless field: `<phone-tailscale-ip>` (port 5555) → connect → Start service.
6. Keep ADB off the public internet; Tailscale only.

## Spec
No separate OpenSpec; plan is authority. Provisional rulings OK.
