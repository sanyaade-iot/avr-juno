#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>

// generate wavetable data (this runs only on a "big" target)

#define BIG_TARGET 1
//#include "juno.h"
#include "wave.h"

// define our own wavetable variables!
//wavetable_t gen_sine_wavetables[BUZZ_WAVETABLE_SCALE];
//fric_wavetable_t gen_frication_wavetable;
//fric_wavetable_t gen_aspiration_wavetable;
//wavetable_t gen_vowel_buzz, gen_nasal_buzz, gen_liquid_buzz, gen_frication_buzz;

//64 sounds decent
#define BUZZ_AMP 20
//8 sounds better with simple (non-delta-sigma) one-bit output
//#define BUZZ_AMP 8


// bandpass a waveform array using low and high window widths with a simple
// rectangular window filter
// low is window width of lower frequency (low-cut)
// high is window width of upper frequency (high-cut)
// low should be larger (lower frequency) than high
// TODO optimize calculating window sums
// XXX this seems to be called only with low=2 and high=1, which is simply a
// high-pass filter with the cut-off frequency set at half the sample rate
static void bandpass(const double in[], double out[], int low, int high)
{
#if 0
	double *window = malloc(width * sizeof *window);
	if (!window) return;

	double sum = 0.;
	int w;
	// prime the pump
	for (w = 0; w < width; ++w) {
		window[w] = in[w%FRIC_WAVETABLE_SIZE];
		//fprintf(stderr, "window[%d]=%f\n", w, window[w]);
		sum += window[w];
	}
#endif
	int i;
	for (i = 0; i < FRIC_WAVETABLE_SIZE; ++i) {
		double lowsum = 0., highsum = 0.;
		int j;
		for (j = 0; j < low; ++j) {
			lowsum += in[(i+j)%FRIC_WAVETABLE_SIZE];
			//fprintf(stderr, "%2d ", (i+j)%FRIC_WAVETABLE_SIZE);
		}
		for (j = 0; j < high; ++j) {
			highsum += in[(i+j)%FRIC_WAVETABLE_SIZE];
		}
		//fprintf(stderr, "\n");

		out[i] = highsum / high - lowsum / low;

#if 0
		out[i] = sum / width;
		fprintf(stderr, "s=%f sum=%f\n", s, sum);
		sum -= window[i%width];
		window[i%width] = in[(i+1)%FRIC_WAVETABLE_SIZE];
		sum += window[i%width];
#endif
		//fprintf(stderr, "out[%d]=%f\n", i, out[i]);

		//fprintf(stderr, "window[%d]=%f ", w%width, window[w%width]);
		//fprintf(stderr, "in[%d]=%f\n", (i+1)%FRIC_WAVETABLE_SIZE, in[(i+1)%FRIC_WAVETABLE_SIZE]);
	}

#if 0
	free(window);
#endif
}

static void generate_fric_wavetable(const char *name, int size, int period, const char *noise)
{
	printf("const fric_wavetable_t %s PROGMEM = { {", name);
	char cmd[1024];
	const int cycles = size / period;
#define BANDWIDTH 0.1
	sprintf(cmd,
	        "sox -r %d -c 1 -n -t sb -"
	        " synth 1 %snoise"
	        " repeat 1"
	        " bandpass %d %f"
	        " trim 0.5 1"
	        " vol 0.25 stat"
	        " | xxd -i",
	        size, noise, cycles, BANDWIDTH * cycles);
	FILE *xxd = popen(cmd, "r");
	if (xxd) {
		int c;
		while ((c = getc(xxd)) != EOF)
			putchar(c);
		pclose(xxd);
	}
	printf("} };\n");
}

static void print_wavetable(const wavetable_t *w)
{
	int i;
	printf("{ {");
	for (i = 0; i < BUZZ_WAVETABLE_SIZE; ++i) {
		printf("%4d,", w->samples[i]);
	}
	printf("} }");
}

static void print_wavetables(const wavetable_t *w, int count)
{
	int i;
	printf("{\n");
	for (i = 0; i < count; ++i) {
		printf("\t");
		print_wavetable(w+i);
		printf(",\n");
	}
	printf("}");
}

// generate a glottal buzz (filtered sawtooth wave)
// TODO generate a more natural glottal buzz
static void makebuzz(double factor, const char *name)
{
	wavetable_t w;
	int i, m;
	for (i = 0; i < BUZZ_WAVETABLE_SIZE; ++i) {
		double buzz = 0.;
		for (m = 1; m < BUZZ_WAVETABLE_SIZE/2; ++m) {
			buzz += sin(i * m * 2 * M_PI / BUZZ_WAVETABLE_SIZE) /
			        pow(m, factor);
		}
		w.samples[i] = 127 * buzz * BUZZ_AMP / 256;
	}
	printf("const wavetable_t %s PROGMEM = ", name);
	print_wavetable(&w);
	printf(";\n");
}

int main()
{
	int i, j;
	double fbuzz[FRIC_WAVETABLE_SIZE];
	srand(time(NULL));
	/*
	 * This generates noise with energy predominantly centered at the
	 * desired frequency plus progressively weaker energy at odd harmonics
	 * (3, 5, 7, ...). This may be an artifact of the crude bandpass method
	 * that I'm using.
	 *
	 * The third harmonic appears to be about 13 dB below the fundamental
	 * frequency.
	 */
	for (i = 0; i < FRIC_WAVETABLE_SIZE; ++i) {
		fbuzz[i] = 2.*rand()/RAND_MAX-1.;
		//fprintf(stderr, "fbuzz[%d]=%f\n", i, fbuzz[i]);
	}
	double fbuzz_filtered[FRIC_WAVETABLE_SIZE];
	// bandpass with window between one cycle and half a cycle
	// TODO take FRIC_WAVETABLE_PERIOD into account
	bandpass(fbuzz, fbuzz_filtered, FRIC_WAVETABLE_PERIOD, FRIC_WAVETABLE_PERIOD/2);

	// write waveform data to stdout
	printf("/* THIS FILE IS AUTO-GENERATED! DO NOT EDIT THIS FILE. */\n\n"
	       "#include \"juno.h\"\n"
	       "#include \"wave.h\"\n\n"
	);
	wavetable_t gen_sine_wavetables[BUZZ_WAVETABLE_SCALE];
	for (j = 0; j < BUZZ_WAVETABLE_SCALE; ++j) {
		for (i = 0; i < BUZZ_WAVETABLE_SIZE; ++i) {
			gen_sine_wavetables[j].samples[i] =
			  127 * sin(i * 2 * M_PI / BUZZ_WAVETABLE_SIZE) *
			  j / (BUZZ_WAVETABLE_SCALE-1) / 8;
			//fprintf(stderr, "sine_wavetable[%2d][%2d] = %4d\n", j, i, (int)sine_wavetables[j].samples[i]);
		}
	}
	printf("const wavetable_t sine_wavetables[BUZZ_WAVETABLE_SCALE] PROGMEM = ");
	print_wavetables(gen_sine_wavetables, BUZZ_WAVETABLE_SCALE);
	printf(";\n");

#if 0
	for (i = 0; i < FRIC_WAVETABLE_SIZE; ++i) {
		gen_aspiration_wavetable.samples[i] = 127 * ((double)2*rand() / RAND_MAX - 1) / 8;
	}
#endif

	generate_fric_wavetable("frication_wavetable",
			FRIC_WAVETABLE_SIZE, FRIC_WAVETABLE_PERIOD, "white");
	generate_fric_wavetable("soft_frication_wavetable",
			FRIC_WAVETABLE_SIZE, FRIC_WAVETABLE_PERIOD, "brown");

	makebuzz(2.5, "vowel_buzz");
	makebuzz(2., "nasal_buzz");
	//makebuzz(2., "liquid_buzz");
	makebuzz(2., "frication_buzz");
	return 0;
}

