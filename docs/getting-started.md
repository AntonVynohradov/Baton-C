# Getting started

## 1. Add the library as a submodule

```sh
git submodule add <repository-url>/intercom.git libs/intercom
git submodule update --init
```

## 2. Copy the configuration into your project

The configuration contains types and sizes specific to your system, so **it is not edited inside the submodule**. Copy the template into your own directory:

```sh
mkdir -p Config
cp libs/intercom/config/IntercomConfig_template.h Config/IntercomConfig.h
```

Define your messages in `IntercomConfig.h`. **Every node on the bus must use the same list**, so it is best kept in a shared repository or directory.

```c
#define INTERCOM_STATE_LIST(X)          \
    X(NODE_STATUS,  0x01, 4)            \
    X(TEMPERATURE,  0x02, 2)

#define INTERCOM_EVENT_LIST(X)          \
    X(CMD_START,    0x10, 8)            \
    X(ALARM,        0x12, 16)
```

Each entry generates `INTERCOM_MSG_<NAME>` (the identifier) and `INTERCOM_MSG_<NAME>_MAX_LEN`. Details are in [Configuration](configuration.md).

## 3. Add it to the build

The library is compiled together with your `IntercomConfig.h`, so every project builds its own copy of `libintercom.a`.

### CMake

Either include the target definition directly (nothing from the library's top-level `CMakeLists.txt` is applied):

```cmake
set(INTERCOM_CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Config)
include(libs/intercom/intercom.cmake)
target_link_libraries(my_firmware PRIVATE intercom)
```

or add the whole project as a subdirectory:

```cmake
set(INTERCOM_CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Config CACHE PATH "" FORCE)
add_subdirectory(libs/intercom)
target_link_libraries(my_firmware PRIVATE intercom)
```

The `intercom` target adds `libs/intercom/include` and the configuration directory to the application's include paths by itself. The version (`INTERCOM_VERSION_MAJOR/MINOR/PATCH`) is taken from the library's latest `vX.Y.Z` Git tag (`cmake/version.cmake`) and passed as compile definitions; without a tag the fallback values from `Intercom.h` apply.

### IDE (STM32CubeIDE, Keil, IAR, Make)

1. Add the `libs/intercom/src/*.c` files to the build. You can also build them as a separate static library project.
2. Add two include paths:
   - `libs/intercom/include`
   - the directory containing your `IntercomConfig.h`

The private headers in `src/` need no include path, because the `.c` files include them relatively. The application only sees `Intercom.h`.

## 4. Minimal slave

```c
#include "Intercom.h"

static const uint8_t SLAVES[] = { 0x11, 0x12, 0x13 };   /* identical list on every node */

static uint32_t port_time(void *ctx)                        { (void)ctx; return HAL_GetTick(); }
static bool     port_tx(const uint8_t *d, uint16_t n, void *ctx) { (void)ctx; return uart_start_tx(d, n); }

static void on_message(uint8_t src, uint8_t id, const uint8_t *data, uint8_t len, void *ctx)
{
    (void)ctx;
    switch (id)
    {
        case INTERCOM_MSG_CMD_START: /* ... */ break;
        case INTERCOM_MSG_TEMPERATURE: /* ... */ break;
        default: break;
    }
}

int main(void)
{
    board_init();

    IntercomCfg cfg;
    Intercom_GetDefaultCfg(&cfg);         /* 9600 bit/s, slot 250 ms, ACK 20 ms ... */
    cfg.role        = INTERCOM_ROLE_SLAVE;
    cfg.ownAddr     = 0x12;
    cfg.masterAddr  = 0x01;
    cfg.slaveAddrs  = SLAVES;
    cfg.slaveCount  = sizeof(SLAVES);
    cfg.getTimeMs   = port_time;
    cfg.uartStartTx = port_tx;
    cfg.onMessage   = on_message;

    if (Intercom_Init(&cfg) != INTERCOM_OK) { error(); }
    uart_enable_rx_irq();                 /* only after Intercom_Init() */

    for (;;)
    {
        Intercom_Process();

        int16_t t = read_temperature();
        Intercom_Publish(INTERCOM_MSG_TEMPERATURE, &t, sizeof t);   /* latest value wins */

        if (button_pressed())
        {
            uint8_t level = 3;
            Intercom_Send(INTERCOM_ADDR_BROADCAST, INTERCOM_MSG_ALARM, &level, 1, INTERCOM_PRIO_HIGH);
        }
    }
}

/* UART interrupt */
void USARTx_IRQHandler(void)
{
    if (rx_not_empty()) { Intercom_RxByteIsr(read_rx_byte()); }
}
```

## 5. Master

The master differs only in its configuration:

```c
cfg.role       = INTERCOM_ROLE_MASTER;
cfg.ownAddr    = 0x01;                   /* == masterAddr */
cfg.masterAddr = 0x01;
cfg.onPollDone = on_poll_done;           /* optional: per-slot diagnostics */
```

The master can also send events (`Intercom_Send`) and publish states. They reach the slaves inside POLL frames.

## 6. Next steps

- Wire up the UART as described in [Porting](porting.md): echo in single-wire mode, or the TX-complete interrupt for RS-485.
- Choose the timings (`slotTimeMs`, `ackTimeoutMs`) according to the [Protocol](protocol.md#time-budget).
- `Intercom_Init()` returns `INTERCOM_ERR_PARAM` when the configuration is inconsistent, e.g. the slot is too short for the maximum frame. The full list of conditions is in the [API](api.md#intercom_init).
