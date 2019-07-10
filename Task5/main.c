#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#define INPUT_FILE_NAME "TestSound4.wav"
#define OUTPUT_FILE_NAME "Output.wav"
#define FILE_HEADER_SIZE 44
#define BYTES_PER_SAMPLE 2
#define DATA_BUFF_SIZE 1000
#define SAMPLE_RATE 48000
#define CHANNELS 2

#define FC 200
#define Q_VALUE 0.707		//must be between 0 and 0.707

#define PI 3.14159265358979323846


typedef struct {
	int16_t x[2];
	int16_t y[2];
	uint32_t remainder;

	double dx[2];
	double dy[2];
} BiquadBuff;

typedef struct {
	int32_t b[3];
	int32_t a[2];
	double db[3];
	double da[2];
} BiquadCoeffs;


int32_t doubleToFixed30(double x);

FILE * openFile(char *fileName, _Bool mode);		//if 0 - read, if 1 - write
void readHeader(uint8_t *headerBuff, FILE *inputFilePtr);
void writeHeader(uint8_t *headerBuff, FILE *outputFilePtr);

void initializeBiquadBuff(BiquadBuff *buff);
void calculateBiquadCoeffs(BiquadCoeffs *coeffs, double Fc, double Q);

int16_t biquadDoubleFilter(int16_t sample, BiquadBuff *buff, BiquadCoeffs *coeffs);
int16_t biquadFilter(int16_t sample, BiquadBuff *buff, BiquadCoeffs *coeffs);
void filterSignal(size_t size, BiquadBuff *buff, BiquadCoeffs *coeffs);
void run(FILE *inputFilePtr, FILE *outputFilePtr, BiquadBuff *buff, BiquadCoeffs *coeffs);

int16_t dataBuff[DATA_BUFF_SIZE * CHANNELS];


int main()
{
	FILE *inputFilePtr = openFile(INPUT_FILE_NAME, 0);
	FILE *outputFilePtr = openFile(OUTPUT_FILE_NAME, 1);
	uint8_t headerBuff[FILE_HEADER_SIZE];
	BiquadBuff buff[2];
	BiquadCoeffs coeffs;

	initializeBiquadBuff(&buff[0]);
	initializeBiquadBuff(&buff[1]);
	calculateBiquadCoeffs(&coeffs, FC, Q_VALUE);

	readHeader(headerBuff, inputFilePtr);
	writeHeader(headerBuff, outputFilePtr);
	run(inputFilePtr, outputFilePtr, buff, &coeffs);
	fclose(inputFilePtr);
	fclose(outputFilePtr);

	return 0;
}


int32_t doubleToFixed30(double x)
{
	if (x >= 2)
	{
		return INT32_MAX;
	}
	else if (x < -2)
	{
		return INT32_MIN;
	}

	return (int32_t)(x * (double)(1LL << 30));
}

FILE * openFile(char *fileName, _Bool mode)		//if 0 - read, if 1 - write
{
	FILE *filePtr;

	if (mode == 0)
	{
		if ((filePtr = fopen(fileName, "rb")) == NULL)
		{
			printf("Error opening input file\n");
			system("pause");
			exit(0);
		}
	}
	else
	{
		if ((filePtr = fopen(fileName, "wb")) == NULL)
		{
			printf("Error opening output file\n");
			system("pause");
			exit(0);
		}
	}

	return filePtr;
}

void readHeader(uint8_t *headerBuff, FILE *inputFilePtr)
{
	if (fread(headerBuff, FILE_HEADER_SIZE, 1, inputFilePtr) != 1)
	{
		printf("Error reading input file (header)\n");
		system("pause");
		exit(0);
	}
}

void writeHeader(uint8_t *headerBuff, FILE *outputFilePtr)
{
	if (fwrite(headerBuff, FILE_HEADER_SIZE, 1, outputFilePtr) != 1)
	{
		printf("Error writing output file (header)\n");
		system("pause");
		exit(0);
	}
}

void initializeBiquadBuff(BiquadBuff *buff)
{
	buff->x[0] = 0;
	buff->x[1] = 0;
	buff->y[0] = 0;
	buff->y[1] = 0;
	buff->remainder = 0;

	buff->dx[0] = 0;
	buff->dx[1] = 0;
	buff->dy[0] = 0;
	buff->dy[1] = 0;
}

void calculateBiquadCoeffs(BiquadCoeffs *coeffs, double Fc, double Q)
{
	double K = tan(PI * Fc / SAMPLE_RATE);
	double norm = 1 / (1 + K / Q + K * K);

	coeffs->db[0] = K * K * norm;
	coeffs->db[1] = 2 * coeffs->db[0];
	coeffs->db[2] = coeffs->db[0];
	coeffs->da[0] = 2 * (K * K - 1) * norm;
	coeffs->da[1] = (1 - K / Q + K * K) * norm;

	coeffs->b[0] = doubleToFixed30(coeffs->db[0]);
	coeffs->b[1] = doubleToFixed30(coeffs->db[1]);
	coeffs->b[2] = coeffs->b[0];
	coeffs->a[0] = doubleToFixed30(coeffs->da[0]);
	coeffs->a[1] = doubleToFixed30(coeffs->da[1]);
}

int16_t biquadDoubleFilter(int16_t sample, BiquadBuff *buff, BiquadCoeffs *coeffs)
{
	double acc = coeffs->db[0] * sample + coeffs->db[1] * buff->dx[0] + coeffs->db[2] * buff->dx[1] -
		coeffs->da[0] * buff->dy[0] - coeffs->da[1] * buff->dy[1];

	buff->dx[1] = buff->dx[0];
	buff->dx[0] = sample;
	buff->dy[1] = buff->dy[0];
	buff->dy[0] = acc;

	return (int16_t)acc;
}

int16_t biquadFilter(int16_t sample, BiquadBuff *buff, BiquadCoeffs *coeffs)
{
	int64_t acc = buff->remainder;

	acc += (int64_t)coeffs->b[0] * sample + (int64_t)coeffs->b[1] * buff->x[0] + (int64_t)coeffs->b[2] *
		buff->x[1] - (int64_t)coeffs->a[0] * buff->y[0] - (int64_t)coeffs->a[1] * buff->y[1];

	buff->x[1] = buff->x[0];
	buff->x[0] = sample;
	buff->y[1] = buff->y[0];
	buff->y[0] = (int32_t)(acc >> 30);

	buff->remainder = (uint32_t)acc & 0x3FFFFFFF;

	return (int16_t)buff->y[0];
}

void filterSignal(size_t size, BiquadBuff *buff, BiquadCoeffs *coeffs)
{
	uint16_t i;

	for (i = 0; i < size / CHANNELS; i++)
	{
		dataBuff[i * CHANNELS] = biquadFilter(dataBuff[i * CHANNELS], &buff[0], coeffs);
		dataBuff[i * CHANNELS + 1] = biquadFilter(dataBuff[i * CHANNELS + 1], &buff[1], coeffs);
	}
}

void run(FILE *inputFilePtr, FILE *outputFilePtr, BiquadBuff *buff, BiquadCoeffs *coeffs)
{
	size_t samplesRead;
	uint32_t i;

	while (1)
	{
		samplesRead = fread(dataBuff, BYTES_PER_SAMPLE, DATA_BUFF_SIZE, inputFilePtr);

		if (!samplesRead)
		{
			break;
		}

		filterSignal(samplesRead, buff, coeffs);
		fwrite(dataBuff, BYTES_PER_SAMPLE, samplesRead, outputFilePtr);
	}
}
