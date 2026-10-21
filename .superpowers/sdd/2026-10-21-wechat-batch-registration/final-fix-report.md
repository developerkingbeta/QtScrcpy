# WeChat batch registration — final review fixes

**Date:** 2026-10-22  
**Worktree:** `wechat-batch-registration`  
**Base:** extends `f11872c` (SMS resend gate for notification fallback)

## Finding 1 — Country gate (+N phones)

**Problem:** Signup forms on WeChat 8.0.66 show country as labels like `United States（+1）`, not bare `+91` rows. The registrar only looked for dial codes that prefix the target number, so `+919…` with US (+1) shown never opened the picker and could not proceed.

**Fix (`wechatregistrar.cpp`):**

- Parse dial codes from any node text via `[+＋](\d{1,3})` (supports fullwidth `（+N）`).
- On `SignUpForm`, compare **shown** vs **desired** dial code:
  - **Match** → `countrySelected = true`, fill nickname/phone/password.
  - **Mismatch** → tap the country/region row (clickable node containing the shown dial), then use `CountryPicker` search (`typeText` national digits) and select the target row.
- Stop overwriting `dialCode` / `phoneEntry` from mismatched visible codes.

**Tests:**

- `countryMismatchOpensPickerForIndia` — `flow_country_us91` replays sanitized 8.0.66 US (+1) form, picker search `91`, India (+91), happy path to `Registered`.
- `flow_country` fixtures trimmed (no spurious picker hop when +91 already shown).

## Finding 2 — Stale notification codes

**Problem:** `extractCodeFromNotifications` has no timestamp; using it on first SMS wait could type old shade codes. The weak `deviceNowMs() < notBeforeMs` guard was insufficient.

**Fix:**

- Keep notification fallback **only when** inbox has no fresh WeChat SMS **and** `smsResendCount >= 1` (from `f11872c`).
- Remove the `deviceNowMs() < notBeforeMs` notification filter (misleading for dumpsys).

**Tests:**

- `staleNotificationCodeNotTyped` — stale inbox SMS + notification `654321`, `maxSmsResend = 0` → `sms timeout`, neither `654321` nor stale SMS typed.

## Verification

```text
cmake -S . -B build-tests -DQSC_BUILD_TESTS=ON
cmake --build build-tests
ctest --test-dir build-tests
```

All **9** tests passed (including `tst_registrar` with 14 slots).
