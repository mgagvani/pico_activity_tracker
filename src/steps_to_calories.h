#ifndef STEPS_TO_CALORIES_H
#define STEPS_TO_CALORIES_H

#include <stdint.h>

/*
 * Steps to Calories Conversion Data
 * 
 * Based on metabolic equivalents (MET) research for walking speeds 2-4 mph.
 * Source: https://www.verywellfit.com/pedometer-steps-to-calories-converter-3882595
 * 
 * Data is organized by height category:
 * - TALL:   6'0" and above  (2,000 steps per mile)
 * - MEDIUM: 5'6" to 5'11"   (2,200 steps per mile)
 * - SHORT:  5'5" and below  (2,400 steps per mile)
 */

typedef enum {
    HEIGHT_TALL,    // 6'0" and above
    HEIGHT_MEDIUM,  // 5'6" to 5'11"
    HEIGHT_SHORT    // 5'5" and below
} height_category_t;

// Weight categories in pounds (index into calorie tables)
// 100, 120, 140, 160, 180, 200, 220, 250, 275, 300 lbs
#define NUM_WEIGHT_CATEGORIES 10

static const uint16_t weight_lbs[NUM_WEIGHT_CATEGORIES] = {
    100, 120, 140, 160, 180, 200, 220, 250, 275, 300
};

// Step-count rows taken directly from Verywell Fit table for
// height 5'6" to 5'11" (2,200 steps/mile) for all weights.
// See: https://www.verywellfit.com/pedometer-steps-to-calories-converter-3882595
#define NUM_STEP_ROWS 20

static const uint16_t step_counts[NUM_STEP_ROWS] = {
    1000,  2000,  3000,  4000,  5000,
    6000,  7000,  8000,  9000,  10000,
    11000, 12000, 13000, 14000, 15000,
    16000, 17000, 18000, 19000, 20000
};

// Calories burned for each (steps, weight) pair for the MEDIUM height
// category (5'6" to 5'11"), scraped from the Verywell Fit table.
// Indexing: [step_row][weight_index]
static const uint16_t cal_table_medium_steps[NUM_STEP_ROWS][NUM_WEIGHT_CATEGORIES] = {
    // steps:  100    120    140    160    180    200    220    250    275    300  (lbs)
    {  25,   30,   35,   40,   45,   50,   55,   62,   68,   75 },   //  1,000
    {  50,   60,   69,   79,   89,   99,  109,  125,  136,  149 },   //  2,000
    {  75,   90,  104,  119,  134,  149,  164,  187,  205,  224 },   //  3,000
    { 100,  120,  138,  158,  178,  198,  218,  249,  273,  298 },   //  4,000
    { 125,  150,  173,  198,  223,  248,  273,  311,  341,  373 },   //  5,000
    { 150,  180,  207,  237,  267,  297,  327,  374,  409,  447 },   //  6,000
    { 175,  210,  242,  277,  312,  347,  382,  436,  477,  522 },   //  7,000
    { 200,  240,  276,  316,  356,  396,  436,  498,  545,  596 },   //  8,000
    { 225,  270,  311,  356,  401,  446,  491,  560,  614,  671 },   //  9,000
    { 250,  300,  345,  395,  445,  495,  545,  623,  682,  745 },   // 10,000
    { 275,  330,  380,  435,  490,  545,  600,  685,  750,  820 },   // 11,000
    { 300,  360,  415,  475,  535,  595,  655,  747,  818,  895 },   // 12,000
    { 325,  390,  449,  514,  579,  644,  709,  810,  886,  969 },   // 13,000
    { 350,  420,  484,  554,  624,  694,  764,  872,  955, 1044 },   // 14,000
    { 375,  450,  518,  593,  668,  743,  818,  934, 1023, 1118 },   // 15,000
    { 400,  480,  553,  633,  713,  793,  873,  996, 1091, 1193 },   // 16,000
    { 425,  510,  587,  672,  757,  842,  927, 1059, 1159, 1267 },   // 17,000
    { 450,  540,  622,  712,  802,  892,  982, 1121, 1227, 1342 },   // 18,000
    { 475,  570,  656,  751,  846,  941, 1036, 1183, 1295, 1416 },   // 19,000
    { 500,  600,  691,  791,  891,  991, 1091, 1245, 1364, 1491 }    // 20,000
};

// Calories burned per 1,000 steps by weight (index matches weight_lbs)
// Height 6'0" and above (2,000 steps/mile)
static const uint8_t cal_per_1000_tall[NUM_WEIGHT_CATEGORIES] = {
    28, 33, 38, 44, 49, 55, 60, 69, 75, 82
};

// Height 5'6" to 5'11" (2,200 steps/mile) - derived from 1,000-step row
// of the Verywell Fit medium-height table above.
static const uint8_t cal_per_1000_medium[NUM_WEIGHT_CATEGORIES] = {
    25, 30, 35, 40, 45, 50, 55, 62, 68, 75
};

// Height 5'5" and below (2,400 steps/mile)
static const uint8_t cal_per_1000_short[NUM_WEIGHT_CATEGORIES] = {
    23, 28, 32, 36, 41, 45, 50, 57, 63, 68
};

/*
 * Calculate calories burned from step count.
 * Uses linear interpolation between weight categories.
 * 
 * steps:    Number of steps taken
 * weight:   Body weight in pounds (100-300 range recommended)
 * height:   HEIGHT_TALL, HEIGHT_MEDIUM, or HEIGHT_SHORT
 * 
 * Returns:  Estimated calories burned
 */
static inline uint32_t steps_to_calories(uint32_t steps, uint16_t weight, height_category_t height)
{
    const uint8_t *cal_table;
    
    switch (height) {
        case HEIGHT_TALL:   cal_table = cal_per_1000_tall;   break;
        case HEIGHT_MEDIUM: cal_table = cal_per_1000_medium; break;
        case HEIGHT_SHORT:  cal_table = cal_per_1000_short;  break;
        default:            cal_table = cal_per_1000_medium; break;
    }
    
    // Clamp weight to valid range
    if (weight < 100) weight = 100;
    if (weight > 300) weight = 300;
    
    // Find weight bracket for interpolation
    uint8_t idx = 0;
    for (uint8_t i = 0; i < NUM_WEIGHT_CATEGORIES - 1; i++) {
        if (weight >= weight_lbs[i] && weight < weight_lbs[i + 1]) {
            idx = i;
            break;
        }
        if (weight >= weight_lbs[NUM_WEIGHT_CATEGORIES - 1]) {
            idx = NUM_WEIGHT_CATEGORIES - 2;
        }
    }
    
    // Linear interpolation between weight brackets
    uint16_t w1 = weight_lbs[idx];
    uint16_t w2 = weight_lbs[idx + 1];
    uint8_t c1 = cal_table[idx];
    uint8_t c2 = cal_table[idx + 1];
    
    // cal_per_1000 = c1 + (c2 - c1) * (weight - w1) / (w2 - w1)
    uint32_t cal_per_1000 = c1 + ((uint32_t)(c2 - c1) * (weight - w1)) / (w2 - w1);
    
    // Calculate total calories: (steps / 1000) * cal_per_1000
    // Multiply first to maintain precision, then divide
    return (steps * cal_per_1000) / 1000;
}

/*
 * Quick estimate using simplified formula.
 * Approximation: ~0.04 calories per step for average 160lb person.
 * Scales linearly with weight.
 * 
 * This is faster but less accurate than the table lookup.
 */
static inline uint32_t steps_to_calories_quick(uint32_t steps, uint16_t weight)
{
    // Base: 0.04 cal/step at 160 lbs → 0.00025 cal/step/lb
    // Simplified: calories = steps * weight / 4000
    return (steps * weight) / 4000;
}

#endif // STEPS_TO_CALORIES_H

