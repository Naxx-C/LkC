#include "zx_fft.h"
//https://github.com/xiahouzuoxin/fft
#define  SAMPLE_NODES              (128)
#define FFT_LOG_ON 0
static COMPLEX x[SAMPLE_NODES];

static void MakeInput() {
    int i;

    for (i = 0; i < SAMPLE_NODES; i++) {
        x[i].real = sin(PI * 8 * i / SAMPLE_NODES);
        x[i].imag = 0.0f;
    }
}

static void MakeInputLk(float *f) {
    int i;

    for (i = 0; i < SAMPLE_NODES; i++) {
        x[i].real = f[i];
        x[i].imag = 0.0f;
    }
}

int fft_main(void) {
    int i = 0;

    /* TEST FFT */
    MakeInput();
    fft(x, SAMPLE_NODES);

#if FFT_LOG_ON == 1
    printf("\nfft\n");
    for (i = 0; i < SAMPLE_NODES; i++) {
        printf("%.5f %.5f %.5f\n", x[i].real, x[i].imag,
                sqrt(x[i].real * x[i].real + x[i].imag * x[i].imag) / (SAMPLE_NODES / 2));
    }

    printf("\nifft\n");
#endif
    ifft(x, SAMPLE_NODES);
#if FFT_LOG_ON == 1
    for (i = 0; i < SAMPLE_NODES; i++) {
        printf("%.5f %.5f %.5f\n", x[i].real, x[i].imag,
                sqrt(x[i].real * x[i].real + x[i].imag * x[i].imag) / (SAMPLE_NODES / 2));
    }
#endif

    /* TEST FFT with REAL INPUTS */
    MakeInput();
#if FFT_LOG_ON == 1
    printf("\nfft_real\n");
#endif
    fft_real(x, SAMPLE_NODES);

#if FFT_LOG_ON == 1
    for (i = 0; i < SAMPLE_NODES; i++) {
        printf("%.5f %.5f %.5f\n", x[i].real, x[i].imag,
                sqrt(x[i].real * x[i].real + x[i].imag * x[i].imag) / (SAMPLE_NODES / 2));
    }

    printf("\nifft_real\n");
#endif
    ifft_real(x, SAMPLE_NODES);
#if FFT_LOG_ON == 1
    for (i = 0; i < SAMPLE_NODES; i++) {
        printf("%.5f %.5f %.5f\n", x[i].real, x[i].imag,
                sqrt(x[i].real * x[i].real + x[i].imag * x[i].imag) / (SAMPLE_NODES / 2));
    }
#endif

    /* TEST FFT with REAL INPUTS */
    float f[128] = { 0.008f, 0.007999999f, -0.0019999994f, 0.033f, -0.013f, 0.030000001f, 0.006000001f, 0.006000001f,
            -0.0039999997f, 0.018000001f, 0.011f, 0.029f, -0.01f, 0.0010000002f, -0.019000001f, -0.027999999f, -0.009f,
            -0.013f, 0.013999999f, 1.8659999f, 3.462f, 3.8179998f, 3.1920002f, 2.759f, 2.6820002f, 2.105f, 1.927f,
            2.094f, 1.9549999f, 1.873f, 1.907f, 1.88f, 1.569f, 1.632f, 1.559f, 1.341f, 1.128f, 0.98099995f, 0.834f,
            0.62100005f, 0.24099998f, -0.003f, -0.003f, -0.009f, -0.019000001f, 0.0139999995f, -0.016f, 0.038000003f,
            -0.029f, -0.013f, -0.001f, -0.016f, -0.042f, -0.012f, 0.018f, 0.007000001f, 0.0069999993f, -0.009f, 0.023f,
            0.038000003f, 0.003f, 0.011f, -0.015999999f, -0.021f, -0.009f, 0.008f, 0.02f, -0.021f, -0.008f,
            -0.0030000005f, -0.044f, -0.011f, 0.052f, -0.019f, -0.037f, -0.015000001f, -0.026999999f, 0.03f, 0.044f,
            0.025f, 0.011f, 0.052f, -0.528f, -2.07f, -3.175f, -3.497f, -2.934f, -2.7940001f, -2.367f, -2.194f,
            -2.1000001f, -1.924f, -1.839f, -1.928f, -1.918f, -1.814f, -1.812f, -1.737f, -1.602f, -1.375f, -1.1320001f,
            -0.95600003f, -0.728f, -0.612f, -0.114f, 0.026f, -0.024999999f, -0.0019999999f, 0.015999999f, -0.032f,
            -0.015999999f, 0.004f, -0.001f, -0.003f, 0.007000001f, -0.031f, -0.018f, -0.031f, 0.0010000002f,
            -0.019000001f, 0.0f, 0.049000002f, -0.018000001f, -0.01f, 0.0019999994f, 0.012f, -0.009f, 0.018000001f};


    MakeInputLk(f);
#if FFT_LOG_ON == 1
    printf("\nfft_real\n");
#endif
    fft(x, SAMPLE_NODES);

#if FFT_LOG_ON == 1
    for (i = 0; i < SAMPLE_NODES; i++) {
        printf("%.5f %.5f %.5f\n", x[i].real, x[i].imag,
                sqrt(x[i].real * x[i].real + x[i].imag * x[i].imag) / (SAMPLE_NODES / 2));
    }
#endif

    return 0;
}
