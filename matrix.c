/* Source file for matrix operations */

#include "matrix.h"
#include "matrix_utils.h"

#include <limits.h>
#include <string.h>
#include <immintrin.h>

/* enum declarations */

/* Defines common operations that can be performed on an element-by-element basis */
typedef enum op_code
{
    ADD,      /* Add op1 to op2 */
    SUBTRACT, /* Subtract op2 from op1 */
    MULTIPLY, /* Multiply op1 by op2 */
    DIVIDE    /* Divide op1 by op2 */
} op_code;

/* Global Variables */

static matrix _recycled_list = NULL; /* Linked list of recycled matrices. Those recycled matrices have garbage values and should NEVER be used. */

/* Functions*/

/* Create a matrix based on given number rows and cols.

   If rows * cols exceeds the upper limit of size_t, or if rows and cols are completely invalid,
   or if no extra memory is available, return NULL. */
matrix create_matrix(const size_t rows, const size_t cols)
{
    /* Check parameters */
    if (__SIZE_MAX__ / rows > cols)
        return NULL;

    matrix result; /* Store the result matrix */
    float *arr;    /* Store the result float array */

    if (_recycled_list != NULL) /* Reuse existing one first. */
    {
        result = _recycled_list;
        _recycled_list = _recycled_list->next;
    }
    else
    {
        result = (matrix)malloc(sizeof(matrix_struct));
        if (result == NULL)
        {
            out_of_memory();
            return NULL;
        }
    }
    arr = (float *)malloc(rows * cols * sizeof(float));
    if (arr == NULL)
    {
        out_of_memory();
        free(result);
        return NULL;
    }

    result->rows = rows;
    result->cols = cols;
    result->arr = arr;
    result->refs = 1;
    result->next = NULL;

    return result;
}

/* Free a matrix, assuming it is completely legal, or simply points to NULL, or is a null pointer.
   Otherwise, nothing is done. */
void delete_matrix(matrix *m)
{
    if (m == NULL || *m == NULL)
        return;
    if ((*m)->refs != 0)
        --((*m)->refs);

    /* If no more references exist */
    if ((*m)->refs == 0)
    {
        free((*m)->arr);
        (*m)->next = _recycled_list;
        _recycled_list = *m;
    }

    *m = NULL;
}

/* Copy src matrix to dest. If the size of dest matrix doesn't fit, reallocate space and modify dest.
   If source matrix is NULL, do nothing; If the destination matrix is NULL, try to allocate a new one.
   It is assumed that both the source and the destination matrix is valid (i.e., created by
   function create_matrix() or NULL), and thus rows * cols won't overflow.
   The caller must check the destination to ensure that all the routines that possess a reference
   to this matrix will allow such changes.

   If failed to reallocate space, first call out_of_memory, then nothing is done on dest. */
int copy_matrix(matrix *dest, const matrix src)
{
    if (src == NULL)
        return OP_NULL_PTR;

    size_t target_size = src->rows * src->cols; /* Target size*/

    /* If destionation is not NULL and size of arr space doesn't match */
    if (*dest != NULL && ((*dest)->rows * (*dest)->cols) != target_size)
    {
        float *newarr;
        if ((newarr = (float *)malloc(target_size * sizeof(float))) == NULL)
        {
            out_of_memory();
            return OUT_OF_MEMORY;
        }
        free((*dest)->arr);
        (*dest)->arr = newarr;
    }
    /* Allocate space for destination matrix if it is NULL. If allocation failed, return immediately. */
    else if (*dest == NULL && (*dest = create_matrix(src->rows, src->cols)) == NULL)
        return OUT_OF_MEMORY;

    /* Start copying */
    (*dest)->rows = src->rows;
    (*dest)->cols = src->cols;
    memcpy((*dest)->arr, src->arr, target_size * sizeof(float));

    return COMPLETED;
}

/* Return a reference to the target matrix.

   If a NULL pointer is received, do nothing and return NULL. */
matrix ref_matrix(const matrix m)
{
    if (m != NULL)
        m->refs++;
    return m;
}

/* Perform arithmetic operation on two matrices and store the result in a third matrix.
   The result matrix can refer to either op1 or op2 at the same time, but the pointer to the result matrix cannot be NULL.
   Two operands are supposed to be NULL or valid (i.e. created using function create_matrix()).
   If not, undefined behaviour will occur. If the result matrix isn't able to store the sum, i.e.,
   either it is NULL or the size doesn't match, the result matrix would be modified to match the need.

   If errors occurred during the operation (for example, operand size unmatches), do nothing on the result matrix.
   However, the user must handle 0 divisor error themselves.
   Returns the corresonding errno code upon failure. */
static inline int
_do_ebe_on_matrix(const matrix op1, const matrix op2, matrix *result, op_code code)
{
    if (op1 == NULL || op2 == NULL)
        return OP_NULL_PTR;

    size_t rows, cols, size;
    rows = op1->rows;
    cols = op1->cols;
    size = rows * cols;

    if (rows != op2->rows || cols != op2->cols)
        return OP_UNMATCHED_SIZE;

    float *newarr; /* Used to store the intermediate result */

    /* Check result matrix */
    if ((*result)->rows * (*result)->cols != size)
    {
        if ((newarr = (float *)malloc(size * sizeof(float))) == NULL)
        {
            out_of_memory();
            return OUT_OF_MEMORY;
        }
    }
    /* If result is NULL */
    else if (*result == NULL)
    {
        if ((*result = create_matrix(rows, cols)) == NULL) /* If failed to create a new matrix */
            return OUT_OF_MEMORY;
        else
            newarr = (*result)->arr;
    }

    /* Start addition */

    // todo: parallel optimization
    switch (code)
    {
    case ADD:
        for (int i = 0; i < size; ++i)
            newarr[i] = op1->arr[i] + op2->arr[i];
        break;
    case SUBTRACT:
        for (int i = 0; i < size; ++i)
            newarr[i] = op1->arr[i] - op2->arr[i];
        break;
    case MULTIPLY:
        for (int i = 0; i < size; ++i)
            newarr[i] = op1->arr[i] * op2->arr[i];
        break;
    case DIVIDE:
        for (int i = 0; i < size; ++i)
            newarr[i] = op1->arr[i] / op2->arr[i];
        break;
    }

    /* Clean up */
    (*result)->rows = rows;
    (*result)->cols = cols;
    if ((*result)->arr != newarr)
    {
        free((*result)->arr);
        (*result)->arr = newarr;
    }

    return COMPLETED;
}

/* Add two matrices and store the result in a third matrix.
   The result matrix can refer to either op1 or op2 at the same time, but the pointer to the result matrix cannot be NULL.
   Two operands are supposed to be NULL or valid (i.e. created using function create_matrix()).
   If not, undefined behaviour will occur. If the result matrix isn't able to store the sum, i.e.,
   either it is NULL or the size doesn't match, the result matrix would be modified to match the need.

   If errors occurred during the operation (for example, operand size unmatches), do nothing on the result matrix.
   Returns the corresonding errno code upon failure. */
int add_matrix(const matrix addend1, const matrix addend2, matrix *result)
{
    return _do_ebe_on_matrix(addend1, addend2, result, ADD);
}

/* Subtract subtractor matrix from subtractend matrix and store the result in a third matrix.
   The result matrix can refer to either op1 or op2 at the same time, but the pointer to the result matrix cannot be NULL.
   Two operands are supposed to be NULL or valid (i.e. created using function create_matrix()).
   If not, undefined behaviour will occur. If the result matrix isn't able to store the sum, i.e.,
   either it is NULL or the size doesn't match, the result matrix would be modified to match the need.

   If errors occurred during the operation (for example, operand size unmatches), do nothing on the result matrix.
   Returns the corresonding errno code upon failure. */
int subtract_matrix(const matrix subtrahend, const matrix subtractor, matrix *result)
{
    return _do_ebe_on_matrix(subtrahend, subtractor, result, SUBTRACT);
}

/* Add a scalar to all elements in this matrix and store the result in a third matrix.
   The result matrix can refer to src, but the pointer to the result matrix cannot be NULL.
   src is supposed to be NULL or valid (i.e. created using function create_matrix()).
   If not, undefined behaviour will occur.

   If errors occurred during the operation, do nothing on the result matrix.
   Returns the corresonding errno code upon failure.
    */
int add_scalar(const matrix src, matrix *result, float val)
{
    if (src == NULL)
        return OP_NULL_PTR;

    if (*result != src)
    {
        if(copy_matrix(result, src) != COMPLETED)
            
    }
}

int subtract_scalar(const matrix src, matrix *result, float val);

int multiply_scalar(const matrix src, matrix *result, float val);

int multiply_matrix(matrix op1, const matrix op2, matrix *result);

float matrix_max(const matrix src);

float matrix_min(const matrix src);

int read_matrix(char *s, matrix *result);

int print_matrix(const matrix src);
