#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <endian.h>
#include <linux/usb/functionfs.h>

// Структура инициализации дескрипторов для FunctionFS v2
struct {
    struct usb_functionfs_descs_head_v2 header;
    __le32 fs_count;
    __le32 hs_count;
    struct {
        struct usb_interface_descriptor intf;
        struct usb_endpoint_descriptor_no_audio sink; // Bulk OUT (Host -> RPi)
        struct usb_endpoint_descriptor_no_audio source; // Bulk IN (RPi -> Host)
    } __attribute__((packed)) fs_descs, hs_descs;
} __attribute__((packed)) descriptors = {
    .header = {
        .magic = htole32(FUNCTIONFS_DESCRIPTORS_MAGIC_V2),
        .flags = htole32(FUNCTIONFS_HAS_FS_DESC | FUNCTIONFS_HAS_HS_DESC),
        .length = htole32(sizeof(descriptors)),
    },
    .fs_count = htole32(3),
    .hs_count = htole32(3),
    .fs_descs = {
        .intf = {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DT_INTERFACE,
            .bNumEndpoints = 2,
            .bInterfaceClass = USB_CLASS_VENDOR_SPEC,
            .bInterfaceSubClass = 0,
            .bInterfaceProtocol = 0,
            .iInterface = 1,
        },
        .sink = {
            .bLength = sizeof(struct usb_endpoint_descriptor_no_audio),
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = 1 | USB_DIR_OUT, // ep1 OUT
            .bmAttributes = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize = htole16(64),
        },
        .source = {
            .bLength = sizeof(struct usb_endpoint_descriptor_no_audio),
            .bDescriptorType = USB_DT_ENDPOINT,
            .bEndpointAddress = 2 | USB_DIR_IN,  // ep2 IN
            .bmAttributes = USB_ENDPOINT_XFER_BULK,
            .wMaxPacketSize = htole16(64),
        },
    },
};

// Строковые дескрипторы
struct {
    struct usb_functionfs_strings_head header;
    struct {
        __le16 code;
        char str1[sizeof("Demo FFS Interface")];
    } __attribute__((packed)) lang0;
} __attribute__((packed)) strings = {
    .header = {
        .magic = htole32(FUNCTIONFS_STRINGS_MAGIC),
        .length = htole32(sizeof(strings)),
        .str_count = htole32(1),
        .lang_count = htole32(1),
    },
    .lang0 = {
        .code = htole16(0x0409),
        .str1 = "Demo FFS Interface",
    },
};

int main(void) {
    int ep0 = open("/dev/ffs/demo/ep0", O_RDWR);
    if (ep0 < 0) {
        perror("Ошибка открытия ep0");
        return 1;
    }

    // Write descriptors & strings
    if (write(ep0, &descriptors, sizeof(descriptors)) < 0) {
        perror("Ошибка записи дескрипторов в ep0");
        close(ep0);
        return 1;
    }
    if (write(ep0, &strings, sizeof(strings)) < 0) {
        perror("Ошибка записи строк в ep0");
        close(ep0);
        return 1;
    }

    printf("[RPi] Инициализация ep0 успешна. Привяжите UDC к гаджету!\n");

    // Открываем созданные ядром эндпоинты
    int ep_out = open("/dev/ffs/demo/ep1", O_RDWR); // Host -> RPi
    int ep_in  = open("/dev/ffs/demo/ep2", O_RDWR); // RPi -> Host

    if (ep_out < 0 || ep_in < 0) {
        perror("Ошибка открытия ep1/ep2");
        return 1;
    }

    char rx_buf[256] = {0};
    printf("[RPi] Ожидание сообщений от хоста...\n");

    // Читаем пакет от хоста
    ssize_t bytes_read = read(ep_out, rx_buf, sizeof(rx_buf) - 1);
    if (bytes_read > 0) {
        rx_buf[bytes_read] = '\0';
        printf("[RPi] Получено от хоста: %s\n", rx_buf);

        // Формируем и отправляем ответ
        char tx_buf[256];
        snprintf(tx_buf, sizeof(tx_buf), "Echo from RPi: %s", rx_buf);
        write(ep_in, tx_buf, strlen(tx_buf));
        printf("[RPi] Ответ отправлен хосту.\n");
    }

    close(ep_out);
    close(ep_in);
    close(ep0);
    return 0;
}
