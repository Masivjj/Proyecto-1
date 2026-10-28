/**
  @File Name
    fvr.c

  @Summary
    Referencia fija de tension (FVR).

  @Description
    Ver fvr.h. Equivalente a lo que genera MCC Classic con
    docs/avance_adc_1ksps.md.
*/

/**
  Section: Included Files
*/

#include <xc.h>
#include "fvr.h"

/**
  Section: FVR APIs
*/

void FVR_Initialize(void)
{
    // CDAFVR off; FVREN enabled; TSRNG Lo_range; ADFVR 1x; TSEN disabled;
    FVRCON = 0x81;
}

bool FVR_IsOutputReady(void)
{
    return (FVRCONbits.FVRRDY);
}
