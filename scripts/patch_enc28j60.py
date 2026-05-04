Import("env")

from pathlib import Path


PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
PIOENV = env.subst("$PIOENV")
LIB_ROOT = PROJECT_DIR / ".pio" / "libdeps" / PIOENV / "ESP32-ENC28J60" / "src"


def apply_replacements(path, replacements):
    if not path.exists():
        print(f"[patch_enc28j60] Skip missing file: {path}")
        return

    text = path.read_text(encoding="utf-8")
    original = text

    for old, new in replacements:
        if old in text:
            text = text.replace(old, new, 1)

    if text != original:
        path.write_text(text, encoding="utf-8")
        print(f"[patch_enc28j60] Patched {path.relative_to(PROJECT_DIR)}")


apply_replacements(
    LIB_ROOT / "ESP32-ENC28J60.h",
    [
        (
            '#include "WiFi.h"\n#include "esp_system.h"',
            '#include "WiFi.h"\n#include "IPv6Address.h"\n#include "esp_system.h"',
        ),
        ("    IPAddress localIPv6();", "    IPv6Address localIPv6();"),
    ],
)

apply_replacements(
    LIB_ROOT / "ESP32-ENC28J60.cpp",
    [
        ("IPAddress ENC28J60Class::localIPv6() {", "IPv6Address ENC28J60Class::localIPv6() {"),
        ("    return IPAddress(IPv6);", "    return IPv6Address();"),
        (
            "  return IPAddress(IPv6, (const uint8_t*)addr.addr, addr.zone);",
            "  return IPv6Address((const uint32_t*)addr.addr);",
        ),
        (
            """  if (esp_read_mac(mac_eth, ESP_MAC_ETH) == ESP_OK) {
    char macStr[18] = { 0 };

    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac_eth[0], mac_eth[1], mac_eth[2],
            mac_eth[3], mac_eth[4], mac_eth[5]);

    log_i("Using built-in mac_eth = %s", macStr);

    esp_base_mac_addr_set(mac_eth);
  } else {
    log_i("Using user mac_eth");
    memcpy(mac_eth, ENC28J60_Mac, sizeof(mac_eth));

    esp_base_mac_addr_set(ENC28J60_Mac);
  }
""",
            """  const bool use_custom_mac = ENC28J60_Mac != nullptr && memcmp(ENC28J60_Mac, ENC28J60_Default_Mac, sizeof(mac_eth)) != 0;
  if (use_custom_mac) {
    log_i("Using user mac_eth");
    memcpy(mac_eth, ENC28J60_Mac, sizeof(mac_eth));
    esp_base_mac_addr_set(ENC28J60_Mac);
  } else if (esp_read_mac(mac_eth, ESP_MAC_ETH) == ESP_OK) {
    char macStr[18] = { 0 };

    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac_eth[0], mac_eth[1], mac_eth[2],
            mac_eth[3], mac_eth[4], mac_eth[5]);

    log_i("Using built-in mac_eth = %s", macStr);

    esp_base_mac_addr_set(mac_eth);
  } else {
    memcpy(mac_eth, ENC28J60_Default_Mac, sizeof(mac_eth));
    esp_base_mac_addr_set(ENC28J60_Default_Mac);
  }
""",
        ),
    ],
)

apply_replacements(
    LIB_ROOT / "extmod" / "esp_eth_mac_enc28j60.c",
    [
        (
            '#include "driver/gpio.h"',
            '#include "driver/gpio.h"\n#include "esp_cpu.h"',
        ),
        ("esp_cpu_get_core_id()", "xPortGetCoreID()"),
    ],
)

PHY_HELPERS_BLOCK = """static esp_err_t enc28j60_advertise_pause_ability(esp_eth_phy_t* phy, uint32_t ability)
{
    (void)phy;
    (void)ability;
    return ESP_OK;
}

static esp_err_t enc28j60_loopback(esp_eth_phy_t* phy, bool enable)
{
    phy_enc28j60_t* enc28j60 = __containerof(phy, phy_enc28j60_t, parent);
    esp_eth_mediator_t* eth = enc28j60->eth;
    phcon1_reg_t phcon1;

    PHY_CHECK(eth->phy_reg_read(eth, enc28j60->addr, ETH_PHY_PHCON1_REG_ADDR, &(phcon1.val)) == ESP_OK,
        "read PHCON1 failed", err);
    phcon1.ploopbk = enable ? 1 : 0;
    PHY_CHECK(eth->phy_reg_write(eth, enc28j60->addr, ETH_PHY_PHCON1_REG_ADDR, phcon1.val) == ESP_OK,
        "write PHCON1 failed", err);
    return ESP_OK;
err:
    return ESP_FAIL;
}

"""


def patch_phy_file(path):
    if not path.exists():
        print(f"[patch_enc28j60] Skip missing file: {path}")
        return

    text = path.read_text(encoding="utf-8")
    original = text

    text = text.replace('#include "eth_phy_802_3_regs.h"', '#include "eth_phy_regs_struct.h"', 1)
    text = text.replace(
        """static esp_err_t enc28j60_autonego_ctrl(esp_eth_phy_t* phy, eth_phy_autoneg_cmd_t cmd, bool* autoneg_en_stat)
{
    /**
     * ENC28J60 does not support automatic duplex negotiation.
     * If it is connected to an automatic duplex negotiation enabled network switch,
     * ENC28J60 will be detected as a half-duplex device.
     * To communicate in Full-Duplex mode, ENC28J60 and the remote node
     * must be manually configured for full-duplex operation.
     */

    switch (cmd)
    {
    case ESP_ETH_PHY_AUTONEGO_RESTART:
        /* Fallthrough */
    case ESP_ETH_PHY_AUTONEGO_EN:
        return ESP_ERR_NOT_SUPPORTED;
    case ESP_ETH_PHY_AUTONEGO_DIS:
        /* Fallthrough */
    case ESP_ETH_PHY_AUTONEGO_G_STAT:
        *autoneg_en_stat = false;
        break;
    default:
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}
""",
        """static esp_err_t enc28j60_negotiate(esp_eth_phy_t* phy)
{
    (void)phy;
    /**
     * ENC28J60 does not support automatic duplex negotiation.
     * If it is connected to an automatic duplex negotiation enabled network switch,
     * ENC28J60 will be detected as a half-duplex device.
     * To communicate in Full-Duplex mode, ENC28J60 and the remote node
     * must be manually configured for full-duplex operation.
     */
    return ESP_ERR_NOT_SUPPORTED;
}
""",
        1,
    )

    while PHY_HELPERS_BLOCK in text:
        text = text.replace(PHY_HELPERS_BLOCK, "")

    text = text.replace(
        "static esp_err_t enc28j60_pwrctl(esp_eth_phy_t* phy, bool enable)\n",
        PHY_HELPERS_BLOCK + "static esp_err_t enc28j60_pwrctl(esp_eth_phy_t* phy, bool enable)\n",
        1,
    )

    text = text.replace(
        """    enc28j60->parent.set_mediator = enc28j60_set_mediator;
    enc28j60->parent.autonego_ctrl = enc28j60_autonego_ctrl;
    enc28j60->parent.get_link = enc28j60_get_link;
    enc28j60->parent.pwrctl = enc28j60_pwrctl;
    enc28j60->parent.get_addr = enc28j60_get_addr;
    enc28j60->parent.set_addr = enc28j60_set_addr;
    enc28j60->parent.set_speed = enc28j60_set_speed;
    enc28j60->parent.set_duplex = enc28j60_set_duplex;
    enc28j60->parent.del = enc28j60_del;
""",
        """    enc28j60->parent.set_mediator = enc28j60_set_mediator;
    enc28j60->parent.negotiate = enc28j60_negotiate;
    enc28j60->parent.get_link = enc28j60_get_link;
    enc28j60->parent.pwrctl = enc28j60_pwrctl;
    enc28j60->parent.get_addr = enc28j60_get_addr;
    enc28j60->parent.set_addr = enc28j60_set_addr;
    enc28j60->parent.advertise_pause_ability = enc28j60_advertise_pause_ability;
    enc28j60->parent.loopback = enc28j60_loopback;
    enc28j60->parent.del = enc28j60_del;
""",
        1,
    )

    if text != original:
        path.write_text(text, encoding="utf-8")
        print(f"[patch_enc28j60] Patched {path.relative_to(PROJECT_DIR)}")


patch_phy_file(LIB_ROOT / "extmod" / "esp_eth_phy_enc28j60.c")
