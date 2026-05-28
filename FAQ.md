# FAQ

## Port dropdown is empty / "Auto-detect: no ELRS TX module found"

The TX module is missing its serial driver. In Device Manager it shows as **USB Composite Device** with no entry under *Ports (COM & LPT)*.

**Fix:**
1. `Win+R` → `devmgmt.msc`
2. *Universal Serial Bus controllers* → right-click **USB Composite Device** (your TX) → **Update driver**
3. **Browse my computer** → **Let me pick from a list** → **Ports (COM & LPT)** → **USB Serial Device (CDC)**
4. Replug the module, click **Refresh** in the app

**Or install the driver directly:**

| Chip | VID/PID | Driver |
|------|---------|--------|
| STM32 VCP (RadioMaster Nomad) | 0483/5740 | [ST VCP](https://www.st.com/en/development-tools/stsw-stm32102.html) |
| CP210x | 10C4/EA60 | [Silicon Labs](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) |
| CH340/CH341 | 1A86/7523 | [WCH](https://www.wch-ic.com/downloads/CH341SER_EXE.html) |
| FTDI FT232 | 0403/6001 | [FTDI](https://ftdichip.com/drivers/vcp-drivers/) |

To find your chip: right-click the device → **Properties → Details → Hardware IDs**.
