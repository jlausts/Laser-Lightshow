#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <windows.h>
#include <sys/time.h>

typedef struct 
{
    uint8_t r, g, b;
    uint16_t laser_x, laser_y, audio_l, audio_r;
} Data;

#define ISR_HZ 40000//50000

enum {XHZ, YHZ, RED, GREEN, BLUE, XOFF, YOFF, ROTATE}TYPES;

HANDLE serial_conn;

void packOLD(const Data *const data_array, uint8_t *const arr, int num_bytes)
{
    
    for (int i = 0, j = 0; j < num_bytes; ++i, j += 8)
    {
        uint8_t *const packed = &arr[j];
        const Data *const data = &data_array[i];

        // rgb: pack to 11 bits (5+6+7 bits, rounded)
        packed[0]  =  (data->r >> 3) & 0b00011111;       // r 
        packed[0] |= ((data->g >> 3) & 0b00000111) << 5; // g 

        packed[1]  = ((data->g >> 6) & 0b00000011);      // g 
        packed[1] |= ((data->b >> 1) & 0b01111100);      // b 

        // laser_x (12 bits)
        packed[2] =   data->laser_x;
        packed[3] = ((data->laser_x >> 8) & 0x0F);

        // laser_y (12 bits)
        packed[3] |= (data->laser_y << 4) & 0xF0;
        packed[4]  =  data->laser_y >> 4;

        // audio_l (12 bits)
        packed[5] =   data->audio_l;
        packed[6] = ((data->audio_l >> 8) & 0x0F);

        // audio_r (12 bits)
        packed[6] |= (data->audio_r << 4) & 0xF0;
        packed[7]  =  data->audio_r >> 4;

        // timestamp: 64-bit little endian
        // packed[8]  = (uint8_t) data->t;
        // packed[9]  = (uint8_t)(data->t >> 8);
        // packed[10] = (uint8_t)(data->t >> 16);

        // packed[11] = (uint8_t)(data->t >> 24);
        // packed[12] = (uint8_t)(data->t >> 32);
        // packed[13] = (uint8_t)(data->t >> 40);
        // packed[14] = (uint8_t)(data->t >> 48);
        // packed[15] = (uint8_t)(data->t >> 56);

    }
}

void pack(const Data *const data_array, uint8_t *const arr, int num_bytes)
{
    
    for (int i = 0, j = 0; j < num_bytes; ++i, j += 8)
    {
        uint8_t *const packed = &arr[j];
        const Data *const data = &data_array[i];

        // rgb: pack to 11 bits (5+6+7 bits, rounded)
        packed[0]  =  (data->r > 31 ? 31 : data->r) & 0b00011111;       // r 
        packed[0] |= (data->g & 0b00000111) << 5; // g 

        packed[1]  = (((data->g > 31 ? 31 : data->g) >> 3) & 0b00000011);      // g 
        packed[1] |= ((data->b > 31 ? 31 : data->b) & 0b00011111) << 2;        // b 

        // laser_x (12 bits)
        packed[2] =   data->laser_x;
        packed[3] = ((data->laser_x >> 8) & 0x0F);

        // laser_y (12 bits)
        packed[3] |= (data->laser_y << 4) & 0xF0;
        packed[4]  =  data->laser_y >> 4;

        // audio_l (12 bits)
        packed[5] =   data->audio_l;
        packed[6] = ((data->audio_l >> 8) & 0x0F);

        // audio_r (12 bits)
        packed[6] |= (data->audio_r << 4) & 0xF0;
        packed[7]  =  data->audio_r >> 4;
    }
}

void pack_arr(HANDLE hSerial, const Data *const data_array, uint8_t *const packed)
{
    DWORD bytesWritten;
    for (int j = 0; j < 256; j += 32)
    {
        pack(&data_array[j], packed, 256);
        WriteFile(hSerial, packed, 256, &bytesWritten, NULL);
    }
}

HANDLE setup_serial()
{
    HANDLE hSerial = CreateFileA("\\\\.\\COM3", GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
    if (hSerial == INVALID_HANDLE_VALUE) 
    {
        fprintf(stderr, "Error opening COM3\n");
        exit(0);
    }

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    GetCommState(hSerial, &dcbSerialParams);
    dcbSerialParams.BaudRate = 1000000;//CBR_115200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity   = NOPARITY;
    SetCommState(hSerial, &dcbSerialParams);

    return hSerial;
}

void square(HANDLE hSerial, const int width, int r, int g, int b, int del)
{
    static Data data_array[256] = {0};
    static uint8_t packed[256] = {0};
    const int mult = 40;

    for (int i = 0, j = 0; j < 256; ++i, ++j)
    {
        for (int d = 0; d < del && i < 256; ++d, ++i)
        {
            data_array[i].r = r;
            data_array[i].b = b;
            data_array[i].g = g;
            data_array[i].laser_x = 0;
            data_array[i].laser_y = 0;
            // data_array[i].audio_l = 0;
            // data_array[i].audio_r = 0;
        }
        if (i == 256)
        {
            pack_arr(hSerial, data_array, packed);
            i = 0;
        }
        for (int d = 0; d < del && i < 256; ++d, ++i)
        {
            data_array[i].r = r;
            data_array[i].b = b;
            data_array[i].g = g;
            data_array[i].laser_x = width;
            data_array[i].laser_y = 0;
            // data_array[i].audio_l = 0;
            // data_array[i].audio_r = 100;
        }
        if (i == 256)
        {
            pack_arr(hSerial, data_array, packed);
            i = 0;
        }
        
        for (int d = 0; d < del && i < 256; ++d, ++i)
        {
            data_array[i].r = r;
            data_array[i].b = b;
            data_array[i].g = g;
            data_array[i].laser_x = width;
            data_array[i].laser_y = width;
            // data_array[i].audio_l = 100;
            // data_array[i].audio_r = 100;
        }
        if (i == 256)
        {
            pack_arr(hSerial, data_array, packed);
            i = 0;
        }
        
        for (int d = 0; d < del && i < 256; ++d, ++i)
        {
            data_array[i].r = r;
            data_array[i].b = b;
            data_array[i].g = g;
            data_array[i].laser_x = 0;
            data_array[i].laser_y = width;
            // data_array[i].audio_l = 100;
            // data_array[i].audio_r = 0;
        }
        if (i == 256)
        {
            pack_arr(hSerial, data_array, packed);
            i = 0;
        }
    }
}

int get_microseconds() 
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int)(tv.tv_sec * 1000000 + tv.tv_usec);
}

void geometry(HANDLE hSerial, float xp, float yp, float xamp, float yamp, int t, int r, int g, int b)
{    
    static Data data_array[256] = {0};
    static uint8_t packed[256] = {0};

    xamp = 4090 * xamp/2;
    yamp = 4090 * yamp/2;
    xp = 2*3.1415926 / xp;
    yp = 2*3.1415926 / yp;

    for (int i = 0; i < t;)
    {
        for (int j = 0; j < 256; ++i, ++j)
        {        
            data_array[j].r = r;
            data_array[j].b = b;
            data_array[j].g = g;
            data_array[j].laser_x = (sinf(i * xp) + 1) * xamp;
            data_array[j].laser_y = (sinf(i * yp) + 1) * yamp;
            data_array[j].laser_y += (sinf(i * yp * 20.0625) + 1) * yamp / 21;

            if (data_array[j].laser_y > 4095)
                data_array[j].laser_y = 4095;
            else if (data_array[j].laser_y < 0)
                data_array[j].laser_y = 0;
        }
        pack_arr(hSerial, data_array, packed);
    }
}

void rotate_point(uint16_t *x, uint16_t *y, float angle, float cx, float cy)
{
    // printf("%d %d %f %f %f\n", *x, *y, angle, cx, cy);
    float dx = (float)(*x) - cx;
    float dy = (float)(*y) - cy;
    float sin_ = sinf(angle);
    float cos_ = cosf(angle);
    float x_rot = cx + dx * cos_ - dy * sin_;
    float y_rot = cy + dx * sin_ + dy * cos_;

    // printf("%f %f\n", x_rot, y_rot);
    // Clamp 
    *x = (uint16_t)(x_rot < 0 ? 0 : (x_rot > 4095 ? 4095 : x_rot + 0.5f));
    *y = (uint16_t)(y_rot < 0 ? 0 : (y_rot > 4095 ? 4095 : y_rot + 0.5f));
}


void send_to_laser(const int len, const float *const arr, const int *const types, int first_one)
{
    if (len == 0)
    {
        serial_conn = setup_serial();
        return;
    }

    static int t = 0;
    static Data data_array[256] = {0};
    static uint8_t packed[256] = {0};
    float amp, p;
    memset(data_array, 0, sizeof(data_array));

    if (first_one)
        t = 0;

    for (int i = 0, type = 0; type < len; ++i, type++)
    {
        switch (types[type])
        {
        case XHZ:
            amp = 4095/2 * arr[i+1];
            p = 2*  3.1415926 * arr[i] / ISR_HZ;
            for (int j = 0, k = t; j < 256; ++j, ++k)
                data_array[j].laser_x += (sinf(k * p) + 1) * amp + 0.5f;
            ++i;
            break;

        case YHZ:
            amp = 4095/2 * arr[i+1];
            p = 2 * 3.1415926*  arr[i] / ISR_HZ;
            for (int j = 0, k = t; j < 256; ++j, ++k)
                data_array[j].laser_y += (sinf(k * p) + 1) * amp + 0.5f;
            ++i;
            break;

        case RED:
            for (int j = 0; j < 256; ++j)
                data_array[j].r = (int)(arr[i] + 0.5f);
            break;

        case GREEN:
            for (int j = 0; j < 256; ++j)
                data_array[j].g = (int)(arr[i] + 0.5f);
            break;

        case BLUE:
            for (int j = 0; j < 256; ++j)
                data_array[j].b = (int)(arr[i] + 0.5f);
            break;

        case XOFF:
            for (int j = 0, k = t; j < 256; ++j, ++k)
                data_array[j].laser_x += arr[i];
            break;

        case YOFF:
            for (int j = 0, k = t; j < 256; ++j, ++k)
                data_array[j].laser_y += arr[i];
            break;

        case ROTATE:
            for (int j = 0, k = t; j < 256; ++j, ++k)
                rotate_point(&data_array[j].laser_x, &data_array[j].laser_y, arr[i], arr[i+1], arr[i+2]);
            i += 2;
            break;

        default:
            break;
        }
    }

    for (int j = 0; j < 256; ++j)
    {
        if (data_array[j].laser_y > 4095)
            data_array[j].laser_y = 4095;
        else if (data_array[j].laser_y < 0)
            data_array[j].laser_y = 0;

        if (data_array[j].laser_x > 4095)
            data_array[j].laser_x = 4095;
        else if (data_array[j].laser_x < 0)
            data_array[j].laser_x = 0;
    }
    
    pack_arr(serial_conn, data_array, packed);
    t += 256;
}

// color: 31 - 255
int main(int argc, char **argv) 
{
    // HANDLE hSerial = setup_serial();
    // int start = get_microseconds();

    int r = 70, g = 70, b = 70, t = 100000;
    float xp = 200, yp = 301;
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i][0] == 'r')
        {
            r = atoi(argv[i+1]);
        }
        if (argv[i][0] == 'g')
        {
            g = atoi(argv[i+1]);
        }
        if (argv[i][0] == 'b')
        {
            b = atoi(argv[i+1]);
        }
        if (argv[i][0] == 'x')
        {
            xp = atof(argv[i+1]);
        }
        if (argv[i][0] == 'y')
        {
            yp = atof(argv[i+1]);
        }
        if (argv[i][0] == 't')
        {
            t = atof(argv[i+1]);
        }
    }

    send_to_laser(0, NULL, NULL, 1);

    float things[9] = {xp, 1, yp, 1, 400, .1, r, g, b};
    int types[6] = {XHZ, YHZ, XHZ, RED, GREEN, BLUE};
    for (int i = 0; i < 1000; ++i)
        send_to_laser(6, things, types, 0);
    return 1;

    // geometry(hSerial, xp, yp, .9, .9, t, r, g, b);
    // printf("%d DONE\n", get_microseconds() - start);
    // return 1;


    // for (int i = 0; i < t; i++)
    //     square(hSerial, 4095, r, g, b, 64);
    // CloseHandle(hSerial);
    // puts("DONE");
    // return 0;
}


// ./serial r 80 b 60 g 50 x 100 y 33.3