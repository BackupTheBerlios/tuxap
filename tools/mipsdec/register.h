#ifndef REGISTER_H
#define REGISTER_H

typedef enum {
	R_ZERO,
	R_A0,
	R_A1,
	R_A2,
	R_A3,
	R_AT,
	R_GP,
	R_RA,
	R_S0,
	R_S1,
	R_S2,
	R_S3,
	R_SP,
	R_V0,
	R_V1,
	R_UNKNOWN,
} tRegister;

tRegister decodeRegister(unsigned uRawData);
const char *getRegName(tRegister eRegister);

#endif
