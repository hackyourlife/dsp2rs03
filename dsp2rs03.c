#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <stdint.h>

#define u8	uint8_t
#define u16	uint16_t
#define u32	uint32_t
#define u64	uint64_t
#define s8	int8_t
#define s16	int16_t
#define s32	int32_t
#define s64	int64_t

/*
 * RS03
 * ====
 * interleave: 0x8F00
 * short last block
 */

const char RS03_magic[] = {'R', 'S', 0x00, 0x03};

int get16bitBE(unsigned char* p)
{
	return (p[0] << 8) | p[1];
}

int get32bitBE(unsigned char* p)
{
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

void put16bitBE(unsigned char* p, int x) {
	p[0] = (x >> 8) & 0xFF;
	p[1] = x & 0xFF;
}

void put32bitBE(unsigned char* p, int x) {
	p[0] = (x >> 24) & 0xFF;
	p[1] = (x >> 16) & 0xFF;
	p[2] = (x >> 8) & 0xFF;
	p[3] = x & 0xFF;
}


typedef struct {
	u32 num_samples;
	u32 num_adpcm_nibbles;
	u32 sample_rate;
	u16 loop_flag;
	u16 format;
	u32 sa;
	u32 ea;
	u32 ca;
	s16 coef[16];
	u16 gain; // never used anyway
	u16 ps,yn1,yn2;
	u16 lps,lyn1,lyn2;
} DSPHeader;

void load_devkit(DSPHeader* dsp, unsigned char* buffer) {
	int i;
	dsp->num_samples = get32bitBE(buffer);
	dsp->num_adpcm_nibbles = get32bitBE(buffer + 4);
	dsp->sample_rate = get32bitBE(buffer + 8);
	dsp->loop_flag = get16bitBE(buffer + 0xC);
	dsp->format = get16bitBE(buffer + 0xE);
	dsp->sa = get32bitBE(buffer + 0x10);
	dsp->ea = get32bitBE(buffer + 0x14);
	dsp->ca = get32bitBE(buffer + 0x18);
	for (i = 0; i < 16; i++)
		dsp->coef[i] = get16bitBE(buffer + 0x1C + i * 2);
	dsp->gain = get16bitBE(buffer + 0x3C);
	dsp->ps = get16bitBE(buffer + 0x3E);
	dsp->yn1 = get16bitBE(buffer + 0x40);
	dsp->yn2 = get16bitBE(buffer + 0x42);
	dsp->lps = get16bitBE(buffer + 0x44);
	dsp->lyn1 = get16bitBE(buffer + 0x46);
	dsp->lyn2 = get16bitBE(buffer + 0x48);
}

int check_headers(DSPHeader* dsp_L, DSPHeader* dsp_R) {
	if(dsp_L->num_samples != dsp_R->num_samples)
		return 0;
	if(dsp_L->num_adpcm_nibbles != dsp_R->num_adpcm_nibbles)
		return 0;
	if(dsp_L->sample_rate != dsp_R->sample_rate)
		return 0;
	if(dsp_L->loop_flag != dsp_R->loop_flag)
		return 0;
	if(dsp_L->format != dsp_R->format)
		return 0;
	if(dsp_L->sa != dsp_R->sa)
		return 0;
	if(dsp_L->ea != dsp_R->ea)
		return 0;
	return 1;
}

u32 store_rs03(DSPHeader** dsp, unsigned long channel_count, u32* num_adpcm_bytes, unsigned char* buffer) {
	int i;
	unsigned long ch;

	*num_adpcm_bytes = (dsp[0]->num_adpcm_nibbles + 15) / 16 * 8;

	memset(buffer, 0, 0x20 + channel_count * 0x20);
	memcpy(buffer, RS03_magic, 0x04);
	put32bitBE(buffer + 0x04, channel_count);
	put32bitBE(buffer + 0x08, dsp[0]->num_samples);
	put32bitBE(buffer + 0x0C, dsp[0]->sample_rate);
	put32bitBE(buffer + 0x10, *num_adpcm_bytes);
	put16bitBE(buffer + 0x14, dsp[0]->loop_flag);
	put16bitBE(buffer + 0x16, dsp[0]->format);
	put32bitBE(buffer + 0x18, dsp[0]->sa / 16 * 8);
	put32bitBE(buffer + 0x1C, dsp[0]->ea / 2);

	for(ch = 0; ch < channel_count; ch++)
		for(i = 0; i < 16; i++)
			put16bitBE(buffer + 0x20 + (ch * 0x20) + i * 2, dsp[ch]->coef[i]);

	return 0x20 + channel_count * 0x20;
}

unsigned long get_file_size(FILE* file) {
	unsigned long actual_pos = ftell(file);
	fseek(file, 0, SEEK_END);
	unsigned long length = ftell(file);
	fseek(file, actual_pos, SEEK_SET);
	return length;
}

void interleave_rs03(DSPHeader* dsp, u32 num_adpcm_bytes, FILE** channels, unsigned long channel_count, FILE* rs03) {
	unsigned int i;
	unsigned long ch;
	unsigned char buffer[0x8F00];

	assert((num_adpcm_bytes % 8) == 0);

	memset(buffer, 0, 0x8F00);
	for(ch = 0; ch < channel_count; ch++)
		fseek(channels[ch], 0x60, SEEK_SET);
	fseek(rs03, 0x20 + channel_count * 0x20, SEEK_SET);
	printf("interleaving...");

	for(i = 0; i < num_adpcm_bytes / 0x8F00; i++) {
		for(ch = 0; ch < channel_count; ch++) {
			fread(&buffer, 1, 0x8F00, channels[ch]);
			fwrite(&buffer, 0x8F00, 1, rs03);
		}
	}

	// last block
	unsigned long last_block_size = num_adpcm_bytes % 0x8F00;
	memset(buffer, 0, 0x8F00);
	for(ch = 0; ch < channel_count; ch++) {
		fread(&buffer, last_block_size, 1, channels[ch]);
		fwrite(&buffer, last_block_size, 1, rs03);
	}

	printf(" done (%d blocks)\n", i);
	printf("last block interleave: 0x%04lX\n", last_block_size);
}


int main(int argc, char** argv) {
	if(argc < 3) {
		puts("converts standard devkit DSP-files into one RS03 file");
		printf("Usage: %s channel-1.dsp channel-2.dsp [...] rs03.dsp\n", argv[0]);
		return 1;
	}

	unsigned char readbuf[0x1000]; // 4k (127 channels)
	unsigned long ch;
	unsigned long channel_count = argc - 2;
	FILE** channels = (FILE**) malloc(channel_count * sizeof(FILE*));
	DSPHeader** headers = (DSPHeader**) malloc(channel_count * sizeof(DSPHeader*));
	FILE* rs03 = fopen(argv[argc - 1], "wb");
	u32 num_adpcm_bytes;

	for(ch = 0; ch < channel_count; ch++) {
		channels[ch] = fopen(argv[ch + 1], "rb");
		headers[ch] = (DSPHeader*) malloc(sizeof(DSPHeader));
		fread(&readbuf, 0x100, 1, channels[ch]);
		load_devkit(headers[ch], (unsigned char*) &readbuf);
	}

	for(ch = 0; ch < (channel_count - 1); ch++) {
		if(!check_headers(headers[ch], headers[ch + 1])) {
			puts("the files are invalid");
			return 1;
		}
	}

	u32 size = store_rs03(headers, channel_count, &num_adpcm_bytes, (unsigned char*) &readbuf);
	printf("header: 0x%04X bytes\n", size);
	fwrite(&readbuf, size, 1, rs03);

	interleave_rs03(headers[0], num_adpcm_bytes, channels, channel_count, rs03);
	for(ch = 0; ch < channel_count; ch++) {
		free(headers[ch]);
		fclose(channels[ch]);
	}

	fclose(rs03);
	free(headers);
	free(channels);
	return 0;
}
