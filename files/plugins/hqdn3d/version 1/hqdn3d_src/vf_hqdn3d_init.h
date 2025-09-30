#include <stddef.h>
#include <stdint.h>

typedef struct {
    int16_t *coefs[4];
    uint16_t *line;
    uint16_t *frame_prev[3];
    double strength[4];
    int hsub, vsub;
    int depth;
    int prev_init;
    void (*denoise_row[17])(uint8_t *src, uint8_t *dst, uint16_t *line_ant, uint16_t *frame_ant, ptrdiff_t w, int16_t *spatial, int16_t *temporal);
} HQDN3DContext;

