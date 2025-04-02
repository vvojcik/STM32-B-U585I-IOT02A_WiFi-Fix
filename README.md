# STM32 B-U585I-IOT02A Wi-Fi Fix (Azure RTOS / NetXDuo)

![STM32](https://img.shields.io/badge/STM32-B--U585I--IOT02A-blue)
![Azure RTOS](https://img.shields.io/badge/Azure_RTOS-NetXDuo-0078D4)
![Wi-Fi](https://img.shields.io/badge/Wi--Fi-MXCHIP_EMW3080-green)
![Firmware](https://img.shields.io/badge/Firmware-V2.3.4+-brightgreen)
![STM32CubeIDE](https://img.shields.io/badge/STM32CubeIDE-compatible-grey)

This repository provides a clean, working baseline for connecting the **STM32 B-U585I-IOT02A** discovery board to a Wi-Fi network using Azure RTOS (NetXDuo) and the MXCHIP EMW3080 Wi-Fi module.

Out of the box, generating a project with Azure RTOS and NetXDuo in STM32CubeIDE results in multiple compile errors, missing headers, and missing connection logic. This repository solves all of those issues.

---

## ⚠️ Prerequisite: Firmware Update

Before applying this code, you **must** update the Wi-Fi module firmware on your board to at least `V2.3.4`.

1. Open the `WIFI_Boot` project from the official STM32Cube firmware package.
2. Flash it to your board.
3. Use the provided ST script to flash the new firmware to the MXCHIP EMW3080 module.

---

## 🚀 The Quick Way (Using files from this repo)

If you just want to make your Wi-Fi and MQTT work immediately without hunting down bugs, follow these steps:

1. Create your project in STM32CubeIDE.
2. Download the 4 files provided in this repository:
   * `app_netxduo.c`
   * `app_netxduo.h`
   * `mx_wifi.c`
   * `mx_wifi_conf.h`
3. **Replace** your existing generated files with the ones from this repo. They are usually located at:
   * `NetXDuo/App/app_netxduo.c`
   * `NetXDuo/App/app_netxduo.h`
   * `Drivers/BSP/Components/mx_wifi/mx_wifi.c`
   * `Core/Inc/mx_wifi_conf.h` *(If you can't find it in the tree, press `Ctrl+Shift+R` in CubeIDE and search for it)*.
4. Open **`mx_wifi_conf.h`**, find lines ~58-59, and enter your actual Wi-Fi credentials:
   ```c
   #define WIFI_SSID       "Your_Network_Name"
   #define WIFI_PASSWORD   "Your_Password"
   ```
6. Right-click your project -> **Clean...** -> **Build Project**. It will compile with `0 errors`.

---

## 🔧 The Manual Way (What was actually fixed?)

If you want to know what exactly was broken by default and how we fixed it, here is the full list of "gotchas":

### 1. The Missing Header Bug (`unknown type name 'MX_WIFIObject_t'`)

By default, `app_netxduo.c` attempts to use the Wi-Fi module but forgets to include its header.

**Fix:** Added `#include "mx_wifi.h"` at the top of `app_netxduo.c`.

---

### 2. The Missing Connection Logic

The auto-generated code initializes the NetX IP instance but **never actually sends the connect command to the Wi-Fi module**.

**Fix:** In `app_netxduo.c`, inside `App_Main_Thread_Entry()`, added:

```c
extern MX_WIFIObject_t *wifi_obj_get(void);
MX_WIFIObject_t *pWifi = wifi_obj_get();

printf("Connecting to Wi-Fi: %s...\n", WIFI_SSID);
if (MX_WIFI_Connect(pWifi, WIFI_SSID, WIFI_PASSWORD, MX_WIFI_SEC_WPA2_AES) != MX_WIFI_STATUS_OK)
    Error_Handler();
```

> **Note:** The security mode MUST be `MX_WIFI_SEC_WPA2_AES` for this specific BSP, otherwise the compiler will throw an undeclared identifier error.

---

### 3. The SSID/Password Conflict (`#error The symbol WIFI_SSID should be defined`)

If you define `WIFI_SSID` in `app_netxduo.h`, you will get a conflict. The lower-level driver (`nx_driver_emw3080.c`) requires the credentials to be defined strictly inside `mx_wifi_conf.h`.

**Fix:** Removed the SSID/Password defines from `app_netxduo.h` completely. They are now solely defined in `mx_wifi_conf.h`.

---

### 4. The Infinite Initialization Loop

Sometimes, `MX_WIFI_Init()` in `mx_wifi.c` hangs forever or throws `MX_ASSERT(false)` because it gets confused by the updated firmware version.

**Fix:** Replaced `MX_WIFI_Init()` with a cleaner version that explicitly requests the MAC address (`MIPC_API_WIFI_GET_MAC_CMD`) and properly verifies the IPC connection before moving forward. Added debug logs.

---

### 5. GDB Server Crash (`Could not determine GDB version`)

STM32CubeIDE's built-in console often vanishes or crashes the GDB server when doing network debugging.

**Fix / Recommendation:**
* Close the IDE console completely.
* Open an external terminal like **Putty**.
* Set it to **Serial, COMx, 115200 baud**.
* Run the code in the IDE (green play button). The logs will stream flawlessly to Putty.

---

## 🎉 Testing the Connection

Once flashed, open Putty. Press the black **Reset** button on your STM32 board. You should see:

```
Wi-Fi Template application started..
Connecting to Wi-Fi: Your_Network_Name...
Wi-Fi connected! Starting DHCP...
STM32 IpAddress: 192.168.x.x
```

After the initial connection, the code enters an **auto-reconnect loop** — it checks the Wi-Fi status every 2 seconds and automatically reconnects if the connection drops.