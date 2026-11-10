#pragma once

/* User sensor calibration and display options.
 *
 * Stored as an independent record in the app DCT section at
 * REWAIR_CALIB_DCT_OFFSET (settings live at 0, MQTT at 512), guarded by its
 * own magic and check word, so older images and stock DCT contents are simply
 * ignored and read back as "no adjustment". */

#include <stdint.h>

#define REWAIR_CALIB_DUST_OFFSET_MAX 250u     /* ug/m3 (F103 clips dust at 250) */
#define REWAIR_CALIB_VOC_OFFSET_MAX  60000u   /* ppb */

typedef struct
{
    uint32_t dust_offset;   /* ug/m3 subtracted from processed dust */
    uint32_t voc_offset;    /* ppb subtracted from processed VOC */
    uint8_t  hide_clock;    /* 1 = never send TIME to the display board */
} rewair_calib_t;

/* Loads the record from DCT. Call once before worker threads start. */
void rewair_calib_init( void );

/* Thread-safe snapshot of the current values. */
void rewair_calib_get( rewair_calib_t* out );

/* Validates, persists and applies new values. Returns 0 on success,
 * -1 for out-of-range input, -2 for a DCT write failure. */
int rewair_calib_set( const rewair_calib_t* in );

/* Fast accessor used on the UART path. */
uint8_t rewair_calib_hide_clock( void );

/* Last readings before calibration was applied (centi-units, as in SENS). */
void rewair_calib_note_uncalibrated( int32_t dust_centi, int32_t voc_centi, uint32_t valid );
void rewair_calib_get_uncalibrated( int32_t* dust_centi, int32_t* voc_centi, uint32_t* valid );
