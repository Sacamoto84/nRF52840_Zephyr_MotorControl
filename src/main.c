#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/sys/printk.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

// Используем devicetree для определения LED
#define LED0_NODE DT_ALIAS(led0)

// Если led0 не определен в devicetree, используем прямое определение
#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
#else
// Альтернативный вариант для прямого указания пина
#define LED0_NODE DT_NODELABEL(gpio0)
static const struct gpio_dt_spec led = {
    .port = DEVICE_DT_GET(LED0_NODE),
    .pin = 15,
    .dt_flags = GPIO_ACTIVE_LOW
};
#endif

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR),
	BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xfe),
	BT_DATA_BYTES(BT_DATA_SVC_DATA16,
				  0xaa, 0xfe, /* Eddystone UUID */
				  0x10,		  /* Eddystone-URL frame type */
				  0x00,		  /* Calibrated Tx power at 0m */
				  0x00,		  /* URL Scheme Prefix http://www. */
				  'z', 'e', 'p', 'h', 'y', 'r',
				  'p', 'r', 'o', 'j', 'e', 'c', 't',
				  0x08) /* .org */
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static void bt_ready(int err)
{
	char addr_s[BT_ADDR_LE_STR_LEN];
	bt_addr_le_t addr = {0};
	size_t count = 1;

	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	err = bt_le_adv_start(BT_LE_ADV_NCONN_IDENTITY, ad, ARRAY_SIZE(ad),
						  sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	bt_id_get(&addr, &count);
	bt_addr_le_to_str(&addr, addr_s, sizeof(addr_s));

	printk("Beacon started, advertising as %s\n", addr_s);
}

int main(void)
{
	printk("!!! Start\n");
	bool state = true;
	int err;

	// Проверяем готовность устройства GPIO
	if (!device_is_ready(led.port)) {
		printk("!!! LED device not ready\n");
		return 0;
	}
	printk("!!! GPIO device ready\n");

	// Настраиваем пин как выход
	err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (err < 0) {
		printk("!!! Error configuring LED pin: %d\n", err);
		return 0;
	}
	printk("!!! LED pin configured\n");

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(bt_ready);
	if (err) {
		printk("EEE Bluetooth init failed (err %d)\n", err);
	}

	printk("!!! Starting main loop\n");
	while (1) {
		gpio_pin_toggle_dt(&led);
		printk("LED toggle\n");
		k_msleep(1500);
	}

	return 0;
}