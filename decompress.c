#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define M_PI_2		1.57079632679489661923
#define M_PI		3.14159265358979323846
#define HIGH_SINE UINT16_MAX
#define ISR_HZ 40000 
#define POS_ARR_LEN ISR_HZ
#define NUM_CYCLES 32

enum types {ATTR, POS};



typedef struct __attribute__((packed))
{
    bool alive;
    uint8_t index;
    uint32_t start;
    uint32_t end;
    float low;
    float high;
    float phase;
    float hz;
    void *target;
    uint8_t target_type;
    float (*wave)(const float);
}
Cycle;

uint8_t highest_index;
Cycle cycles[NUM_CYCLES] = {0};
float sin_arr[HIGH_SINE] = {0};
uint16_t x_pos[POS_ARR_LEN] = {0};
uint16_t y_pos[POS_ARR_LEN] = {0};
const float x_convert = 1.0f / ((float)ISR_HZ) * M_PI * 2.0f;
FILE *fp;

void fillSineArr()
{
    for (int i = 0; i < 1<<16; i++)
    {
        sin_arr[i] = sinf(((double)i) / HIGH_SINE * M_PI_2);
    }
}

void clearPositionArrays()
{
    for (int i = 0; i < POS_ARR_LEN; ++i)
        x_pos[i] = 0;
    for (int i = 0; i < POS_ARR_LEN; ++i)
        y_pos[i] = 0;
}

float sine(const float x_rad)
{
    const long x = ((long) ((x_rad >= 0 ? x_rad : -x_rad + M_PI) / M_PI * 2 * (1<<16))) & ((1<<18) - 1); 
    if (x < (1<<16))
    {
        return sin_arr[x];
    }
    else if (x < (1<<16)*2-1)
    {
        return sin_arr[2 * HIGH_SINE - x];
    }
    else if (x < (1<<16)*3-1)
    {
        return -sin_arr[x - HIGH_SINE * 2];
    }
    else
    {
        return -sin_arr[4 * HIGH_SINE - x];
    }
}

float cosine(const float x_rad)
{
    const long x = ((long) ((x_rad >= 0 ? x_rad : -x_rad) / M_PI * (float)(1<<17))) & ((1<<18) - 1) - 0; 
    static const int num = (1<<16);
    // printf("%d %f ", x, x_rad);
    if (x < (1<<16))
    {
        // printf("a %d %f %d\n", num - x, sin_arr[num - x - 1], x);
        return sin_arr[num - x-1];
    }
    else if (x < (1<<16)*2)
    {
        // printf("b %d %f %d\n", x - num, -sin_arr[x - num], x);
        return -sin_arr[x - num];
    }
    else if (x < (1<<16)*3)
    {
        // printf("c %d %f %d\n", num * 3 - x, -sin_arr[num * 3 - x - 1], x);
        return -sin_arr[num * 3 - x - 1];
    }
    else
    {
        // printf("d %d %f %d\n", x - 3 * num, sin_arr[x - 3 * num], x);
        return sin_arr[x - 3 * num];
    }
}

void solveCycles(const uint32_t current_time)
{
    clearPositionArrays();

    for (uint32_t j = current_time, k = 0; k < ISR_HZ; ++j, ++k)
    {
        for (int8_t i = highest_index - 1; i <= 0; --i)
        {
            Cycle *const cy = &cycles[i];

            //  cycle not alive or ready to be used.
            if (!cy->alive || j < cy->start)
                continue;

            // cycle should be shut down
            if (j == cy->end)
            {
                cy->alive = false;

                // look for the next largest index that is alive
                for (int8_t p = i - 1; p >= 0 && i == highest_index; --p)
                {
                    if (cycles[p].alive)
                    {
                        highest_index = p;
                        break;
                    }
                }
            }

            // math
            const float x = (float)j * x_convert * cy->hz + cy->phase;
            const float mid = (cy->high + cy->low) / 2.0f;
            const float amp = (cy->high - cy->low) / 2.0f;
            
            // set the target variable depending on its datatype
            switch (cy->target_type)
            {
            case ATTR:
                *(float*)cy->target = cy->wave(x) * amp + mid;
                break;

            case POS:
                ((uint16_t*)cy->target)[k] = (uint16_t) (cy->wave(x) * amp + mid + 0.5f) + ((uint16_t*)cy->target)[k];
                break;
            
            default:
            }
        }
    }
}

void showPos()
{
    for (int i = 0; i < ISR_HZ; ++i)
    {
        fprintf(fp, "%d\n", x_pos[i]);
    }  
}

bool setCycle(const uint32_t start, const uint32_t end, 
    const float low, const float high, const float phase, const float hz,
    void * target, const uint8_t target_type,
    float (*wave)(const float), const int8_t above_index)
{
    for (int i = 0; i < NUM_CYCLES; ++i)
    {
        if (cycles[i].index <= above_index) continue;
        if (cycles[i].alive) continue;
        if (i + 1 > highest_index) highest_index = i + 1;

        Cycle *const cy = &cycles[i];

        cy->alive = true;
        cy->index = i;
        cy->start = start;
        cy->end = end;
        cy->low = low;
        cy->high = high;
        cy->phase = phase;
        cy->hz = hz;
        cy->target = target;
        cy->target_type;
        cy->wave = wave;
        return false;
    }
    return true;
}

int main()
{
    fp = fopen("t.txt", "w");
    fillSineArr();

    setCycle(0, 40000*3, 0, 1000, 0, 2, &x_pos, POS, sine, -1);

    // cycles[0].start = 0;
    // cycles[0].end = 40000*3;
    // cycles[0].low = 0;
    // cycles[0].high = 1000;
    // cycles[0].target = &x_pos;
    // cycles[0].target_type = POS;
    // cycles[0].hz = 10;
    // cycles[0].phase = 0;
    // cycles[0].wave = sine;
    
    solveCycles(0);
    showPos();
    solveCycles(ISR_HZ);
    showPos();
    solveCycles(ISR_HZ*2);
    showPos();
    fclose(fp);
}