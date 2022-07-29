/* This file is part of msolve.
 *
 * msolve is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * msolve is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with msolve.  If not, see <https://www.gnu.org/licenses/>
 *
 * Authors:
 * Jérémy Berthomieu
 * Christian Eder
 * Mohab Safey El Din */


typedef struct{
  uint32_t len; /* length of the encoded polynomial */
  uint32_t **modpcfs; /* array of arrays of coefficients
                       * modulo several primes
                       */
} modpolys_struct;

typedef modpolys_struct modpolys_t[1];

typedef struct {
  uint32_t alloc; /* alloc -> max number of primes */
  uint32_t nprimes; /* number of primes */
  uint32_t *primes; /* array of prime numbers */
  uint32_t npolys; /* number of polynomials */
  modpolys_t *modpolys; /* array of polynomials modulo primes */
} gb_modpoly_array_struct;

typedef gb_modpoly_array_struct gb_modpoly_t[1];

static inline void gb_modpoly_init(gb_modpoly_t modgbs,
                                   uint32_t alloc, uint32_t *lens,
                                   uint32_t npolys){
  modgbs->alloc = alloc;
  modgbs->nprimes = 0;
  modgbs->primes = calloc(sizeof(uint32_t), alloc);
  modgbs->npolys = npolys;
  modgbs->modpolys = malloc(sizeof(modpolys_struct) * npolys);
  for(uint32_t i = 0; i < npolys; i++){
    modgbs->modpolys[i]->len = lens[i];
    modgbs->modpolys[i]->modpcfs = malloc(sizeof(uint32_t **)*lens[i]);
    for(uint32_t j = 0; j < lens[i]; j++){
      modgbs->modpolys[i]->modpcfs[j] = calloc(sizeof(uint32_t), alloc);
    }
  }
}

static inline void gb_modpoly_realloc(gb_modpoly_t modgbs,
                                      uint32_t newalloc){
  uint32_t oldalloc = modgbs->alloc;
  modgbs->alloc += newalloc;
  uint32_t *newprimes = (uint32_t *)realloc(modgbs->primes,
                                            modgbs->alloc * sizeof(uint32_t));
  if(newprimes == NULL){
    fprintf(stderr, "Problem when reallocating modgbs (primes)\n");
    exit(1);
  }
  modgbs->primes = newprimes;
  for(uint32_t i = oldalloc; i < modgbs->alloc; i++){
    modgbs->primes[i] = 0;
  }
  for(uint32_t i = 0; i < modgbs->npolys; i++){
    for(uint32_t j = 0; j < modgbs->modpolys[i]->len; j++){
      uint32_t *newcfs = (uint32_t *)realloc(modgbs->modpolys[i]->modpcfs[j],
                                            modgbs->alloc);
      if(newcfs == NULL){
        fprintf(stderr, "Problem when reallocating modgbs (cfs)\n");
      }
      modgbs->modpolys[i]->modpcfs[j] = newcfs;
      for(uint32_t k = oldalloc; k < modgbs->alloc; k++){
        modgbs->modpolys[i]->modpcfs[j][k] = 0;
      }
    }
  }
}


static inline void display_gbmodpoly(FILE *file,
                                     gb_modpoly_t modgbs){
  fprintf(file, "alloc = %d\n", modgbs->alloc);
  fprintf(file, "nprimes = %d\n", modgbs->nprimes);
  fprintf(stderr, "primes = [");
  for(uint32_t i = 0; i < modgbs->alloc-1; i++){
    fprintf(file, "%d, ", modgbs->primes[i]);
  }
  fprintf(file, "%d]\n", modgbs->primes[modgbs->alloc -1]);
  fprintf(file, "numpolys = %d\n", modgbs->npolys);
  fprintf(file, "[\n");
  for(uint32_t i = 0; i < modgbs->npolys; i++){
    uint32_t len = modgbs->modpolys[i]->len;
    fprintf(file, "[%d, ", len);
    for(uint32_t j = 0; j < len; j++){
      fprintf(stderr, "[");
      for(uint32_t k = 0; k < modgbs->alloc-1; k++){
        fprintf(file, "%d, ", modgbs->modpolys[i]->modpcfs[j][k]);
      }
      if(j < len - 1){
        fprintf(file, "%d], ", modgbs->modpolys[i]->modpcfs[j][modgbs->alloc-1]);
      }
      else{
        fprintf(file, "%d]\n", modgbs->modpolys[i]->modpcfs[j][modgbs->alloc-1]);
      }
    }
    fprintf(file, "],\n");
  }
  fprintf(file, "]\n");
}

static inline void gb_modpoly_clear(gb_modpoly_t modgbs){
  free(modgbs->primes);
  for(uint32_t i = 0; i < modgbs->npolys; i++){
    for(uint32_t j = 0; j < modgbs->modpolys[i]->len; j++){
      free(modgbs->modpolys[i]->modpcfs[j]);
    }
    free(modgbs->modpolys[i]->modpcfs);
  }
  free(modgbs->modpolys);
}

static inline int32_t degree(int32_t *mon, int nv){
  int32_t deg = 0;
  for(int i = 0; i < nv; i++){
    deg += mon[i];
  }
  return deg;
}

/*
  bexp_lm is a list of monomials involving nv variables of increasing degrees
  return an array of length *nb containing at the i-th position the number of
  monomials of the i-th degree (hence nb is the number of different degrees).
 */
static inline int32_t *array_nbdegrees(int32_t *bexp_lm, int len,
                                     int nv, int * nb){
  *nb = 1;
  int32_t deg = degree(bexp_lm, nv);

  for(int32_t i = 1; i < len; i++){
    int32_t newdeg = degree(bexp_lm + i * nv, nv);

    if(deg != newdeg){
      (*nb)++;
      deg = newdeg;
    }
  }
  int32_t *ldeg = calloc(sizeof(int32_t), *nb);
  deg = degree(bexp_lm, nv);
  ldeg[0] = deg;
  int32_t i = 0, j = 1;
  while(j < len){
    int32_t newdeg = degree(bexp_lm + j * nv, nv);
    if(deg == newdeg){
      ldeg[i]++;
    }
    else{
      i++;
      ldeg[i] = 1;
      deg = newdeg;
    }
    j++;
  }
  return ldeg;
}

static inline int grevlex_is_less_than(int nv, int32_t* m1, int32_t *m2){
  int32_t deg1 = 0, deg2 = 0;
  for(int i = 0; i < nv; i++){
    deg1 += m1[i];
  }
  for(int i = 0; i < nv; i++){
    deg2 += m2[i];
  }
  fprintf(stderr, "[deg1 = %d, deg2 = %d]\n", deg1, deg2);
  if(deg1 < deg2){
    return 1;
  }
  if(deg1 > deg2){
    return 0;
  }
  for(int i = 0; i < nv; i++){
    if(m1[i] < m2[i]){
      return 0;
    }
  }
  return 1;
}

static inline int32_t compute_length(int32_t *mon, int nv,
                                     int32_t *basis, int dquot){
  fprintf(stderr, "mon = [");
  for(int i = 0; i < nv ; i++){
    fprintf(stderr, "%d, ", mon[i]);
  }
  fprintf(stderr, "\n");
  for(int i = dquot - 1; i >= 0; i--){
    if(!grevlex_is_less_than(nv, mon, basis + i * nv)){
      return i + 1;
    }
  }
}

/*
 * bexp_lm is an array of len  monomials with nv variables
 * basis is an array of dquot monomials with nv variables
 * (basis of some quotient defined with bexp_lm, all with grevlex order)
 * basis is sorted increasingly, as for bexp_lm
 *
 * returns an array of length len containing the maximum possible lengths
 * of polynomials with leading terms in bexp_lm (length excluding the
 * leading term).
 */
static inline int32_t *array_of_lengths(int32_t *bexp_lm, int len,
                                        int32_t *basis, int dquot, int nv){
  int32_t *lens = calloc(sizeof(int32_t), len);
  for(int i = 0; i < len; i++){
    lens[i] = compute_length(bexp_lm + (i * nv), nv, basis, dquot);
  }
  return lens;
}

/* returns 0 in case of failure else returns 1 */
static inline int modpgbs_set(gb_modpoly_t modgbs,
                               const bs_t *bs, const ht_t * const ht,
                               const int32_t fc,
                               int32_t *basis, const int dquot,
                               int *mgb){
  if(modgbs->nprimes >= modgbs->alloc-1){
    fprintf(stderr, "Not enough space in modgbs\n");
    return 0;
  }
  modgbs->primes[modgbs->nprimes] = fc;

  len_t i, j, k, idx;

  len_t len   = 0;
  hm_t *hm    = NULL;

  const len_t nv  = ht->nv;
  const len_t ebl = ht->ebl;
  const len_t evl = ht->evl;

  int *evi    =   (int *)malloc((unsigned long)ht->nv * sizeof(int));
  if (ebl == 0) {
    for (i = 1; i < evl; ++i) {
      evi[i-1]    =   i;
    }
  } else {
    for (i = 1; i < ebl; ++i) {
      evi[i-1]    =   i;
    }
    for (i = ebl+1; i < evl; ++i) {
      evi[i-2]    =   i;
    }
  }

  for(i = 0; i < modgbs->npolys; i++){
    idx = bs->lmps[i];
    if (bs->hm[idx] == NULL) {
      fprintf(stderr, " poly is 0\n");
      exit(1);
    } else {
      hm  = bs->hm[idx]+OFFSET;
      len = bs->hm[idx][LENGTH];
    }
    int bc = modgbs->modpolys[i]->len;
    for (j = 1; j < len; ++j) {
      uint32_t c = bs->cf_32[bs->hm[idx][COEFFS]][j];
      for (k = 0; k < nv; ++k) {
          mgb[k] = ht->ev[hm[j]][evi[k]];
      }
      while(!is_equal_exponent(mgb, basis + (bc * nv), nv)){
        bc--;
      }
      modgbs->modpolys[i]->modpcfs[bc][modgbs->nprimes] = c;
      bc--;
    }
  }

  modgbs->nprimes++;

  free(evi);
  return 1;
}


static int32_t * gb_modular_trace_learning(
                                           int32_t *num_gb,
                                           int32_t **leadmons,
                                           trace_t *trace,
                                           ht_t *tht,
                                           bs_t *bs_qq,
                                           ht_t *bht,
                                           stat_t *st,
                                           const int32_t fc,
                                           int info_level,
                                           int print_gb,
                                           int *dim,
                                           long *dquot_ori,
                                           data_gens_ff_t *gens,
                                           files_gb *files,
                                           int *success)
{
    double ca0, rt;
    ca0 = realtime();

    bs_t *bs = NULL;
    if(gens->field_char){
      bs = bs_qq;
      int boo = core_gba(&bs, &bht, &st);
      if (!boo) {
        printf("Problem with F4, stopped computation.\n");
        exit(1);
      }
      free_shared_hash_data(bht);
    }
    else{
      if(st->laopt > 40){
        bs = modular_f4(bs_qq, bht, st, fc);
      }
      else{
        bs = gba_trace_learning_phase(trace, tht, bs_qq, bht, st, fc);
      }
    }
    rt = realtime()-ca0;

    if(info_level > 1){
        fprintf(stderr, "Learning phase %.2f Gops/sec\n",
                (st->trace_nr_add+st->trace_nr_mult)/1000.0/1000.0/rt);
    }
    if(info_level > 2){
        fprintf(stderr, "------------------------------------------\n");
        fprintf(stderr, "#ADDITIONS       %13lu\n", (unsigned long)st->trace_nr_add * 1000);
        fprintf(stderr, "#MULTIPLICATIONS %13lu\n", (unsigned long)st->trace_nr_mult * 1000);
        fprintf(stderr, "#REDUCTIONS      %13lu\n", (unsigned long)st->trace_nr_red);
        fprintf(stderr, "------------------------------------------\n");
    }

    /* Leading monomials from Grobner basis */
    int32_t *bexp_lm = get_lm_from_bs(bs, bht);
    leadmons[0] = bexp_lm;
    num_gb[0] = bs->lml;

    if(bs->lml == 1){
        if(info_level){
            fprintf(stderr, "Grobner basis has a single element\n");
        }
        int is_empty = 1;
        for(int i = 0; i < bht->nv; i++){
            if(bexp_lm[i]!=0){
                is_empty = 0;
            }
        }
        if(is_empty){
            *dquot_ori = 0;
            *dim = 0;
            if(info_level){
              fprintf(stderr, "No solution\n");
            }
            print_ff_basis_data(
                                files->out_file, "a", bs, bht, st, gens, print_gb);
            return NULL;
        }
    }

      if(st->fc == 0){

        /**************************************************/
        long dquot = 0;
        int32_t *lmb = monomial_basis_enlarged(bs->lml, bht->nv,
                                               bexp_lm, &dquot);

        /************************************************/
        /************************************************/
        fprintf(stderr, "nvars = %d\n", st->nvars);
        fprintf(stderr, "dquot = %ld\n", dquot);
        for(int32_t i = 0; i < dquot; i++){
          fprintf(stderr, "[");
          for(int32_t j = 0; j < st->nvars - 1; j++){
            fprintf(stderr, "%d, ", lmb[j+i*st->nvars]);
          }
          fprintf(stderr, "%d], ", lmb[st->nvars - 1 + i*st->nvars]);
        }
        fprintf(stderr, "\n");
        /************************************************/
        /************************************************/

        int nb = 0;
        int32_t *ldeg = array_nbdegrees(bexp_lm, bs->lml, bht->nv, &nb);
        fprintf(stderr, "nb = %d => ldeg = [", nb);

        /************************************************/
        /************************************************/
        for(int i = 0; i < nb; i++){
          fprintf(stderr, "%d, ", ldeg[i]);
        }
        fprintf(stderr, "]\n");
        /************************************************/
        /************************************************/

        int32_t *lens = array_of_lengths(bexp_lm, bs->lml, lmb, dquot, bht->nv);

        /************************************************/
        /************************************************/
        fprintf(stderr, "lens = [");
        for(int32_t i = 0; i < bs->lml; i++){
          fprintf(stderr, "%d, ", lens[i]);
        }
        fprintf(stderr, "]\n");
        /************************************************/
        /************************************************/
        gb_modpoly_t modgbs;
        gb_modpoly_init(modgbs, 3, lens, bs->lml);
        display_gbmodpoly(stderr, modgbs);
        gb_modpoly_realloc(modgbs, 2);
        display_gbmodpoly(stderr, modgbs);

        uint32_t *mgb = calloc(sizeof(uint32_t), bht->nv);
        modpgbs_set(modgbs, bs, bht, fc, lmb, dquot, mgb);

        display_gbmodpoly(stderr, modgbs);
        fprintf(stderr, "Clear gb_modpolys\n");
        free(mgb);
        gb_modpoly_clear(modgbs);
      }
      else{
        print_ff_basis_data(
                            files->out_file, "a", bs, bht, st, gens, print_gb);
      }
}


static void gb_modular_trace_application(int32_t *num_gb,
                                         int32_t **leadmons_ori,
                                         int32_t **leadmons_current,

                                         trace_t **btrace,
                                         ht_t **btht,
                                         const bs_t *bs_qq,
                                         ht_t **bht,
                                         stat_t *st,
                                         const int32_t fc,
                                         int info_level,
                                         bs_t **bs,
                                         int32_t *lmb_ori,
                                         int32_t dquot_ori,
                                         primes_t *lp,
                                         data_gens_ff_t *gens,
                                         double *stf4,
                                         int *bad_primes){

  st->info_level = 0;

  /* tracing phase */
  len_t i;
  double ca0;

  /* F4 and FGLM are run using a single thread */
  /* st->nthrds is reset to its original value afterwards */
  const int nthrds = st->nthrds;
  st->nthrds = 1 ;
  /*at the moment multi-threading is not supprted here*/
  memset(bad_primes, 0, (unsigned long)st->nprimes * sizeof(int));
  #pragma omp parallel for num_threads(st->nthrds/* nthrds */)  \
    private(i) schedule(static)
  for (i = 0; i < st->nprimes; ++i){
    ca0 = realtime();
    if(st->laopt > 40){
      bs[i] = modular_f4(bs_qq, bht[i], st, lp->p[i]);
    }
    else{
      bs[i] = gba_trace_application_phase(btrace[i], btht[i], bs_qq, bht[i], st, lp->p[i]);
    }
    *stf4 = realtime()-ca0;
    /* printf("F4 trace timing %13.2f\n", *stf4); */

    if(bs[i]->lml != num_gb[i]){
      if (bs[i] != NULL) {
        free_basis(&(bs[i]));
      }
      bad_primes[i] = 1;
      /* return; */
    }
    get_lm_from_bs_trace(bs[i], bht[i], leadmons_current[i]);

    if(!equal_staircase(leadmons_current[i], leadmons_ori[i],
                       num_gb[i], num_gb[i], bht[i]->nv)){
      bad_primes[i] = 1;
    }
    if (bs[i] != NULL) {
      free_basis(&(bs[i]));
    }
  }
  st->nthrds = nthrds;
}
