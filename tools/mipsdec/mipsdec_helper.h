#ifndef MIPSEC_HELPER_H
#define MIPSEC_HELPER_H

static unsigned REG_a0;
static unsigned REG_a1;
static unsigned REG_a2;
static unsigned REG_a3;

static unsigned REG_sp;
static unsigned REG_ra;
static unsigned REG_at;
static unsigned REG_gp;
static unsigned REG_fp;

static unsigned REG_s0;
static unsigned REG_s1;
static unsigned REG_s2;
static unsigned REG_s3;
static unsigned REG_s4;
static unsigned REG_s5;
static unsigned REG_s6;
static unsigned REG_s7;

static unsigned REG_t0;
static unsigned REG_t1;
static unsigned REG_t2;
static unsigned REG_t3;

static unsigned REG_v0;
static unsigned REG_v1;

static unsigned read_c0_status(void) { return 0; }
static void write_c0_status(unsigned uVal) {}

#endif
