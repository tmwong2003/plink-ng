// This file is part of PLINK 2.00, copyright (C) 2005-2017 Shaun Purcell,
// Christopher Chang.
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
// more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.


#include "plink2_ld.h"
#include "plink2_stats.h"

#ifdef __cplusplus
#include <functional> // std::greater
#endif

#ifdef __cplusplus
namespace plink2 {
#endif

void init_ld(ld_info_t* ldip) {
  ldip->prune_modifier = kfLdPrune0;
  ldip->prune_window_size = 0;
  ldip->prune_window_incr = 0;
  ldip->prune_last_param = 0.0;
  ldip->ld_console_modifier = kfLdConsole0;
  ldip->ld_console_varids[0] = nullptr;
  ldip->ld_console_varids[1] = nullptr;
}

void cleanup_ld(ld_info_t* ldip) {
  free_cond(ldip->ld_console_varids[0]);
  free_cond(ldip->ld_console_varids[1]);
}


static inline void popcount_vecs_2intersect(const vul_t* __restrict vvec1_iter, const vul_t* __restrict vvec2a_iter, const vul_t* __restrict vvec2b_iter, uintptr_t vec_ct, uint32_t* popcount_1_2a_ptr, uint32_t* popcount_1_2b_ptr) {
  // popcounts (vvec1 AND vvec2a[0..(ct-1)]) as well as (vvec1 AND vvec2b).  ct
  // is a multiple of 3.
  assert(!(vec_ct % 3));
  const vul_t m1 = VCONST_UL(kMask5555);
  const vul_t m2 = VCONST_UL(kMask3333);
  const vul_t m4 = VCONST_UL(kMask0F0F);

  // todo: check if moving this right before usage is better, looks like we
  // barely have enough registers...
  const vul_t m8 = VCONST_UL(kMask00FF);
  uint32_t popcount_1_2a = 0;
  uint32_t popcount_1_2b = 0;

  while (1) {
    univec_t acc_a;
    univec_t acc_b;
    acc_a.vi = vul_setzero();
    acc_b.vi = vul_setzero();

    const vul_t* vvec1_stop;
    if (vec_ct < 30) {
      if (!vec_ct) {
	*popcount_1_2a_ptr = popcount_1_2a;
	*popcount_1_2b_ptr = popcount_1_2b;
	return;
      }
      vvec1_stop = &(vvec1_iter[vec_ct]);
      vec_ct = 0;
    } else {
      vvec1_stop = &(vvec1_iter[30]);
      vec_ct -= 30;
    }
    do {
      vul_t loader = *vvec1_iter++;
      vul_t count1a = loader & (*vvec2a_iter++);
      vul_t count1b = loader & (*vvec2b_iter++);
      loader = *vvec1_iter++;
      vul_t count2a = loader & (*vvec2a_iter++);
      vul_t count2b = loader & (*vvec2b_iter++);
      loader = *vvec1_iter++;
      vul_t half1a = loader & (*vvec2a_iter++);
      vul_t half1b = loader & (*vvec2b_iter++);
      const vul_t half2a = vul_rshift(half1a, 1) & m1;
      const vul_t half2b = vul_rshift(half1b, 1) & m1;
      half1a = half1a & m1;
      half1b = half1b & m1;
      count1a = count1a - (vul_rshift(count1a, 1) & m1);
      count1b = count1b - (vul_rshift(count1b, 1) & m1);
      count2a = count2a - (vul_rshift(count2a, 1) & m1);
      count2b = count2b - (vul_rshift(count2b, 1) & m1);
      count1a = count1a + half1a;
      count1b = count1b + half1b;
      count2a = count2a + half2a;
      count2b = count2b + half2b;
      count1a = (count1a & m2) + (vul_rshift(count1a, 2) & m2);
      count1b = (count1b & m2) + (vul_rshift(count1b, 2) & m2);
      count1a = count1a + (count2a & m2) + (vul_rshift(count2a, 2) & m2);
      count1b = count1b + (count2b & m2) + (vul_rshift(count2b, 2) & m2);
      acc_a.vi = acc_a.vi + (count1a & m4) + (vul_rshift(count1a, 4) & m4);
      acc_b.vi = acc_b.vi + (count1b & m4) + (vul_rshift(count1b, 4) & m4);
    } while (vvec1_iter < vvec1_stop);
    acc_a.vi = (acc_a.vi & m8) + (vul_rshift(acc_a.vi, 8) & m8);
    acc_b.vi = (acc_b.vi & m8) + (vul_rshift(acc_b.vi, 8) & m8);
    popcount_1_2a += univec_hsum_16bit(acc_a);
    popcount_1_2b += univec_hsum_16bit(acc_b);
  }
}

// don't bother with popcount_vecs_3intersect for now, but test later

void popcount_longs_2intersect(const uintptr_t* __restrict bitvec1_iter, const uintptr_t* __restrict bitvec2a_iter, const uintptr_t* __restrict bitvec2b_iter, uintptr_t word_ct, uint32_t* popcount_1_2a_ptr, uint32_t* popcount_1_2b_ptr) {
  const uintptr_t* bitvec1_end = &(bitvec1_iter[word_ct]);
  uintptr_t trivec_ct = word_ct / (3 * kWordsPerVec);
  uint32_t popcount_1_2a;
  uint32_t popcount_1_2b;
  popcount_vecs_2intersect((const vul_t*)bitvec1_iter, (const vul_t*)bitvec2a_iter, (const vul_t*)bitvec2b_iter, trivec_ct * 3, &popcount_1_2a, &popcount_1_2b);
  bitvec1_iter = &(bitvec1_iter[trivec_ct * (3 * kWordsPerVec)]);
  bitvec2a_iter = &(bitvec2a_iter[trivec_ct * (3 * kWordsPerVec)]);
  bitvec2b_iter = &(bitvec2b_iter[trivec_ct * (3 * kWordsPerVec)]);
  while (bitvec1_iter < bitvec1_end) {
    const uintptr_t loader1 = *bitvec1_iter++;
    popcount_1_2a += popcount_long(loader1 & (*bitvec2a_iter++));
    popcount_1_2b += popcount_long(loader1 & (*bitvec2b_iter++));
  }
  *popcount_1_2a_ptr = popcount_1_2a;
  *popcount_1_2b_ptr = popcount_1_2b;
}


static inline int32_t dotprod_vecs(const vul_t* __restrict vvec1a_iter, const vul_t* __restrict vvec1b_iter, const vul_t* __restrict vvec2a_iter, const vul_t* __restrict vvec2b_iter, uintptr_t vec_ct) {
  // assumes vvec1a/vvec2a represesent +1s, vvec1b/vvec2b represent -1s, and
  // everything else is 0.  computes
  //   popcount(vvec1a & vvec2a) + popcount(vvec1b & vvec2b)
  //   - popcount(vvec1a & vvec2b) - popcount(vvec1b & vvec2a).
  // ct must be a multiple of 3.
  assert(!(vec_ct % 3));
  const vul_t m1 = VCONST_UL(kMask5555);
  const vul_t m2 = VCONST_UL(kMask3333);
  const vul_t m4 = VCONST_UL(kMask0F0F);
  int32_t tot = 0;
  while (1) {
    univec_t acc_plus;
    univec_t acc_minus;
    acc_plus.vi = vul_setzero();
    acc_minus.vi = vul_setzero();

    const vul_t* vvec1a_stop;
    if (vec_ct < 30) {
      if (!vec_ct) {
	return tot;
      }
      vvec1a_stop = &(vvec1a_iter[vec_ct]);
      vec_ct = 0;
    } else {
      vvec1a_stop = &(vvec1a_iter[30]);
      vec_ct -= 30;
    }
    do {
      vul_t loader1a = *vvec1a_iter++;
      vul_t loader1b = *vvec1b_iter++;
      vul_t loader2a = *vvec2a_iter++;
      vul_t loader2b = *vvec2b_iter++;
      // loader1a and loader1b are disjoint, etc.; take advantage of that
      vul_t count1_plus = (loader1a & loader2a) | (loader1b & loader2b);
      vul_t count1_minus = (loader1a & loader2b) | (loader1b & loader2a);

      loader1a = *vvec1a_iter++;
      loader1b = *vvec1b_iter++;
      loader2a = *vvec2a_iter++;
      loader2b = *vvec2b_iter++;
      vul_t count2_plus = (loader1a & loader2a) | (loader1b & loader2b);
      vul_t count2_minus = (loader1a & loader2b) | (loader1b & loader2a);

      loader1a = *vvec1a_iter++;
      loader1b = *vvec1b_iter++;
      loader2a = *vvec2a_iter++;
      loader2b = *vvec2b_iter++;
      vul_t half1_plus = (loader1a & loader2a) | (loader1b & loader2b);
      vul_t half1_minus = (loader1a & loader2b) | (loader1b & loader2a);
      const vul_t half2_plus = vul_rshift(half1_plus, 1) & m1;
      const vul_t half2_minus = vul_rshift(half1_minus, 1) & m1;
      half1_plus = half1_plus & m1;
      half1_minus = half1_minus & m1;
      count1_plus = count1_plus - (vul_rshift(count1_plus, 1) & m1);
      count1_minus = count1_minus - (vul_rshift(count1_minus, 1) & m1);
      count2_plus = count2_plus - (vul_rshift(count2_plus, 1) & m1);
      count2_minus = count2_minus - (vul_rshift(count2_minus, 1) & m1);
      count1_plus = count1_plus + half1_plus;
      count1_minus = count1_minus + half1_minus;
      count2_plus = count2_plus + half2_plus;
      count2_minus = count2_minus + half2_minus;
      count1_plus = (count1_plus & m2) + (vul_rshift(count1_plus, 2) & m2);
      count1_minus = (count1_minus & m2) + (vul_rshift(count1_minus, 2) & m2);
      count1_plus = count1_plus + (count2_plus & m2) + (vul_rshift(count2_plus, 2) & m2);
      count1_minus = count1_minus + (count2_minus & m2) + (vul_rshift(count2_minus, 2) & m2);
      acc_plus.vi = acc_plus.vi + (count1_plus & m4) + (vul_rshift(count1_plus, 4) & m4);
      acc_minus.vi = acc_minus.vi + (count1_minus & m4) + (vul_rshift(count1_minus, 4) & m4);
    } while (vvec1a_iter < vvec1a_stop);
    const vul_t m8 = VCONST_UL(kMask00FF);
    acc_plus.vi = (acc_plus.vi & m8) + (vul_rshift(acc_plus.vi, 8) & m8);
    acc_minus.vi = (acc_minus.vi & m8) + (vul_rshift(acc_minus.vi, 8) & m8);
    tot += (uint32_t)univec_hsum_16bit(acc_plus);
    tot -= (uint32_t)univec_hsum_16bit(acc_minus);
  }
}

int32_t dotprod_longs(const uintptr_t* __restrict bitvec1a_iter, const uintptr_t* __restrict bitvec1b_iter, const uintptr_t* __restrict bitvec2a_iter, const uintptr_t* __restrict bitvec2b_iter, uintptr_t word_ct) {
  const uintptr_t* bitvec1a_end = &(bitvec1a_iter[word_ct]);
  uintptr_t trivec_ct = word_ct / (kWordsPerVec * 3);
  int32_t tot = dotprod_vecs((const vul_t*)bitvec1a_iter, (const vul_t*)bitvec1b_iter, (const vul_t*)bitvec2a_iter, (const vul_t*)bitvec2b_iter, trivec_ct * 3);
  bitvec1a_iter = &(bitvec1a_iter[trivec_ct * (3 * kWordsPerVec)]);
  bitvec1b_iter = &(bitvec1b_iter[trivec_ct * (3 * kWordsPerVec)]);
  bitvec2a_iter = &(bitvec2a_iter[trivec_ct * (3 * kWordsPerVec)]);
  bitvec2b_iter = &(bitvec2b_iter[trivec_ct * (3 * kWordsPerVec)]);
  while (bitvec1a_iter < bitvec1a_end) {
    uintptr_t loader1a = *bitvec1a_iter++;
    uintptr_t loader1b = *bitvec1b_iter++;
    uintptr_t loader2a = *bitvec2a_iter++;
    uintptr_t loader2b = *bitvec2b_iter++;
    tot += popcount_long((loader1a & loader2a) | (loader1b & loader2b));
    tot -= popcount_long((loader1a & loader2b) | (loader1b & loader2a));
  }
  return tot;
}

void ldprune_next_subcontig(const uintptr_t* variant_include, const uint32_t* variant_bps, const uint32_t* subcontig_info, const uint32_t* subcontig_thread_assignments, uint32_t x_start, uint32_t x_len, uint32_t y_start, uint32_t y_len, uint32_t founder_ct, uint32_t founder_male_ct, uint32_t prune_window_size, uint32_t thread_idx, uint32_t* subcontig_idx_ptr, uint32_t* subcontig_end_tvidx_ptr, uint32_t* next_window_end_tvidx_ptr, uint32_t* is_x_ptr, uint32_t* is_y_ptr, uint32_t* cur_founder_ct_ptr, uint32_t* cur_founder_ctaw_ptr, uint32_t* cur_founder_ctl_ptr, uintptr_t* entire_variant_buf_word_ct_ptr, uint32_t* variant_uidx_winstart_ptr, uint32_t* variant_uidx_winend_ptr) {
  uint32_t subcontig_idx = *subcontig_idx_ptr;
  do {
    ++subcontig_idx;
  } while (subcontig_thread_assignments[subcontig_idx] != thread_idx);
  *subcontig_idx_ptr = subcontig_idx;
  const uint32_t subcontig_first_tvidx = *subcontig_end_tvidx_ptr;
  const uint32_t subcontig_len = subcontig_info[3 * subcontig_idx];
  const uint32_t variant_uidx_winstart = subcontig_info[3 * subcontig_idx + 2];
  const uint32_t subcontig_end_tvidx = subcontig_first_tvidx + subcontig_len;
  *subcontig_end_tvidx_ptr = subcontig_end_tvidx;
  if (variant_bps) {
    const uint32_t variant_bp_thresh = variant_bps[variant_uidx_winstart] + prune_window_size;
    uint32_t variant_uidx_winend = variant_uidx_winstart;
    uint32_t first_window_len = 1;
    do {
      ++variant_uidx_winend;
      next_set_unsafe_ck(variant_include, &variant_uidx_winend);
    } while ((variant_bps[variant_uidx_winend] <= variant_bp_thresh) && (++first_window_len < subcontig_len));
    *next_window_end_tvidx_ptr = subcontig_first_tvidx + first_window_len;
    *variant_uidx_winend_ptr = variant_uidx_winend;
  } else {
    *next_window_end_tvidx_ptr = subcontig_first_tvidx + MINV(subcontig_len, prune_window_size);
  }

  *variant_uidx_winstart_ptr = variant_uidx_winstart;
  // _len is better than _end here since we can exploit unsignedness
  const uint32_t is_x = ((variant_uidx_winstart - x_start) < x_len);
  const uint32_t is_y = ((variant_uidx_winstart - y_start) < y_len);
  if ((is_x != (*is_x_ptr)) || (is_y != (*is_y_ptr))) {
    *is_x_ptr = is_x;
    *is_y_ptr = is_y;
    const uint32_t cur_founder_ct = (is_x || is_y)? founder_male_ct : founder_ct;
    const uint32_t cur_founder_ctaw = BITCT_TO_ALIGNED_WORDCT(cur_founder_ct);
    *cur_founder_ct_ptr = cur_founder_ct;
    *cur_founder_ctaw_ptr = cur_founder_ctaw;
    *cur_founder_ctl_ptr = BITCT_TO_WORDCT(cur_founder_ct);
    *entire_variant_buf_word_ct_ptr = 3 * cur_founder_ctaw;
    if (is_x) {
      *entire_variant_buf_word_ct_ptr += 3 * BITCT_TO_ALIGNED_WORDCT(founder_ct - founder_male_ct);
    }
  }
}

void genoarr_split_02nm(const uintptr_t* __restrict genoarr, uint32_t sample_ct, uintptr_t* __restrict zero_bitarr, uintptr_t* __restrict two_bitarr, uintptr_t* __restrict nm_bitarr) {
  // ok if trailing bits of genoarr are not zeroed out
  // trailing bits of {zero,two,nm}_bitarr are zeroed out
  const uint32_t sample_ctl2 = QUATERCT_TO_WORDCT(sample_ct);
  halfword_t* zero_bitarr_alias = (halfword_t*)zero_bitarr;
  halfword_t* two_bitarr_alias = (halfword_t*)two_bitarr;
  halfword_t* nm_bitarr_alias = (halfword_t*)nm_bitarr;
  for (uint32_t widx = 0; widx < sample_ctl2; ++widx) {
    const uintptr_t cur_geno_word = genoarr[widx];
    const uint32_t low_halfword = pack_word_to_halfword(cur_geno_word & kMask5555);
    const uint32_t high_halfword = pack_word_to_halfword((cur_geno_word >> 1) & kMask5555);
    zero_bitarr_alias[widx] = ~(low_halfword | high_halfword);
    two_bitarr_alias[widx] = high_halfword & (~low_halfword);
    nm_bitarr_alias[widx] = ~(low_halfword & high_halfword);
  }

  // had code which operated directly on zero_bitarr/two_bitarr/nm_bitarr here,
  // but that technically breaks the strict-aliasing rule, and this isn't a
  // primary bottleneck so may as well be paranoid
  const uint32_t sample_ct_rem = sample_ct % kBitsPerWordD2;
  if (sample_ct_rem) {
    const halfword_t trailing_mask = (1U << sample_ct_rem) - 1;
    zero_bitarr_alias[sample_ctl2 - 1] &= trailing_mask;
    two_bitarr_alias[sample_ctl2 - 1] &= trailing_mask;
    nm_bitarr_alias[sample_ctl2 - 1] &= trailing_mask;
  }
  if (sample_ctl2 % 2) {
    zero_bitarr_alias[sample_ctl2] = 0;
    two_bitarr_alias[sample_ctl2] = 0;
    nm_bitarr_alias[sample_ctl2] = 0;
  }
}

void ldprune_next_window(const uintptr_t* __restrict variant_include, const uint32_t* __restrict variant_bps, const uint32_t* __restrict tvidxs, const uintptr_t* __restrict cur_window_removed, uint32_t prune_window_size, uint32_t window_incr, uint32_t window_maxl, uint32_t subcontig_end_tvidx, uint32_t* cur_window_size_ptr, uint32_t* __restrict window_start_tvidx_ptr, uint32_t* __restrict variant_uidx_winstart_ptr, uint32_t* __restrict next_window_end_tvidx_ptr, uint32_t* __restrict variant_uidx_winend_ptr, uintptr_t* __restrict occupied_window_slots, uint32_t* winpos_to_slot_idx) {
  uint32_t next_window_end_tvidx = *next_window_end_tvidx_ptr;
  if (next_window_end_tvidx == subcontig_end_tvidx) {
    // just completed last window in subcontig
    *cur_window_size_ptr = 0;
    *window_start_tvidx_ptr = subcontig_end_tvidx;
    fill_ulong_zero(window_maxl, occupied_window_slots);
    return;
  }
  uint32_t next_window_start_tvidx = *window_start_tvidx_ptr;
  if (variant_bps) {
    // this is guaranteed to be nonnegative
    uint32_t variant_uidx_winstart = *variant_uidx_winstart_ptr;
    uint32_t variant_uidx_winend = *variant_uidx_winend_ptr;
    const uint32_t window_start_min_bp = variant_bps[variant_uidx_winend] - prune_window_size;
    uint32_t window_start_bp;
    do {
      // advance window start by as much as necessary to make end advance by at
      // least 1
      ++next_window_start_tvidx;
      ++variant_uidx_winstart;
      next_set_unsafe_ck(variant_include, &variant_uidx_winstart);
      window_start_bp = variant_bps[variant_uidx_winstart];
    } while (window_start_bp < window_start_min_bp);
    // now advance window end as appropriate
    const uint32_t window_end_thresh = window_start_bp + prune_window_size;
    do {
      if (++next_window_end_tvidx == subcontig_end_tvidx) {
	break;
      }
      ++variant_uidx_winend;
      next_set_unsafe_ck(variant_include, &variant_uidx_winend);
    } while (variant_bps[variant_uidx_winend] <= window_end_thresh);
    *variant_uidx_winstart_ptr = variant_uidx_winstart;
    *variant_uidx_winend_ptr = variant_uidx_winend;
  } else {
    next_window_start_tvidx += window_incr;
    next_window_end_tvidx = MINV(next_window_start_tvidx + prune_window_size, subcontig_end_tvidx);
  }
  const uint32_t cur_window_size = *cur_window_size_ptr;
  uint32_t winpos_write = 0;
  for (uint32_t winpos_read = 0; winpos_read < cur_window_size; ++winpos_read) {
    const uint32_t slot_idx = winpos_to_slot_idx[winpos_read];
    if (IS_SET(cur_window_removed, winpos_read) || (tvidxs[slot_idx] < next_window_start_tvidx)) {
      CLEAR_BIT(slot_idx, occupied_window_slots);
    } else {
      winpos_to_slot_idx[winpos_write++] = slot_idx;
    }
  }
  *cur_window_size_ptr = winpos_write;
  *window_start_tvidx_ptr = next_window_start_tvidx;
  *next_window_end_tvidx_ptr = next_window_end_tvidx;
}

void compute_indep_pairwise_r2_components(const uintptr_t* __restrict first_genobufs, const uintptr_t* __restrict second_genobufs, const int32_t* __restrict second_vstats, uint32_t founder_ct, uint32_t* cur_nm_ct_ptr, int32_t* cur_first_sum_ptr, uint32_t* cur_first_ssq_ptr, int32_t* second_sum_ptr, uint32_t* second_ssq_ptr, int32_t* cur_dotprod_ptr) {
  const uint32_t founder_ctaw = BITCT_TO_ALIGNED_WORDCT(founder_ct);
  const uint32_t founder_ctl = BITCT_TO_WORDCT(founder_ct);
  *cur_dotprod_ptr = dotprod_longs(first_genobufs, &(first_genobufs[founder_ctaw]), second_genobufs, &(second_genobufs[founder_ctaw]), founder_ctl);
  if (*cur_nm_ct_ptr != founder_ct) {
    uint32_t plusone_ct;
    uint32_t minusone_ct;
    popcount_longs_2intersect(&(first_genobufs[2 * founder_ctaw]), second_genobufs, &(second_genobufs[founder_ctaw]), founder_ctl, &plusone_ct, &minusone_ct);
    *second_sum_ptr = ((int32_t)plusone_ct) - ((int32_t)minusone_ct);
    *second_ssq_ptr = plusone_ct + minusone_ct;
  } else {
    *second_sum_ptr = second_vstats[1];
    *second_ssq_ptr = second_vstats[2];
  }
  const uint32_t second_nm_ct = second_vstats[0];
  if (second_nm_ct == founder_ct) {
    // assumed that cur_first_nm initialized to first_vstats[0], cur_first_sum
    // initialized to first_vstats[1], cur_first_ssq to first_vstats[2]
    return;
  }
  uint32_t plusone_ct;
  uint32_t minusone_ct;
  popcount_longs_2intersect(&(second_genobufs[2 * founder_ctaw]), first_genobufs, &(first_genobufs[founder_ctaw]), founder_ctl, &plusone_ct, &minusone_ct);
  *cur_first_sum_ptr = ((int32_t)plusone_ct) - ((int32_t)minusone_ct);
  *cur_first_ssq_ptr = plusone_ct + minusone_ct;
  if (*cur_nm_ct_ptr == founder_ct) {
    *cur_nm_ct_ptr = second_nm_ct;
    return;
  }
  *cur_nm_ct_ptr = popcount_longs_intersect(&(first_genobufs[2 * founder_ctaw]), &(second_genobufs[2 * founder_ctaw]), founder_ctl);
}

// multithread globals
static const uint32_t* g_subcontig_info = nullptr;
static const uint32_t* g_subcontig_thread_assignments = nullptr;
static const uintptr_t* g_variant_include = nullptr;
static const uintptr_t* g_variant_allele_idxs = nullptr;
static const alt_allele_ct_t* g_maj_alleles = nullptr;
static const double* g_all_allele_freqs = nullptr;
static const uint32_t* g_variant_bps = nullptr;
static uint32_t* g_tvidx_end = nullptr;
static uint32_t g_x_start = 0;
static uint32_t g_x_len = 0;
static uint32_t g_y_start = 0;
static uint32_t g_y_len = 0;
static uint32_t g_founder_ct = 0;
static uint32_t g_founder_male_ct = 0;
static uint32_t g_prune_window_size = 0;
static uint32_t g_window_maxl = 0;
static double g_prune_ld_thresh = 0.0;
static uint32_t g_window_incr = 0;
static uint32_t g_cur_batch_size = 0;
static uintptr_t** g_genobufs = nullptr;
static uintptr_t** g_occupied_window_slots = nullptr;
static uintptr_t** g_cur_window_removed = nullptr;
static double** g_cur_maj_freqs = nullptr;
static uintptr_t** g_removed_variants_write = nullptr;
static int32_t** g_vstats = nullptr;
static int32_t** g_nonmale_vstats = nullptr;
static uint32_t** g_winpos_to_slot_idx = nullptr;
static uint32_t** g_tvidxs = nullptr;
static uint32_t** g_first_unchecked_tvidx = nullptr;
static uintptr_t** g_raw_tgenovecs[2] = {nullptr, nullptr};

THREAD_FUNC_DECL indep_pairwise_thread(void* arg) {
  const uintptr_t tidx = (uintptr_t)arg;
  const uint32_t* subcontig_info = g_subcontig_info;
  const uint32_t* subcontig_thread_assignments = g_subcontig_thread_assignments;
  const uintptr_t* variant_include = g_variant_include;
  const uint32_t x_start = g_x_start;
  const uint32_t x_len = g_x_len;
  const uint32_t y_start = g_y_start;
  const uint32_t y_len = g_y_len;
  const uintptr_t* variant_allele_idxs = g_variant_allele_idxs;
  const alt_allele_ct_t* maj_alleles = g_maj_alleles;
  const double* all_allele_freqs = g_all_allele_freqs;
  const uint32_t* variant_bps = g_variant_bps;
  const uint32_t founder_ct = g_founder_ct;
  const uint32_t founder_male_ct = g_founder_male_ct;
  const uint32_t founder_male_ctl2 = QUATERCT_TO_WORDCT(founder_male_ct);
  const uint32_t nonmale_ct = founder_ct - founder_male_ct;
  const uint32_t nonmale_ctaw = BITCT_TO_ALIGNED_WORDCT(nonmale_ct);
  const uint32_t nonmale_ctl = BITCT_TO_WORDCT(nonmale_ct);
  const uintptr_t raw_tgenovec_single_variant_word_ct = round_up_pow2(QUATERCT_TO_WORDCT(nonmale_ct) + founder_male_ctl2, kWordsPerVec);
  const uint32_t prune_window_size = g_prune_window_size;
  const uint32_t window_maxl = g_window_maxl;
  const double prune_ld_thresh = g_prune_ld_thresh;
  const uint32_t window_incr = g_window_incr;
  const uint32_t tvidx_end = g_tvidx_end[tidx];
  uintptr_t* genobufs = g_genobufs[tidx];
  uintptr_t* occupied_window_slots = g_occupied_window_slots[tidx];
  uintptr_t* cur_window_removed = g_cur_window_removed[tidx];
  uintptr_t* removed_variants_write = g_removed_variants_write[tidx];
  double* cur_maj_freqs = g_cur_maj_freqs[tidx];
  int32_t* vstats = g_vstats[tidx];
  int32_t* nonmale_vstats = g_nonmale_vstats[tidx];
  uint32_t* winpos_to_slot_idx = g_winpos_to_slot_idx[tidx];
  uint32_t* tvidxs = g_tvidxs[tidx];
  uint32_t* first_unchecked_tvidx = g_first_unchecked_tvidx[tidx];
  
  uint32_t subcontig_end_tvidx = 0;
  uint32_t subcontig_idx = 0xffffffffU; // deliberate overflow
  uint32_t window_start_tvidx = 0;
  uint32_t next_window_end_tvidx = 0;
  uint32_t write_slot_idx = 0;
  uint32_t is_x = 0;
  uint32_t is_y = 0;
  uint32_t cur_window_size = 0;
  uint32_t tvidx_start = 0;
  uint32_t cur_founder_ct = founder_ct;
  uint32_t cur_founder_ctaw = BITCT_TO_ALIGNED_WORDCT(founder_ct);
  uint32_t cur_founder_ctl = BITCT_TO_WORDCT(founder_ct);
  uint32_t variant_uidx = 0;
  uint32_t variant_uidx_winstart = 0;
  uint32_t variant_uidx_winend = 0;
  uintptr_t entire_variant_buf_word_ct = 3 * cur_founder_ctaw;
  uint32_t cur_allele_ct = 2;
  uint32_t parity = 0;
  while (1) {
    const uint32_t is_last_block = g_is_last_thread_block;
    const uint32_t cur_batch_size = g_cur_batch_size;
    const uint32_t tvidx_stop = MINV(tvidx_start + cur_batch_size, tvidx_end);
    // main loop has to be variant-, not window-, based due to how datasets too
    // large to fit in memory are handled: we may have to halt in the middle of
    // unpacking data for a window, waiting until the current I/O pass is
    // complete before proceeding
    const uintptr_t* raw_tgenovecs = g_raw_tgenovecs[parity][tidx];
    for (uint32_t cur_tvidx = tvidx_start; cur_tvidx < tvidx_stop; ++variant_uidx) {
      if (cur_tvidx == subcontig_end_tvidx) {
	ldprune_next_subcontig(variant_include, variant_bps, subcontig_info, subcontig_thread_assignments, x_start, x_len, y_start, y_len, founder_ct, founder_male_ct, prune_window_size, tidx, &subcontig_idx, &subcontig_end_tvidx, &next_window_end_tvidx, &is_x, &is_y, &cur_founder_ct, &cur_founder_ctaw, &cur_founder_ctl, &entire_variant_buf_word_ct, &variant_uidx_winstart, &variant_uidx_winend);
	variant_uidx = variant_uidx_winstart;
      }
      next_set_unsafe_ck(variant_include, &variant_uidx);
      write_slot_idx = next_unset_unsafe(occupied_window_slots, write_slot_idx);
      uintptr_t tvidx_offset = cur_tvidx - tvidx_start;
      const uintptr_t* cur_raw_tgenovecs = &(raw_tgenovecs[tvidx_offset * raw_tgenovec_single_variant_word_ct]);
      uintptr_t* cur_genobuf = &(genobufs[write_slot_idx * entire_variant_buf_word_ct]);
      uintptr_t* cur_genobuf_minus = &(cur_genobuf[cur_founder_ctaw]);
      uintptr_t* cur_genobuf_nm = &(cur_genobuf_minus[cur_founder_ctaw]);
      genoarr_split_02nm(cur_raw_tgenovecs, cur_founder_ct, cur_genobuf, cur_genobuf_minus, cur_genobuf_nm);
      uint32_t nm_ct = popcount_longs(cur_genobuf_nm, cur_founder_ctl);
      uint32_t plusone_ct = popcount_longs(cur_genobuf, cur_founder_ctl);
      uint32_t minusone_ct = popcount_longs(cur_genobuf_minus, cur_founder_ctl);
      vstats[3 * write_slot_idx] = nm_ct;
      vstats[3 * write_slot_idx + 1] = ((int32_t)plusone_ct) - ((int32_t)minusone_ct);
      vstats[3 * write_slot_idx + 2] = plusone_ct + minusone_ct;
      if (is_x) {
	cur_genobuf = &(cur_genobuf[3 * cur_founder_ctaw]);
	cur_genobuf_minus = &(cur_genobuf[nonmale_ctaw]);
	cur_genobuf_nm = &(cur_genobuf_minus[nonmale_ctaw]);
	genoarr_split_02nm(&(cur_raw_tgenovecs[founder_male_ctl2]), nonmale_ct, cur_genobuf, cur_genobuf_minus, cur_genobuf_nm);
	const uint32_t x_nonmale_nm_ct = popcount_longs(cur_genobuf_nm, nonmale_ctl);
	const uint32_t x_nonmale_plusone_ct = popcount_longs(cur_genobuf, nonmale_ctl);
	const uint32_t x_nonmale_minusone_ct = popcount_longs(cur_genobuf_minus, nonmale_ctl);
	nonmale_vstats[3 * write_slot_idx] = x_nonmale_nm_ct;
	nonmale_vstats[3 * write_slot_idx + 1] = ((int32_t)x_nonmale_plusone_ct) - ((int32_t)x_nonmale_minusone_ct);
	nonmale_vstats[3 * write_slot_idx + 2] = x_nonmale_plusone_ct + x_nonmale_minusone_ct;
	nm_ct += 2 * x_nonmale_nm_ct;
	plusone_ct += 2 * x_nonmale_plusone_ct;
	minusone_ct += 2 * x_nonmale_minusone_ct;
      }
      if (((!plusone_ct) && (!minusone_ct)) || (plusone_ct == nm_ct) || (minusone_ct == nm_ct)) {
	SET_BIT(cur_window_size, cur_window_removed);
	SET_BIT(cur_tvidx, removed_variants_write);
      } else {
	tvidxs[write_slot_idx] = cur_tvidx;
	uintptr_t allele_idx_base;
	if (!variant_allele_idxs) {
	  allele_idx_base = variant_uidx;
	} else {
	  allele_idx_base = variant_allele_idxs[variant_uidx];
	  cur_allele_ct = variant_allele_idxs[variant_uidx + 1] - allele_idx_base;
	  allele_idx_base -= variant_uidx;
	}
	cur_maj_freqs[write_slot_idx] = get_allele_freq(&(all_allele_freqs[allele_idx_base]), maj_alleles[variant_uidx], cur_allele_ct);
	first_unchecked_tvidx[write_slot_idx] = cur_tvidx + 1;
      }
      SET_BIT(write_slot_idx, occupied_window_slots);
      winpos_to_slot_idx[cur_window_size++] = write_slot_idx;
      // are we at the end of a window?
      if (++cur_tvidx == next_window_end_tvidx) {
	// possible for cur_window_size == 1, if all variants at the end of the
	// previous window were pruned
	uint32_t cur_removed_ct = popcount_longs(cur_window_removed, BITCT_TO_WORDCT(cur_window_size));
	uint32_t prev_removed_ct;
	do {
	  prev_removed_ct = cur_removed_ct;
	  uint32_t first_winpos = 0;
	  // const uint32_t debug_print = (!IS_SET(cur_window_removed, 0)) && (tvidxs[winpos_to_slot_idx[0]] == 0);
	  while (1) {
	    next_unset_unsafe_ck(cur_window_removed, &first_winpos);
	    // can assume empty trailing bit for cur_window_removed
	    if (first_winpos == cur_window_size) {
	      break;
	    }
	    uint32_t first_slot_idx = winpos_to_slot_idx[first_winpos];
	    const uint32_t cur_first_unchecked_tvidx = first_unchecked_tvidx[first_slot_idx];
	    uint32_t second_winpos = first_winpos;
	    while (1) {
	      ++second_winpos;
	      next_unset_unsafe_ck(cur_window_removed, &second_winpos);
	      if (second_winpos == cur_window_size) {
		break;
	      }
	      uint32_t second_slot_idx = winpos_to_slot_idx[second_winpos];
	      if (tvidxs[second_slot_idx] >= cur_first_unchecked_tvidx) {
		uintptr_t* first_genobufs = &(genobufs[first_slot_idx * entire_variant_buf_word_ct]);
		const uint32_t first_nm_ct = vstats[3 * first_slot_idx];
		const int32_t first_sum = vstats[3 * first_slot_idx + 1];
		const uint32_t first_ssq = vstats[3 * first_slot_idx + 2];
		while (1) {
		  uintptr_t* second_genobufs = &(genobufs[second_slot_idx * entire_variant_buf_word_ct]);
		  uint32_t cur_nm_ct = first_nm_ct;
		  int32_t cur_first_sum = first_sum;
		  uint32_t cur_first_ssq = first_ssq;
		  int32_t second_sum;
		  uint32_t second_ssq;
		  int32_t cur_dotprod;
		  compute_indep_pairwise_r2_components(first_genobufs, second_genobufs, &(vstats[3 * second_slot_idx]), cur_founder_ct, &cur_nm_ct, &cur_first_sum, &cur_first_ssq, &second_sum, &second_ssq, &cur_dotprod);
		  if (is_x) {
		    uint32_t nonmale_nm_ct = nonmale_vstats[3 * first_slot_idx];
		    int32_t nonmale_first_sum = nonmale_vstats[3 * first_slot_idx + 1];
		    uint32_t nonmale_first_ssq = nonmale_vstats[3 * first_slot_idx + 2];
		    int32_t nonmale_dotprod;
		    int32_t nonmale_second_sum;
		    uint32_t nonmale_second_ssq;
		    compute_indep_pairwise_r2_components(&(first_genobufs[3 * cur_founder_ctaw]), &(second_genobufs[3 * cur_founder_ctaw]), &(nonmale_vstats[3 * second_slot_idx]), nonmale_ct, &nonmale_nm_ct, &nonmale_first_sum, &nonmale_first_ssq, &nonmale_second_sum, &nonmale_second_ssq, &nonmale_dotprod);
		    // only --ld-xchr 3 for now
		    // assumes founder_ct < 2^30
		    cur_nm_ct += 2 * nonmale_nm_ct;
		    cur_first_sum += 2 * nonmale_first_sum;
		    cur_first_ssq += 2 * nonmale_first_ssq;
		    second_sum += 2 * nonmale_second_sum;
		    second_ssq += 2 * nonmale_second_ssq;
		    cur_dotprod += 2 * nonmale_dotprod;
		  }
		  // these three values are actually cur_nm_ct times their
		  // true values, but that cancels out
		  const double cov12 = (double)(cur_dotprod * ((int64_t)cur_nm_ct) - ((int64_t)cur_first_sum) * second_sum);
		  const double variance1 = (double)(cur_first_ssq * ((int64_t)cur_nm_ct) - ((int64_t)cur_first_sum) * cur_first_sum);
		  const double variance2 = (double)(second_ssq * ((int64_t)cur_nm_ct) - ((int64_t)second_sum) * second_sum);
		  // > instead of >=, so we don't prune from a pair of
		  // variants with zero common observations
		  if (cov12 * cov12 > prune_ld_thresh * variance1 * variance2) {
		    // strictly speaking, the (1 + kSmallEpsilon) tolerance
		    // does not appear to be needed yet, but it will be once
		    // --read-freq is implemented.
		    // this has a surprisingly large ~3% speed penalty on my
		    // main test scenario, but that's an acceptable price to
		    // pay for reproducibility.
		    if (cur_maj_freqs[first_slot_idx] > cur_maj_freqs[second_slot_idx] * (1 + kSmallEpsilon)) {
		      /*
		      if (debug_print) {
			printf("removing %u, keeping %u, freqs %g/%g, r2 = %g\n", tvidxs[first_slot_idx], tvidxs[second_slot_idx], cur_maj_freqs[first_slot_idx], cur_maj_freqs[second_slot_idx], cov12 * cov12 / (variance1 * variance2));
		      }
		      */
		      SET_BIT(first_winpos, cur_window_removed);
		      SET_BIT(tvidxs[first_slot_idx], removed_variants_write);
		    } else {
		      /*
		      if (debug_print) {
		        printf("removing %u (second), keeping %u, freqs %g/%g, r2 = %g\n", tvidxs[second_slot_idx], tvidxs[first_slot_idx], cur_maj_freqs[second_slot_idx], cur_maj_freqs[first_slot_idx], cov12 * cov12 / (variance1 * variance2));
		      }
		      */
		      SET_BIT(second_winpos, cur_window_removed);
		      SET_BIT(tvidxs[second_slot_idx], removed_variants_write);
		      const uint32_t next_start_winpos = next_unset_unsafe(cur_window_removed, second_winpos);
		      if (next_start_winpos < cur_window_size) {
			first_unchecked_tvidx[first_slot_idx] = tvidxs[winpos_to_slot_idx[next_start_winpos]];
		      } else {
			first_unchecked_tvidx[first_slot_idx] = cur_tvidx;
		      }
		    }
		    break;
		  }
		  ++second_winpos;
		  next_unset_unsafe_ck(cur_window_removed, &second_winpos);
		  if (second_winpos == cur_window_size) {
		    first_unchecked_tvidx[first_slot_idx] = cur_tvidx;
		    break;
		  }
		  second_slot_idx = winpos_to_slot_idx[second_winpos];
		} // while (1)
		break;
	      }
	    }
	    ++first_winpos;
	  }
	  cur_removed_ct = popcount_longs(cur_window_removed, BITCT_TO_WORDCT(cur_window_size));
	} while (cur_removed_ct > prev_removed_ct);
	const uint32_t prev_window_size = cur_window_size;
	ldprune_next_window(variant_include, variant_bps, tvidxs, cur_window_removed, prune_window_size, window_incr, window_maxl, subcontig_end_tvidx, &cur_window_size, &window_start_tvidx, &variant_uidx_winstart, &next_window_end_tvidx, &variant_uidx_winend, occupied_window_slots, winpos_to_slot_idx);
	// clear bits here since we set cur_window_removed bits during loading
	// process in monomorphic case
	fill_ulong_zero(BITCT_TO_WORDCT(prev_window_size), cur_window_removed);
	write_slot_idx = 0;
      }
    }
    if (is_last_block) {
      THREAD_RETURN;
    }
    THREAD_BLOCK_FINISH(tidx);
    parity = 1 - parity;
    tvidx_start = tvidx_stop;
  }
}

pglerr_t indep_pairwise(const uintptr_t* variant_include, const chr_info_t* cip, const uint32_t* variant_bps, const uintptr_t* variant_allele_idxs, const alt_allele_ct_t* maj_alleles, const double* allele_freqs, const uintptr_t* founder_info, const uint32_t* founder_info_cumulative_popcounts, const uintptr_t* founder_nonmale, const uintptr_t* founder_male, const ld_info_t* ldip, const uint32_t* subcontig_info, const uint32_t* subcontig_thread_assignments, uint32_t raw_sample_ct, uint32_t founder_ct, uint32_t founder_male_ct, uint32_t subcontig_ct, uintptr_t window_max, uint32_t calc_thread_ct, uint32_t max_load, pgen_reader_t* simple_pgrp, uintptr_t* removed_variants_collapsed) {
  pglerr_t reterr = kPglRetSuccess;
  {
    const uint32_t founder_nonmale_ct = founder_ct - founder_male_ct;
    if (founder_nonmale_ct * 2 + founder_male_ct > 0x7fffffffU) {
      // may as well document this
      logerrprint("Error: --indep-pairwise does not support >= 2^30 founders.\n");
      goto indep_pairwise_ret_NOT_YET_SUPPORTED;
    }
    const uint32_t founder_nonmale_ctaw = BITCT_TO_ALIGNED_WORDCT(founder_nonmale_ct);
    const uint32_t founder_male_ctaw = BITCT_TO_ALIGNED_WORDCT(founder_male_ct);
    // Per-thread allocations:
    // - tvidx_batch_size * raw_tgenovec_single_variant_word_ct *
    //     sizeof(intptr_t) for raw genotype data (g_raw_tgenovecs)
    // - tvidx_batch_size * sizeof(double) for g_maj_freqs
    // - if pos-based window, tvidx_batch_size * sizeof(int32_t)
    // - All of the above again, to allow loader thread to operate
    //     independently
    // - window_max * 3 * (founder_nonmale_ctaw + founder_male_ctaw) *
    //     kBytesPerVec for split genotype data
    // - max_loadl * sizeof(intptr_t) for removed-variant bitarray
    // - window_max * 3 * sizeof(int32_t) for main missing_ct, sum(x_i),
    //     sum(x_i^2) array
    // - window_max * 3 * sizeof(int32_t) for chrX founder_male missing_ct,
    //     sum(x_i), sum(x_i^2) array
    // - window_max * sizeof(int32_t) for indexes into genotype data bitarrays
    //     (for now, anyway)
    // - window_max * sizeof(int32_t) for live_indices (variant_idxs?)
    // - window_max * sizeof(int32_t) for start_arr (first uncompared
    //     variant_idx)
    uintptr_t* tmp_genovec;
    uint32_t* thread_last_subcontig;
    uint32_t* thread_subcontig_start_tvidx;
    uint32_t* thread_last_tvidx;
    uint32_t* thread_last_uidx;
    pthread_t* threads = nullptr;
    if (bigstack_alloc_ul(QUATERCT_TO_WORDCT(raw_sample_ct), &tmp_genovec) ||
	bigstack_calloc_ui(calc_thread_ct, &g_tvidx_end) ||
	bigstack_calloc_ui(calc_thread_ct, &thread_last_subcontig) ||
	bigstack_calloc_ui(calc_thread_ct, &thread_subcontig_start_tvidx) ||
	bigstack_calloc_ui(calc_thread_ct, &thread_last_tvidx) ||
	bigstack_calloc_ui(calc_thread_ct, &thread_last_uidx) ||
	bigstack_alloc_ulp(calc_thread_ct, &g_genobufs) ||
	bigstack_alloc_ulp(calc_thread_ct, &g_occupied_window_slots) ||
        bigstack_alloc_ulp(calc_thread_ct, &g_cur_window_removed) ||
	bigstack_alloc_dp(calc_thread_ct, &g_cur_maj_freqs) ||
	bigstack_alloc_ulp(calc_thread_ct, &g_removed_variants_write) ||
	bigstack_alloc_ip(calc_thread_ct, &g_vstats) ||
	bigstack_alloc_ip(calc_thread_ct, &g_nonmale_vstats) ||
	bigstack_alloc_uip(calc_thread_ct, &g_winpos_to_slot_idx) ||
	bigstack_alloc_uip(calc_thread_ct, &g_tvidxs) ||
	bigstack_alloc_uip(calc_thread_ct, &g_first_unchecked_tvidx) ||
	bigstack_alloc_ulp(calc_thread_ct, &(g_raw_tgenovecs[0])) ||
        bigstack_alloc_ulp(calc_thread_ct, &(g_raw_tgenovecs[1])) ||
	bigstack_alloc_thread(calc_thread_ct, &threads)) {
      goto indep_pairwise_ret_NOMEM;
    }
    for (uint32_t subcontig_idx = 0; subcontig_idx < subcontig_ct; ++subcontig_idx) {
      const uint32_t cur_thread_idx = subcontig_thread_assignments[subcontig_idx];
      g_tvidx_end[cur_thread_idx] += subcontig_info[3 * subcontig_idx];
    }
    const uintptr_t entire_variant_buf_word_ct = 3 * (founder_nonmale_ctaw + founder_male_ctaw);
    const uint32_t window_maxl = BITCT_TO_WORDCT(window_max);
    const uint32_t max_loadl = BITCT_TO_WORDCT(max_load);
    const uintptr_t genobuf_alloc = round_up_pow2(window_max * entire_variant_buf_word_ct * sizeof(intptr_t), kCacheline);
    const uintptr_t occupied_window_slots_alloc = round_up_pow2(window_maxl * sizeof(intptr_t), kCacheline);
    const uintptr_t cur_window_removed_alloc = round_up_pow2((1 + window_max / kBitsPerWord) * sizeof(intptr_t), kCacheline);
    const uintptr_t cur_maj_freqs_alloc = round_up_pow2(window_max * sizeof(double), kCacheline);
    const uintptr_t removed_variants_write_alloc = round_up_pow2(max_loadl * sizeof(intptr_t), kCacheline);
    const uintptr_t vstats_alloc = round_up_pow2(3 * window_max * sizeof(int32_t), kCacheline); // two of these
    const uintptr_t window_int32_alloc = round_up_pow2(window_max * sizeof(int32_t), kCacheline); // three of these
    const uintptr_t thread_alloc_base = genobuf_alloc + occupied_window_slots_alloc + cur_window_removed_alloc + cur_maj_freqs_alloc + removed_variants_write_alloc + 2 * vstats_alloc + 3 * window_int32_alloc;

    const uint32_t founder_ctl2 = QUATERCT_TO_WORDCT(founder_ct);
    const uint32_t founder_male_ctl2 = QUATERCT_TO_WORDCT(founder_male_ct);
    const uint32_t founder_nonmale_ctl2 = QUATERCT_TO_WORDCT(founder_nonmale_ct);
    const uintptr_t raw_tgenovec_single_variant_word_ct = round_up_pow2(founder_nonmale_ctl2 + founder_male_ctl2, kWordsPerVec);
    // round down
    uintptr_t bigstack_avail_per_thread = round_down_pow2(bigstack_left() / calc_thread_ct, kCacheline);
    // may as well require capacity for >= 256 variants per thread per pass
    if (bigstack_avail_per_thread <= thread_alloc_base + 2 * 256 * raw_tgenovec_single_variant_word_ct * sizeof(intptr_t)) {
      goto indep_pairwise_ret_NOMEM;
    }
    bigstack_avail_per_thread -= thread_alloc_base;
    uint32_t tvidx_batch_size = DIV_UP(max_load, 2);
    // tried a bunch of powers of two, this seems to be a good value
    if (tvidx_batch_size > 65536) {
      tvidx_batch_size = 65536;
    }
    // tvidx_batch_size = max_load; // temporary debugging
    if (2 * tvidx_batch_size * raw_tgenovec_single_variant_word_ct * sizeof(intptr_t) > bigstack_avail_per_thread) {
      tvidx_batch_size = bigstack_avail_per_thread / round_up_pow2(raw_tgenovec_single_variant_word_ct * 2 * sizeof(intptr_t), kCacheline);
    }
    for (uint32_t tidx = 0; tidx < calc_thread_ct; ++tidx) {
      g_genobufs[tidx] = (uintptr_t*)bigstack_alloc_raw(genobuf_alloc);
      g_occupied_window_slots[tidx] = (uintptr_t*)bigstack_alloc_raw(occupied_window_slots_alloc);
      fill_ulong_zero(window_maxl, g_occupied_window_slots[tidx]);
      g_cur_window_removed[tidx] = (uintptr_t*)bigstack_alloc_raw(cur_window_removed_alloc);
      fill_ulong_zero(1 + window_max / kBitsPerWord, g_cur_window_removed[tidx]);
      g_cur_maj_freqs[tidx] = (double*)bigstack_alloc_raw(cur_maj_freqs_alloc);
      g_removed_variants_write[tidx] = (uintptr_t*)bigstack_alloc_raw(removed_variants_write_alloc);
      fill_ulong_zero(max_loadl, g_removed_variants_write[tidx]);
      g_vstats[tidx] = (int32_t*)bigstack_alloc_raw(vstats_alloc);
      g_nonmale_vstats[tidx] = (int32_t*)bigstack_alloc_raw(vstats_alloc);
      g_winpos_to_slot_idx[tidx] = (uint32_t*)bigstack_alloc_raw(window_int32_alloc);
      g_tvidxs[tidx] = (uint32_t*)bigstack_alloc_raw(window_int32_alloc);
      g_first_unchecked_tvidx[tidx] = (uint32_t*)bigstack_alloc_raw(window_int32_alloc);
      g_raw_tgenovecs[0][tidx] = (uintptr_t*)bigstack_alloc_raw_rd(tvidx_batch_size * raw_tgenovec_single_variant_word_ct * sizeof(intptr_t));
      g_raw_tgenovecs[1][tidx] = (uintptr_t*)bigstack_alloc_raw_rd(tvidx_batch_size * raw_tgenovec_single_variant_word_ct * sizeof(intptr_t));
    }
    g_subcontig_info = subcontig_info;
    g_subcontig_thread_assignments = subcontig_thread_assignments;
    g_variant_include = variant_include;
    g_variant_allele_idxs = variant_allele_idxs;
    g_maj_alleles = maj_alleles;
    g_all_allele_freqs = allele_freqs;
    g_variant_bps = variant_bps;
    g_founder_ct = founder_ct;
    g_founder_male_ct = founder_male_ct;
    g_prune_ld_thresh = ldip->prune_last_param * (1 + kSmallEpsilon);
    g_prune_window_size = ldip->prune_window_size;
    g_window_maxl = window_maxl;
    g_window_incr = ldip->prune_window_incr;
    g_cur_batch_size = tvidx_batch_size;

    const uint32_t all_haploid = IS_SET(cip->haploid_mask, 0);
    uint32_t x_start = 0;
    uint32_t x_end = 0;
    uint32_t y_start = 0;
    uint32_t y_end = 0;
    get_xymt_start_and_end(cip, kChrOffsetX, &x_start, &x_end);
    get_xymt_start_and_end(cip, kChrOffsetY, &y_start, &y_end);
    const uint32_t x_len = x_end - x_start;
    const uint32_t y_len = y_end - y_start;
    g_x_start = x_start;
    g_x_len = x_len;
    g_y_start = y_start;
    g_y_len = y_len;
    // Main workflow:
    // 1. Set n=0, load batch 0
    
    // 2. Spawn threads processing batch n
    // 3. Increment n by 1
    // 4. Load batch n unless eof
    // 5. Join threads
    // 6. Goto step 2 unless eof
    //
    // 7. Assemble final results with copy_bitarr_range()
    uint32_t cur_tvidx_start = 0;
    uint32_t is_last_batch = 0;
    uint32_t parity = 0;
    uint32_t pct = 0;
    uint32_t next_print_tvidx_start = max_load / 100;
    LOGPRINTF("--indep-pairwise (%u compute thread%s): ", calc_thread_ct, (calc_thread_ct == 1)? "" : "s");
    fputs("0%", stdout);
    fflush(stdout);
    while (1) {
      if (!is_last_batch) {
	pgr_clear_ld_cache(simple_pgrp);
	uintptr_t** cur_raw_tgenovecs = g_raw_tgenovecs[parity];
	const uint32_t cur_tvidx_end = cur_tvidx_start + tvidx_batch_size;
	uint32_t is_x_or_y = 0;
	for (uint32_t subcontig_idx = 0; subcontig_idx < subcontig_ct; ++subcontig_idx) {
	  const uint32_t cur_thread_idx = subcontig_thread_assignments[subcontig_idx];
	  if (thread_last_subcontig[cur_thread_idx] > subcontig_idx) {
	    continue;
	  }
	  uint32_t cur_tvidx = thread_last_tvidx[cur_thread_idx];
	  if (cur_tvidx == cur_tvidx_end) {
	    continue;
	  }
	  uint32_t subcontig_start_tvidx = thread_subcontig_start_tvidx[cur_thread_idx];
	  uint32_t tvidx_end = subcontig_start_tvidx + subcontig_info[3 * subcontig_idx];
	  if (tvidx_end > cur_tvidx_end) {
	    tvidx_end = cur_tvidx_end;
	    thread_last_subcontig[cur_thread_idx] = subcontig_idx;
	  } else {
	    thread_subcontig_start_tvidx[cur_thread_idx] = tvidx_end;
	    thread_last_subcontig[cur_thread_idx] = subcontig_idx + 1;
	  }
	  uintptr_t tvidx_offset_end = tvidx_end - cur_tvidx_start;
	  uint32_t variant_uidx;
	  if (subcontig_start_tvidx == cur_tvidx) {
	    variant_uidx = subcontig_info[3 * subcontig_idx + 2];
	  } else {
	    variant_uidx = thread_last_uidx[cur_thread_idx];
	  }
	  const uint32_t is_haploid = IS_SET(cip->haploid_mask, get_variant_chr(cip, variant_uidx));
	  uint32_t is_x = ((variant_uidx - x_start) < x_len);
	  const uint32_t new_is_x_or_y = is_x || ((variant_uidx - y_start) < y_len);

	  // due to nonempty subset requirement (removed?)
	  is_x = is_x && founder_nonmale_ct;
	  if (is_x_or_y != new_is_x_or_y) {
	    is_x_or_y = new_is_x_or_y;
	    pgr_clear_ld_cache(simple_pgrp);
	  }
	  uintptr_t* cur_thread_raw_tgenovec = cur_raw_tgenovecs[cur_thread_idx];
	  for (uintptr_t tvidx_offset = cur_tvidx - cur_tvidx_start; tvidx_offset < tvidx_offset_end; ++tvidx_offset, ++variant_uidx) {
	    next_set_unsafe_ck(variant_include, &variant_uidx);
	    uintptr_t* cur_raw_tgenovec = &(cur_thread_raw_tgenovec[tvidx_offset * raw_tgenovec_single_variant_word_ct]);
	    if (!is_x_or_y) {
	      reterr = pgr_read_allele_countvec_subset_unsafe(founder_info, founder_info_cumulative_popcounts, founder_ct, variant_uidx, maj_alleles[variant_uidx], simple_pgrp, cur_raw_tgenovec);
	      if (is_haploid) {
		set_het_missing(founder_ctl2, cur_raw_tgenovec);
	      }
	    } else {
	      reterr = pgr_read_allele_countvec_subset_unsafe(nullptr, nullptr, raw_sample_ct, variant_uidx, maj_alleles[variant_uidx], simple_pgrp, tmp_genovec);
	      if (founder_male_ct) {
		copy_quaterarr_nonempty_subset(tmp_genovec, founder_male, raw_sample_ct, founder_male_ct, cur_raw_tgenovec);
		set_het_missing(founder_male_ctl2, cur_raw_tgenovec);
	      }
	      if (is_x) {
	        copy_quaterarr_nonempty_subset(tmp_genovec, founder_nonmale, raw_sample_ct, founder_nonmale_ct, &(cur_raw_tgenovec[founder_male_ctl2]));
		if (all_haploid) {
		  // don't just treat chrX identically to autosomes, since for
		  // doubled haploids we still want to give females 2x the
		  // weight of males.  I think.
		  set_het_missing(founder_nonmale_ctl2, &(cur_raw_tgenovec[founder_male_ctl2]));
		}
	      }
	    }
	    if (reterr) {
	      if (cur_tvidx_start) {
		join_threads2z(calc_thread_ct, 0, threads);
		g_cur_batch_size = 0;
		error_cleanup_threads2z(indep_pairwise_thread, calc_thread_ct, threads);
	      }
	      if (reterr != kPglRetReadFail) {
		logprint("\n");
		logerrprint("Error: Malformed .pgen file.\n");
	      }
	      goto indep_pairwise_ret_1;
	    }
	  }
	  thread_last_tvidx[cur_thread_idx] = tvidx_end;
	  thread_last_uidx[cur_thread_idx] = variant_uidx;
	}
      }
      if (cur_tvidx_start) {
	join_threads2z(calc_thread_ct, is_last_batch, threads);
	if (is_last_batch) {
	  break;
	}
	if (cur_tvidx_start >= next_print_tvidx_start) {
	  if (pct > 10) {
	    putc_unlocked('\b', stdout);
	  }
	  pct = (cur_tvidx_start * 100LLU) / max_load;
	  printf("\b\b%u%%", pct++);
	  fflush(stdout);
	  next_print_tvidx_start = (pct * ((uint64_t)max_load)) / 100;
	}
      }
      is_last_batch = (cur_tvidx_start + tvidx_batch_size >= max_load);
      if (spawn_threads2z(indep_pairwise_thread, calc_thread_ct, is_last_batch, threads)) {
	goto indep_pairwise_ret_THREAD_CREATE_FAIL;
      }
      parity = 1 - parity;
      cur_tvidx_start += tvidx_batch_size;
    }
    fill_uint_zero(calc_thread_ct, thread_subcontig_start_tvidx);
    for (uint32_t subcontig_idx = 0; subcontig_idx < subcontig_ct; ++subcontig_idx) {
      const uint32_t cur_thread_idx = subcontig_thread_assignments[subcontig_idx];
      const uintptr_t* cur_removed_variants = g_removed_variants_write[cur_thread_idx];
      const uint32_t subcontig_len = subcontig_info[3 * subcontig_idx];
      const uint32_t subcontig_idx_start = subcontig_info[3 * subcontig_idx + 1];
      copy_bitarr_range(cur_removed_variants, thread_subcontig_start_tvidx[cur_thread_idx], subcontig_idx_start, subcontig_len, removed_variants_collapsed);
      thread_subcontig_start_tvidx[cur_thread_idx] += subcontig_len;
    }
    if (pct > 10) {
      putc_unlocked('\b', stdout);
    }
    fputs("\b\b", stdout);
  }
  while (0) {
  indep_pairwise_ret_NOMEM:
    reterr = kPglRetNomem;
    break;
  indep_pairwise_ret_THREAD_CREATE_FAIL:
    reterr = kPglRetThreadCreateFail;
    break;
  indep_pairwise_ret_NOT_YET_SUPPORTED:
    reterr = kPglRetNotYetSupported;
    break;
  }
 indep_pairwise_ret_1:
  // caller will free memory
  return reterr;
}

pglerr_t indep_pairphase() {
  logerrprint("Error: --indep-pairphase is currently under development.\n");
  return kPglRetNotYetSupported;
}

pglerr_t ld_prune_subcontig_split_all(const uintptr_t* variant_include, const chr_info_t* cip, const uint32_t* variant_bps, uint32_t prune_window_size, uint32_t* window_max_ptr, uint32_t** subcontig_info_ptr, uint32_t* subcontig_ct_ptr) {
  // variant_bps must be nullptr if window size is not bp-based
  // chr0 assumed to already be removed from variant_include.
  // this will skip over chromosomes/contigs with only 1 variant.
  const uint32_t chr_ct = cip->chr_ct;
  uint32_t* subcontig_info = (uint32_t*)g_bigstack_base;
  uint32_t* subcontig_info_iter = subcontig_info;
  uint32_t* subcontig_info_limit = &(((uint32_t*)g_bigstack_end)[-3]);
  uint32_t window_max = 0;
  uint32_t variant_idx = 0;
  if (variant_bps) {
    window_max = 1;
    for (uint32_t chr_fo_idx = 0; chr_fo_idx < chr_ct; ++chr_fo_idx) {
      const uint32_t chr_end = cip->chr_fo_vidx_start[chr_fo_idx + 1];
      uint32_t variant_uidx = next_set(variant_include, cip->chr_fo_vidx_start[chr_fo_idx], chr_end);
      const uint32_t chr_variant_ct = popcount_bit_idx(variant_include, variant_uidx, chr_end);
      const uint32_t variant_idx_end = variant_idx + chr_variant_ct;
      if (chr_variant_ct > 1) {
	uint32_t subcontig_uidx_first = variant_uidx;
	uint32_t subcontig_idx_first = variant_idx;
	uint32_t window_idx_first = variant_idx;
	uint32_t window_uidx_first = variant_uidx;
	uint32_t window_pos_first = variant_bps[variant_uidx];
	uint32_t prev_pos = window_pos_first;
	++variant_idx;
	do {
	  ++variant_uidx;
	  next_set_unsafe_ck(variant_include, &variant_uidx);
	  uint32_t variant_bp_thresh = variant_bps[variant_uidx];
	  if (variant_bp_thresh < prune_window_size) {
	    prev_pos = variant_bp_thresh;
	    variant_bp_thresh = 0;
	  } else {
	    if (variant_bp_thresh - prune_window_size > prev_pos) {
	      if (variant_idx > subcontig_idx_first + 1) {
		if (subcontig_info_iter > subcontig_info_limit) {
		  return kPglRetNomem;
		}
		*subcontig_info_iter++ = variant_idx - subcontig_idx_first;
		*subcontig_info_iter++ = subcontig_idx_first;
		*subcontig_info_iter++ = subcontig_uidx_first;
	      }
	      subcontig_uidx_first = variant_uidx;
	      subcontig_idx_first = variant_idx;
	    }
	    prev_pos = variant_bp_thresh;
	    variant_bp_thresh -= prune_window_size;
	  }
	  if (variant_bp_thresh > window_pos_first) {
	    do {
	      ++window_uidx_first;
	      next_set_unsafe_ck(variant_include, &window_uidx_first);
	      window_pos_first = variant_bps[window_uidx_first];
	      ++window_idx_first;
	    } while (variant_bp_thresh > window_pos_first);
	  } else if (variant_idx - window_idx_first == window_max) {
	    ++window_max;
	  }
	} while (++variant_idx < variant_idx_end);
	if (variant_idx > subcontig_idx_first + 1) {
	  if (subcontig_info_iter > subcontig_info_limit) {
	    return kPglRetNomem;
	  }
	  *subcontig_info_iter++ = variant_idx - subcontig_idx_first;
	  *subcontig_info_iter++ = subcontig_idx_first;
	  *subcontig_info_iter++ = subcontig_uidx_first;
	}
      }
      variant_idx = variant_idx_end;
    }
  } else {
    for (uint32_t chr_fo_idx = 0; chr_fo_idx < chr_ct; ++chr_fo_idx) {
      const uint32_t chr_end = cip->chr_fo_vidx_start[chr_fo_idx + 1];
      const uint32_t first_variant_uidx = next_set(variant_include, cip->chr_fo_vidx_start[chr_fo_idx], chr_end);
      const uint32_t chr_variant_ct = popcount_bit_idx(variant_include, first_variant_uidx, chr_end);
      if (chr_variant_ct > 1) {
	if (subcontig_info_iter > subcontig_info_limit) {
	  return kPglRetNomem;
	}
	*subcontig_info_iter++ = chr_variant_ct;
	*subcontig_info_iter++ = variant_idx;
	*subcontig_info_iter++ = first_variant_uidx;
	if (window_max < prune_window_size) {
	  if (chr_variant_ct > window_max) {
	    window_max = chr_variant_ct;
	  }
	}
      }
      variant_idx += chr_variant_ct;
    }
    if (window_max > prune_window_size) {
      window_max = prune_window_size;
    }
  }
  *subcontig_ct_ptr = ((uintptr_t)(subcontig_info_iter - subcontig_info)) / 3;
  *subcontig_info_ptr = subcontig_info;
  bigstack_finalize_ui(subcontig_info, (*subcontig_ct_ptr) * 3);
  *window_max_ptr = window_max;
  return kPglRetSuccess;
}

// next several functions (including load_balance()) will probably move to
// plink2_common
void minheap64_replace_root(uint32_t heap_size, uint64_t new_root, uint64_t* minheap64_preroot) {
  uint32_t cur_pos = 1;
  while (1) {
    uint32_t child_pos = cur_pos * 2;
    if (child_pos >= heap_size) {
      if (child_pos == heap_size) {
	// special case: one child at end of heap
	const uint64_t child_val = minheap64_preroot[child_pos];
	if (new_root > child_val) {
	  minheap64_preroot[cur_pos] = child_val;
	  cur_pos = child_pos;
	}
      }
      break;
    }
    uint64_t min_child_val = minheap64_preroot[child_pos];
    const uint64_t child_val2 = minheap64_preroot[child_pos + 1];
    if (child_val2 < min_child_val) {
      min_child_val = child_val2;
      ++child_pos;
    }
    if (new_root <= min_child_val) {
      break;
    }
    minheap64_preroot[cur_pos] = min_child_val;
    cur_pos = child_pos;
  }
  minheap64_preroot[cur_pos] = new_root;
}

/*
void minheap64_delete_root(uint64_t* minheap64_preroot, uint32_t* heap_size_ptr) {
  uint32_t heap_size = *heap_size_ptr;
  const uint64_t new_root = minheap64_preroot[heap_size];
  minheap64_replace_root(--heap_size, new_root, minheap64_preroot);
  *heap_size_ptr = heap_size;
}
*/

void minheap64_insert(uint64_t new_entry, uint64_t* minheap64_preroot, uint32_t* heap_size_ptr) {
  // assumes minheap64_preroot[0] == 0
  const uint32_t heap_size = 1 + (*heap_size_ptr);
  *heap_size_ptr = heap_size;
  uint32_t cur_pos = heap_size;
  while (1) {
    uint32_t parent_pos = cur_pos / 2;
    const uint64_t parent_val = minheap64_preroot[parent_pos];
    if (new_entry >= parent_val) {
      minheap64_preroot[cur_pos] = new_entry;
      return;
    }
    minheap64_preroot[cur_pos] = parent_val;
    cur_pos = parent_pos;
  }
}

// This is intended to split a relatively small number of contig-like regions
// between threads, but it shouldn't totally fall apart if there are millions
// of regions and hundreds of threads.
// Based on the Longest Processing Time algorithm, but with a few adjustments:
// * max(largest_weight, round_up(total_weight / thread_ct)) is noted, and the
//   first 8 * thread_ct thread assignments are based on best-fit to that
//   capacity.  The constant 8 is chosen to be enough to beat basic LPT's
//   4/3 - 1/{3m} approximation factor by a relevant margin, while keeping
//   runtime under control.  (In the event that there is no fit, the capacity
//   is increased.)
// * If any task assignments remain, we use LPT, but attempt to use a lower
//   number of threads; we only add another thread if we would otherwise have
//   to increase max_load.
pglerr_t load_balance(const uint32_t* task_weights, uint32_t task_ct, uint32_t* thread_ct_ptr, uint32_t* thread_assignments, uint32_t* max_load_ptr) {
  // max_load assumed to be initialized to zero
  assert(task_ct);
  const uint32_t orig_thread_ct = *thread_ct_ptr;
  if (orig_thread_ct == 1) {
    fill_uint_zero(task_ct, thread_assignments);
    // replace this with an acc_uint32 call?
    uint32_t max_load = task_weights[0];
    for (uint32_t task_idx = 1; task_idx < task_ct; ++task_idx) {
      max_load += task_weights[task_idx];
    }
    *max_load_ptr = max_load;
    return kPglRetSuccess;
  }
  assert(task_ct >= orig_thread_ct);
  uint64_t* sorted_tagged_weights;
  uint64_t* minheap64_preroot;
  if (bigstack_alloc_ull(task_ct, &sorted_tagged_weights) ||
      bigstack_alloc_ull(orig_thread_ct + 2, &minheap64_preroot)) {
    return kPglRetNomem;
  }
  minheap64_preroot[0] = 0;
  uint64_t* minheap64 = &(minheap64_preroot[1]);
  uint32_t total_weight = 0;
  for (uintptr_t task_idx = 0; task_idx < task_ct; ++task_idx) {
    const uintptr_t cur_weight = task_weights[task_idx];
    total_weight += cur_weight;
    sorted_tagged_weights[task_idx] = (((uint64_t)cur_weight) << 32) + (uint64_t)task_idx;
  }
  uint64_t* sorted_tagged_weights_end = &(sorted_tagged_weights[task_ct]);
#ifdef __cplusplus
  // could try std::nth_element if this is ever a bottleneck
  std::sort(sorted_tagged_weights, sorted_tagged_weights_end, std::greater<uint64_t>());
#else
  qsort(sorted_tagged_weights, task_ct, sizeof(int64_t), uint64cmp_decr);
#endif
  const uint64_t largest_tagged_weight = sorted_tagged_weights[0];
  uint32_t initial_max_load = largest_tagged_weight >> 32;
  uint32_t thread_ct = 1 + (total_weight - 1) / initial_max_load;
  if (thread_ct > orig_thread_ct) {
    thread_ct = orig_thread_ct;
    initial_max_load = 1 + (total_weight - 1) / orig_thread_ct;
  }
  
  for (uintptr_t thread_idx = 1; thread_idx < thread_ct; ++thread_idx) {
    minheap64[thread_idx - 1] = thread_ct - thread_idx;
  }
  minheap64[thread_ct - 1] = largest_tagged_weight & 0xffffffff00000000LLU;
  for (uint32_t thread_idx = thread_ct; thread_idx <= orig_thread_ct; ++thread_idx) {
    minheap64[thread_idx] = 0xffffffffffffffffLLU;
  }
  thread_assignments[(uint32_t)largest_tagged_weight] = 0;
  uint64_t max_load_shifted = (((uint64_t)initial_max_load) << 32) | 0xffffffffLLU;
  uint64_t* best_fit_end = sorted_tagged_weights_end;
  if (task_ct > 8 * orig_thread_ct) {
    // stop best-fit here
    best_fit_end = &(sorted_tagged_weights[8 * orig_thread_ct]);
  }
  uint64_t* sorted_tagged_weights_iter = &(sorted_tagged_weights[1]);
  while (sorted_tagged_weights_iter != best_fit_end) {
    // maintain minheap64 as fully sorted list
    uint64_t cur_tagged_weight = *sorted_tagged_weights_iter++;
    const uint32_t task_idx = (uint32_t)cur_tagged_weight;
    cur_tagged_weight &= 0xffffffff00000000LLU;
    const uintptr_t idxp1 = uint64arr_greater_than(minheap64, thread_ct, max_load_shifted - cur_tagged_weight);
    if (idxp1) {
      uintptr_t idx = idxp1 - 1;
      const uint64_t new_entry = minheap64[idx] + cur_tagged_weight;
      while (1) {
	const uint64_t next_entry = minheap64[idx + 1];
	if (new_entry < next_entry) {
	  break;
	}
	minheap64[idx++] = next_entry;
      }
      thread_assignments[task_idx] = (uint32_t)new_entry;
      minheap64[idx] = new_entry;
    } else if (thread_ct < orig_thread_ct) {
      const uint64_t new_entry = cur_tagged_weight + thread_ct;
      const uintptr_t insert_pt = uint64arr_greater_than(minheap64, thread_ct, new_entry);
      for (uintptr_t thread_idx = thread_ct; thread_idx > insert_pt; --thread_idx) {
	minheap64[thread_idx] = minheap64[thread_idx - 1];
      }
      minheap64[insert_pt] = new_entry;
      thread_assignments[task_idx] = thread_ct++;
    } else {
      // move lowest entry to end of list, shift everything else down
      const uint64_t new_entry = minheap64[0] + cur_tagged_weight;
      for (uint32_t thread_idx = 1; thread_idx < thread_ct; ++thread_idx) {
	minheap64[thread_idx - 1] = minheap64[thread_idx];
      }
      minheap64[thread_ct - 1] = new_entry;
      max_load_shifted = new_entry | 0xffffffffLLU;
      thread_assignments[task_idx] = (uint32_t)new_entry;
    }
  }
  if (best_fit_end != sorted_tagged_weights_end) {
    do {
      const uint64_t cur_heaproot = minheap64[0];
      uint64_t cur_tagged_weight = *sorted_tagged_weights_iter++;
      const uint32_t task_idx = (uint32_t)cur_tagged_weight;
      uint32_t cur_thread = (uint32_t)cur_heaproot;
      cur_tagged_weight &= 0xffffffff00000000LLU;
      uint64_t new_entry = cur_heaproot + cur_tagged_weight;
      if (new_entry > max_load_shifted) {
	if (thread_ct < orig_thread_ct) {
	  thread_assignments[task_idx] = thread_ct;
	  minheap64_insert(cur_tagged_weight + thread_ct, minheap64_preroot, &thread_ct);
	  continue;
	} else {
	  max_load_shifted = new_entry | 0xffffffffLLU;
	}
      }
      thread_assignments[task_idx] = cur_thread;
      minheap64_replace_root(thread_ct, new_entry, minheap64_preroot);
    } while (sorted_tagged_weights_iter != sorted_tagged_weights_end);
  }  
  bigstack_reset(sorted_tagged_weights);
  *thread_ct_ptr = thread_ct;
  *max_load_ptr = max_load_shifted >> 32;
  return kPglRetSuccess;
}

pglerr_t ld_prune_write(const uintptr_t* variant_include, const uintptr_t* removed_variants_collapsed, char** variant_ids, uint32_t variant_ct, char* outname, char* outname_end) {
  FILE* outfile = nullptr;
  pglerr_t reterr = kPglRetSuccess;
  {
    fputs("Writing...", stdout);
    fflush(stdout);
    strcpy(outname_end, ".prune.in");
    if (fopen_checked(outname, FOPEN_WB, &outfile)) {
      goto ld_prune_write_ret_OPEN_FAIL;
    }
    char* write_iter = g_textbuf;
    char* textbuf_flush = &(write_iter[kMaxMediumLine]);
    uint32_t variant_uidx = 0;
    for (uint32_t variant_idx = 0; variant_idx < variant_ct; ++variant_idx, ++variant_uidx) {
      next_set_unsafe_ck(variant_include, &variant_uidx);
      if (is_set(removed_variants_collapsed, variant_idx)) {
	continue;
      }
      write_iter = strcpya(write_iter, variant_ids[variant_uidx]);
      append_binary_eoln(&write_iter);
      if (fwrite_ck(textbuf_flush, outfile, &write_iter)) {
	goto ld_prune_write_ret_WRITE_FAIL;
      }
    }
    if (fclose_flush_null(textbuf_flush, write_iter, &outfile)) {
      goto ld_prune_write_ret_WRITE_FAIL;
    }

    strcpy(&(outname_end[7]), "out");
    if (fopen_checked(outname, FOPEN_WB, &outfile)) {
      goto ld_prune_write_ret_OPEN_FAIL;
    }
    write_iter = g_textbuf;
    variant_uidx = 0;
    for (uint32_t variant_idx = 0; variant_idx < variant_ct; ++variant_idx, ++variant_uidx) {
      next_set_unsafe_ck(variant_include, &variant_uidx);
      if (!is_set(removed_variants_collapsed, variant_idx)) {
	continue;
      }
      write_iter = strcpya(write_iter, variant_ids[variant_uidx]);
      append_binary_eoln(&write_iter);
      if (fwrite_ck(textbuf_flush, outfile, &write_iter)) {
	goto ld_prune_write_ret_WRITE_FAIL;
      }
    }
    if (fclose_flush_null(textbuf_flush, write_iter, &outfile)) {
      goto ld_prune_write_ret_WRITE_FAIL;
    }
    *outname_end = '\0';
    putc_unlocked('\r', stdout);
    LOGPRINTFWW("Variant lists written to %s.prune.in and %s.prune.out .\n", outname, outname);
  }
  while (0) {
  ld_prune_write_ret_OPEN_FAIL:
    reterr = kPglRetOpenFail;
    break;
  ld_prune_write_ret_WRITE_FAIL:
    reterr = kPglRetWriteFail;
    break;
  }
  fclose_cond(outfile);
  return reterr;
}

pglerr_t ld_prune(const uintptr_t* orig_variant_include, const chr_info_t* cip, const uint32_t* variant_bps, char** variant_ids, const uintptr_t* variant_allele_idxs, const alt_allele_ct_t* maj_alleles, const double* allele_freqs, const uintptr_t* founder_info, const uintptr_t* sex_male, const ld_info_t* ldip, uint32_t raw_variant_ct, uint32_t variant_ct, uint32_t raw_sample_ct, uint32_t founder_ct, uint32_t max_thread_ct, pgen_reader_t* simple_pgrp, char* outname, char* outname_end) {
  // common initialization between --indep-pairwise and --indep-pairphase
  unsigned char* bigstack_mark = g_bigstack_base;
  unsigned char* bigstack_end_mark = g_bigstack_end;
  pglerr_t reterr = kPglRetSuccess;
  {
    const uint32_t is_pairphase = (ldip->prune_modifier / kfLdPrunePairphase) & 1;
    if (founder_ct < 2) {
      LOGERRPRINTF("Warning: Skipping --indep-pair%s since there are less than two founders.\n(--make-founders may come in handy here.)\n", is_pairphase? "phase" : "wise");
      goto ld_prune_ret_1;
    }
    uint32_t skipped_variant_ct = 0;
    if (is_set(cip->chr_mask, 0)) {
      skipped_variant_ct = count_chr_variants_unsafe(orig_variant_include, cip, 0);
    }
    const uint32_t chr_code_end = cip->max_code + 1 + cip->name_ct;
    if (cip->zero_extra_chrs) {
      for (uint32_t chr_idx = cip->max_code + 1; chr_idx < chr_code_end; ++chr_idx) {
	if (is_set(cip->chr_mask, chr_idx)) {
	  skipped_variant_ct += count_chr_variants_unsafe(orig_variant_include, cip, cip->chr_idx_to_foidx[chr_idx]);
	}
      }
    }
    const uint32_t raw_variant_ctl = BITCT_TO_WORDCT(raw_variant_ct);
    const uintptr_t* variant_include;
    if (skipped_variant_ct) {
      uintptr_t* new_variant_include;
      if (bigstack_alloc_ul(raw_variant_ctl, &new_variant_include)) {
	goto ld_prune_ret_NOMEM;
      }
      memcpy(new_variant_include, orig_variant_include, raw_variant_ctl * sizeof(intptr_t));
      if (is_set(cip->chr_mask, 0)) {
	const uint32_t chr_fo_idx = cip->chr_idx_to_foidx[0];
	const uint32_t start_uidx = cip->chr_fo_vidx_start[chr_fo_idx];
	clear_bits_nz(start_uidx, cip->chr_fo_vidx_start[chr_fo_idx + 1], new_variant_include);
      }
      if (cip->zero_extra_chrs) {
        for (uint32_t chr_idx = cip->max_code + 1; chr_idx < chr_code_end; ++chr_idx) {
	  const uint32_t chr_fo_idx = cip->chr_idx_to_foidx[chr_idx];
	  const uint32_t start_uidx = cip->chr_fo_vidx_start[chr_fo_idx];
	  clear_bits_nz(start_uidx, cip->chr_fo_vidx_start[chr_fo_idx + 1], new_variant_include);
	}
      }
      variant_include = new_variant_include;
      variant_ct -= skipped_variant_ct;
      LOGPRINTF("--indep-pair%s: Ignoring %u chromosome 0 variant%s.\n", is_pairphase? "phase" : "wise", skipped_variant_ct, (skipped_variant_ct == 1)? "" : "s");
    } else {
      variant_include = orig_variant_include;
    }

    if (!(ldip->prune_modifier & kfLdPruneWindowBp)) {
      variant_bps = nullptr;
    }
    const uint32_t prune_window_size = ldip->prune_window_size;
    uint32_t* subcontig_info;
    uint32_t window_max;
    uint32_t subcontig_ct;
    if (ld_prune_subcontig_split_all(variant_include, cip, variant_bps, prune_window_size, &window_max, &subcontig_info, &subcontig_ct)) {
      return kPglRetNomem;
    }
    if (!subcontig_ct) {
      LOGERRPRINTF("Warning: Skipping --indep-pair%s since there are no pairs of variants to\nprocess.\n", is_pairphase? "phase" : "wise");
      goto ld_prune_ret_1;
    }
    if (max_thread_ct > 2) {
      --max_thread_ct;
    }
    if (max_thread_ct > subcontig_ct) {
      max_thread_ct = subcontig_ct;
    }
    const uint32_t raw_sample_ctl = BITCT_TO_WORDCT(raw_sample_ct);
    const uint32_t variant_ctl = BITCT_TO_WORDCT(variant_ct);
    const uint32_t founder_male_ct = popcount_longs_intersect(founder_info, sex_male, raw_sample_ctl);
    const uint32_t founder_ctl = BITCT_TO_WORDCT(founder_ct);
    uint32_t* founder_info_cumulative_popcounts;
    uintptr_t* founder_nonmale_collapsed;
    uintptr_t* founder_male_collapsed;
    uintptr_t* removed_variants_collapsed;
    uint32_t* subcontig_thread_assignments;
    if (bigstack_alloc_ui(raw_sample_ctl, &founder_info_cumulative_popcounts) ||
	bigstack_alloc_ul(founder_ctl, &founder_nonmale_collapsed) ||
	bigstack_alloc_ul(founder_ctl, &founder_male_collapsed) ||
	bigstack_calloc_ul(variant_ctl, &removed_variants_collapsed) ||
	bigstack_alloc_ui(subcontig_ct, &subcontig_thread_assignments)) {
      goto ld_prune_ret_NOMEM;
    }
    fill_cumulative_popcounts(founder_info, raw_sample_ctl, founder_info_cumulative_popcounts);
    copy_bitarr_subset(sex_male, founder_info, founder_ct, founder_male_collapsed);
    bitarr_invert_copy(founder_male_collapsed, founder_ct, founder_nonmale_collapsed);
    uint32_t* subcontig_weights;
    if (bigstack_end_alloc_ui(subcontig_ct, &subcontig_weights)) {
      goto ld_prune_ret_NOMEM;
    }

    // initial window_max-based memory requirement estimate
    if (is_pairphase) {
      // todo
    } else {
      const uintptr_t entire_variant_buf_word_ct = 3 * (BITCT_TO_ALIGNED_WORDCT(founder_ct - founder_male_ct) + BITCT_TO_ALIGNED_WORDCT(founder_male_ct));
      // reserve ~1/2 of space for main variant data buffer,
      //   removed_variant_write
      // everything else:
      //   genobufs: thread_ct * window_max * entire_variant_buf_word_ct * word
      //   occupied_window_slots: thread_ct * window_maxl * word
      //   cur_window_removed: thread_ct * (1 + window_max / kBitsPerWord) *
      //     word
      //   (ignore removed_variant_write)
      //   maj_freqs: thread_ct * window_max * 8
      //   vstats, nonmale_vstats: thread_ct * window_max * 3 * int32
      //   winpos_to_slot_idx, tvidxs, first_unchecked_vidx: window_max * 3 *
      //     int32
      uintptr_t per_thread_alloc = round_up_pow2(window_max * entire_variant_buf_word_ct * sizeof(intptr_t), kCacheline) + 2 * round_up_pow2((1 + window_max / kBitsPerWord) * sizeof(intptr_t), kCacheline) + round_up_pow2(window_max * sizeof(double), kCacheline) + 2 * round_up_pow2(window_max * (3 * sizeof(int32_t)), kCacheline) + 3 * round_up_pow2(window_max * sizeof(int32_t), kCacheline);
      uintptr_t bigstack_left2 = bigstack_left();
      if (per_thread_alloc * max_thread_ct > bigstack_left2) {
	if (per_thread_alloc > bigstack_left2) {
	  goto ld_prune_ret_NOMEM;
	}
	max_thread_ct = bigstack_left2 / per_thread_alloc;
      }
    }

    
    for (uint32_t subcontig_idx = 0; subcontig_idx < subcontig_ct; ++subcontig_idx) {
      // todo: adjust chrX weights upward, and chrY downward
      subcontig_weights[subcontig_idx] = subcontig_info[3 * subcontig_idx];
      // printf("%u %u %u\n", subcontig_info[3 * subcontig_idx], subcontig_info[3 * subcontig_idx + 1], subcontig_info[3 * subcontig_idx + 2]);
    }
    uint32_t max_load = 0;
    if (load_balance(subcontig_weights, subcontig_ct, &max_thread_ct, subcontig_thread_assignments, &max_load)) {
      goto ld_prune_ret_NOMEM;
    }
    bigstack_end_reset(bigstack_end_mark);
    
    if (is_pairphase) {
      reterr = indep_pairphase();
    } else {
      reterr = indep_pairwise(variant_include, cip, variant_bps, variant_allele_idxs, maj_alleles, allele_freqs, founder_info, founder_info_cumulative_popcounts, founder_nonmale_collapsed, founder_male_collapsed, ldip, subcontig_info, subcontig_thread_assignments, raw_sample_ct, founder_ct, founder_male_ct, subcontig_ct, window_max, max_thread_ct, max_load, simple_pgrp, removed_variants_collapsed);
    }
    if (reterr) {
      goto ld_prune_ret_1;
    }
    const uint32_t removed_ct = popcount_longs(removed_variants_collapsed, variant_ctl);
    LOGPRINTF("%u/%u variants removed.\n", removed_ct, variant_ct);
    reterr = ld_prune_write(variant_include, removed_variants_collapsed, variant_ids, variant_ct, outname, outname_end);
    if (reterr) {
      goto ld_prune_ret_1;
    }
  }
  while (0) {
  ld_prune_ret_NOMEM:
    reterr = kPglRetNomem;
    break;
  }
 ld_prune_ret_1:
  bigstack_double_reset(bigstack_mark, bigstack_end_mark);
  return reterr;
}


void genoarr_split_12nm(const uintptr_t* __restrict genoarr, uint32_t sample_ct, uintptr_t* __restrict one_bitarr, uintptr_t* __restrict two_bitarr, uintptr_t* __restrict nm_bitarr) {
  // ok if trailing bits of genoarr are not zeroed out
  // trailing bits of {one,two,nm}_bitarr are zeroed out
  const uint32_t sample_ctl2 = QUATERCT_TO_WORDCT(sample_ct);
  halfword_t* one_bitarr_alias = (halfword_t*)one_bitarr;
  halfword_t* two_bitarr_alias = (halfword_t*)two_bitarr;
  halfword_t* nm_bitarr_alias = (halfword_t*)nm_bitarr;
  for (uint32_t widx = 0; widx < sample_ctl2; ++widx) {
    const uintptr_t cur_geno_word = genoarr[widx];
    const uint32_t low_halfword = pack_word_to_halfword(cur_geno_word & kMask5555);
    const uint32_t high_halfword = pack_word_to_halfword((cur_geno_word >> 1) & kMask5555);
    one_bitarr_alias[widx] = low_halfword & (~high_halfword);
    two_bitarr_alias[widx] = high_halfword & (~low_halfword);
    nm_bitarr_alias[widx] = ~(low_halfword & high_halfword);
  }

  const uint32_t sample_ct_rem = sample_ct % kBitsPerWordD2;
  if (sample_ct_rem) {
    const halfword_t trailing_mask = (1U << sample_ct_rem) - 1;
    one_bitarr_alias[sample_ctl2 - 1] &= trailing_mask;
    two_bitarr_alias[sample_ctl2 - 1] &= trailing_mask;
    nm_bitarr_alias[sample_ctl2 - 1] &= trailing_mask;
  }
  if (sample_ctl2 % 2) {
    one_bitarr_alias[sample_ctl2] = 0;
    two_bitarr_alias[sample_ctl2] = 0;
    nm_bitarr_alias[sample_ctl2] = 0;
  }
}

uint32_t geno_bitvec_sum_main(const vul_t* one_vvec, const vul_t* two_vvec, uint32_t vec_ct) {
  // Analog of popcount_vecs.
  const vul_t m1 = VCONST_UL(kMask5555);
  const vul_t m2 = VCONST_UL(kMask3333);
  const vul_t m4 = VCONST_UL(kMask0F0F);
  const vul_t m8 = VCONST_UL(kMask00FF);
  const vul_t* one_vvec_iter = one_vvec;
  const vul_t* two_vvec_iter = two_vvec;
  uint32_t tot = 0;
  while (1) {
    univec_t acc;
    acc.vi = vul_setzero();
    const vul_t* one_vvec_stop;
    if (vec_ct < 15) {
      if (!vec_ct) {
        return tot;
      }
      one_vvec_stop = &(one_vvec_iter[vec_ct]);
      vec_ct = 0;
    } else {
      one_vvec_stop = &(one_vvec_iter[15]);
      vec_ct -= 15;
    }
    do {
      vul_t one_count = *one_vvec_iter++;
      vul_t two_count = *two_vvec_iter++;
      one_count = one_count - (vul_rshift(one_count, 1) & m1);
      two_count = two_count - (vul_rshift(two_count, 1) & m1);
      one_count = (one_count & m2) + (vul_rshift(one_count, 2) & m2);
      two_count = (two_count & m2) + (vul_rshift(two_count, 2) & m2);
      // one_count and two_count now contain 4-bit partial bitcounts, each in
      // the range 0..4.  finally enough room to compute
      //   2 * two_count + one_count
      // in parallel and add it to the accumulator.
      one_count = vul_lshift(two_count, 1) + one_count;
      acc.vi = acc.vi + (one_count & m4) + (vul_rshift(one_count, 4) & m4);
    } while (one_vvec_iter < one_vvec_stop);
    acc.vi = (acc.vi & m8) + (vul_rshift(acc.vi, 8) & m8);
    tot += univec_hsum_16bit(acc);
  }
}

uint32_t geno_bitvec_sum(const uintptr_t* one_bitvec, const uintptr_t* two_bitvec, uint32_t word_ct) {
  //   popcount(one_bitvec) + 2 * popcount(two_bitvec)
  uint32_t tot = 0;
#ifdef __LP64__
  if (word_ct >= kWordsPerVec) {
#endif
    const uint32_t remainder = word_ct % kWordsPerVec;
    const uint32_t main_block_word_ct = word_ct - remainder;
    word_ct = remainder;
    tot = geno_bitvec_sum_main((const vul_t*)one_bitvec, (const vul_t*)two_bitvec, main_block_word_ct / kWordsPerVec);
#ifdef __LP64__
    one_bitvec = &(one_bitvec[main_block_word_ct]);
    two_bitvec = &(two_bitvec[main_block_word_ct]);
  }
  for (uint32_t trailing_word_idx = 0; trailing_word_idx < word_ct; ++trailing_word_idx) {
    tot += popcount_long(one_bitvec[trailing_word_idx]) + 2 * popcount_long(two_bitvec[trailing_word_idx]);
  }
#endif
  return tot;
}

uint32_t geno_bitvec_sum_subset_main(const vul_t* subset_vvec, const vul_t* one_vvec, const vul_t* two_vvec, uint32_t vec_ct) {
  // Same as geno_bitvec_sum_main(), just with an additional mask.
  const vul_t m1 = VCONST_UL(kMask5555);
  const vul_t m2 = VCONST_UL(kMask3333);
  const vul_t m4 = VCONST_UL(kMask0F0F);
  const vul_t m8 = VCONST_UL(kMask00FF);
  const vul_t* subset_vvec_iter = subset_vvec;
  const vul_t* one_vvec_iter = one_vvec;
  const vul_t* two_vvec_iter = two_vvec;
  uint32_t tot = 0;
  while (1) {
    univec_t acc;
    acc.vi = vul_setzero();
    const vul_t* subset_vvec_stop;
    if (vec_ct < 15) {
      if (!vec_ct) {
        return tot;
      }
      subset_vvec_stop = &(subset_vvec_iter[vec_ct]);
      vec_ct = 0;
    } else {
      subset_vvec_stop = &(subset_vvec_iter[15]);
      vec_ct -= 15;
    }
    do {
      vul_t maskv = *subset_vvec_iter++;
      vul_t one_count = (*one_vvec_iter++) & maskv;
      vul_t two_count = (*two_vvec_iter++) & maskv;
      one_count = one_count - (vul_rshift(one_count, 1) & m1);
      two_count = two_count - (vul_rshift(two_count, 1) & m1);
      one_count = (one_count & m2) + (vul_rshift(one_count, 2) & m2);
      two_count = (two_count & m2) + (vul_rshift(two_count, 2) & m2);
      one_count = vul_lshift(two_count, 1) + one_count;
      acc.vi = acc.vi + (one_count & m4) + (vul_rshift(one_count, 4) & m4);
    } while (subset_vvec_iter < subset_vvec_stop);
    acc.vi = (acc.vi & m8) + (vul_rshift(acc.vi, 8) & m8);
    tot += univec_hsum_16bit(acc);
  }
}

uint32_t geno_bitvec_sum_subset(const uintptr_t* subset_mask, const uintptr_t* one_bitvec, const uintptr_t* two_bitvec, uint32_t word_ct) {
  //   popcount(subset_mask & one_bitvec)
  // + 2 * popcount(subset_mask & two_bitvec)
  uint32_t tot = 0;
#ifdef __LP64__
  if (word_ct >= kWordsPerVec) {
#endif
    const uint32_t remainder = word_ct % kWordsPerVec;
    const uint32_t main_block_word_ct = word_ct - remainder;
    word_ct = remainder;
    tot = geno_bitvec_sum_subset_main((const vul_t*)subset_mask, (const vul_t*)one_bitvec, (const vul_t*)two_bitvec, main_block_word_ct / kWordsPerVec);
#ifdef __LP64__
    subset_mask = &(subset_mask[main_block_word_ct]);
    one_bitvec = &(one_bitvec[main_block_word_ct]);
    two_bitvec = &(two_bitvec[main_block_word_ct]);
  }
  for (uint32_t trailing_word_idx = 0; trailing_word_idx < word_ct; ++trailing_word_idx) {
    const uintptr_t subset_word = subset_mask[trailing_word_idx];
    tot += popcount_long(subset_word & one_bitvec[trailing_word_idx]) + 2 * popcount_long(subset_word & two_bitvec[trailing_word_idx]);
  }
#endif
  return tot;
}

// phased-hardcall r^2 computation:
//   definitely-known part of dot product is
//     popcount((one_bitvec0 & two_bitvec1) | (two_bitvec0 & one_bitvec1))
//   + popcount(two_bitvec0 & two_bitvec1) * 2
//   + possible phased-het-het term
//   possibly-unknown part is
//     popcount(one_bitvec0 & one_bitvec1) - phased-het-het count
//   when nm_bitvec0 isn't all-ones, also necessary to compute
//     popcount(nm_bitvec0 & one_bitvec1)
//   + popcount(nm_bitvec0 & two_bitvec1) * 2
//   analogous statement is true for nm_bitvec1
//   if both are incomplete, also need popcount of intersection (compute this
//     first and skip rest of computation when zero).
//
// for possibly-unknown part, --ld reports all solutions when multiple
// solutions exist, everything else uses EM solution

void geno_bitvec_phased_dotprod_main(const vul_t* one_vvec0, const vul_t* two_vvec0, const vul_t* one_vvec1, const vul_t* two_vvec1, uint32_t vec_ct, uint32_t* __restrict known_dotprod_ptr, uint32_t* __restrict hethet_ct_ptr) {
  const vul_t m1 = VCONST_UL(kMask5555);
  const vul_t m2 = VCONST_UL(kMask3333);
  const vul_t m4 = VCONST_UL(kMask0F0F);
  const vul_t* one_vvec0_iter = one_vvec0;
  const vul_t* two_vvec0_iter = two_vvec0;
  const vul_t* one_vvec1_iter = one_vvec1;
  const vul_t* two_vvec1_iter = two_vvec1;
  uint32_t known_dotprod = 0;
  uint32_t hethet_ct = 0;
  while (1) {
    univec_t dotprod_acc;
    univec_t hethet_acc;
    dotprod_acc.vi = vul_setzero();
    hethet_acc.vi = vul_setzero();
    const vul_t* one_vvec0_stop;
    if (vec_ct < 15) {
      if (!vec_ct) {
        *known_dotprod_ptr = known_dotprod;
        *hethet_ct_ptr = hethet_ct;
        return;
      }
      one_vvec0_stop = &(one_vvec0_iter[vec_ct]);
      vec_ct = 0;
    } else {
      one_vvec0_stop = &(one_vvec0_iter[15]);
      vec_ct -= 15;
    }
    do {
      vul_t one_vword0 = *one_vvec0_iter++;
      vul_t two_vword0 = *two_vvec0_iter++;
      vul_t one_vword1 = *one_vvec1_iter++;
      vul_t two_vword1 = *two_vvec1_iter++;

      vul_t dotprod_1x_bits = (one_vword0 & two_vword1) | (one_vword1 & two_vword0);
      vul_t dotprod_2x_bits = two_vword0 & two_vword1;
      vul_t hethet_bits = one_vword0 & one_vword1;
      dotprod_1x_bits = dotprod_1x_bits - (vul_rshift(dotprod_1x_bits, 1) & m1);
      dotprod_2x_bits = dotprod_2x_bits - (vul_rshift(dotprod_2x_bits, 1) & m1);
      hethet_bits = hethet_bits - (vul_rshift(hethet_bits, 1) & m1);
      dotprod_1x_bits = (dotprod_1x_bits & m2) + (vul_rshift(dotprod_1x_bits, 2) & m2);
      dotprod_2x_bits = (dotprod_2x_bits & m2) + (vul_rshift(dotprod_2x_bits, 2) & m2);
      hethet_bits = (hethet_bits & m2) + (vul_rshift(hethet_bits, 2) & m2);

      // we now have 4-bit partial bitcounts in the range 0..4.  finally have
      // enough room to compute 2 * dotprod_2x_bits + dotprod_1x_bits.      
      dotprod_1x_bits = vul_lshift(dotprod_2x_bits, 1) + dotprod_1x_bits;
      hethet_acc.vi = hethet_acc.vi + ((hethet_bits + vul_rshift(hethet_bits, 4)) & m4);
      dotprod_acc.vi = dotprod_acc.vi + (dotprod_1x_bits & m4) + (vul_rshift(dotprod_1x_bits, 4) & m4);
    } while (one_vvec0_iter < one_vvec0_stop);
    const vul_t m8 = VCONST_UL(kMask00FF);
    hethet_acc.vi = (hethet_acc.vi + vul_rshift(hethet_acc.vi, 8)) & m8;
    dotprod_acc.vi = (dotprod_acc.vi & m8) + (vul_rshift(dotprod_acc.vi, 8) & m8);
    hethet_ct += univec_hsum_16bit(hethet_acc);
    known_dotprod += univec_hsum_16bit(dotprod_acc);
  }
}

void geno_bitvec_phased_dotprod(const uintptr_t* one_bitvec0, const uintptr_t* two_bitvec0, const uintptr_t* one_bitvec1, const uintptr_t* two_bitvec1, uint32_t word_ct, uint32_t* __restrict known_dotprod_ptr, uint32_t* __restrict hethet_ct_ptr) {
  // known_dotprod := popcount((one_bitvec0 & two_bitvec1) |
  //                           (two_bitvec0 & one_bitvec1)) +
  //                  2 * popcount(subset_mask & two_bitvec)
  // hethet_ct := popcount(one_bitvec0 & one_bitvec1)
  uint32_t known_dotprod = 0;
  uint32_t hethet_ct = 0;
#ifdef __LP64__
  if (word_ct >= kWordsPerVec) {
#endif
    const uint32_t remainder = word_ct % kWordsPerVec;
    const uint32_t main_block_word_ct = word_ct - remainder;
    word_ct = remainder;
    geno_bitvec_phased_dotprod_main((const vul_t*)one_bitvec0, (const vul_t*)two_bitvec0, (const vul_t*)one_bitvec1, (const vul_t*)two_bitvec1, main_block_word_ct / kWordsPerVec, &known_dotprod, &hethet_ct);
#ifdef __LP64__
    one_bitvec0 = &(one_bitvec0[main_block_word_ct]);
    two_bitvec0 = &(two_bitvec0[main_block_word_ct]);
    one_bitvec1 = &(one_bitvec1[main_block_word_ct]);
    two_bitvec1 = &(two_bitvec1[main_block_word_ct]);
  }
  for (uint32_t trailing_word_idx = 0; trailing_word_idx < word_ct; ++trailing_word_idx) {
    const uintptr_t one_word0 = one_bitvec0[trailing_word_idx];
    const uintptr_t two_word0 = two_bitvec0[trailing_word_idx];
    const uintptr_t one_word1 = one_bitvec1[trailing_word_idx];
    const uintptr_t two_word1 = two_bitvec1[trailing_word_idx];
    known_dotprod += popcount_long((one_word0 & two_word1) | (one_word1 & two_word0)) + 2 * popcount_long(two_word0 & two_word1);
    hethet_ct += popcount_long(one_word0 & one_word1);
  }
#endif
  *known_dotprod_ptr = known_dotprod;
  *hethet_ct_ptr = hethet_ct;
}

// alt_cts[] must be initialized to correct values for
// no-missing-values-in-other-variant case.
uint32_t hardcall_phased_r2_stats(const uintptr_t* one_bitvec0, const uintptr_t* two_bitvec0, const uintptr_t* nm_bitvec0, const uintptr_t* one_bitvec1, const uintptr_t* two_bitvec1, const uintptr_t* nm_bitvec1, uint32_t sample_ct, uint32_t nm_ct0, uint32_t nm_ct1, uint32_t* __restrict alt_cts, uint32_t* __restrict known_dotprod_ptr, uint32_t* __restrict hethet_ct_ptr) {
  const uint32_t sample_ctl = BITCT_TO_WORDCT(sample_ct);
  uint32_t nm_intersection_ct;
  if ((nm_ct0 != sample_ct) && (nm_ct1 != sample_ct)) {
    nm_intersection_ct = popcount_longs_intersect(nm_bitvec0, nm_bitvec1, sample_ctl);
    if (!nm_intersection_ct) {
      alt_cts[0] = 0;
      alt_cts[1] = 0;
      *known_dotprod_ptr = 0;
      *hethet_ct_ptr = 0;
      return 0;
    }
  } else {
    nm_intersection_ct = MINV(nm_ct0, nm_ct1);
  }
  if (nm_ct0 != nm_intersection_ct) {
    alt_cts[0] = geno_bitvec_sum_subset(nm_bitvec1, one_bitvec0, two_bitvec0, sample_ctl);
  }
  if (nm_ct1 != nm_intersection_ct) {
    alt_cts[1] = geno_bitvec_sum_subset(nm_bitvec0, one_bitvec1, two_bitvec1, sample_ctl);
  }
  geno_bitvec_phased_dotprod(one_bitvec0, two_bitvec0, one_bitvec1, two_bitvec1, sample_ctl, known_dotprod_ptr, hethet_ct_ptr);
  return nm_intersection_ct;
}

void hardcall_phased_r2_refine_main(const vul_t* phasepresent0_vvec, const vul_t* phaseinfo0_vvec, const vul_t* phasepresent1_vvec, const vul_t* phaseinfo1_vvec, uint32_t vec_ct, uint32_t* __restrict hethet_decr_ptr, uint32_t* __restrict not_dotprod_ptr) {
  // vec_ct must be a multiple of 3
  const vul_t m1 = VCONST_UL(kMask5555);
  const vul_t m2 = VCONST_UL(kMask3333);
  const vul_t m4 = VCONST_UL(kMask0F0F);
  const vul_t* phasepresent0_vvec_iter = phasepresent0_vvec;
  const vul_t* phaseinfo0_vvec_iter = phaseinfo0_vvec;
  const vul_t* phasepresent1_vvec_iter = phasepresent1_vvec;
  const vul_t* phaseinfo1_vvec_iter = phaseinfo1_vvec;
  uint32_t hethet_decr = 0;
  uint32_t not_dotprod = 0;  // like not_hotdog, but more useful
  while (1) {
    univec_t hethet_decr_acc;
    univec_t not_dotprod_acc;
    not_dotprod_acc.vi = vul_setzero();
    hethet_decr_acc.vi = vul_setzero();
    const vul_t* phasepresent0_vvec_stop;
    if (vec_ct < 30) {
      if (!vec_ct) {
        *hethet_decr_ptr = hethet_decr;
        *not_dotprod_ptr = not_dotprod;
        return;
      }
      phasepresent0_vvec_stop = &(phasepresent0_vvec_iter[vec_ct]);
      vec_ct = 0;
    } else {
      phasepresent0_vvec_stop = &(phasepresent0_vvec_iter[30]);
      vec_ct -= 30;
    }
    do {
      // todo: benchmark against simpler one-vec-at-a-time loop
      vul_t mask1 = (*phasepresent0_vvec_iter++) & (*phasepresent1_vvec_iter++);
      vul_t mask2 = (*phasepresent0_vvec_iter++) & (*phasepresent1_vvec_iter++);
      vul_t mask_half1 = (*phasepresent0_vvec_iter++) & (*phasepresent1_vvec_iter++);
      vul_t mask_half2 = vul_rshift(mask_half1, 1) & m1;
      mask_half1 = mask_half1 & m1;

      vul_t not_dotprod_count1 = (*phaseinfo0_vvec_iter++) ^ (*phaseinfo1_vvec_iter++);
      vul_t not_dotprod_count2 = (*phaseinfo0_vvec_iter++) ^ (*phaseinfo1_vvec_iter++);
      vul_t not_dotprod_half1 = (*phaseinfo0_vvec_iter++) ^ (*phaseinfo1_vvec_iter++);
      vul_t not_dotprod_half2 = vul_rshift(not_dotprod_half1, 1) & mask_half1;
      not_dotprod_count1 = not_dotprod_count1 & mask1;
      not_dotprod_count2 = not_dotprod_count2 & mask2;
      not_dotprod_half1 = not_dotprod_half1 & mask_half1;

      mask1 = mask1 - (vul_rshift(mask1, 1) & m1);
      mask2 = mask2 - (vul_rshift(mask2, 1) & m1);
      not_dotprod_count1 = not_dotprod_count1 - (vul_rshift(not_dotprod_count1, 1) & m1);
      not_dotprod_count2 = not_dotprod_count2 - (vul_rshift(not_dotprod_count2, 1) & m1);
      mask1 = mask1 + mask_half1;
      mask2 = mask2 + mask_half2;
      not_dotprod_count1 = not_dotprod_count1 + not_dotprod_half1;
      not_dotprod_count2 = not_dotprod_count2 + not_dotprod_half2;

      mask1 = (mask1 & m2) + (vul_rshift(mask1, 2) & m2);
      not_dotprod_count1 = (not_dotprod_count1 & m2) + (vul_rshift(not_dotprod_count1, 2) & m2);
      mask1 = mask1 + (mask2 & m2) + (vul_rshift(mask2, 2) & m2);
      not_dotprod_count1 = not_dotprod_count1 + (not_dotprod_count2 & m2) + (vul_rshift(not_dotprod_count2, 2) & m2);

      hethet_decr_acc.vi = hethet_decr_acc.vi + (mask1 & m4) + (vul_rshift(mask1, 4) & m4);
      not_dotprod_acc.vi = not_dotprod_acc.vi + (not_dotprod_count1 & m4) + (vul_rshift(not_dotprod_count1, 4) & m4);
    } while (phasepresent0_vvec_iter < phasepresent0_vvec_stop);
    const vul_t m8 = VCONST_UL(kMask00FF);
    hethet_decr_acc.vi = (hethet_decr_acc.vi & m8) + (vul_rshift(hethet_decr_acc.vi, 8) & m8);
    not_dotprod_acc.vi = (not_dotprod_acc.vi & m8) + (vul_rshift(not_dotprod_acc.vi, 8) & m8);
    hethet_decr += univec_hsum_16bit(hethet_decr_acc);
    not_dotprod += univec_hsum_16bit(not_dotprod_acc);
  }
}

// only needs to be called when hethet_ct > 0, phasepresent0_ct > 0, and
// phasepresent1_ct > 0.
void hardcall_phased_r2_refine(const uintptr_t* phasepresent0, const uintptr_t* phaseinfo0, const uintptr_t* phasepresent1, const uintptr_t* phaseinfo1, uint32_t word_ct, uint32_t* __restrict known_dotprod_ptr, uint32_t* __restrict unknown_hethet_ct_ptr) {
  // unknown_hethet_ct -= popcount(phasepresent0 & phasepresent1)
  // known_dotprod_ptr += popcount(phasepresent0 & phasepresent1 &
  //                               (~(phaseinfo0 ^ phaseinfo1)))
  uint32_t hethet_decr = 0;
  uint32_t not_dotprod = 0;
  if (word_ct >= 3 * kWordsPerVec) {
    const uint32_t remainder = word_ct % (3 * kWordsPerVec);
    const uint32_t main_block_word_ct = word_ct - remainder;
    word_ct = remainder;
    hardcall_phased_r2_refine_main((const vul_t*)phasepresent0, (const vul_t*)phaseinfo0, (const vul_t*)phasepresent1, (const vul_t*)phaseinfo1, main_block_word_ct / kWordsPerVec, &hethet_decr, &not_dotprod);
    phasepresent0 = &(phasepresent0[main_block_word_ct]);
    phaseinfo0 = &(phaseinfo0[main_block_word_ct]);
    phasepresent1 = &(phasepresent1[main_block_word_ct]);
    phaseinfo1 = &(phaseinfo1[main_block_word_ct]);
  }
  for (uint32_t trailing_word_idx = 0; trailing_word_idx < word_ct; ++trailing_word_idx) {
    const uintptr_t mask = phasepresent0[trailing_word_idx] & phasepresent1[trailing_word_idx];
    const uintptr_t xor_word = phaseinfo0[trailing_word_idx] ^ phaseinfo1[trailing_word_idx];
    hethet_decr += popcount_long(mask);
    not_dotprod += popcount_long(mask & xor_word);
  }
  *known_dotprod_ptr += hethet_decr - not_dotprod;
  *unknown_hethet_ct_ptr -= hethet_decr;
}

// phased-dosage r^2 computation:
//   just do brute force for now
//   use dense dosage_sum/(optional dosage_diff phase info) representation
//     when either dosage_diff is null, can skip that dot product
//   also have a unphased_het_dosage array pointer.  this is null if all
//     dosages are phased.  otherwise...
//     suppose one unphased sample has dosage(var0)=0.2 and dosage(var1)=1.4.
//     this is stored as strandA0[] = strandB0[] = 0.1,
//                       strandA1[] = strandB1[] = 0.7,
//     so the sum of the two products is 0.14.
//     we treat this as P(var0=0/0)=0.8, P(var0=0/1)=0.2,
//                      P(var1=0/1)=0.6, P(var1=1/1)=0.4,
//     so the diplotype dosages are
//       0-0: 2 * 0.9 * 0.3 = 0.54
//       0-1: 2 * 0.9 * 0.7 = 1.26
//       1-0: 2 * 0.1 * 0.3 = 0.06
//       1-1: 2 * 0.1 * 0.7 = 0.14
//     the no-phasing-error components of this are:
//       var0=0/0, var1=0/1:
//         0-0: 0.8 * 0.6 = 0.48
//         0-1:           = 0.48
//       var0=0/0, var1=1/1:
//         0-1: 2 * 0.8 * 0.4 = 0.64
//       var0=0/1, var1=1/1:
//         0-1: 0.2 * 0.4 = 0.08
//         1-1:           = 0.08
//     the uncertain-phasing component of this is 2 * 0.2 * 0.6 = 0.24; a
//       quarter of this contributes to the sum-of-products.
//     if we save P(var=0/1) = (1 - abs(1 - dosagesum)) in unphased_het_dosage
//       for each unphased dosage (and 0 for each phased dosage), subtracting
//       half of the unphased_het_dosage dot product (0.12/2 = 0.06 in this
//       case) from the main dot product yields the definitely-known portion.
//       the unhalved unphased_het_dosage dot product is the maximum possible
//       value of the unknown portion (half_hethet_share).

static_assert(sizeof(dosage_t) == 2, "plink2_ld dosage-handling routines must be updated.");
#ifdef __LP64__
static_assert(kBytesPerVec == 16, "plink2_ld dosage-handling routines must be updated.");
void fill_dosage_uhet(const dosage_t* dosage_vec, uint32_t dosagev_ct, dosage_t* dosage_uhet) {
  const __m128i* dosage_vvec_iter = (const __m128i*)dosage_vec;
  #if defined(__APPLE__) && ((!defined(__cplusplus)) || (__cplusplus < 201103L))
  const __m128i all_n32768 = {0x8000800080008000LLU, 0x8000800080008000LLU};
  const __m128i all_n16384 = {0xc000c000c000c000LLU, 0xc000c000c000c000LLU};
  #else
  const __m128i all_n32768 = {-0x7fff7fff7fff8000LL, -0x7fff7fff7fff8000LL};
  const __m128i all_n16384 = {-0x3fff3fff3fff4000LL, -0x3fff3fff3fff4000LL};
  #endif
  const __m128i all0 = _mm_setzero_si128();
  const __m128i all1 = _mm_cmpeq_epi16(all0, all0);
  // 0-16384: leave unchanged
  // 16385-32768: subtract from 32768
  // 65535: set to 0

  // subtract from 0, _mm_cmplt_epi16 to produce mask, add 32768
  __m128i* dosage_uhet_iter = (__m128i*)dosage_uhet;
  for (uint32_t vec_idx = 0; vec_idx < dosagev_ct; ++vec_idx) {
    __m128i dosagev = *dosage_vvec_iter++;

    __m128i cur_mask = _mm_cmpeq_epi16(dosagev, all1);
    dosagev = _mm_andnot_si128(cur_mask, dosagev);  // 65535 -> 0

    // xor with -32768 is same as subtracting it
    __m128i dosagev_opp = _mm_xor_si128(dosagev, all_n32768);
    // anything > -16384 after this subtraction was originally >16384.
    // calling the original value x, we want to flip the sign of (x - 32768)
    cur_mask = _mm_cmpgt_epi16(dosagev_opp, all_n16384);
    dosagev_opp = _mm_and_si128(cur_mask, dosagev_opp);
    dosagev = _mm_andnot_si128(cur_mask, dosagev);  // has the <= 16384 values
    dosagev_opp = _mm_sub_epi16(all0, dosagev_opp);
    *dosage_uhet_iter++ = _mm_add_epi16(dosagev, dosagev_opp);
  }
}

uint64_t dense_dosage_sum(const dosage_t* dosage_vec, uint32_t vec_ct) {
  // end of dosage_vec assumed to be missing-padded (0-padded also ok)
  const __m128i* dosage_vvec_iter = (const __m128i*)dosage_vec;
  const __m128i m16 = {kMask0000FFFF, kMask0000FFFF};
  const __m128i all1 = _mm_cmpeq_epi16(m16, m16);
  uint64_t sum = 0;
  while (1) {
    __m128i sumv = _mm_setzero_si128();
    const __m128i* dosage_vvec_stop;
    // individual values in [0..32768]
    // 32768 * 16383 * 8 dosages per __m128i = just under 2^32
    if (vec_ct < 16383) {
      if (!vec_ct) {
	return sum;
      }
      dosage_vvec_stop = &(dosage_vvec_iter[vec_ct]);
      vec_ct = 0;
    } else {
      dosage_vvec_stop = &(dosage_vvec_iter[16383]);
      vec_ct -= 16383;
    }
    do {
      __m128i dosagev = *dosage_vvec_iter++;
      __m128i invmask = _mm_cmpeq_epi16(dosagev, all1);
      dosagev = _mm_andnot_si128(invmask, dosagev);

      dosagev = _mm_add_epi64(_mm_and_si128(dosagev, m16), _mm_and_si128(_mm_srli_epi64(dosagev, 16), m16));
      sumv = _mm_add_epi64(sumv, dosagev);
    } while (dosage_vvec_iter < dosage_vvec_stop);
    univec16_t acc;
    acc.vi = sumv;
    sum += univec16_hsum_32bit(acc);
  }
}

uint64_t dense_dosage_sum_subset(const dosage_t* dosage_vec, const dosage_t* dosage_mask_vec, uint32_t vec_ct) {
  // end of dosage_vec assumed to be missing-padded (0-padded also ok)
  const __m128i* dosage_vvec_iter = (const __m128i*)dosage_vec;
  const __m128i* dosage_mask_vvec_iter = (const __m128i*)dosage_mask_vec;
  const __m128i m16 = {kMask0000FFFF, kMask0000FFFF};
  const __m128i all1 = _mm_cmpeq_epi16(m16, m16);
  uint64_t sum = 0;
  while (1) {
    __m128i sumv = _mm_setzero_si128();
    const __m128i* dosage_vvec_stop;
    if (vec_ct < 16383) {
      if (!vec_ct) {
	return sum;
      }
      dosage_vvec_stop = &(dosage_vvec_iter[vec_ct]);
      vec_ct = 0;
    } else {
      dosage_vvec_stop = &(dosage_vvec_iter[16383]);
      vec_ct -= 16383;
    }
    do {
      __m128i invmask = *dosage_mask_vvec_iter++;
      __m128i dosagev = *dosage_vvec_iter++;
      invmask = _mm_cmpeq_epi16(invmask, all1);
      invmask = _mm_or_si128(invmask, _mm_cmpeq_epi16(dosagev, all1));
      dosagev = _mm_andnot_si128(invmask, dosagev);

      dosagev = _mm_add_epi64(_mm_and_si128(dosagev, m16), _mm_and_si128(_mm_srli_epi64(dosagev, 16), m16));
      sumv = _mm_add_epi64(sumv, dosagev);
    } while (dosage_vvec_iter < dosage_vvec_stop);
    univec16_t acc;
    acc.vi = sumv;
    sum += univec16_hsum_32bit(acc);
  }
}

// 65535 treated as missing
uint64_t dosage_unsigned_dotprod(const dosage_t* dosage_vec0, const dosage_t* dosage_vec1, uint32_t vec_ct) {
  const __m128i* dosage_vvec0_iter = (const __m128i*)dosage_vec0;
  const __m128i* dosage_vvec1_iter = (const __m128i*)dosage_vec1;
  const __m128i m16 = {kMask0000FFFF, kMask0000FFFF};
  const __m128i all1 = _mm_cmpeq_epi16(m16, m16);
  uint64_t dotprod = 0;
  while (1) {
    __m128i dotprod_lo = _mm_setzero_si128();
    __m128i dotprod_hi = _mm_setzero_si128();
    const __m128i* dosage_vvec0_stop;
    if (vec_ct < 8192) {
      if (!vec_ct) {
        return dotprod;
      }
      dosage_vvec0_stop = &(dosage_vvec0_iter[vec_ct]);
      vec_ct = 0;
    } else {
      dosage_vvec0_stop = &(dosage_vvec0_iter[8192]);
      vec_ct -= 8192;
    }
    do {
      __m128i dosage0 = *dosage_vvec0_iter++;
      __m128i dosage1 = *dosage_vvec1_iter++;
      __m128i invmask = _mm_cmpeq_epi16(dosage0, all1);
      invmask = _mm_or_si128(invmask, _mm_cmpeq_epi16(dosage1, all1));
      dosage0 = _mm_andnot_si128(invmask, dosage0);
      dosage1 = _mm_andnot_si128(invmask, dosage1);

      __m128i lo16 = _mm_mullo_epi16(dosage0, dosage1);
      __m128i hi16 = _mm_mulhi_epu16(dosage0, dosage1);
      lo16 = _mm_add_epi64(_mm_and_si128(lo16, m16), _mm_and_si128(_mm_srli_epi64(lo16, 16), m16));
      hi16 = _mm_and_si128(_mm_add_epi64(hi16, _mm_srli_epi64(hi16, 16)), m16);
      dotprod_lo = _mm_add_epi64(dotprod_lo, lo16);
      dotprod_hi = _mm_add_epi64(dotprod_hi, hi16);
    } while (dosage_vvec0_iter < dosage_vvec0_stop);
    univec16_t acc_lo;
    univec16_t acc_hi;
    acc_lo.vi = dotprod_lo;
    acc_hi.vi = dotprod_hi;
    dotprod += univec16_hsum_32bit(acc_lo) + 65536 * univec16_hsum_32bit(acc_hi);
  }
}

uint64_t dosage_unsigned_nomiss_dotprod(const dosage_t* dosage_vec0, const dosage_t* dosage_vec1, uint32_t vec_ct) {
  const __m128i* dosage_vvec0_iter = (const __m128i*)dosage_vec0;
  const __m128i* dosage_vvec1_iter = (const __m128i*)dosage_vec1;
  const __m128i m16 = {kMask0000FFFF, kMask0000FFFF};
  uint64_t dotprod = 0;
  while (1) {
    __m128i dotprod_lo = _mm_setzero_si128();
    __m128i dotprod_hi = _mm_setzero_si128();
    const __m128i* dosage_vvec0_stop;
    if (vec_ct < 8192) {
      if (!vec_ct) {
        return dotprod;
      }
      dosage_vvec0_stop = &(dosage_vvec0_iter[vec_ct]);
      vec_ct = 0;
    } else {
      dosage_vvec0_stop = &(dosage_vvec0_iter[8192]);
      vec_ct -= 8192;
    }
    do {
      __m128i dosage0 = *dosage_vvec0_iter++;
      __m128i dosage1 = *dosage_vvec1_iter++;

      __m128i lo16 = _mm_mullo_epi16(dosage0, dosage1);
      __m128i hi16 = _mm_mulhi_epu16(dosage0, dosage1);
      lo16 = _mm_add_epi64(_mm_and_si128(lo16, m16), _mm_and_si128(_mm_srli_epi64(lo16, 16), m16));
      hi16 = _mm_and_si128(_mm_add_epi64(hi16, _mm_srli_epi64(hi16, 16)), m16);
      dotprod_lo = _mm_add_epi64(dotprod_lo, lo16);
      dotprod_hi = _mm_add_epi64(dotprod_hi, hi16);
    } while (dosage_vvec0_iter < dosage_vvec0_stop);
    univec16_t acc_lo;
    univec16_t acc_hi;
    acc_lo.vi = dotprod_lo;
    acc_hi.vi = dotprod_hi;
    dotprod += univec16_hsum_32bit(acc_lo) + 65536 * univec16_hsum_32bit(acc_hi);
  }
}

int64_t dosage_signed_dotprod(const dosage_t* dosage_diff0, const dosage_t* dosage_diff1, uint32_t vec_ct) {
  const __m128i* dosage_diff0_iter = (const __m128i*)dosage_diff0;
  const __m128i* dosage_diff1_iter = (const __m128i*)dosage_diff1;
  const __m128i m16 = {kMask0000FFFF, kMask0000FFFF};
  const __m128i all_4096 = {0x1000100010001000LLU, 0x1000100010001000LLU};
  uint64_t dotprod = 0;
  uint32_t vec_ct_rem = vec_ct;
  while (1) {
    __m128i dotprod_lo = _mm_setzero_si128();
    __m128i dotprod_hi = _mm_setzero_si128();
    const __m128i* dosage_diff0_stop;
    if (vec_ct_rem < 8192) {
      if (!vec_ct_rem) {
	// this cancels out the shift-hi16-by-4096 below
        return ((int64_t)dotprod) - (0x10000000LLU * kDosagePerVec) * vec_ct;
      }
      dosage_diff0_stop = &(dosage_diff0_iter[vec_ct_rem]);
      vec_ct_rem = 0;
    } else {
      dosage_diff0_stop = &(dosage_diff0_iter[8192]);
      vec_ct_rem -= 8192;
    }
    do {
      __m128i dosage0 = *dosage_diff0_iter++;
      __m128i dosage1 = *dosage_diff1_iter++;

      __m128i hi16 = _mm_mulhi_epi16(dosage0, dosage1);
      __m128i lo16 = _mm_mullo_epi16(dosage0, dosage1);
      // original values are in [-16384, 16384]
      // product is in [-2^28, 2^28], so hi16 is in [-4096, 4096]
      // so if we add 4096 to hi16, we can treat it as an unsigned value in the
      //   rest of this loop
      hi16 = _mm_add_epi16(hi16, all_4096);
      lo16 = _mm_add_epi64(_mm_and_si128(lo16, m16), _mm_and_si128(_mm_srli_epi64(lo16, 16), m16));
      hi16 = _mm_and_si128(_mm_add_epi64(hi16, _mm_srli_epi64(hi16, 16)), m16);
      dotprod_lo = _mm_add_epi64(dotprod_lo, lo16);
      dotprod_hi = _mm_add_epi64(dotprod_hi, hi16);
    } while (dosage_diff0_iter < dosage_diff0_stop);
    univec16_t acc_lo;
    univec16_t acc_hi;
    acc_lo.vi = dotprod_lo;
    acc_hi.vi = dotprod_hi;
    dotprod += univec16_hsum_32bit(acc_lo) + 65536 * univec16_hsum_32bit(acc_hi);
  }
}
#else
void fill_dosage_uhet(const dosage_t* dosage_vec, uint32_t dosagev_ct, dosage_t* dosage_uhet) {
  const uint32_t sample_cta2 = dosagev_ct * 2;
  for (uint32_t sample_idx = 0; sample_idx < sample_cta2; ++sample_idx) {
    const uint32_t cur_dosage = dosage_vec[sample_idx];
    uint32_t cur_hetval = cur_dosage;
    if (cur_hetval > 16384) {
      if (cur_hetval == kDosageMissing) {
        cur_hetval = 0;
      } else {
        cur_hetval = 32768 - cur_hetval;
      }
    }
    dosage_uhet[sample_idx] = cur_hetval;
  }
}

uint64_t dense_dosage_sum(const dosage_t* dosage_vec, uint32_t vec_ct) {
  const uint32_t sample_cta2 = vec_ct * 2;
  uint64_t sum = 0;
  for (uint32_t sample_idx = 0; sample_idx < sample_cta2; ++sample_idx) {
    const uint32_t cur_dosage = dosage_vec[sample_idx];
    if (cur_dosage != kDosageMissing) {
      sum += cur_dosage;
    }
  }
  return sum;
}

uint64_t dense_dosage_sum_subset(const dosage_t* dosage_vec, const dosage_t* dosage_mask_vec, uint32_t vec_ct) {
  const uint32_t sample_cta2 = vec_ct * 2;
  uint64_t sum = 0;
  for (uint32_t sample_idx = 0; sample_idx < sample_cta2; ++sample_idx) {
    const uint32_t cur_dosage = dosage_vec[sample_idx];
    const uint32_t other_dosage = dosage_mask_vec[sample_idx];
    if ((cur_dosage != kDosageMissing) && (other_dosage != kDosageMissing)) {
      sum += cur_dosage;
    }
  }
  return sum;
}

uint64_t dosage_unsigned_dotprod(const dosage_t* dosage_vec0, const dosage_t* dosage_vec1, uint32_t vec_ct) {
  const uint32_t sample_cta2 = vec_ct * 2;
  uint64_t dotprod = 0;
  for (uint32_t sample_idx = 0; sample_idx < sample_cta2; ++sample_idx) {
    const uint32_t cur_dosage0 = dosage_vec0[sample_idx];
    const uint32_t cur_dosage1 = dosage_vec1[sample_idx];
    if ((cur_dosage0 != kDosageMissing) && (cur_dosage1 != kDosageMissing)) {
      dotprod += cur_dosage0 * cur_dosage1;
    }
  }
  return dotprod;
}

uint64_t dosage_unsigned_nomiss_dotprod(const dosage_t* dosage_vec0, const dosage_t* dosage_vec1, uint32_t vec_ct) {
  const uint32_t sample_cta2 = vec_ct * 2;
  uint64_t dotprod = 0;
  for (uint32_t sample_idx = 0; sample_idx < sample_cta2; ++sample_idx) {
    const uint32_t cur_dosage0 = dosage_vec0[sample_idx];
    const uint32_t cur_dosage1 = dosage_vec1[sample_idx];
    dotprod += cur_dosage0 * cur_dosage1;
  }
  return dotprod;
}

int64_t dosage_signed_dotprod(const dosage_t* dosage_diff0, const dosage_t* dosage_diff1, uint32_t vec_ct) {
  const uint32_t sample_cta2 = vec_ct * 2;
  int64_t dotprod = 0;
  for (uint32_t sample_idx = 0; sample_idx < sample_cta2; ++sample_idx) {
    const int32_t cur_diff0 = ((int16_t)dosage_diff0[sample_idx]);
    const int32_t cur_diff1 = ((int16_t)dosage_diff1[sample_idx]);
    dotprod += cur_diff0 * cur_diff1;
  }
  return dotprod;
}
#endif

void dosage_phaseinfo_patch(const uintptr_t* phasepresent, const uintptr_t* phaseinfo, const uintptr_t* dosage_present, uint32_t sample_ct, dosage_t* dosage_uhet, dosage_t* dosage_diff) {
  const uint32_t sample_ctl = BITCT_TO_WORDCT(sample_ct);
  for (uint32_t widx = 0; widx < sample_ctl; ++widx) {
    uintptr_t phasepresent_nodosage_word = phasepresent[widx] & (~dosage_present[widx]);
    if (phasepresent_nodosage_word) {
      const uintptr_t phaseinfo_word = phaseinfo[widx];
      const uint32_t sample_idx_offset = widx * kBitsPerWord;
      do {
	const uint32_t sample_idx_lowbits = CTZLU(phasepresent_nodosage_word);
	const uint32_t cur_diff = 49152 - ((phaseinfo_word >> sample_idx_lowbits) & 1) * 32768;
	const uint32_t sample_idx = sample_idx_offset + sample_idx_lowbits;
	dosage_uhet[sample_idx] = 0;
	dosage_diff[sample_idx] = cur_diff;
	phasepresent_nodosage_word &= phasepresent_nodosage_word - 1;
      } while (phasepresent_nodosage_word);
    }
  }
}

uint32_t dosage_phased_r2_prod(const dosage_t* dosage_vec0, const uintptr_t* nm_bitvec0, const dosage_t* dosage_vec1, const uintptr_t* nm_bitvec1, uint32_t sample_ct, uint32_t nm_ct0, uint32_t nm_ct1, uint64_t* __restrict alt_dosages, uint64_t* __restrict dosageprod_ptr) {
  const uint32_t sample_ctl = BITCT_TO_WORDCT(sample_ct);
  uint32_t nm_intersection_ct;
  if ((nm_ct0 != sample_ct) && (nm_ct1 != sample_ct)) {
    nm_intersection_ct = popcount_longs_intersect(nm_bitvec0, nm_bitvec1, sample_ctl);
    if (!nm_intersection_ct) {
      alt_dosages[0] = 0;
      alt_dosages[1] = 0;
      *dosageprod_ptr = 0;
      return 0;
    }
  } else {
    nm_intersection_ct = MINV(nm_ct0, nm_ct1);
  }
  const uint32_t vec_ct = DIV_UP(sample_ct, kDosagePerVec);
  if (nm_ct0 != nm_intersection_ct) {
    alt_dosages[0] = dense_dosage_sum_subset(dosage_vec0, dosage_vec1, vec_ct);
  }
  if (nm_ct1 != nm_intersection_ct) {
    alt_dosages[1] = dense_dosage_sum_subset(dosage_vec1, dosage_vec0, vec_ct);
  }
  // could conditionally use dosage_unsigned_nomiss here
  *dosageprod_ptr = dosage_unsigned_dotprod(dosage_vec0, dosage_vec1, vec_ct);

  return nm_intersection_ct;
}


// "unscaled" because you need to multiply by allele count to get the proper
// log-likelihood
double em_phase_unscaled_lnlike(double freq11, double freq12, double freq21, double freq22, double half_hethet_share, double freq11_incr) {
  freq11 += freq11_incr;
  freq22 += freq11_incr;
  freq12 += half_hethet_share - freq11_incr;
  freq21 += half_hethet_share - freq11_incr;
  const double cross_sum = freq11 * freq22 + freq12 * freq21;
  double lnlike = 0.0;
  if (cross_sum != 0.0) {
    lnlike = half_hethet_share * log(cross_sum);
  }
  if (freq11 != 0.0) {
    lnlike += freq11 * log(freq11);
  }
  if (freq12 != 0.0) {
    lnlike += freq12 * log(freq12);
  }
  if (freq21 != 0.0) {
    lnlike += freq21 * log(freq21);
  }
  if (freq22 != 0.0) {
    lnlike += freq22 * log(freq22);
  }
  return lnlike;
}

pglerr_t ld_console(const uintptr_t* variant_include, const chr_info_t* cip, char** variant_ids, const uintptr_t* variant_allele_idxs, char** allele_storage, const uintptr_t* founder_info, const uintptr_t* sex_nm, const uintptr_t* sex_male, const ld_info_t* ldip, uint32_t variant_ct, uint32_t raw_sample_ct, uint32_t founder_ct, pgen_reader_t* simple_pgrp) {
  unsigned char* bigstack_mark = g_bigstack_base;
  pglerr_t reterr = kPglRetSuccess;
  {
    if (!founder_ct) {
      logerrprint("Warning: Skipping --ld since there are no founders.  (--make-founders may come\nin handy here.)\n");
      goto ld_console_ret_1;
    }
    char** ld_console_varids = (char**)ldip->ld_console_varids;
    // ok to ignore chr_mask here
    const int32_t x_code = cip->xymt_codes[kChrOffsetX];
    const int32_t y_code = cip->xymt_codes[kChrOffsetY];
    const int32_t mt_code = cip->xymt_codes[kChrOffsetMT];
    // is_x:
    // * male het calls treated as missing hardcalls
    // * males only have half weight in all computations (or sqrt(0.5) if one
    //   variant on chrX and one variant elsewhere)
    // * SNPHWEX used for HWE stats
    //
    // is_nonx_haploid_or_mt:
    // * all het calls treated as missing hardcalls
    uint32_t var_uidxs[2];
    uint32_t chr_idxs[2];
    uint32_t is_xs[2];
    uint32_t is_nonx_haploid_or_mts[2];
    uint32_t y_ct = 0;
    for (uint32_t var_idx = 0; var_idx < 2; ++var_idx) {
      char* cur_varid = ld_console_varids[var_idx];
      int32_t ii = get_variant_uidx_without_htable(cur_varid, variant_ids, variant_include, variant_ct);
      if (ii == -1) {
	sprintf(g_logbuf, "Error: --ld variant '%s' does not appear in dataset.\n", cur_varid);
	goto ld_console_ret_INCONSISTENT_INPUT_WW;
      } else if (ii == -2) {
	sprintf(g_logbuf, "Error: --ld variant '%s' appears multiple times in dataset.\n", cur_varid);
	goto ld_console_ret_INCONSISTENT_INPUT_WW;
      }
      var_uidxs[var_idx] = (uint32_t)ii;
      const uint32_t chr_idx = get_variant_chr(cip, (uint32_t)ii);
      chr_idxs[var_idx] = chr_idx;
      const uint32_t is_x = ((int32_t)chr_idx == x_code);
      is_xs[var_idx] = is_x;
      uint32_t is_nonx_haploid_or_mt;
      if (is_set(cip->haploid_mask, chr_idx)) {
	is_nonx_haploid_or_mt = 1 - is_x;
        y_ct += ((int32_t)chr_idx == y_code);
      } else {
	is_nonx_haploid_or_mt = ((int32_t)chr_idx == mt_code);
      }
      is_nonx_haploid_or_mts[var_idx] = is_nonx_haploid_or_mt;
    }
    const uint32_t raw_sample_ctl = BITCT_TO_WORDCT(raw_sample_ct);
    // if both unplaced, don't count as same-chromosome
    const uint32_t is_same_chr = chr_idxs[0] && (chr_idxs[0] == chr_idxs[1]);
    if (y_ct) {
      // only keep male founders
      uintptr_t* founder_info_tmp;
      if (bigstack_alloc_ul(raw_sample_ctl, &founder_info_tmp)) {
        goto ld_console_ret_NOMEM;
      }
      bitvec_and_copy(founder_info, sex_male, raw_sample_ctl, founder_info_tmp);
      founder_info = founder_info_tmp;
      founder_ct = popcount_longs(founder_info, raw_sample_ctl);
      if (!founder_ct) {
        LOGERRPRINTFWW("Warning: Skipping --ld since there are no male founders, and %s specified. (--make-founders may come in handy here.)\n", is_same_chr? "chrY variants were" : "a chrY variant was");
        goto ld_console_ret_1;
      }
    }
    const uint32_t founder_ctl = BITCT_TO_WORDCT(founder_ct);
    const uint32_t founder_ctl2 = QUATERCT_TO_WORDCT(founder_ct);
    uint32_t* founder_info_cumulative_popcounts;
    uintptr_t* genovecs[2];
    uintptr_t* phasepresents[2];
    uintptr_t* phaseinfos[2];
    uint32_t phasepresent_cts[2];
    uintptr_t* dosage_presents[2];
    dosage_t* dosage_vals[2];
    uint32_t dosage_cts[2];
    if (bigstack_alloc_ui(founder_ctl, &founder_info_cumulative_popcounts) ||
	bigstack_alloc_ul(founder_ctl2, &(genovecs[0])) ||
        bigstack_alloc_ul(founder_ctl2, &(genovecs[1])) ||
	bigstack_alloc_ul(founder_ctl, &(phasepresents[0])) ||
	bigstack_alloc_ul(founder_ctl, &(phasepresents[1])) ||
	bigstack_alloc_ul(founder_ctl, &(phaseinfos[0])) ||
	bigstack_alloc_ul(founder_ctl, &(phaseinfos[1])) ||
	bigstack_alloc_ul(founder_ctl, &(dosage_presents[0])) ||
	bigstack_alloc_ul(founder_ctl, &(dosage_presents[1])) ||
	bigstack_alloc_dosage(founder_ct, &(dosage_vals[0])) ||
	bigstack_alloc_dosage(founder_ct, &(dosage_vals[1]))) {
      goto ld_console_ret_NOMEM;
    }
    const uint32_t x_present = (is_xs[0] || is_xs[1]);
    const uint32_t founder_ctv = BITCT_TO_VECCT(founder_ct);
    const uint32_t founder_ctaw = founder_ctv * kWordsPerVec;
    uintptr_t* sex_male_collapsed = nullptr;
    uintptr_t* sex_male_collapsed_interleaved = nullptr;
    uint32_t x_male_ct = 0;
    if (x_present) {
      if (bigstack_alloc_ul(founder_ctaw, &sex_male_collapsed) ||
	  bigstack_alloc_ul(founder_ctaw, &sex_male_collapsed_interleaved)) {
	goto ld_console_ret_NOMEM;
      }
#ifdef __LP64__
      fill_ulong_zero(kWordsPerVec - 1, &(sex_male_collapsed[founder_ctaw + 1 - kWordsPerVec]));
#endif
      copy_bitarr_subset(sex_male, founder_info, founder_ct, sex_male_collapsed);
      fill_interleaved_mask_vec(sex_male_collapsed, founder_ctv, sex_male_collapsed_interleaved);
      x_male_ct = popcount_longs(sex_male_collapsed, founder_ctaw);
    }
    fill_cumulative_popcounts(founder_info, founder_ctl, founder_info_cumulative_popcounts);
    uint32_t use_dosage = ldip->ld_console_modifier & kfLdConsoleDosage;

    pgr_clear_ld_cache(simple_pgrp);
    for (uint32_t var_idx = 0; var_idx < 2; ++var_idx) {
      const uint32_t variant_uidx = var_uidxs[var_idx];
      uintptr_t* cur_genovec = genovecs[var_idx];
      uintptr_t* cur_phasepresent = phasepresents[var_idx];
      uintptr_t* cur_phaseinfo = phaseinfos[var_idx];
      uintptr_t* cur_dosage_present = dosage_presents[var_idx];
      dosage_t* cur_dosage_vals = dosage_vals[var_idx];
      // (unconditionally allocating phaseinfo/dosage_vals and using the most
      // general-purpose loader makes sense when this loop only executes twice,
      // but --r2 will want to use different pgenlib loaders depending on
      // context.)

      // todo: multiallelic case
      uint32_t is_explicit_alt1;
      reterr = pgr_read_refalt1_genovec_hphase_dosage16_subset_unsafe(founder_info, founder_info_cumulative_popcounts, founder_ct, variant_uidx, simple_pgrp, cur_genovec, cur_phasepresent, cur_phaseinfo, &(phasepresent_cts[var_idx]), cur_dosage_present, cur_dosage_vals, &(dosage_cts[var_idx]), &is_explicit_alt1);
      if (reterr) {
	if (reterr == kPglRetMalformedInput) {
	  logprint("\n");
	  logerrprint("Error: Malformed .pgen file.\n");
	}
	goto ld_console_ret_1;
      }
      zero_trailing_quaters(founder_ct, cur_genovec);
      if (is_nonx_haploid_or_mts[var_idx]) {
	if (!use_dosage) {
	  set_het_missing(founder_ctl2, cur_genovec);
	}
	phasepresent_cts[var_idx] = 0;
	// todo: erase phased dosages
      } else if (x_male_ct && is_xs[var_idx]) {
	if (!use_dosage) {
	  set_male_het_missing(sex_male_collapsed_interleaved, founder_ctv, cur_genovec);
	}
	if (phasepresent_cts[var_idx]) {
	  bitvec_andnot(sex_male_collapsed, founder_ctl, cur_phasepresent);
	  phasepresent_cts[var_idx] = popcount_longs(cur_phasepresent, founder_ctl);
	}
	// todo: erase male phased dosages
      }
    }
    const uint32_t use_phase = is_same_chr && (!is_nonx_haploid_or_mts[0]) && phasepresent_cts[0] && phasepresent_cts[1];
    const uint32_t ignore_hethet = is_nonx_haploid_or_mts[0] || is_nonx_haploid_or_mts[1];
    if ((!dosage_cts[0]) && (!dosage_cts[1]) && (!ignore_hethet)) {
      use_dosage = 0;
    }

    // values of interest:
    //   mutually-nonmissing observation count
    //   (all other values computed over mutually-nonmissing set)
    //   4 known-diplotype dosages (0..2 for each sample, in unphased het-het)
    //   (unphased het-het fractional count can be inferred)
    //   dosage sum for each variant
    double x_male_known_dotprod_d = 0.0;
    uint32_t valid_x_male_ct = 0;
    double altsums_d[2];
    double x_male_altsums_d[2];
    double known_dotprod_d;
    double unknown_hethet_d;
    uint32_t valid_obs_ct;
    uint32_t hethet_present;
    if (!use_dosage) {
      // While we could theoretically optimize around the fact that we only
      // need to make a single phased-r^2 computation, that's silly; it makes a
      // lot more sense to use this as a testing ground for algorithms and data
      // representations suitable for --r/--r2, etc.
      uintptr_t* one_bitvecs[2];
      uintptr_t* two_bitvecs[2];
      uintptr_t* nm_bitvecs[2];
      if (bigstack_alloc_ul(founder_ctaw, &one_bitvecs[0]) ||
          bigstack_alloc_ul(founder_ctaw, &two_bitvecs[0]) ||
          bigstack_alloc_ul(founder_ctaw, &nm_bitvecs[0]) ||
          bigstack_alloc_ul(founder_ctaw, &one_bitvecs[1]) ||
          bigstack_alloc_ul(founder_ctaw, &two_bitvecs[1]) ||
          bigstack_alloc_ul(founder_ctaw, &nm_bitvecs[1])) {
        goto ld_console_ret_NOMEM;
      }
      uint32_t alt_cts[2];
      uint32_t nm_cts[2];
      for (uint32_t var_idx = 0; var_idx < 2; ++var_idx) {
        genoarr_split_12nm(genovecs[var_idx], founder_ct, one_bitvecs[var_idx], two_bitvecs[var_idx], nm_bitvecs[var_idx]);
        alt_cts[var_idx] = geno_bitvec_sum(one_bitvecs[var_idx], two_bitvecs[var_idx], founder_ctl);
	nm_cts[var_idx] = popcount_longs(nm_bitvecs[var_idx], founder_ctl);
      }
      const uint32_t orig_alt_ct1 = alt_cts[1];
      uint32_t known_dotprod;
      uint32_t unknown_hethet_ct;
      valid_obs_ct = hardcall_phased_r2_stats(one_bitvecs[0], two_bitvecs[0], nm_bitvecs[0], one_bitvecs[1], two_bitvecs[1], nm_bitvecs[1], founder_ct, nm_cts[0], nm_cts[1], alt_cts, &known_dotprod, &unknown_hethet_ct);
      if (!valid_obs_ct) {
        goto ld_console_ret_NO_VALID_OBSERVATIONS;
      }
      hethet_present = (unknown_hethet_ct != 0);
      if (use_phase && hethet_present) {
        // all that's needed for the hardcall-phase correction is:
        //   popcount(phasepresent0 & phasepresent1)
        //   popcount(phasepresent0 & phasepresent1 &
        //            (phaseinfo0 ^ phaseinfo1))
        hardcall_phased_r2_refine(phasepresents[0], phaseinfos[0], phasepresents[1], phaseinfos[1], founder_ctl, &known_dotprod, &unknown_hethet_ct);
      }
      altsums_d[0] = (int32_t)alt_cts[0];
      altsums_d[1] = (int32_t)alt_cts[1];
      known_dotprod_d = known_dotprod;
      unknown_hethet_d = (int32_t)unknown_hethet_ct;
      if (x_male_ct) {
        // on chrX, store separate full-size copies of one_bitvec, two_bitvec,
        //   and nm_bitvec with nonmales masked out
        // (we can bitvec-and here because we're not doing any further
        // calculations.  it suffices to bitvec-and one side)
        bitvec_and(sex_male_collapsed, founder_ctl, one_bitvecs[0]);
        bitvec_and(sex_male_collapsed, founder_ctl, two_bitvecs[0]);
        bitvec_and(sex_male_collapsed, founder_ctl, nm_bitvecs[0]);
        uint32_t x_male_alt_cts[2];
        x_male_alt_cts[0] = geno_bitvec_sum(one_bitvecs[0], two_bitvecs[0], founder_ctl);

        x_male_alt_cts[1] = orig_alt_ct1;

        const uint32_t x_male_nm_ct0 = popcount_longs(nm_bitvecs[0], founder_ctl);
        uint32_t x_male_known_dotprod;
        uint32_t x_male_unknown_hethet_ct;  // ignore
        valid_x_male_ct = hardcall_phased_r2_stats(one_bitvecs[0], two_bitvecs[0], nm_bitvecs[0], one_bitvecs[1], two_bitvecs[1], nm_bitvecs[1], founder_ct, x_male_nm_ct0, nm_cts[1], x_male_alt_cts, &x_male_known_dotprod, &x_male_unknown_hethet_ct);
        x_male_altsums_d[0] = (int32_t)x_male_alt_cts[0];
        x_male_altsums_d[1] = (int32_t)x_male_alt_cts[1];
        x_male_known_dotprod_d = x_male_known_dotprod;
        // hethet impossible for chrX males
        assert(!x_male_unknown_hethet_ct);
      }
    } else {
      // Current brute-force strategy:
      // 1. Expand each variant to all-dosage_t format, with an optional
      //    phased dosage signed-difference track.
      // 2. Given (a0+b0), (a0-b0), (a1+b1), and (a1-b1)
      //    We wish to compute a0*a1 + b0*b1
      //      (a0+b0) * (a1+b1) = a0*a1 + b0*b1 + a0*b1 + a1*b0
      //      (a0-b0) * (a1-b1) = a0*a1 + b0*b1 - a0*b1 - a1*b0
      //      so halving the sum of these two dot products works.
      const uint32_t founder_dosagev_ct = DIV_UP(founder_ct, kDosagePerVec);
      dosage_t* dosage_vecs[2];
      dosage_t* dosage_uhets[2];
      uintptr_t* nm_bitvecs[2];
      // founder_ct automatically rounded up as necessary
      if (bigstack_alloc_dosage(founder_ct, &dosage_vecs[0]) ||
          bigstack_alloc_dosage(founder_ct, &dosage_vecs[1]) ||
          bigstack_alloc_dosage(founder_ct, &dosage_uhets[0]) ||
          bigstack_alloc_dosage(founder_ct, &dosage_uhets[1]) ||
	  bigstack_alloc_ul(founder_ctl, &nm_bitvecs[0]) ||
	  bigstack_alloc_ul(founder_ctl, &nm_bitvecs[1])) {
        goto ld_console_ret_NOMEM;
      }
      uint64_t alt_dosages[2];
      uint32_t nm_cts[2];
      for (uint32_t var_idx = 0; var_idx < 2; ++var_idx) {
        populate_dense_dosage(genovecs[var_idx], dosage_presents[var_idx], dosage_vals[var_idx], founder_ct, dosage_cts[var_idx], dosage_vecs[var_idx]);
        alt_dosages[var_idx] = dense_dosage_sum(dosage_vecs[var_idx], founder_dosagev_ct);
        fill_dosage_uhet(dosage_vecs[var_idx], founder_dosagev_ct, dosage_uhets[var_idx]);
	genovec_to_nonmissingness_unsafe(genovecs[var_idx], founder_ct, nm_bitvecs[var_idx]);
	zero_trailing_bits(founder_ct, nm_bitvecs[var_idx]);
	bitvec_or(dosage_presents[var_idx], founder_ctl, nm_bitvecs[var_idx]);
	nm_cts[var_idx] = popcount_longs(nm_bitvecs[var_idx], founder_ctl);
      }
      dosage_t* dosage_diffs[2];
      dosage_diffs[0] = nullptr;
      dosage_diffs[1] = nullptr;
      if (use_phase) {
	if (bigstack_alloc_dosage(founder_ct, &dosage_diffs[0]) ||
	    bigstack_alloc_dosage(founder_ct, &dosage_diffs[1])) {
	  goto ld_console_ret_NOMEM;
	}
	for (uint32_t var_idx = 0; var_idx < 2; ++var_idx) {
	  fill_dosage_zero(founder_dosagev_ct * kDosagePerVec, dosage_diffs[var_idx]);
	  if (phasepresent_cts[var_idx]) {
	    dosage_phaseinfo_patch(phasepresents[var_idx], phaseinfos[var_idx], dosage_presents[var_idx], founder_ct, dosage_uhets[var_idx], dosage_diffs[var_idx]);
	  }
	  // todo: patch in phased-dosage values
	}
      }
      const uint64_t orig_alt_dosage1 = alt_dosages[1];
      uint64_t dosageprod;
      valid_obs_ct = dosage_phased_r2_prod(dosage_vecs[0], nm_bitvecs[0], dosage_vecs[1], nm_bitvecs[1], founder_ct, nm_cts[0], nm_cts[1], alt_dosages, &dosageprod);
      if (!valid_obs_ct) {
	goto ld_console_ret_NO_VALID_OBSERVATIONS;
      }
      uint64_t uhethet_dosageprod = 0;
      if (!ignore_hethet) {
        uhethet_dosageprod = dosage_unsigned_nomiss_dotprod(dosage_uhets[0], dosage_uhets[1], founder_dosagev_ct);
      }
      hethet_present = (uhethet_dosageprod != 0);
      if (use_phase && hethet_present) {
	dosageprod = ((int64_t)dosageprod) + dosage_signed_dotprod(dosage_diffs[0], dosage_diffs[1], founder_dosagev_ct);
      }
      altsums_d[0] = ((int64_t)alt_dosages[0]) * kRecipDosageMid;
      altsums_d[1] = ((int64_t)alt_dosages[1]) * kRecipDosageMid;
      known_dotprod_d = ((int64_t)(dosageprod - uhethet_dosageprod)) * (kRecipDosageMidSq * 0.5);
      unknown_hethet_d = ((int64_t)uhethet_dosageprod) * kRecipDosageMidSq;
      if (x_male_ct) {
	dosage_t* x_male_dosage_invmask;
	if (bigstack_alloc_dosage(founder_ct, &x_male_dosage_invmask)) {
	  goto ld_console_ret_NOMEM;
	}
	fill_dosage_one(founder_dosagev_ct * kDosagePerVec, x_male_dosage_invmask);
	uint32_t sample_midx = 0;
	for (uint32_t uii = 0; uii < x_male_ct; ++uii, ++sample_midx) {
	  next_set_unsafe_ck(sex_male_collapsed, &sample_midx);
	  x_male_dosage_invmask[sample_midx] = 0;
	}
	bitvec_or((uintptr_t*)x_male_dosage_invmask, founder_dosagev_ct * kWordsPerVec, (uintptr_t*)dosage_vecs[0]);
	bitvec_and(sex_male_collapsed, founder_ctl, nm_bitvecs[0]);
	uint64_t x_male_alt_dosages[2];
	x_male_alt_dosages[0] = dense_dosage_sum(dosage_vecs[0], founder_dosagev_ct);
	x_male_alt_dosages[1] = orig_alt_dosage1;
	const uint32_t x_male_nm_ct0 = popcount_longs(nm_bitvecs[0], founder_ctl);
	uint64_t x_male_dosageprod;
	valid_x_male_ct = dosage_phased_r2_prod(dosage_vecs[0], nm_bitvecs[0], dosage_vecs[1], nm_bitvecs[1], founder_ct, x_male_nm_ct0, nm_cts[1], x_male_alt_dosages, &x_male_dosageprod);
	if (!ignore_hethet) {
	  bitvec_andnot((uintptr_t*)x_male_dosage_invmask, founder_dosagev_ct * kWordsPerVec, (uintptr_t*)dosage_uhets[0]);
	  const uint64_t invalid_uhethet_dosageprod = dosage_unsigned_nomiss_dotprod(dosage_uhets[0], dosage_uhets[1], founder_dosagev_ct);
	  unknown_hethet_d -= ((int64_t)invalid_uhethet_dosageprod) * kRecipDosageMidSq;
	}
        x_male_altsums_d[0] = ((int64_t)x_male_alt_dosages[0]) * kRecipDosageMid;
        x_male_altsums_d[1] = ((int64_t)x_male_alt_dosages[1]) * kRecipDosageMid;
        x_male_known_dotprod_d = ((int64_t)x_male_dosageprod) * (kRecipDosageMidSq * 0.5);
      }
    }
    double valid_obs_d = (int32_t)valid_obs_ct;
    if (valid_x_male_ct) {
      // males have sqrt(0.5) weight if one variant is chrX, half-weight if
      // both are chrX
      const double male_decr = (is_xs[0] && is_xs[1])? 0.5 : (1.0 - 0.5 * kSqrt2);
      altsums_d[0] -= male_decr * x_male_altsums_d[0];
      altsums_d[1] -= male_decr * x_male_altsums_d[1];
      known_dotprod_d -= male_decr * x_male_known_dotprod_d;
      valid_obs_d -= male_decr * ((int32_t)valid_x_male_ct);
    }

    const double twice_tot_recip = 0.5 / valid_obs_d;
    // in plink 1.9, "freq12" refers to first variant=1, second variant=2
    // this most closely corresponds to freq_ra here

    // known-diplotype dosages (sum is 2 * (valid_obs_d - unknown_hethet_d)):
    //   var0  var1
    //     0  -  0 : 2 * valid_obs_d - altsums[0] - altsums[1] + known_dotprod
    //     1  -  0 : altsums[0] - known_dotprod - unknown_hethet_d
    //     0  -  1 : altsums[1] - known_dotprod - unknown_hethet_d
    //     1  -  1 : known_dotprod
    double freq_rr = 1.0 - (altsums_d[0] + altsums_d[1] - known_dotprod_d) * twice_tot_recip;
    double freq_ra = (altsums_d[1] - known_dotprod_d - unknown_hethet_d) * twice_tot_recip;
    double freq_ar = (altsums_d[0] - known_dotprod_d - unknown_hethet_d) * twice_tot_recip;
    double freq_aa = known_dotprod_d * twice_tot_recip;
    const double half_unphased_hethet_share = unknown_hethet_d * twice_tot_recip;
    const double freq_rx = freq_rr + freq_ra + half_unphased_hethet_share;
    const double freq_ax = 1.0 - freq_rx;
    const double freq_xr = freq_rr + freq_ar + half_unphased_hethet_share;
    const double freq_xa = 1.0 - freq_xr;
    // frequency of ~2^{-46} is actually possible with dosages and 2 billion
    // samples, so set this threshold at 2^{-47}
    if ((freq_rx < (kSmallEpsilon * 0.125)) || (freq_ax < (kSmallEpsilon * 0.125))) {
      LOGERRPRINTFWW("Warning: Skipping --ld since %s is monomorphic across all valid observations.\n", ld_console_varids[0]);
      goto ld_console_ret_1;
    }
    if ((freq_xr < (kSmallEpsilon * 0.125)) || (freq_xa < (kSmallEpsilon * 0.125))) {
      LOGERRPRINTFWW("Warning: Skipping --ld since %s is monomorphic across all valid observations.\n", ld_console_varids[1]);
      goto ld_console_ret_1;
    }
    logprint("\n");
    LOGPRINTFWW("--ld %s %s:\n", ld_console_varids[0], ld_console_varids[1]);
    logprint("\n");
    
    char* write_poststop = &(g_logbuf[80]);
    uint32_t varid_slens[2];
    uint32_t cur_allele_ct = 2;
    for (uint32_t var_idx = 0; var_idx < 2; ++var_idx) {
      const uint32_t cur_variant_uidx = var_uidxs[var_idx];
      uintptr_t variant_allele_idx_base = cur_variant_uidx * 2;
      if (variant_allele_idxs) {
        variant_allele_idx_base = variant_allele_idxs[cur_variant_uidx];
        cur_allele_ct = variant_allele_idxs[cur_variant_uidx + 1] - variant_allele_idx_base;
      }
      char** cur_alleles = &(allele_storage[variant_allele_idx_base]);

      const char* cur_varid = ld_console_varids[var_idx];
      const uint32_t cur_varid_slen = strlen(ld_console_varids[var_idx]);
      varid_slens[var_idx] = cur_varid_slen;
      char* write_iter = memcpya(g_logbuf, cur_varid, cur_varid_slen);
      write_iter = strcpya(write_iter, " alleles:\n");
      *write_iter = '\0';
      wordwrapb(0);
      logprintb();
      write_iter = strcpya(g_logbuf, "  REF = ");
      const char* ref_allele = cur_alleles[0];
      const uint32_t ref_slen = strlen(ref_allele);
      if (ref_slen < 72) {
        write_iter = memcpyax(write_iter, ref_allele, ref_slen, '\n');
      } else {
        write_iter = memcpya(write_iter, ref_allele, 69);
        write_iter = strcpya(write_iter, "...\n");
      }
      *write_iter = '\0';
      logprintb();
      write_iter = strcpya(g_logbuf, "  ALT = ");
      uint32_t allele_idx = 1;
      while (1) {
        const char* cur_allele = cur_alleles[allele_idx];
        const uint32_t cur_slen = strlen(cur_allele);
        if ((uintptr_t)(write_poststop - write_iter) <= cur_slen) {
          char* write_ellipsis_start = &(g_logbuf[76]);
          if (write_ellipsis_start > write_iter) {
            const uint32_t final_char_ct = (uintptr_t)(write_ellipsis_start - write_iter);
            memcpy(write_iter, cur_allele, final_char_ct);
          }
          write_iter = memcpyl3a(write_ellipsis_start, "...");
          break;
        }
        write_iter = memcpya(write_iter, cur_allele, cur_slen);
        if (++allele_idx == cur_allele_ct) {
          break;
        }
        *write_iter++ = ',';
      }
      *write_iter++ = '\n';
      *write_iter = '\0';
      logprintb();
    }
    logprint("\n");
    char* write_iter = uint32toa(valid_obs_ct, g_logbuf);
    write_iter = strcpya(write_iter, " valid");
    if (y_ct) {
      write_iter = strcpya(write_iter, " male");
    }
    write_iter = strcpya(write_iter, " sample");
    if (valid_obs_ct != 1) {
      *write_iter++ = 's';
    }
    if (valid_x_male_ct && (!y_ct)) {
      write_iter = strcpya(write_iter, " (");
      write_iter = uint32toa(valid_x_male_ct, write_iter);
      write_iter = strcpya(write_iter, " male)");
    }
    if ((!is_nonx_haploid_or_mts[0]) && (!is_nonx_haploid_or_mts[1])) {
      write_iter = strcpya(write_iter, "; ");
      if (unknown_hethet_d == 0.0) {
        if (hethet_present) {
          write_iter = strcpya(write_iter, "all phased");
        } else {
          write_iter = strcpya(write_iter, "no het pairs present");
        }
      } else {
        // print_dosage assumes kDosageMax rather than kDosageMid multiplier
        const uint64_t unknown_hethet_int_dosage = (int64_t)(unknown_hethet_d * kDosageMax);
        write_iter = print_dosage(unknown_hethet_int_dosage, write_iter);
        write_iter = strcpya(write_iter, " het pair");
        if (unknown_hethet_int_dosage != kDosageMax) {
          *write_iter++ = 's';
        }
        write_iter = strcpya(write_iter, " statistically phased");
      }
    }
    strcpy(write_iter, ".\n");
    logprintb();

    uint32_t cubic_sol_ct = 0;
    uint32_t first_relevant_sol_idx = 0;
    uint32_t best_lnlike_mask = 0;
    double cubic_sols[3];
    if (half_unphased_hethet_share) {
      // detect degenerate cases to avoid e-17 ugliness
      // possible todo: when there are multiple solutions, mark the EM solution
      //   in some manner
      if ((freq_rr * freq_aa != 0.0) || (freq_ra * freq_ar != 0.0)) {
	// (f11 + x)(f22 + x)(K - x) = x(f12 + K - x)(f21 + K - x)
	// (x - K)(x + f11)(x + f22) + x(x - K - f12)(x - K - f21) = 0
	//   x^3 + (f11 + f22 - K)x^2 + (f11*f22 - K*f11 - K*f22)x
	// - K*f11*f22 + x^3 - (2K + f12 + f21)x^2 + (K + f12)(K + f21)x = 0
	cubic_sol_ct = cubic_real_roots(0.5 * (freq_rr + freq_aa - freq_ra - freq_ar - 3 * half_unphased_hethet_share), 0.5 * (freq_rr * freq_aa + freq_ra * freq_ar + half_unphased_hethet_share * (freq_ra + freq_ar - freq_rr - freq_aa + half_unphased_hethet_share)), -0.5 * half_unphased_hethet_share * freq_rr * freq_aa, cubic_sols);
        if (cubic_sol_ct > 1) {
          while (cubic_sols[cubic_sol_ct - 1] > half_unphased_hethet_share + kSmallishEpsilon) {
            --cubic_sol_ct;
          }
          if (cubic_sols[cubic_sol_ct - 1] > half_unphased_hethet_share - kSmallishEpsilon) {
            cubic_sols[cubic_sol_ct - 1] = half_unphased_hethet_share;
          }
          while (cubic_sols[first_relevant_sol_idx] < -kSmallishEpsilon) {
            ++first_relevant_sol_idx;
          }
          if (cubic_sols[first_relevant_sol_idx] < kSmallishEpsilon) {
            cubic_sols[first_relevant_sol_idx] = 0.0;
          }
        }
      } else {
        // At least one of {f11, f22} is zero, and one of {f12, f21} is zero.
        // Initially suppose that the zero-values are f11 and f12.  Then the
        // equality becomes
        //   x(f22 + x)(K - x) = x(K - x)(f21 + K - x)
        //   x=0 and x=K are always solutions; the rest becomes
        //     f22 + x = f21 + K - x
        //     2x = K + f21 - f22
        //     x = (K + f21 - f22)/2; in-range iff (f21 - f22) in (-K, K).
        // So far so good.  However, plink 1.9 incorrectly *always* checked
        // (f21 - f22) before 6 Oct 2017, when it needed to use all the nonzero
        // values.
        cubic_sols[0] = 0.0;
        const double nonzero_freq_xx = freq_rr + freq_aa;
        const double nonzero_freq_xy = freq_ra + freq_ar;
        // (current code still works if three or all four values are zero)
        if ((nonzero_freq_xx + kSmallishEpsilon < half_unphased_hethet_share + nonzero_freq_xy) && (nonzero_freq_xy + kSmallishEpsilon < half_unphased_hethet_share + nonzero_freq_xx)) {
          cubic_sol_ct = 3;
          cubic_sols[1] = (half_unphased_hethet_share + nonzero_freq_xy - nonzero_freq_xx) * 0.5;
          cubic_sols[2] = half_unphased_hethet_share;
        } else {
          cubic_sol_ct = 2;
          cubic_sols[1] = half_unphased_hethet_share;
        }
      }
      // cubic_sol_ct does not contain trailing too-large solutions
      if (cubic_sol_ct > first_relevant_sol_idx + 1) {
        logprint("Multiple phasing solutions; sample size, HWE, or random mating assumption may\nbe violated.\n\nHWE exact test p-values\n-----------------------\n");
        // (can't actually get here in nonx_haploid_or_mt case, impossible to
        // have a hethet)

        const uint32_t hwe_midp = (ldip->ld_console_modifier / kfLdConsoleHweMidp) & 1;
        uint32_t x_nosex_ct = 0; // usually shouldn't exist, but...
        uintptr_t* nosex_collapsed = nullptr;
        if (x_present) {
          x_nosex_ct = founder_ct - popcount_longs_intersect(founder_info, sex_nm, raw_sample_ctl);
          if (x_nosex_ct) {
            if (bigstack_alloc_ul(founder_ctl, &nosex_collapsed)) {
              goto ld_console_ret_NOMEM;
            }
            copy_bitarr_subset(sex_nm, founder_info, founder_ct, nosex_collapsed);
            bitarr_invert(founder_ct, nosex_collapsed);
          }
        }
        // Unlike plink 1.9, we don't restrict these HWE computations to the
        // nonmissing intersection.
        for (uint32_t var_idx = 0; var_idx < 2; ++var_idx) {
          const uintptr_t* cur_genovec = genovecs[var_idx];
          uint32_t genocounts[4];
          genovec_count_freqs_unsafe(cur_genovec, founder_ct, genocounts);
          double hwe_pval;
          if (!is_xs[var_idx]) {
            hwe_pval = SNPHWE2(genocounts[1], genocounts[0], genocounts[2], hwe_midp);
          } else {
            uint32_t male_genocounts[4];
            genovec_count_subset_freqs(cur_genovec, sex_male_collapsed_interleaved, founder_ct, x_male_ct, male_genocounts);
            assert(!male_genocounts[1]);
            if (x_nosex_ct) {
              uint32_t nosex_genocounts[4];
              genoarr_count_subset_freqs2(cur_genovec, nosex_collapsed, founder_ct, x_nosex_ct, nosex_genocounts);
              genocounts[0] -= nosex_genocounts[0];
              genocounts[1] -= nosex_genocounts[1];
              genocounts[2] -= nosex_genocounts[2];
            }
            hwe_pval = SNPHWEX(genocounts[1], genocounts[0] - male_genocounts[0], genocounts[2] - male_genocounts[2], male_genocounts[0], male_genocounts[2], hwe_midp);
          }
          LOGPRINTF("  %s: %g\n", ld_console_varids[var_idx], hwe_pval);
        }

        double best_unscaled_lnlike = -DBL_MAX;
        for (uint32_t sol_idx = first_relevant_sol_idx; sol_idx < cubic_sol_ct; ++sol_idx) {
          const double cur_unscaled_lnlike = em_phase_unscaled_lnlike(freq_rr, freq_ra, freq_ar, freq_aa, half_unphased_hethet_share, cubic_sols[sol_idx]);
          if (cur_unscaled_lnlike > best_unscaled_lnlike) {
            best_unscaled_lnlike = cur_unscaled_lnlike;
            best_lnlike_mask = 1 << sol_idx;
          } else if (cur_unscaled_lnlike == best_unscaled_lnlike) {
            best_lnlike_mask |= 1 << sol_idx;
          }
        }
      }
    } else {
      cubic_sol_ct = 1;
      cubic_sols[0] = 0.0;
    }
    logprint("\n");

    for (uint32_t sol_idx = first_relevant_sol_idx; sol_idx < cubic_sol_ct; ++sol_idx) {
      if (cubic_sol_ct - first_relevant_sol_idx > 1) {
        write_iter = strcpya(g_logbuf, "Solution #");
        write_iter = uint32toa(sol_idx + 1 - first_relevant_sol_idx, write_iter);
        if ((best_lnlike_mask >> sol_idx) & 1) {
	  write_iter = strcpya(write_iter, " (");
	  if (best_lnlike_mask & ((1 << sol_idx) - 1)) {
	    write_iter = strcpya(write_iter, "tied for ");
	  }
          write_iter = strcpya(write_iter, "best likelihood)");
        }
        strcpy(write_iter, ":\n");
        logprintb();
      }
      const double cur_sol_xx = cubic_sols[sol_idx];
      double dd = freq_rr + cur_sol_xx - freq_rx * freq_xr;
      if (fabs(dd) < kSmallEpsilon) {
        dd = 0.0;
      }
      write_iter = strcpya(g_logbuf, "  r^2 = ");
      write_iter = dtoa_g(dd * dd / (freq_rx * freq_xr * freq_ax * freq_xa), write_iter);
      write_iter = strcpya(write_iter, "    D' = ");
      double d_prime;
      if (dd >= 0.0) {
        d_prime = dd / MINV(freq_xr * freq_ax, freq_xa * freq_rx);
      } else {
        d_prime = -dd / MINV(freq_xr * freq_rx, freq_xa * freq_ax);
      }
      write_iter = dtoa_g(d_prime, write_iter);
      strcpy(write_iter, "\n");
      logprintb();

      logprint("\n");

      // Default layout:
      // [8 spaces]Frequencies      :        [centered varID[1]]
      //     (expectations under LE)           REF         ALT
      //                                    ----------  ----------
      //                               REF   a.bcdefg    a.bcdefg
      //                                    (a.bcdefg)  (a.bcdefg)
      //       [r-justified varID[0]]
      //                               ALT   a.bcdefg    a.bcdefg
      //                                    (a.bcdefg)  (a.bcdefg)
      //
      // (decimals are fixed-point, and trailing zeroes are erased iff there is
      // an exact match to ~13-digit precision; this is slightly more stringent
      // than plink 1.9's dtoa_f_w9p6_spaced() since there isn't much room here
      // for floating-point error to accumulate)
      // As for long variant IDs:
      // The default layout uses 54 columns, and stops working when
      // strlen(varID[0]) > 26.  So the right half can be shifted up to 25
      // characters before things get ugly in terminal windows.  Thus, once
      // string length > 51, we print only the first 48 characters of varID and
      // follow it with "...".
      // Similarly, when strlen(varID[1]) <= 51, centering is pretty
      // straightforward; beyond that, we also print only the first 48 chars.
      uint32_t extra_initial_spaces = 0;
      const uint32_t varid_slen0 = varid_slens[0];
      if (varid_slen0 > 26) {
        extra_initial_spaces = MINV(varid_slen0 - 26, 51);
      }
      write_iter = strcpya(g_logbuf, "        Frequencies      :  ");
      // default center column index is 43 + extra_initial_spaces; we're
      //   currently at column 28
      // for length-1, we want to occupy just the center column index; for
      //   length-2, both center and (center + 1), etc.
      // ((16 + extra_initial_spaces) * 2 - strlen(varID[1])) / 2
      const uint32_t varid_slen1 = varid_slens[1];
      if (varid_slen1 > 51) {
        write_iter = memcpya(write_iter, ld_console_varids[1], 48);
        write_iter = memcpyl3a(write_iter, "...");
      } else {
        uint32_t offset_x2 = (16 + extra_initial_spaces) * 2;
        if (offset_x2 > varid_slen1) {
          uint32_t varid1_padding = (offset_x2 - varid_slen1) / 2;
          if (varid1_padding + varid_slen1 > 51) {
            varid1_padding = 51 - varid_slen1;
          }
          write_iter = (char*)memseta(write_iter, 32, varid1_padding);
        }
        write_iter = memcpya(write_iter, ld_console_varids[1], varid_slen1);
      }
      strcpy(write_iter, "\n");
      logprintb();

      write_iter = strcpya(g_logbuf, "  (expectations under LE)");
      write_iter = (char*)memseta(write_iter, 32, extra_initial_spaces + 11);
      strcpy(write_iter, "REF         ALT\n");
      logprintb();

      write_iter = (char*)memseta(g_logbuf, 32, extra_initial_spaces + 33);
      strcpy(write_iter, "----------  ----------\n");
      logprintb();

      write_iter = strcpya(&(g_logbuf[28 + extra_initial_spaces]), "REF   ");
      write_iter = dtoa_f_probp6_spaced(freq_rr + cur_sol_xx, write_iter);
      write_iter = strcpya(write_iter, "    ");
      const double cur_sol_xy = half_unphased_hethet_share - cur_sol_xx;
      write_iter = dtoa_f_probp6_clipped(freq_ra + cur_sol_xy, write_iter);
      strcpy(write_iter, "\n");
      logprintb();

      write_iter = strcpya(&(g_logbuf[28 + extra_initial_spaces]), "     (");
      write_iter = dtoa_f_probp6_spaced(freq_xr * freq_rx, write_iter);
      write_iter = strcpya(write_iter, ")  (");
      write_iter = dtoa_f_probp6_clipped(freq_xa * freq_rx, write_iter);
      strcpy(write_iter, ")\n");
      logprintb();

      write_iter = g_logbuf;
      if (varid_slen0 < 26) {
        write_iter = &(write_iter[26 - varid_slen0]);
      }
      write_iter = memcpya(write_iter, ld_console_varids[0], varid_slen0);
      strcpy(write_iter, "\n");
      logprintb();

      write_iter = (char*)memseta(g_logbuf, 32, 28 + extra_initial_spaces);
      write_iter = strcpya(write_iter, "ALT   ");
      write_iter = dtoa_f_probp6_spaced(freq_ar + cur_sol_xy, write_iter);
      write_iter = strcpya(write_iter, "    ");
      write_iter = dtoa_f_probp6_clipped(freq_aa + cur_sol_xx, write_iter);
      strcpy(write_iter, "\n");
      logprintb();

      write_iter = strcpya(&(g_logbuf[28 + extra_initial_spaces]), "     (");
      write_iter = dtoa_f_probp6_spaced(freq_xr * freq_ax, write_iter);
      write_iter = strcpya(write_iter, ")  (");
      write_iter = dtoa_f_probp6_clipped(freq_xa * freq_ax, write_iter);
      strcpy(write_iter, ")\n");
      logprintb();

      logprint("\n");
      if (dd > 0.0) {
        logprint("  REF alleles are in phase with each other.\n\n");
      } else if (dd < 0.0) {
        logprint("  REF alleles are out of phase with each other.\n\n");
      }
    }
  }
  while (0) {
  ld_console_ret_NOMEM:
    reterr = kPglRetNomem;
    break;
  ld_console_ret_INCONSISTENT_INPUT_WW:
    wordwrapb(0);
    logerrprintb();
    reterr = kPglRetInconsistentInput;
    break;
  ld_console_ret_NO_VALID_OBSERVATIONS:
    logerrprint("Error: No valid observations for --ld.\n");
    reterr = kPglRetInconsistentInput;
    break;
  }
 ld_console_ret_1:
  bigstack_reset(bigstack_mark);
  return reterr;
}

#ifdef __cplusplus
} // namespace plink2
#endif
