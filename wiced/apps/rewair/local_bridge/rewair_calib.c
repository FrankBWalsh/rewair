#include "rewair_calib.h"

#include <stdio.h>
#include <string.h>

#include "wiced.h"
#include "wiced_framework.h"

/* Settings occupy offset 0 (well under 256 bytes); MQTT config starts at 512. */
#define REWAIR_CALIB_DCT_OFFSET 256u
#define REWAIR_CALIB_MAGIC      0x52574331u   /* 'RWC1' */
#define REWAIR_CALIB_CHECK_SALT 0xA5C3E1F7u

typedef struct
{
    uint32_t magic;
    uint32_t dust_offset;
    uint32_t voc_offset;
    uint8_t  hide_clock;
    uint8_t  reserved[3];
    uint32_t check;
} rewair_calib_record_t;

static wiced_mutex_t  calib_mutex;
static uint8_t        calib_ready = 0u;
static rewair_calib_t calib_current;

static int32_t  uncal_dust = 0;
static int32_t  uncal_voc = 0;
static uint32_t uncal_valid = 0u;

static uint32_t record_check( const rewair_calib_record_t* r )
{
    return r->magic ^ ( r->dust_offset * 2654435761u ) ^ ( r->voc_offset * 40503u ) ^
           ( (uint32_t)r->hide_clock << 24 ) ^ REWAIR_CALIB_CHECK_SALT;
}

static int values_valid( const rewair_calib_t* c )
{
    return c->dust_offset <= REWAIR_CALIB_DUST_OFFSET_MAX &&
           c->voc_offset <= REWAIR_CALIB_VOC_OFFSET_MAX &&
           c->hide_clock <= 1u;
}

void rewair_calib_init( void )
{
    rewair_calib_record_t* stored = NULL;

    memset( &calib_current, 0, sizeof( calib_current ) );
    if ( wiced_rtos_init_mutex( &calib_mutex ) != WICED_SUCCESS )
    {
        printf( "[calib] mutex init failed\n" );
        return;
    }

    if ( wiced_dct_read_lock( (void**)&stored, WICED_FALSE, DCT_APP_SECTION,
                              REWAIR_CALIB_DCT_OFFSET, sizeof( *stored ) ) == WICED_SUCCESS )
    {
        if ( stored->magic == REWAIR_CALIB_MAGIC && stored->check == record_check( stored ) )
        {
            rewair_calib_t loaded;
            loaded.dust_offset = stored->dust_offset;
            loaded.voc_offset = stored->voc_offset;
            loaded.hide_clock = stored->hide_clock;
            if ( values_valid( &loaded ) )
            {
                calib_current = loaded;
            }
        }
        wiced_dct_read_unlock( stored, WICED_FALSE );
    }

    calib_ready = 1u;
    printf( "[calib] dust_offset=%lu voc_offset=%lu hide_clock=%u\n",
            (unsigned long)calib_current.dust_offset, (unsigned long)calib_current.voc_offset,
            (unsigned)calib_current.hide_clock );
}

void rewair_calib_get( rewair_calib_t* out )
{
    if ( calib_ready == 0u )
    {
        memset( out, 0, sizeof( *out ) );
        return;
    }
    wiced_rtos_lock_mutex( &calib_mutex );
    *out = calib_current;
    wiced_rtos_unlock_mutex( &calib_mutex );
}

int rewair_calib_set( const rewair_calib_t* in )
{
    rewair_calib_record_t record;
    wiced_result_t result;

    if ( in == NULL || !values_valid( in ) || calib_ready == 0u )
    {
        return -1;
    }

    memset( &record, 0, sizeof( record ) );
    record.magic = REWAIR_CALIB_MAGIC;
    record.dust_offset = in->dust_offset;
    record.voc_offset = in->voc_offset;
    record.hide_clock = in->hide_clock;
    record.check = record_check( &record );

    wiced_rtos_lock_mutex( &calib_mutex );
    result = wiced_dct_write( &record, DCT_APP_SECTION, REWAIR_CALIB_DCT_OFFSET, sizeof( record ) );
    if ( result == WICED_SUCCESS )
    {
        calib_current = *in;
    }
    wiced_rtos_unlock_mutex( &calib_mutex );

    printf( "[calib] save %s\n", result == WICED_SUCCESS ? "ok" : "FAILED" );
    return result == WICED_SUCCESS ? 0 : -2;
}

uint8_t rewair_calib_hide_clock( void )
{
    return calib_current.hide_clock;
}

void rewair_calib_note_uncalibrated( int32_t dust_centi, int32_t voc_centi, uint32_t valid )
{
    if ( calib_ready == 0u )
    {
        return;
    }
    wiced_rtos_lock_mutex( &calib_mutex );
    uncal_dust = dust_centi;
    uncal_voc = voc_centi;
    uncal_valid = valid;
    wiced_rtos_unlock_mutex( &calib_mutex );
}

void rewair_calib_get_uncalibrated( int32_t* dust_centi, int32_t* voc_centi, uint32_t* valid )
{
    if ( calib_ready == 0u )
    {
        *dust_centi = 0;
        *voc_centi = 0;
        *valid = 0u;
        return;
    }
    wiced_rtos_lock_mutex( &calib_mutex );
    *dust_centi = uncal_dust;
    *voc_centi = uncal_voc;
    *valid = uncal_valid;
    wiced_rtos_unlock_mutex( &calib_mutex );
}
