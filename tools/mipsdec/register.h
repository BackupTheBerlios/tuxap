#ifndef REGISTER_H
#define REGISTER_H

typedef enum {
	R_ZERO,
	R_A0,
	R_A1,
	R_A2,
	R_A3,
	R_AT,
	R_FP,
	R_GP,
	R_RA,
	R_S0,
	R_S1,
	R_S2,
	R_S3,
	R_S4,
	R_S5,
	R_S6,
	R_S7,
	R_SP,
	R_T0,
	R_T1,
	R_T2,
	R_T3,
	R_T4,
	R_T5,
	R_T6,
	R_T7,
	R_T8,
	R_T9,
	R_V0,
	R_V1,
	R_UNKNOWN,
} tRegister;

tRegister decodeRegister(unsigned uRawData);
const char *getRegName(tRegister eRegister);

#endif
