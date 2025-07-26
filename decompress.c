#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define M_PI_2		1.57079632679489661923
#define M_PI		3.14159265358979323846
#define HIGH_SINE UINT16_MAX
#define ISR_HZ 40000 
#define POS_ARR_LEN ISR_HZ
#define NUM_CYCLES 64
#define INSTRUCTIONS_FILE "instructions.txt"

enum types {ATTR, POS, COLOR, ROTATE};


typedef struct __attribute__((packed))
{
    bool alive;
    uint32_t start;
    uint32_t end;
    float low;
    float high;
    float phase;
    float hz;
    void *target;
    uint8_t target_type;
    float (*wave)(const float);
    float center_x;
    float center_y;
}
Cycle;

typedef struct __attribute__((packed))
{
    uint16_t x_pos[POS_ARR_LEN];
    uint16_t y_pos[POS_ARR_LEN];
    uint8_t r[POS_ARR_LEN];
    uint8_t g[POS_ARR_LEN];
    uint8_t b[POS_ARR_LEN];
}
Laser;

typedef struct __attribute__((packed))
{
    uint8_t r, g, b;
    uint16_t laser_x, laser_y, audio_l, audio_r;
} LaserBytes;

char instruction_file[256*256];
long instructions_len;
Cycle cycles[NUM_CYCLES] = {0};
float sin_arr[(1<<16) + 2] = {0}; // added some padding just in case.
uint8_t highest_index;
Laser laser;


const float x_convert = 1.0f / ((float)ISR_HZ) * M_PI * 2.0f;
FILE *fp;

void fillSineArr()
{
    for (int i = 0; i < 1<<16; i++)
    {
        sin_arr[i] = sinf(((double)i) / HIGH_SINE * M_PI_2);
    }
}

float sine(const float x_rad)
{
    const long x = ((long) ((x_rad >= 0 ? x_rad : -x_rad + M_PI) / M_PI * 2 * (1<<16))) & ((1<<18) - 1); 
    if (x < (1<<16))
    {
        // printf("a %d %f\n", x, sin_arr[x]);
        return sin_arr[x];
    }
    else if (x < (1<<16)*2-1)
    {
        // printf("b %d %f %d\n", x, sin_arr[2 * HIGH_SINE - x], 2 * HIGH_SINE - x);
        return sin_arr[2 * HIGH_SINE - x];
    }
    else if (x < (1<<16)*3-1)
    {
        // printf("c %d %f %d\n", x, -sin_arr[x - HIGH_SINE * 2 - 1], x - HIGH_SINE * 2 - 1);
        return -sin_arr[x - HIGH_SINE * 2 - 1];
    }
    else
    {
        // printf("d %d %f %d\n", x, -sin_arr[4 * HIGH_SINE - x], 4 * HIGH_SINE - x);
        return -sin_arr[4 * HIGH_SINE - x];
    }
}

float cosine(const float x_rad)
{
    const long x = ((long) ((x_rad >= 0 ? x_rad : -x_rad) / M_PI * (float)(1<<17))) & ((1<<18) - 1) - 0; 
    static const int num = (1<<16);
    // printf("%8.4f ", x_rad);
    if (x < (1<<16))
    {
        // printf("a %d %f %d\n", num - x-1, sin_arr[num - x - 1], x);
        return sin_arr[num - x - 1];
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

void rotate_point(const int k, const float angle, const float cx, const float cy)
{
    const float sin_ = sine(angle);
    const float cos_ = cosine(angle);
    const float dx = (float)(laser.x_pos[k]) - cx;
    const float dy = (float)(laser.y_pos[k]) - cy;
    const float x_rot = cx + dx * cos_ - dy * sin_;
    const float y_rot = cy + dx * sin_ + dy * cos_;

    // Clamp 
    laser.x_pos[k] = (uint16_t)(x_rot < 0 ? 0 : (x_rot > 4095 ? 4095 : x_rot + 0.5f));
    laser.y_pos[k] = (uint16_t)(y_rot < 0 ? 0 : (y_rot > 4095 ? 4095 : y_rot + 0.5f));
}

void solveCycles(const uint32_t current_time)
{
    memset(&laser, 0, sizeof(Laser));

    for (uint32_t j = current_time, k = 0; k < ISR_HZ; ++j, ++k)
    {
        for (int8_t i = highest_index; i >= 0; --i)
        {
            Cycle *const cy = &cycles[i];

            //  cycle not alive or ready to be used.
            if (!cy->alive || j < cy->start)
                continue;

            // cycle should be shut down
            if (j >= cy->end)
            {
                cy->alive = false;

                // look for the next largest index that is alive and set the 'highest_index' to that
                for (int8_t p = i - 1; p >= 0 && i == highest_index; --p)
                {
                    if (cycles[p].alive)
                    {
                        highest_index = p;
                        break;
                    }
                }
                continue;   
            }

            // color is set by the cycle.low value
            if (cy->target_type == COLOR)
            {
                ((uint8_t*)cy->target)[k]= (uint8_t) (cy->low + 0.5f);
                continue;
            }

            // math
            // printf("%d %d %f %f ", j, i, x_convert, cy->hz);
            const float x = (float)(j - cy->start) * x_convert * cy->hz + cy->phase;
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

            case ROTATE:
                rotate_point(k, (cy->wave(x) * amp + mid + 0.5f), cy->center_x, cy->center_y);
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
        fprintf(fp, "%d\n", laser.x_pos[i]);
        // printf("%d\n", x_pos[i]);
    }  
}

void removeDeadCycles()
{
    for (int i = 0; i < NUM_CYCLES; ++i)
    {
        // if cycle not alive, then replace it with the next one in the list
        if (!cycles[i].alive)
        {
            // iterate through the remaining cycles
            for (int j = i + 1; j < NUM_CYCLES; ++j)
            {
                // if cycle alive then copy it over to the non-alive one, and kill the old cycle so it doesn't get used.
                if (cycles[j].alive)
                {
                    memcpy(&cycles[i], &cycles[j], sizeof(Cycle));
                    cycles[j].alive = false;
                }
            }
        }
    }
}

bool setCycle(const Cycle *const cycle, bool firstCall)
{
    for (int i = 0; i < NUM_CYCLES; ++i)
    {
        // if the target address is at or below the current cycle, then continue up the array
        if ((void*)cycle->target > (void*)&cycles[i + 1] 
            && cycle->target_type != POS
            && cycle->target_type != ROTATE) 
            continue;
        if (cycles[i].alive) continue;
        if (i > highest_index) highest_index = i;

        Cycle *const cy = &cycles[i];

        cy->alive = true;
        cy->start = cycle->start;
        cy->end = cycle->end;
        cy->low = cycle->low;
        cy->high = cycle->high;
        cy->phase = cycle->phase;
        cy->hz = cycle->hz;
        cy->target = cycle->target;
        cy->target_type = cycle->target_type;
        cy->wave = cycle->wave;
        return false;
    }

    if (firstCall)
    {
        removeDeadCycles();
        return setCycle(cycle, false); 
    }
    return true;
}

void setTargetVariable(Cycle *const cycle, const char *const val)
{
    switch (val[0])
    {
    case 'x':
        cycle->target = (void*)laser.x_pos;
        cycle->target_type = POS;
        break;
    case 'y':
        cycle->target = (void*)laser.y_pos;
        cycle->target_type = POS;
        break;
    case 'r':
        cycle->target = (void*)laser.r;
        cycle->target_type = COLOR;
        break;
    case 'g':
        cycle->target = (void*)laser.g;
        cycle->target_type = COLOR;
        break;
    case 'b':
        cycle->target = (void*)laser.b;
        cycle->target_type = COLOR;
        break;
    case 'o':
        cycle->target = NULL;
        cycle->target_type = ROTATE;
        break;
        
    default:
        char num[4] = {0};
        int char_index = 2;

        // first digit
        num[0] = val[0]; 

        // there could be 2 digits
        if (val[1] != '.')
        {
            char_index = 3;
            num[1] = val[1];

            // there could be 3 digits
            if (val[2] != '.')
            {
                num[2] = val[2];
                char_index = 4;
            }
        }
        int cycle_index = atoi(num);
        switch (val[char_index])
        {
        case 'h':
            cycle->target = &cycles[cycle_index].high;
            cycle->target_type = ATTR;
            break;
        case 'l':
            cycle->target = &cycles[cycle_index].low;
            cycle->target_type = ATTR;
            break;
        case 'p':
            cycle->target = &cycles[cycle_index].phase;
            cycle->target_type = ATTR;
            break;
        }
        break;
    }
}

void setupOneVariable(int *argc, int *valc, char *val, int *max_time, char info)
{
    static Cycle cycle;
    switch (*argc)
    {
    case 0:
        cycle.start = atoi(val);
        break;
    
    case 1:
        cycle.end = atoi(val);
        if (cycle.end > *max_time) *max_time = cycle.end;
        break;
    
    case 2:
        cycle.low = atof(val);
        break;
    
    case 3:
        cycle.high = atof(val);
        break;
    
    case 4:
        cycle.phase = atof(val);
        break;
    
    case 5:
        cycle.hz = atof(val);
        break;
    
    case 6:
        setTargetVariable(&cycle, val);
        break;
    
    case 7:
        cycle.wave = (val[0] == 's' ? sine : cosine);
        break;
    
    case 8:
        cycle.center_x = atof(val);
        break;
    
    case 9:
        cycle.center_y = atof(val);
        break;
    
    default:
        printf("%s:%d Invalid value: %s in switch case: %d\n"__FILE__, __LINE__, val, argc);
        exit(1);
        break;
    }
    *argc = *argc + 1;
    *valc = 0;
    memset(val, 0, sizeof(val));    

    if (info == '\n')
    {
        setCycle(&cycle, true);
        *valc = 0;
        *argc = 0;
    } 
}

void getRawInstructions()
{
    FILE *f = fopen(INSTRUCTIONS_FILE, "r");
    fseek(f, 0, SEEK_END);
    instructions_len = ftell(f);
    rewind(f);
    fread(instruction_file, 1, instructions_len, f);
    fclose(f);
}

void readInstructions(int *max_time, const int current_time)
{
    char val[16] = {0};
    int valc = 0, argc = 0;
    *max_time = 0;
    for (int i = 0; i < instructions_len; ++i)
    {
        switch (instruction_file[i])
        {
        case '#':
            for (;i < instructions_len && instruction_file[i] != '\n'; ++i);
            break;
        
        case '\0':
            goto EXIT_FOR;
            break;
        
        case ' ':
            break;
        
        case '\n':
            setupOneVariable(&argc, &valc, val, max_time, instruction_file[i]);
            break;
        case ',':
            setupOneVariable(&argc, &valc, val, max_time, instruction_file[i]);
            break;

        default:
            val[valc++] = instruction_file[i];
            break;
        }
    }

    EXIT_FOR:
}

int main()
{
    int max_time;
    getRawInstructions();
    readInstructions(&max_time, 0);
    fp = fopen("t.txt", "w");
    fillSineArr();

    for (int i = 0; i < max_time; i += ISR_HZ)
    {
        solveCycles(i);
        showPos();
    }
    fclose(fp);
    puts("DONE");
    return 0;
}


// TODO
/* 
    * make an instructions compiler so that they can be read quickly in the form of a struct.
    * install some logic to handle if an instruction is too old to keep around.
    * read for new instruction every second.
    * determine if a pile of 64 instructions is enough of a buffer for one second.
    * think about other types of function that could be used. the rotatw one was the new addition
    * make the rotate function the very last thing that happens to the x and y position.
*/