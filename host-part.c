#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

#define VENDOR_ID  0x1234
#define PRODUCT_ID 0x5678

#define EP_OUT 0x01 // Endpoint 1 OUT (Host -> Device)
#define EP_IN  0x82 // Endpoint 2 IN  (Device -> Host)

int main(void) {
    libusb_context *ctx = NULL;
    libusb_device_handle *dev_handle = NULL;
    int r;

    r = libusb_init(&ctx);
    if (r < 0) {
        fprintf(stderr, "Ошибка инициализации libusb: %s\n", libusb_error_name(r));
        return 1;
    }

    dev_handle = libusb_open_device_with_vid_pid(ctx, VENDOR_ID, PRODUCT_ID);
    if (!dev_handle) {
        fprintf(stderr, "Устройство VID 0x%04x PID 0x%04x не найдено!\n", VENDOR_ID, PRODUCT_ID);
        libusb_exit(ctx);
        return 1;
    }

    // Отключаем стандартный драйвер ядра, если он захватил интерфейс
    if (libusb_kernel_driver_active(dev_handle, 0) == 1) {
        libusb_detach_kernel_driver(dev_handle, 0);
    }

    r = libusb_claim_interface(dev_handle, 0);
    if (r < 0) {
        fprintf(stderr, "Не удалось захватить интерфейс: %s\n", libusb_error_name(r));
        libusb_close(dev_handle);
        libusb_exit(ctx);
        return 1;
    }

    // 1. Отправка данных на RPi Zero
    unsigned char message[] = "Hello RPi Zero via USB!";
    int transferred = 0;
    
    r = libusb_bulk_transfer(dev_handle, EP_OUT, message, sizeof(message), &transferred, 2000);
    if (r == 0) {
        printf("[Host] Успешно отправлено %d байт: %s\n", transferred, message);
    } else {
        fprintf(stderr, "Ошибка записи: %s\n", libusb_error_name(r));
    }

    // 2. Чтение ответа от RPi Zero
    unsigned char buffer[256] = {0};
    r = libusb_bulk_transfer(dev_handle, EP_IN, buffer, sizeof(buffer) - 1, &transferred, 2000);
    if (r == 0) {
        buffer[transferred] = '\0';
        printf("[Host] Получен ответ (%d байт): %s\n", transferred, buffer);
    } else {
        fprintf(stderr, "Ошибка чтения: %s\n", libusb_error_name(r));
    }

    // Освобождение ресурсов
    libusb_release_interface(dev_handle, 0);
    libusb_close(dev_handle);
    libusb_exit(ctx);
    return 0;
}
