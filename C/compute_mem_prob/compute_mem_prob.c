#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>


// MACROS //

#define LIBNAME "compute_mem_prob"
#define VERSION "0.9 04-27-2018"

#define YES 1
#define NO  0

#define SUCCESS 1
#define FAILURE 0

// Maximum allowed number of duplicates.
#define MAXN 1024

// Compute omega and tilde-omega.
#define  OMEGA ( P*pow(1.0-U/3, N) )
#define _OMEGA ( P*(1-pow(1.0-U/3, N)) )

// Creation of a new 'trunc_pol_t' is just a call to 'calloc()'.
#define new_zero_trunc_pol() calloc(1, KSZ)

// Prob that one of m altnerative threads survives i steps.
#define xi(i,m) ( 1.0 - pow( 1.0 - pow(1.0-U,(i)), (m) ))

// Calculation intermediates (one index).
#define aN(i) pow( 1.0 - pow(1.0-U,(i)) * U/3.0, N )
#define gN(i) pow( 1.0 - pow(1.0-U,(i)), N )
#define dN(i) pow( 1.0 - (1.0 - U + U*U/3.0) * pow(1.0-U,(i)), N )

// Calculation intermediates (two indices).
#define bN(j,i) pow( 1.0 - pow(1.0-U,(j))*U/3.0 - \
                           pow(1.0-U,(i))*(1.0-U/3.0), N)


// Macro to simplify error handling.
#define handle_memory_error(x) do { \
   if ((x) == NULL) { \
      warning("memory_error", __func__, __LINE__); \
      ERRNO = __LINE__; \
      goto in_case_of_failure; \
}} while (0)


//  TYPE DECLARATIONS  //

typedef struct trunc_pol_t  trunc_pol_t;
typedef struct matrix_t     matrix_t;
typedef struct monomial_t   monomial_t;

// TODO : explain that monomials must not be just deg and coeff //

struct monomial_t {
   size_t deg;           // Degree of the coefficent.
   double coeff;         // Value of the coefficient
};

struct trunc_pol_t {
   monomial_t mono;      // Monomial (if applicable).
   double coeff[];       // Terms of the polynomial.
};

struct matrix_t {
   const size_t    dim;  // Column / row number.
   trunc_pol_t * term[]; // Terms of the matrix.
};



// GLOBAL VARIABLES / CONSTANTS //

size_t    G;       // Minimum size of MEM seeds.
size_t    N;       // Number of duplicate sequences.
size_t    K;       // Max degree of the polynomials (read size).

size_t    HIGH;    // Proxy for infinite degree polynomials.

double    P;       // Probability of a read error.
double    U;       // Divergence rate between duplicates.

size_t    KSZ;     // Size of the 'trunc_pol_t' struct.

trunc_pol_t * TEMP = NULL;        // For matrix multipliciation.
trunc_pol_t * ARRAY[MAXN] = {0};  // Store the results (indexed by N).

int       ERRNO = 0;
char      ERRMSG[1024] = {0};

// Error message.
const char internal_error[] =
   "internal error (please contact guillaume.filion@gmail.com)";



// FUNCTION DEFINITIONS //

// VISLBLE IO and error report functions.
int    get_mem_prob_error_code (void) { return ERRNO; }
char * get_mem_prob_error_msg (void) { return ERRMSG; }
void   reset_mem_prob_error (void) { ERRNO = 0; }


void
warning
(
   const char * msg,
   const char * function,
   const int    line
)
// Print warning message to stderr or copy it to internal
// variable ERRMSG for the user to consult later.
{
   fprintf(stderr, "[%s] error in function `%s' (line %d): %s\n",
         LIBNAME, function, line, msg);
}


int
set_params_mem_prob // VISIBLE //
(
   size_t g,
   size_t k,
   double p,
   double u
)
// Initialize the global variables from user-defined values.
{

   // Check input
   if (p <= 0.0 || p >= 1.0) {
      ERRNO = __LINE__;
      warning("parameter p must be between 0 and 1",
            __func__, __LINE__); 
      goto in_case_of_failure;
   }

   if (u <= 0.0 || u >= 1.0) {
      ERRNO = __LINE__;
      warning("parameter u must be between 0 and 1",
            __func__, __LINE__); 
      goto in_case_of_failure;
   }

   G = g;  // MEM size.
   K = k;  // Read size.
   P = p;  // Sequencing error.
   U = u;  // Divergence rate.

   // Set 'HIGH' to the greater of 'K' or 'G'.
   HIGH = K > G ? K : G;

   // All 'trunc_pol_t' must be congruent.
   KSZ = sizeof(trunc_pol_t) + (K+1) * sizeof(double);

   // Clean previous values (if any).
   for (int i = 0 ; i < MAXN ; i++) free(ARRAY[i]);
   bzero(ARRAY, MAXN * sizeof(trunc_pol_t *));

   // Allocate or reallocate 'TEMP'.
   free(TEMP);
   TEMP = calloc(1, KSZ);
   handle_memory_error(TEMP);

   return SUCCESS;

in_case_of_failure:
   return FAILURE;

}


void
clean_mem_prob // VISIBLE //
(void)
{

   free(TEMP);
   TEMP = NULL;

   for (int i = 0 ; i < MAXN ; i++) free(ARRAY[i]);
   bzero(ARRAY, MAXN * sizeof(trunc_pol_t *));

   ERRNO = 0;
   bzero(ERRMSG, 1024);

   return;

}



trunc_pol_t *
new_trunc_pol_A
(
   const size_t deg,  // Degree of polynomial D.
   const size_t N,    // Number of duplicates.
   const int    tilde // Return D or tilde D.
)
{

   if (deg > K || deg == 0) {
      warning(internal_error, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   // See definition of polynomial A.
   const int d = deg <= G ? deg : G;
   const double cst = tilde ? _OMEGA : OMEGA;
   double pow_of_q = 1.0;
   for (int i = 1 ; i <= d ; i++) {
      new->coeff[i] = cst * (xi(i-1,N)) * pow_of_q;
      pow_of_q *= (1.0-P);
   }
   // Terms of the polynomials with degree higher than 'G' (if any).
   for (int i = d+1 ; i <= deg ; i++) {
      new->coeff[i] = P * (1-aN(i-1)) * pow_of_q;
      pow_of_q *= (1.0-P);
   }

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_B
(
   const size_t deg,  // Degree of polynomial B.
   const size_t N,    // Number of duplicates.
   const int    tilde // Return D or tilde B.
)
{

   if (deg > K || deg == 0) {
      warning(internal_error, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   // See definition of polynomial B.
   const double cst = tilde ? _OMEGA : OMEGA;
   const double denom = 1.0 - pow(1-U/3.0, N);
   double pow_of_q = 1.0;
   for (int i = 1 ; i <= deg ; i++) {
      double numer = 1.0 - aN(i-1);
      new->coeff[i] = cst * numer / denom * pow_of_q;
      pow_of_q *= (1.0-P);
   }

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_C
(
   const size_t deg,  // Degree of polynomial C.
   const size_t N,    // Number of duplicates.
   const int    tilde // Return C or tilde C.
)
{

   // Avoid division by zero when N = 1 (not a failure).
   if (N == 1) return NULL;

   if (deg > K || deg == 0) {
      warning(internal_error, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   // See definition of polynomial C.
   const int j = G - deg;
   const double denom = aN(j) - aN(j-1) - gN(j) + dN(j-1);
   const double cst = tilde ? _OMEGA : OMEGA;
   double pow_of_q = 1.0;
   for (int i = 1 ; i <= deg ; i++) {
      double numer = aN(j) - aN(j-1) - bN(j,i+j-1) + bN(j-1,i+j-1);
      new->coeff[i] = cst * numer / denom * pow_of_q;
      pow_of_q *= (1.0-P);
   }

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_D
(
   const size_t deg,  // Degree of polynomial D.
   const size_t N,    // Number of duplicates.
   const int    tilde // Return D or tilde D.
)
{

   if (deg > K || deg == 0) {
      warning(internal_error, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   // See definition of polynomial D.
   const double cst = tilde ? _OMEGA : OMEGA;
   double pow_of_q = 1.0;
   for (int i = 1 ; i <= deg ; i++) {
      new->coeff[i] = cst * pow_of_q;
      pow_of_q *= (1.0-P);
   }

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_u
(
   const size_t deg,  // Degree of polynomial u.
   const size_t N     // Number of duplicates.
)
{

   if (deg > K || deg >= G || deg == 0) {
      warning(internal_error, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   // See definition of polynomial u.
   new->mono.deg = deg;
   new->mono.coeff = (xi(deg-1,N) - xi(deg,N)) * pow(1.0-P,deg);
   new->coeff[deg] = new->mono.coeff;

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_v
(
   const size_t deg,  // Degree of polynomial v.
   const size_t N     // Number of duplicates.
)
{

   if (deg > K || deg >= G || deg == 0) {
      warning(internal_error, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   // See definition of polynomial v.
   new->mono.deg = deg;
   double numer =  aN(deg) - aN(deg-1) - gN(deg) + dN(deg-1);
   double denom = 1.0 - pow(1-U/3.0, N);
   new->mono.coeff = numer / denom * pow(1.0-P, deg);
   new->coeff[deg] = new->mono.coeff;

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_w
(
   const size_t deg,  // Degree of polynomial w.
   const size_t N     // Number of duplicates.
)
{

   if (deg > K || deg >= G || deg == 0) {
      warning(internal_error, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   // See definition of polynomial w.
   new->mono.deg = deg;
   double numer = gN(deg) - dN(deg-1);
   double denom = 1.0 - pow(1.0-U/3.0, N);
   new->mono.coeff = numer / denom * pow(1.0-P, deg);
   new->coeff[deg] = new->mono.coeff;

   return new;

in_case_of_failure:
   return NULL;

}



trunc_pol_t *
new_trunc_pol_y
(
   const int j,      // Initial state.
   const int i,      // Degree of the polynomial.
   const size_t N    // Number of duplicates.
)
{

   // Avoid division by zero when N = 1 (not a failure).
   if (N == 1) return NULL;

   if (i > K || i >= G || i == 0) {
      warning(internal_error, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   // See definition of polynomial y.
   new->mono.deg = i;
   double numer = bN(j,j+i) - bN(j,j+i-1) - bN(j-1,i+j) + bN(j-1,j+i-1);
   double denom = aN(j) - aN(j-1) - gN(j) + dN(j-1);
   new->mono.coeff = numer / denom * pow(1.0-P, i);
   new->coeff[i] = new->mono.coeff;

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_T_down
(
   const size_t N    // Number of duplicates.
)
{

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   const double denom = 1.0 - pow(1-U/3.0, N);
   double pow_of_q = 1.0;
   for (int i = 0 ; i <= HIGH ; i++) {
      double numer = 1.0 - aN(i);
      new->coeff[i] = numer / denom * pow_of_q;
      pow_of_q *= (1.0-P);
   }

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_T_double_down
(
   const size_t N    // Number of duplicates.
)
{

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   double pow_of_q = 1.0;
   for (int i = 0 ; i <= G-1 ; i++) {
      new->coeff[i] = xi(i,N) * pow_of_q;
      pow_of_q *= (1-P);
   }

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_T_up
(
   size_t deg,       // Degree of the polynomial.
   const size_t N    // Number of duplicates.
)
{

   if (deg > K || deg >= G) {
      warning(internal_error, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   double pow_of_q = 1.0;
   for (int i = 0 ; i <= deg ; i++) {
      new->coeff[i] = pow_of_q;
      pow_of_q *= (1-P);
   }

   return new;

in_case_of_failure:
   return NULL;

}


trunc_pol_t *
new_trunc_pol_T_sim
(
   size_t deg,       // Degree of the polynomial.
   const size_t N    // Number of duplicates.
)
{

   // Avoid division by zero when N = 1 (not a failure).
   if (N == 1) return NULL;

   if (deg > K || deg >= G) {
      warning(internal_error, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   trunc_pol_t *new = new_zero_trunc_pol();
   handle_memory_error(new);

   const int j = G-1 - deg;
   const double denom = aN(j) - aN(j-1) - gN(j) + dN(j-1);
   double pow_of_q = 1.0;
   for (int i = 0 ; i <= deg ; i++) {
      double numer = aN(j) - aN(j-1) - bN(j,i+j) + bN(j-1,i+j);
      new->coeff[i] = numer / denom * pow_of_q;
      pow_of_q *= (1.0-P);
   }

   return new;

in_case_of_failure:
   return NULL;

}



matrix_t *
new_null_matrix
(
   const size_t dim
)
// Create a matrix where all truncated polynomials
// (trunc_pol_t) are set NULL.
{

   // Initialize to zero.
   size_t sz = sizeof(matrix_t) + dim*dim * sizeof(trunc_pol_t *);
   matrix_t *new = calloc(1, sz);
   handle_memory_error(new);

   // The dimension is set upon creation
   // and must never change afterwards.
   *(size_t *)&new->dim = dim;

   return new;

in_case_of_failure:
   return NULL;

}


void
destroy_mat
(
   matrix_t * mat
)
{

   // Do not try anything on NULL.
   if (mat == NULL) return;

   size_t nterms = (mat->dim)*(mat->dim);
   for (size_t i = 0 ; i < nterms ; i++)
      free(mat->term[i]);
   free(mat);

}



matrix_t *
new_zero_matrix
(
   const size_t dim
)
// Create a matrix where all terms
// are 'trunc_pol_t' struct set to zero.
{

   matrix_t *new = new_null_matrix(dim);
   handle_memory_error(new);

   for (int i = 0 ; i < dim*dim ; i++) {
      new->term[i] = new_zero_trunc_pol();
      handle_memory_error(new->term[i]);
   }

   return new;

in_case_of_failure:
   destroy_mat(new);
   return NULL;

}



matrix_t *
new_matrix_M
(
   const size_t N    // Number of duplicates.
)
{

   const size_t dim = 2*G+2;
   matrix_t *M = new_null_matrix(dim);
   handle_memory_error(M);

   // First row.
   M->term[0*dim+1] = new_zero_trunc_pol();
   handle_memory_error(M->term[0*dim+1]);
   M->term[0*dim+1]->coeff[0] = 1.0;
   M->term[0*dim+1]->mono.coeff = 1.0;

   // Second row.
   M->term[1*dim+1] = new_trunc_pol_A(G, N, NO);
   handle_memory_error(M->term[1*dim+1]);
   M->term[1*dim+2] = new_trunc_pol_A(HIGH, N, YES);
   handle_memory_error(M->term[1*dim+2]);
   for (int j = 1 ; j <= G-1 ; j++) {
      M->term[1*dim+(j+G+1)] = new_trunc_pol_u(j, N);
      handle_memory_error(M->term[1*dim+(j+G+1)]);
   }
   M->term[1*dim+dim-1] = new_trunc_pol_T_double_down(N);
   handle_memory_error(M->term[1*dim+dim-1]);

   // Third row.
   M->term[2*dim+1] = new_trunc_pol_B(HIGH, N, NO);
   handle_memory_error(M->term[2*dim+1]);
   M->term[2*dim+2] = new_trunc_pol_B(HIGH, N, YES);
   handle_memory_error(M->term[2*dim+2]);
   for (int j = 1 ; j <= G-1 ; j++) {
      M->term[2*dim+j+2] = new_trunc_pol_v(j, N);
      handle_memory_error(M->term[2*dim+j+2]);
   }
   for (int j = 1 ; j <= G-1 ; j++) {
      M->term[2*dim+j+G+1] = new_trunc_pol_w(j, N);
      handle_memory_error(M->term[2*dim+j+G+1]);
   }
   M->term[2*dim+dim-1] = new_trunc_pol_T_down(N);
   handle_memory_error(M->term[2*dim+dim-1]);

   // First series of middle rows.
   for (int j = 1 ; j <= G-1 ; j++) {
      M->term[(j+2)*dim+1] = new_trunc_pol_C(G-j, N, NO);
      handle_memory_error(M->term[(j+2)*dim+1]);
      M->term[(j+2)*dim+2] = new_trunc_pol_C(G-j, N, YES);
      handle_memory_error(M->term[(j+2)*dim+2]);
      for (int i = 1 ; i <= G-j-1 ; i++) {
         M->term[(j+2)*dim+G+j+i+1] = new_trunc_pol_y(j, i ,N);
         handle_memory_error(M->term[(j+2)*dim+G+j+i+1]);
      }
      M->term[(j+2)*dim+dim-1] = new_trunc_pol_T_sim(G-j-1, N);
      handle_memory_error(M->term[(j+2)*dim+dim-1]);
   }

   // Second series of middle rows.
   for (int j = 1 ; j <= G-1 ; j++) {
      M->term[(j+G+1)*dim+1] = new_trunc_pol_D(G-j, N, NO);
      handle_memory_error(M->term[(j+G+1)*dim+1]);
      M->term[(j+G+1)*dim+2] = new_trunc_pol_D(G-j, N, YES);
      handle_memory_error(M->term[(j+G+1)*dim+2]);
      M->term[(j+G+1)*dim+dim-1] = new_trunc_pol_T_up(G-j-1, N);
      handle_memory_error(M->term[(j+G+1)*dim+dim-1]);
   }

   // Last row is null.

   return M;

in_case_of_failure:
   destroy_mat(M);
   return NULL;

}


trunc_pol_t *
trunc_pol_mult
(
         trunc_pol_t * dest,
   const trunc_pol_t * a,
   const trunc_pol_t * b
)
{


   // If any of the two k-polynomials is zero,
   // set 'dest' to zero and return 'NULL'.
   if (a == NULL || b == NULL) {
      bzero(dest, KSZ);
      return NULL;
   }

   if (a->mono.coeff && b->mono.coeff) {
      // Both are monomials, just do one multiplication.
      bzero(dest, KSZ);
      // If degree is too high, all coefficients are zero.
      if (a->mono.deg + b->mono.deg > K)
         return NULL;
      // Otherwise do the multiplication.
      dest->mono.deg = a->mono.deg + b->mono.deg;
      dest->mono.coeff = a->mono.coeff * b->mono.coeff;
      dest->coeff[dest->mono.deg] = dest->mono.coeff;
   }
   else if (a->mono.coeff) {
      // 'a' is a monomial, do one "row" of multiplications.
      bzero(dest, KSZ);
      for (int i = a->mono.deg ; i <= K ; i++)
         dest->coeff[i] = a->mono.coeff * b->coeff[i-a->mono.deg];
   }
   else if (b->mono.coeff) {
      // 'b' is a monomial, do one "row" of multiplications.
      bzero(dest, KSZ);
      for (int i = b->mono.deg ; i <= K ; i++)
         dest->coeff[i] = b->mono.coeff * a->coeff[i-b->mono.deg];
   }
   else {
      // Standard convolution product.
      for (int i = 0 ; i <= K ; i++) {
         dest->coeff[i] = a->coeff[0] * b->coeff[i];
         for (int j = 1 ; j <= i ; j++) {
            dest->coeff[i] += a->coeff[j] * b->coeff[i-j];
         }
      }
   }

   return dest;

}



void
trunc_pol_update_add
(
         trunc_pol_t * dest,
   const trunc_pol_t * a
)
{

   // No update when adding zero.
   if (a == NULL) return;

   for (int i = 0 ; i <= K ; i++) {
      dest->coeff[i] += a->coeff[i];
   }

}



matrix_t *
matrix_mult
(
         matrix_t * dest,
   const matrix_t * a,
   const matrix_t * b
)
{

   if (a->dim != dest->dim || b->dim != dest->dim) {
      warning(internal_error, __func__, __LINE__);
      ERRNO = __LINE__;
      return NULL;
   }

   size_t dim = dest->dim;

   for (int i = 0 ; i < dim ; i++) {
   for (int j = 0 ; j < dim ; j++) {
      // Erase destination entry.
      bzero(dest->term[i*dim+j], KSZ);
      for (int m = 0 ; m < dim ; m++) {
         trunc_pol_update_add(dest->term[i*dim+j],
            trunc_pol_mult(TEMP, a->term[i*dim+m], b->term[m*dim+j]));
      }
   }
   }

   return dest;

}


double
compute_mem_prob // VISIBLE //
(
   const size_t N,    // Number of duplicates.
   const size_t k     // Segment or read size.
)
{

   // Those variables must be declared here so that
   // they can be cleaned in case of failure.
   trunc_pol_t  *w = NULL;
   matrix_t     *M = NULL;
   matrix_t *powM1 = NULL;
   matrix_t *powM2 = NULL;

   // Check input.
   if (N > MAXN-1) {
      char msg[128];
      snprintf(msg, 128, "argument N greater than %d", MAXN);
      warning(msg, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   if (k > K) {
      char msg[128];
      snprintf(msg, 128, "argument k greater than set value (%ld)", K);
      warning(msg, __func__, __LINE__);
      ERRNO = __LINE__;
      goto in_case_of_failure;
   }

   if (ARRAY[N] == NULL) {
      // Need to compute truncated genearting function and store.
      
      w = new_zero_trunc_pol();
      handle_memory_error(w);
      M = new_matrix_M(N);
      handle_memory_error(M);
      powM1 = new_zero_matrix(2*G+2);
      handle_memory_error(powM1);
      powM2 = new_zero_matrix(2*G+2);
      handle_memory_error(powM2);

      matrix_mult(powM1, M, M);
      trunc_pol_update_add(w, powM1->term[2*G+1]);

      // FIXME: 10 must be replaced by a smart number. //
      for (int i = 0 ; i < 10 ; i++) {
         matrix_mult(powM2, powM1, M);
         trunc_pol_update_add(w, powM2->term[2*G+1]);
         matrix_mult(powM1, powM2, M);
         trunc_pol_update_add(w, powM1->term[2*G+1]);
      }

      // Clean temporary variables.
      destroy_mat(powM1);
      destroy_mat(powM2);
      destroy_mat(M);

      ARRAY[N] = w;

   }

   return ARRAY[N]->coeff[k];

in_case_of_failure:
   // Clean everything.
   free(w);
   destroy_mat(powM1);
   destroy_mat(powM2);
   destroy_mat(M);
   // Return 'nan'.
   return 0.0/0.0;

}