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

// Calories burned per 1,000 steps by weight (index matches weight_lbs)
// Height 6'0" and above (2,000 steps/mile)
static const uint8_t cal_per_1000_tall[NUM_WEIGHT_CATEGORIES] = {
    28, 33, 38, 44, 49, 55, 60, 69, 75, 82
};

// Height 5'6" to 5'11" (2,200 steps/mile) - interpolated from source data
static const uint8_t cal_per_1000_medium[NUM_WEIGHT_CATEGORIES] = {
    25, 30, 35, 40, 45, 50, 55, 63, 69, 75
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

