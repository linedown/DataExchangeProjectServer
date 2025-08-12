#include <iostream>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <clocale>
#include <thread>
#include <chrono>
#include <cstdint>
#pragma comment(lib, "Ws2_32.lib")

#define IPPROTO_LAN 63

using namespace std;
using namespace chrono;

const int messages = 10;
const int dataLength = 1024;
const int packetLength = 2048;
const int delayMS = 100;

struct iphdr
{
#if __BYTE_ORDER == __LITTLE_ENDIAN
    unsigned int ihl : 4;
    unsigned int version : 4;
#elif __BYTE_ORDER == __BIG_ENDIAN
    unsigned int version : 4;
    unsigned int ihl : 4;
#else
# error	"Please fix <bits/endian.h>"
#endif
    uint8_t tos;
    uint16_t tot_len;
    uint16_t id;
    uint16_t frag_off;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t check;
    uint32_t saddr;
    uint32_t daddr;
    /*The options start here. */
};

/*Оператор >>  означает побитовый сдвиг числа вправо на определенное количество разрядов. В данном случае sum >> 16
означает сдвиг sum на 16 разрядов вправо.

Побитовые операторы & и | используются для манипуляции битами чисел. В случае подсчета контрольной суммы IP-заголовка,
побитовые операторы используются для корректного вычисления контрольной суммы.

sum & 0xFFFF побитово "и" (&) используется для получения младшего слова (нижние 16 бит) sum.
sum >> 16 сдвигает sum на 16 разрядов вправо.
sum = (sum >> 16) + (sum & 0xFFFF) складывает младшее слово и результат сдвига, учитывая переносы.
sum + (sum >> 16) корректно складывает результаты сдвига и младшего слова, учитывая возможные переносы.
result является однократно инвертированным(~) суммарным результатом.
Эти операции являются частью алгоритма подсчета контрольной суммы IPv4 заголовка и позволяют вычислить контрольную сумму, учитывая все поля структуры. */

// Функция для подсчета контрольной суммы
unsigned short checksum(void* b, int len) {
    unsigned short* buf = static_cast<unsigned short*>(b);
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;

    if (len == 1)
        sum += static_cast<unsigned char>(*buf);

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    result = static_cast<unsigned short>(~sum);
    return result;
}

void serverFunction() {
    WSAData wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd) != 0) {
        cerr << "Ошибка при инициализации винсока: " << WSAGetLastError() << "\n";
        exit(1);
    }
    else cout << "Винсок проинициализирован успешно!!\n";

    int rawSocket = socket(AF_INET, SOCK_RAW, IPPROTO_LAN);
    if (rawSocket < 0) {
        cerr << "Ошибка при создании сокета " << WSAGetLastError() << "\n";
        WSACleanup();
        exit(1);
    }
    else cout << "Сокет создан успешно!!\n";

    struct sockaddr_in clientAddr;
    memset(&clientAddr, 0, sizeof(clientAddr));
    clientAddr.sin_family = AF_INET;
    if (inet_pton(AF_INET, "192.168.88.44", &clientAddr.sin_addr) < 0) {
        cerr << "Ошибка при переводе адреса: " << WSAGetLastError() << "\n";
        closesocket(rawSocket);
        WSACleanup();
        exit(1);
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof serverAddr);
    serverAddr.sin_family = AF_INET;
    /*serverAddr.sin_port = htons(0);*/
    if (inet_pton(AF_INET, "192.168.88.43", &serverAddr.sin_addr) < 0) {
        cerr << "Ошибка при переводе адреса: " << WSAGetLastError() << "\n";
        closesocket(rawSocket);
        WSACleanup();
        exit(1);
    }
    else cout << "Адрес переведен успешно!!\n";

    if (bind(rawSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Ошибка привязки сокета: " << WSAGetLastError() << "\n";
        closesocket(rawSocket);
        WSACleanup();
        exit(1);
    }
    else cout << "Привязка сокета прошла успешно!!\n";

    for (int i = 0; i < messages; ++i) {
        iphdr ipheader;
        memset(&ipheader, 0, sizeof(ipheader));
        ipheader.version = 4;
        ipheader.ihl = 5;
        ipheader.tos = 0;
        ipheader.tot_len = htons(sizeof(iphdr) + dataLength);
        ipheader.id = htons(i);
        ipheader.frag_off = 0;
        ipheader.ttl = 64;
        ipheader.protocol = IPPROTO_LAN;
        ipheader.check = 0;
        inet_pton(AF_INET, "192.168.88.43", &(ipheader.saddr));
        inet_pton(AF_INET, "192.168.88.44", &(ipheader.daddr));
        ipheader.check = checksum(&ipheader, sizeof(ipheader));

        char data[dataLength];
        memset(data, 'A' + i % 26, dataLength);

        char packetBuffer[packetLength];
        memcpy(packetBuffer, &ipheader, sizeof iphdr);
        memcpy(packetBuffer + sizeof iphdr, data, dataLength);

        int bytesToSend = sendto(rawSocket, packetBuffer, sizeof iphdr + dataLength, 0, (struct sockaddr*)&clientAddr, sizeof(clientAddr));
        if (bytesToSend < 0) cerr << "Ошибка при передаче пакета с номером " << i + 1 << ". Код ошибки: " << WSAGetLastError();
        else cout << "Пакет с номером " << i + 1 << " отправлен. Размер: " << bytesToSend << " байт.\n";
        cout << "Данные: \n";
        for (char i : packetBuffer) cout << i;
        cout << "\n";

        //this_thread::sleep_for(milliseconds(delayMS));
    }

    closesocket(rawSocket);
    WSACleanup();
}

int main() {
    setlocale(LC_ALL, "ru");
    serverFunction();
    /*thread serverThread(serverFunction);*/
    /*this_thread::sleep_for(milliseconds(1000));
    thread clientThread(clientFunction);

    serverThread.join();
    clientThread.join();*/

    cout << "Конец программы\n";
    //cout << "Версия протокола IP: " << ipheader.version << "\n";
    //cout << "Длина заголовка IP: " << ipheader.ihl << " байтов" << "\n";
    //cout << "ToS: " << static_cast<int>(ipheader.tos) << "\n";
    //cout << "Суммарная длина: " << ipheader.tot_len << " байтов" << "\n";
    //cout << "Идентификатор: " << ipheader.id << "\n";
    //cout << "TTL: " << static_cast<int>(ipheader.ttl) << "\n";
    //cout << "Протокол: " << static_cast<int>(ipheader.protocol) << "\n";
    //cout << "Контрольная сумма: " << ntohs(ipheader.check) << "\n"; // hex - для 16-ного перевода, dec (по умолчанию) - для 10-ного

    return 0;
}