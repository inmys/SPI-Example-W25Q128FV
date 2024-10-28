#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

#define SPI_DEVICE          "/dev/spidev0.0"
#define MAX_SPI_SPEED       1000000
#define BUFFER_SIZE         6

struct spi_ioc_transfer trx;
uint8_t tx_buffer[BUFFER_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t rx_buffer[BUFFER_SIZE] = {0x00, 0x00, 0x00, 0x00, 0x00};

/**
 * @brief Инициализирует SPI устройство с заданным файловым дескриптором.
 *
 * Устанавливает 0-й режим SPI и максимальную скорость передачи.
 * 
 * @param[in] fd Файловый дескриптор SPI устройства.
 * 
 * @return true, если инициализация прошла успешно, в противном случае false.
 */
bool init_spi_device(int fd)
{
    uint32_t setting = SPI_MODE_0;
    int ret = ioctl(fd, SPI_IOC_WR_MODE, &setting);
    if(ret != 0)
    {
        printf("Could not write SPI mode...\r\n");
        return false;
    }

    setting = MAX_SPI_SPEED;
    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &setting);
    if(ret != 0)
    {
        printf("Could not write the SPI max speed...\r\n");
        return false;
    }

    trx.tx_buf = (uint64_t)tx_buffer;
    trx.rx_buf = (uint64_t)rx_buffer;
    trx.bits_per_word = 0;
    trx.speed_hz = MAX_SPI_SPEED;
    trx.delay_usecs = 0;
    trx.len = BUFFER_SIZE;
    return true;
}

/**
 * @brief Отправляет по SPI устройству байты, заданные в виде строки.
 *        Строка имеет вид "0x01 0x02 0x03 0x04", либо "1 2 3 4".
 *        Иными словами, используется 16-ричная запись числа.
 *
 * @param[in] fd      файловый дескриптор SPI устройства
 * @param[in] tx_rx   строка, содержащая байты, которые нужно
 *                    отправить.
 **/
void send_cmd(int fd, const char *tx_rx)
{
    for (int i = 0; i < BUFFER_SIZE; ++i)
    {
        tx_buffer[i] = 0x00;
        rx_buffer[i] = 0x00;
    }

    
    // Парсит строку вида "0x01 0x02 0x03 0x04" или "1 2 3 4"
    // и сохраняет байты в tx_buffer
    char byte[5];
    for (int i = 0, buf_i = 0; i < strlen(tx_rx) && buf_i < sizeof(tx_buffer);)
    {
        if (tx_rx[i] == ' ' || tx_rx[i] == '\n')
        {
            ++i;
            continue;
        }

        if (tx_rx[i+1] == 'x')
        {
            strncpy(byte, &tx_rx[i], 4);
            i += 4;
        }
        else
        {
            strncpy(byte, "0x", 2);
            strncpy(&byte[2], &tx_rx[i], 2);
            i += 2;
        }

        tx_buffer[buf_i] = strtol(byte, NULL, 16);
        buf_i++;
    }
    
    // Сама отправка байт
    int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &trx);
    if(ret < 1) {
        printf("Failed to write to SPI message...\r\n");
    }

    printf("TX: ");
    for (int i = 0; i < BUFFER_SIZE; ++i)
        printf("%02x ", tx_buffer[i]);

    printf("\r\n");
    printf("RX: ");
    for (int i = 0; i < BUFFER_SIZE; ++i)
        printf("%02x ", rx_buffer[i]);

    printf("\r\n");
}


/**
 * @brief Точка входа.
 *
 * @details
 *     1. Проверяет, что был передан один аргумент - путь к SPI устройству.
 *     2. Инициализирует SPI устройство.
 *     3. Вводит пользователю строку, содержащую байты, которые нужно
 *        отправить. Если пользователь ввел 'q', то программа завершается.
 *     4. Отправляет байты, введенные пользователем, по SPI устройству.
 *     5. Закрывает дескриптор SPI устройства.
 *
 * @param[in] argc Количество аргументов, переданных в программу.
 * @param[in] argv  Массив, содержащий аргументы, переданные в программу.
 *
 * @return 0, если программа была успешно завершена, 1 - если возникли ошибки.
 */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: %s <spi_device>\r\n", argv[0]);
        return 1;
    }

    const char* spi_device = argv[1];
    int fd = open(SPI_DEVICE, O_RDWR);
    if(fd < 0) {
        printf("Could not open the SPI device...\r\n");
        return 1;
    }

    if (!init_spi_device(fd))
    {
        printf("Could not init SPI device...\r\n");
        close(fd);
        return 1;
    }

    char cmd[256] = {0};
    while (cmd[0] != 'q')
    {
        printf("Enter bytes to send or 'q' to quit\n");
        fgets(cmd, sizeof(cmd), stdin);
        if (cmd[0] == 'q')
            break;

        if (cmd[0] != '\n' && cmd[0] != '\0')
            send_cmd(fd, cmd);
    }
    close(fd);
    return 0;
}