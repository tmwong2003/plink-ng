// Resources needed across all wdist modules.

#ifndef __WDIST_COMMON_H__
#define __WDIST_COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <limits.h>

#ifdef __cplusplus
#include <algorithm>
#endif

#if __LP64__
#include <emmintrin.h>
#define FIVEMASK 0x5555555555555555LU

typedef union {
  __m128i vi;
  __m128d vd;
  uintptr_t u8[2];
  double d8[2];
  uint32_t u4[4];
} __uni16;
#else
#define FIVEMASK 0x55555555
#endif

#include "zlib-1.2.7/zlib.h"
#include "SFMT.h"

#define PI 3.141592653589793
#define RECIP_2_32 0.00000000023283064365386962890625
// floating point comparison-to-nonzero tolerance, currently 2^{-30}
#define EPSILON 0.000000000931322574615478515625
// less tolerant version (2^{-44}) for some exact calculations
#define SMALL_EPSILON 0.00000000000005684341886080801486968994140625

#define RET_SUCCESS 0
#define RET_NOMEM 1
#define RET_OPEN_FAIL 2
#define RET_INVALID_FORMAT 3
#define RET_CALC_NOT_YET_SUPPORTED 4
#define RET_INVALID_CMDLINE 5
#define RET_WRITE_FAIL 6
#define RET_READ_FAIL 7
#define RET_HELP 8
#define RET_THREAD_CREATE_FAIL 9
#define RET_ALLELE_MISMATCH 10
#define RET_NULL_CALC 11

#define CALC_RELATIONSHIP 1
#define CALC_IBC 2
#define CALC_DISTANCE 4
#define CALC_PLINK_DISTANCE_MATRIX 8
#define CALC_PLINK_IBS_MATRIX 0x10
#define CALC_GDISTANCE_MASK 0x1c
#define CALC_LOAD_DISTANCES 0x20
#define CALC_GROUPDIST 0x40
#define CALC_REGRESS_DISTANCE 0x80
#define CALC_UNRELATED_HERITABILITY 0x100
#define CALC_UNRELATED_HERITABILITY_STRICT 0x200
#define CALC_FREQ 0x400
#define CALC_REL_CUTOFF 0x800
#define CALC_WRITE_SNPLIST 0x1000
#define CALC_GENOME 0x2000
#define CALC_REGRESS_REL 0x4000
#define CALC_LD_PRUNE 0x8000
#define CALC_LD_PRUNE_PAIRWISE 0x10000
#define CALC_REGRESS_PCS 0x20000
#define CALC_REGRESS_PCS_DISTANCE 0x40000
#define CALC_MAKE_BED 0x80000
#define CALC_RECODE 0x100000
#define CALC_MERGE 0x200000

#define DISTANCE_SQ 1
#define DISTANCE_SQ0 2
#define DISTANCE_TRI 3
#define DISTANCE_SHAPEMASK 3
#define DISTANCE_GZ 4
#define DISTANCE_BIN 8
#define DISTANCE_IBS 0x10
#define DISTANCE_1_MINUS_IBS 0x20
#define DISTANCE_ALCT 0x40
#define DISTANCE_TYPEMASK 0x70

#define RECODE_12 1
#define RECODE_TAB 2
#define RECODE_DELIMX 4
#define RECODE_COMPOUND 8
#define RECODE_A 16
#define RECODE_AD 32
#define RECODE_LGEN 64
#define RECODE_RLIST 128
#define RECODE_TRANSPOSE 256
#define RECODE_TYPEMASK 504

#define MERGE_MODE_MASK 7
#define MERGE_ALLOW_EQUAL_POS 8
#define MERGE_BINARY 16
#define MERGE_LIST 32

#define INDIV_SORT_NONE 1
#define INDIV_SORT_NATURAL 2
#define INDIV_SORT_ASCII 3

#define DUMMY_MISSING_GENO 1
#define DUMMY_MISSING_PHENO 2
#define DUMMY_SCALAR_PHENO 4
#define DUMMY_ACGT 8
#define DUMMY_1234 16
#define DUMMY_12 32

#define WKSPACE_MIN_MB 64
#define WKSPACE_DEFAULT_MB 2048

#define CACHELINE 64 // assumed number of bytes per cache line, for alignment
#define CACHELINE_DBL (CACHELINE / sizeof(double))

#define CACHEALIGN(val) ((val + (CACHELINE - 1)) & (~(CACHELINE - 1)))
#define CACHEALIGN_DBL(val) ((val + (CACHELINE_DBL - 1)) & (~(CACHELINE_DBL - 1)))
#define MAXV(aa, bb) ((bb) > (aa))? (bb) : (aa)

#define _FILE_OFFSET_BITS 64
#define MAX_THREADS 127
#define MAX_THREADS_P1 128

#ifdef __LP64__
#define BITCT 64
#else
#define BITCT 32
#endif

#define BITCT2 (BITCT / 2)

// size of generic text line load buffer.  .ped lines can of course be longer
#define MAXLINELEN 131072

// note that this is NOT foolproof: see e.g.
// http://insanecoding.blogspot.com/2007/11/pathmax-simply-isnt.html .  (This
// is why I haven't bothered with OS-based #ifdefs here.)  But it should be
// good enough in practice.
#define FNAMESIZE 4096

// allow .mdist.bin.xxxxxxxxxx extension
#define MAX_POST_EXT 22

// number of different types of jackknife values to precompute (x^2, x, y, xy)
#define JACKKNIFE_VALS_DIST 5

#if __LP64__
#define AAAAMASK 0xaaaaaaaaaaaaaaaaLU
// number of snp-major .bed lines to read at once for distance calc if exponent
// is nonzero.
#define MULTIPLEX_DIST_EXP 64
// number of snp-major .bed lines to read at once for relationship calc
#define MULTIPLEX_REL 60
#else
// N.B. 32-bit version not as carefully tested or optimized, but I'll try to
// make sure it works properly
#define AAAAMASK 0xaaaaaaaa
#define MULTIPLEX_DIST_EXP 28
#define MULTIPLEX_REL 30
#endif

// fit 4 pathologically long IDs plus a bit extra
extern char tbuf[];

extern sfmt_t sfmt;

extern const char errstr_fopen[];
extern const char errstr_append[];
extern const char errstr_thread_create[];

extern FILE* logfile;
extern char logbuf[MAXLINELEN];
extern int32_t debug_on;
extern int32_t log_failed;

void logstr(const char* ss);

void logprint(const char* ss);

void logprintb();

int32_t fopen_checked(FILE** target_ptr, const char* fname, const char* mode);

static inline int32_t fwrite_checked(const void* buf, size_t len, FILE* outfile) {
  if ((!len) || fwrite(buf, len, 1, outfile)) {
    return 0;
  }
  return -1;
}

static inline int32_t fputs_checked(const char* ss, FILE* outfile) {
  return (fputs(ss, outfile) == EOF);
}

static inline void fclose_cond(FILE* fptr) {
  if (fptr) {
    fclose(fptr);
  }
}

static inline int32_t fclose_null(FILE** fptr_ptr) {
  int32_t ii;
  ii = fclose(*fptr_ptr);
  *fptr_ptr = NULL;
  return ii;
}

int32_t gzopen_checked(gzFile* target_ptr, const char* fname, const char* mode);

static inline int32_t gzwrite_checked(gzFile gz_outfile, const void* buf, size_t len) {
  if ((!len) || gzwrite(gz_outfile, buf, len)) {
    return 0;
  }
  return -1;
}

static inline int32_t flexwrite_checked(FILE* outfile, gzFile gz_outfile, char* contents, uintptr_t len) {
  if (outfile) {
    return fwrite_checked(contents, len, outfile);
  } else {
    return gzwrite_checked(gz_outfile, contents, len);
  }
}

static inline int32_t bed_suffix_conflict(int32_t calculation_type, uint32_t recode_modifier) {
  return (calculation_type & CALC_MAKE_BED) || ((calculation_type & CALC_RECODE) && (recode_modifier & RECODE_LGEN));
}

// manually managed, very large stack
extern unsigned char* wkspace_base;
extern uintptr_t wkspace_left;

unsigned char* wkspace_alloc(uintptr_t size);

static inline int32_t wkspace_alloc_c_checked(char** dc_ptr, uintptr_t size) {
  *dc_ptr = (char*)wkspace_alloc(size);
  return (*dc_ptr)? 0 : 1;
}

static inline int32_t wkspace_alloc_d_checked(double** dp_ptr, uintptr_t size) {
  *dp_ptr = (double*)wkspace_alloc(size);
  return (*dp_ptr)? 0 : 1;
}

static inline int32_t wkspace_alloc_f_checked(float** fp_ptr, uintptr_t size) {
  *fp_ptr = (float*)wkspace_alloc(size);
  return (*fp_ptr)? 0 : 1;
}

static inline int32_t wkspace_alloc_i_checked(int32_t** ip_ptr, uintptr_t size) {
  *ip_ptr = (int32_t*)wkspace_alloc(size);
  return (*ip_ptr)? 0 : 1;
}

static inline int32_t wkspace_alloc_uc_checked(unsigned char** ucp_ptr, uintptr_t size) {
  *ucp_ptr = wkspace_alloc(size);
  return (*ucp_ptr)? 0 : 1;
}

static inline int32_t wkspace_alloc_ui_checked(uint32_t** uip_ptr, uintptr_t size) {
  *uip_ptr = (uint32_t*)wkspace_alloc(size);
  return (*uip_ptr)? 0 : 1;
}

static inline int32_t wkspace_alloc_ul_checked(uintptr_t** ulp_ptr, uintptr_t size) {
  *ulp_ptr = (uintptr_t*)wkspace_alloc(size);
  return (*ulp_ptr)? 0 : 1;
}

static inline int32_t wkspace_alloc_ll_checked(int64_t** llp_ptr, uintptr_t size) {
  *llp_ptr = (int64_t*)wkspace_alloc(size);
  return (*llp_ptr)? 0 : 1;
}

static inline int32_t wkspace_alloc_ull_checked(uint64_t** ullp_ptr, uintptr_t size) {
  *ullp_ptr = (uint64_t*)wkspace_alloc(size);
  return (*ullp_ptr)? 0 : 1;
}

void wkspace_reset(void* new_base);

static inline unsigned char* top_alloc(uintptr_t* topsize_ptr, uint32_t size) {
  uintptr_t ts = *topsize_ptr + ((size + 15) & (~15LU));
  if (ts > wkspace_left) {
    return NULL;
  } else {
    *topsize_ptr = ts;
    return &(wkspace_base[wkspace_left - ts]);
  }
}

static inline int32_t is_letter(char cc) {
  return (((((unsigned char)cc) & 192) == 64) && (((((unsigned char)cc) - 1) & 31) < 26));
}

static inline int32_t is_digit(char cc) {
  return (cc <= '9') && (cc >= '0');
}

static inline int32_t is_not_digit(char cc) {
  return (cc > '9') || (cc < '0');
}

static inline int32_t is_not_nzdigit(char cc) {
  return (cc > '9') || (cc <= '0');
}

// may as well treat all chars < 32, except tab, as eoln...
static inline int32_t is_eoln(char cc) {
  return (((unsigned char)cc) < 32) && (cc != '\t');
}

// kns = "known non-space" (where tab counts as a space)
static inline int32_t is_eoln_kns(char cc) {
  return ((unsigned char)cc) < 32;
}

static inline int32_t is_eoln_or_comment(char cc) {
  return (((unsigned char)cc) < 32) || (cc == '#');
}

static inline int32_t no_more_items(char* sptr) {
  return ((!sptr) || is_eoln(*sptr));
}

static inline int32_t no_more_items_kns(char* sptr) {
  return ((!sptr) || is_eoln_kns(*sptr));
}

static inline char* skip_initial_spaces(char* sptr) {
  while ((*sptr == ' ') || (*sptr == '\t')) {
    sptr++;
  }
  return sptr;
}

/*
static inline int32_t is_space_or_eoln(char cc) {
  // ' ', \t, \n, \0, \r
#if __LP64__
  return ((((unsigned char)cc) <= 32) && (0x100002601LLU & (1LLU << cc)));
#else
  return ((((unsigned char)cc) <= 32) && ((cc == ' ') || (0x2601LU & (1LU << cc))));
#endif
}
*/
static inline int32_t is_space_or_eoln(char cc) {
  return ((unsigned char)cc) <= 32;
}

int32_t get_next_noncomment(FILE* fptr, char** lptr_ptr);

int32_t get_next_noncomment_excl(FILE* fptr, char** lptr_ptr, uintptr_t* marker_exclude, uintptr_t* marker_uidx_ptr);

char* item_end(char* sptr);

// does not return NULL if item ends with null char
char* item_endl(char* sptr);

// item_endl without checking if sptr == NULL
// also assumes we are currently in an item
static inline char* item_endnn(char* sptr) {
  while (!is_space_or_eoln(*(++sptr)));
  return sptr;
}

void get_top_two(uint32_t* uint_arr, uint32_t uia_size, uint32_t* top_idx_ptr, uint32_t* second_idx_ptr);

int32_t intlen(int32_t num);

int32_t strlen_se(char* ss);

int32_t strcmp_se(char* s_read, const char* s_const, int32_t len);

char* next_item(char* sptr);

char* next_item_mult(char* sptr, uint32_t ct);

static inline void copy_nse(char* target, char* source) {
  uint32_t uii;
  if (source) {
    uii = strlen_se(source);
    memcpy(target, source, uii);
    target[uii] = '\0';
  } else {
    *target = '\0';
  }
}

void copy_item(char* writebuf, uint32_t* offset, char** prev_item_ptr);

static inline void read_next_terminate(char* target, char* source) {
  while (!is_space_or_eoln(*source)) {
    *target++ = *source++;
  }
  *target = '\0';
}

static inline void set_bit_noct(uintptr_t* exclude_arr, uint32_t loc) {
  exclude_arr[loc / BITCT] |= (1LU << (loc % BITCT));
}

void set_bit(uintptr_t* bit_arr, uint32_t loc, uintptr_t* bit_set_ct_ptr);

void set_bit_sub(uintptr_t* bit_arr, uint32_t loc, uintptr_t* bit_unset_ct_ptr);

static inline void clear_bit_noct(uintptr_t* exclude_arr, uint32_t loc) {
  exclude_arr[loc / BITCT] &= ~(1LU << (loc % BITCT));
}

void clear_bit(uintptr_t* exclude_arr, uint32_t loc, uintptr_t* include_ct_ptr);

static inline int32_t is_set(uintptr_t* exclude_arr, uint32_t loc) {
  return (exclude_arr[loc / BITCT] >> (loc % BITCT)) & 1;
}

int32_t next_non_set_unsafe(uintptr_t* exclude_arr, uint32_t loc);

int32_t next_set_unsafe(uintptr_t* include_arr, uint32_t loc);

// These functions seem to optimize better than memset(arr, 0, x) under gcc.
static inline void fill_long_zero(intptr_t* larr, size_t size) {
  intptr_t* lptr = &(larr[size]);
  while (larr < lptr) {
    *larr++ = 0;
  }
}

static inline void fill_ulong_zero(uintptr_t* ularr, size_t size) {
  uintptr_t* ulptr = &(ularr[size]);
  while (ularr < ulptr) {
    *ularr++ = 0;
  }
}

static inline void fill_long_one(intptr_t* larr, size_t size) {
  intptr_t* lptr = &(larr[size]);
  while (larr < lptr) {
    *larr++ = -1;
  }
}

static inline void fill_ulong_one(uintptr_t* ularr, size_t size) {
  uintptr_t* ulptr = &(ularr[size]);
  while (ularr < ulptr) {
    *ularr++ = ~0LU;
  }
}

static inline void fill_int_zero(int32_t* iarr, size_t size) {
#if __LP64__
  fill_long_zero((intptr_t*)iarr, size >> 1);
  if (size & 1) {
    iarr[size - 1] = 0;
  }
#else
  fill_long_zero((intptr_t*)iarr, size);
#endif
}

static inline void fill_int_one(int32_t* iarr, size_t size) {
#if __LP64__
  fill_long_one((intptr_t*)iarr, size >> 1);
  if (size & 1) {
    iarr[size - 1] = -1;
  }
#else
  fill_long_one((intptr_t*)iarr, size);
#endif
}

static inline void fill_uint_zero(uint32_t* uiarr, size_t size) {
#if __LP64__
  fill_long_zero((intptr_t*)uiarr, size >> 1);
  if (size & 1) {
    uiarr[size - 1] = 0;
  }
#else
  fill_long_zero((intptr_t*)uiarr, size);
#endif
}

static inline void fill_uint_one(uint32_t* uiarr, size_t size) {
#if __LP64__
  fill_ulong_one((uintptr_t*)uiarr, size >> 1);
  if (size & 1) {
    uiarr[size - 1] = ~0U;
  }
#else
  fill_ulong_one((uintptr_t*)uiarr, size);
#endif
}

static inline void fill_float_zero(float* farr, size_t size) {
  float* fptr = &(farr[size]);
  while (farr < fptr) {
    *farr++ = 0.0;
  }
}

static inline void fill_double_zero(double* darr, size_t size) {
  double* dptr = &(darr[size]);
  while (darr < dptr) {
    *darr++ = 0.0;
  }
}

static inline void free_cond(void* memptr) {
  if (memptr) {
    free(memptr);
  }
}

static inline char convert_to_1234(char cc) {
  if (cc == 'A') {
    return '1';
  } else if (cc == 'C') {
    return '2';
  } else if (cc == 'G') {
    return '3';
  } else if (cc == 'T') {
    return '4';
  }
  return cc;
}

static inline char convert_to_acgt(char cc) {
  if (cc == '1') {
    return 'A';
  } else if (cc == '2') {
    return 'C';
  } else if (cc == '3') {
    return 'G';
  } else if (cc == '4') {
    return 'T';
  }
  return cc;
}

// maximum accepted chromosome index is this minus 1.
// currently unsafe to set this above 60 due to using a single uint64_t
// chrom_mask, and reserving the top 4 bits
#define MAX_POSSIBLE_CHROM 42
#define CHROM_X MAX_POSSIBLE_CHROM
#define CHROM_Y (MAX_POSSIBLE_CHROM + 1)
#define CHROM_XY (MAX_POSSIBLE_CHROM + 2)
#define CHROM_MT (MAX_POSSIBLE_CHROM + 3)

typedef struct {
  // no point to dynamic allocation when MAX_POSSIBLE_CHROM is small

  // order of chromosomes in input files
  // currently tolerates out-of-order chromosomes, as long as markers within a
  // chromosome are not out of order, and all markers for any given chromosome
  // are together
  uint32_t chrom_file_order[MAX_POSSIBLE_CHROM];
  uint32_t chrom_ct; // length of chrom_file_order
  uint32_t chrom_file_order_marker_idx[MAX_POSSIBLE_CHROM + 1];

  // markers chrom_start[k] to (chrom_end[k] - 1) are part of chromosome k
  uint32_t chrom_start[MAX_POSSIBLE_CHROM];
  uint32_t chrom_end[MAX_POSSIBLE_CHROM];

  uint32_t species;
  uint64_t chrom_mask;
} Chrom_info;

#define SPECIES_HUMAN 0
#define SPECIES_COW 1
#define SPECIES_DOG 2
#define SPECIES_HORSE 3
#define SPECIES_MOUSE 4
#define SPECIES_RICE 5
#define SPECIES_SHEEP 6

// extern const uint64_t species_def_chrom_mask[];
extern const uint64_t species_autosome_mask[];
extern const uint64_t species_valid_chrom_mask[];
extern const uint64_t species_haploid_mask[];
// extern const char species_regchrom_ct_p1[];
extern const char species_x_code[];
extern const char species_y_code[];
extern const char species_xy_code[];
extern const char species_mt_code[];
extern const char species_max_code[];
extern const uint64_t species_haploid_mask[];
extern char species_singulars[][7];
extern char species_plurals[][8];

extern char* species_singular;
extern char* species_plural;

static inline char* species_str(uintptr_t ct) {
  return (ct == 1LU)? species_singular : species_plural;
}

int32_t marker_code_raw(char* sptr);

int32_t marker_code(uint32_t species, char* sptr);

int32_t marker_code2(uint32_t species, char* sptr, uint32_t slen);

int32_t strcmp_natural(const void* s1, const void* s2);

int32_t strcmp_deref(const void* s1, const void* s2);

int32_t strcmp_natural_deref(const void* s1, const void* s2);

int32_t is_missing(char* bufptr, int32_t missing_pheno, int32_t missing_pheno_len, int32_t affection_01);

int32_t eval_affection(char* bufptr, int32_t missing_pheno, int32_t missing_pheno_len, int32_t affection_01);

void triangle_fill(uint32_t* target_arr, int32_t ct, int32_t pieces, int32_t parallel_idx, int32_t parallel_tot, int32_t start, int32_t align);

int32_t write_ids(char* outname, uint32_t unfiltered_indiv_ct, uintptr_t* indiv_exclude, char* person_ids, uintptr_t max_person_id_len);

int32_t distance_d_write_ids(char* outname, char* outname_end, int32_t dist_calc_type, uint32_t unfiltered_indiv_ct, uintptr_t* indiv_exclude, char* person_ids, uintptr_t max_person_id_len);

int32_t distance_req(int32_t calculation_type);

int32_t double_cmp(const void* aa, const void* bb);

int32_t double_cmp_deref(const void* aa, const void* bb);

void qsort_ext2(char* main_arr, int32_t arr_length, int32_t item_length, int(* comparator_deref)(const void*, const void*), char* secondary_arr, int32_t secondary_item_len, char* proxy_arr, int32_t proxy_len);

int32_t qsort_ext(char* main_arr, int32_t arr_length, int32_t item_length, int(* comparator_deref)(const void*, const void*), char* secondary_arr, int32_t secondary_item_len);

int32_t bsearch_str(char* id_buf, char* lptr, intptr_t max_id_len, int32_t min_idx, int32_t max_idx);

int32_t bsearch_str_natural(char* id_buf, char* lptr, intptr_t max_id_len, int32_t min_idx, int32_t max_idx);

void fill_idbuf_fam_indiv(char* id_buf, char* fam_indiv, char fillchar);

int32_t bsearch_fam_indiv(char* id_buf, char* lptr, intptr_t max_id_len, int32_t filter_line_ct, char* fam_id, char* indiv_id);

static inline uint32_t popcount2_long(uintptr_t val) {
#if __LP64__
  val = (val & 0x3333333333333333LU) + ((val >> 2) & 0x3333333333333333LU);
  return (((val + (val >> 4)) & 0x0f0f0f0f0f0f0f0fLU) * 0x0101010101010101LU) >> 56;
#else
  val = (val & 0x33333333) + ((val >> 2) & 0x33333333);
  return (((val + (val >> 4)) & 0x0f0f0f0f) * 0x01010101) >> 24;
#endif
}

static inline uint32_t popcount_long(uintptr_t val) {
  // the simple version, good enough for all non-time-critical stuff
  return popcount2_long(val - ((val >> 1) & FIVEMASK));
}

uintptr_t popcount_longs(uintptr_t* lptr, uintptr_t start_idx, uintptr_t end_idx);

uintptr_t popcount_chars(uintptr_t* lptr, uintptr_t start_idx, uintptr_t end_idx);

uintptr_t popcount_longs_exclude(uintptr_t* lptr, uintptr_t* exclude_arr, uintptr_t end_idx);

int32_t distance_d_write(FILE** outfile_ptr, FILE** outfile2_ptr, FILE** outfile3_ptr, gzFile* gz_outfile_ptr, gzFile* gz_outfile2_ptr, gzFile* gz_outfile3_ptr, int32_t dist_calc_type, char* outname, char* outname_end, double* dists, double half_marker_ct_recip, uint32_t indiv_ct, int32_t first_indiv_idx, int32_t end_indiv_idx, int32_t parallel_idx, int32_t parallel_tot, unsigned char* membuf);

void collapse_arr(char* item_arr, int32_t fixed_item_len, uintptr_t* exclude_arr, int32_t exclude_arr_size);

// double rand_unif(void);

double rand_normal(double* secondval_ptr);

// void pick_d(unsigned char* cbuf, uint32_t ct, uint32_t dd);

void pick_d_small(unsigned char* tmp_cbuf, int32_t* ibuf, uint32_t ct, uint32_t dd);

void print_pheno_stdev(double* pheno_d, uint32_t indiv_ct);

uint32_t set_default_jackknife_d(uint32_t ct);

int32_t regress_distance(int32_t calculation_type, double* dists_local, double* pheno_d_local, uint32_t unfiltered_indiv_ct, uintptr_t* indiv_exclude, uint32_t indiv_ct_local, uint32_t thread_ct, uintptr_t regress_iters, uint32_t regress_d);

#endif // __WDIST_COMMON_H__
