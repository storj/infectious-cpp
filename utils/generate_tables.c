#include <stdio.h>
#include "tables.h"

uint8_t gf_mul_table[256][256];

const int cols = 11;

int main(int argc, char* argv[]) {
	int i, j;
	int log_i, log_j;
	int col;

	for (i = 0; i < 256; ++i) {
		for (j = 0; j < 256; ++j) {
			log_i = gf_log[i];
			log_j = gf_log[j];
			gf_mul_table[i][j] = gf_exp[(log_i+log_j)%255];
		}
	}
	for (i = 0; i < 256; ++i) {
		gf_mul_table[0][i] = gf_mul_table[i][0] = 0;
	}

	printf("const uint8_t gf_mul_table[256][256] = {\n");

	for (i = 0; i < 256; ++i) {
		printf("\t{\n\t\t");
		col = 0;
		for (j = 0; j < 256; ++j) {
			printf("0x%02X", gf_mul_table[i][j]);
			if (j != 255) {
				printf(", ");
			}
			if (++col >= cols) {
				printf("\n\t\t");
				col = 0;
			}
		}
		printf("\n\t}");
		if (i != 255) {
			printf(",");
		}
		printf("\n");
	}

	printf("};\n");
}
