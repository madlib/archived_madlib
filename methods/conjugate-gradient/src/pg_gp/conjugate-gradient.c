#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "libpq/pqformat.h"
#include "parser/parse_coerce.h"
#include "catalog/pg_type.h"
#include "utils/array.h"
#include "utils/numeric.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"

#include "access/hash.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

Datum vector_of( PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(vector_of);
Datum vector_of(PG_FUNCTION_ARGS)
{
	int size = PG_GETARG_INT32(0);
	float8 value = PG_GETARG_FLOAT8(1);
	float8* array = (float8*) malloc (sizeof(float8)*size);
	float8* point_array = array;
	int i = 0;
	memset(array, 0, sizeof(float8)*size);
	
	if(size != 0){
		for(; i < size; ++i){
			point_array[i] = value;
		}
	}
	
	ArrayType *pgarray;
    pgarray = construct_array((Datum *)array,
		size,FLOAT8OID,
		sizeof(float8),true,'d');
    PG_RETURN_ARRAYTYPE_P(pgarray);
}


Datum array_add( PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(array_add);
Datum array_add( PG_FUNCTION_ARGS)
{
  	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType  *array2 = PG_GETARG_ARRAYTYPE_P(1);
   	int         ndims1 = ARR_NDIM(array1);
   	int         ndims2 = ARR_NDIM(array2);
   	int        *dims1 = ARR_DIMS(array1);
   	int        *dims2 = ARR_DIMS(array2);
     int         nitems1 = ArrayGetNItems(ndims1, dims1);
     int         nitems2 = ArrayGetNItems(ndims2, dims2);
     Oid         element_type = ARR_ELEMTYPE(array1);
     float8        *result = (float8*) malloc(sizeof(float8)*nitems1);
     TypeCacheEntry *typentry;
     int         typlen;
     bool        typbyval;
     char        typalign;
     int         min_nitems;
     char       *ptr1;
     char       *ptr2;
     bits8      *bitmap1;
     bits8      *bitmap2;
     int         bitmask;
     int         i;
     float8 a;
     float8 b;
 
 	memset(result, 0, sizeof(float8)*nitems1);
     if (element_type != ARR_ELEMTYPE(array2))
         ereport(ERROR,
                 (errcode(ERRCODE_DATATYPE_MISMATCH),
                  errmsg("cannot compare arrays of different element types")));
 

     typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;
     if (typentry == NULL ||
         typentry->type_id != element_type)
     {
         typentry = lookup_type_cache(element_type,
                                      TYPECACHE_CMP_PROC_FINFO);
         if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
             ereport(ERROR,
                     (errcode(ERRCODE_UNDEFINED_FUNCTION),
                errmsg("could not identify a comparison function for type %s",
                       format_type_be(element_type))));
         fcinfo->flinfo->fn_extra = (void *) typentry;
     }
     typlen = typentry->typlen;
     typbyval = typentry->typbyval;
     typalign = typentry->typalign;
 
     min_nitems = Min(nitems1, nitems2);
     ptr1 = ARR_DATA_PTR(array1);
     ptr2 = ARR_DATA_PTR(array2);
     bitmap1 = ARR_NULLBITMAP(array1);
     bitmap2 = ARR_NULLBITMAP(array2);
     bitmask = 1;                /* use same bitmask for both arrays */
 
     for (i = 0; i < min_nitems; i++)
     {
         Datum       elt1;
         Datum       elt2;
 
         /* Get elements, checking for NULL */
         if ((bitmap1 && (*bitmap1 & bitmask) == 0) || (bitmap2 && (*bitmap2 & bitmask) == 0))
         {
             result[i] = 0;
         }
         else
         {
             elt1 = fetch_att(ptr1, typbyval, typlen);
             ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
             ptr1 = (char *) att_align_nominal(ptr1, typalign);
             elt2 = fetch_att(ptr2, typbyval, typlen);
             ptr2 = att_addlength_pointer(ptr2, typlen, ptr2);
             ptr2 = (char *) att_align_nominal(ptr2, typalign);
             
             a = DatumGetFloat8(elt1);
             b = DatumGetFloat8(elt2);
             result[i] = a+b;
         }
 
         /* advance bitmap pointers if any */
         bitmask <<= 1;
         if (bitmask == 0x100)
         {
             if (bitmap1)
                 bitmap1++;
             if (bitmap2)
                 bitmap2++;
             bitmask = 1;
         }
      }
 

    
     PG_FREE_IF_COPY(array1, 0);
     PG_FREE_IF_COPY(array2, 1);
 
 	ArrayType *pgarray;
	 pgarray = construct_array((Datum *)result,
		min_nitems,FLOAT8OID,
		sizeof(float8),true,'d');
     PG_RETURN_ARRAYTYPE_P(pgarray);
 }
 
 Datum array_add_remove_null( PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(array_add_remove_null);
Datum array_add_remove_null( PG_FUNCTION_ARGS)
{
  	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType  *array2 = PG_GETARG_ARRAYTYPE_P(1);
	int offset1 = *(ARR_LBOUND(array1))-1;
	int offset2 = *(ARR_LBOUND(array2))-1;
   	int         ndims1 = ARR_NDIM(array1);
   	int         ndims2 = ARR_NDIM(array2);
   	int        *dims1 = ARR_DIMS(array1);
   	int        *dims2 = ARR_DIMS(array2);
     int         nitems1 = ArrayGetNItems(ndims1, dims1)+offset1;
     int         nitems2 = ArrayGetNItems(ndims2, dims2)+offset2;
     Oid         element_type = ARR_ELEMTYPE(array1);
     float8        *result = (float8*) malloc(sizeof(float8)*Max(nitems1,nitems2));
     TypeCacheEntry *typentry;
     int         typlen;
     bool        typbyval;
     char        typalign;
     int         min_nitems;
     int 		max_nitems;
     char       *ptr1;
     char       *ptr2;
     bits8      *bitmap1;
     bits8      *bitmap2;
     int         bitmask1;
     int         bitmask2;
     int         i;
     float8 a;
     float8 b;
 
 	memset(result, 0, sizeof(float8)*Max(offset1+nitems1,offset2+nitems2));
 	//memset(result, 0, sizeof(float8)*Max(nitems1,nitems2));
     if (element_type != ARR_ELEMTYPE(array2))
         ereport(ERROR,
                 (errcode(ERRCODE_DATATYPE_MISMATCH),
                  errmsg("cannot compare arrays of different element types")));
 

     typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;
     if (typentry == NULL ||
         typentry->type_id != element_type)
     {
         typentry = lookup_type_cache(element_type,
                                      TYPECACHE_CMP_PROC_FINFO);
         if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
             ereport(ERROR,
                     (errcode(ERRCODE_UNDEFINED_FUNCTION),
                errmsg("could not identify a comparison function for type %s",
                       format_type_be(element_type))));
         fcinfo->flinfo->fn_extra = (void *) typentry;
     }
     typlen = typentry->typlen;
     typbyval = typentry->typbyval;
     typalign = typentry->typalign;
 
     min_nitems = Min(nitems1, nitems2);
     max_nitems = Max(nitems1, nitems2);
     ptr1 = ARR_DATA_PTR(array1);
     ptr2 = ARR_DATA_PTR(array2);
     bitmap1 = ARR_NULLBITMAP(array1);
     bitmap2 = ARR_NULLBITMAP(array2);
     bitmask1 = 1; 
     bitmask2 = 1;                /* use same bitmask for both arrays */
     
     if(offset1 < offset2){
     		Datum       elt1;
     		for (i = offset1; i < Min(offset2,nitems1); i++){
     	if (!(bitmap1 && (*bitmap1 & bitmask1) == 0))
         {
     			elt1 = fetch_att(ptr1, typbyval, typlen);
            	ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
            	ptr1 = (char *) att_align_nominal(ptr1, typalign);
            	result[i] = DatumGetFloat8(elt1);
     	}
     	
     	 bitmask1 <<= 1;
         if (bitmask1 == 0x100)
         {
             if (bitmap1)
                 bitmap1++;
             bitmask1 = 1;
         }
     		}
     }else{
         Datum       elt2;
         for (i = offset2; i < Min(offset1,nitems2); i++){
     		if (!(bitmap2 && (*bitmap2 & bitmask2) == 0 && i < nitems2))
         {
     			elt2 = fetch_att(ptr2, typbyval, typlen);
            	ptr2 = att_addlength_pointer(ptr2, typlen, ptr2);
            	ptr2 = (char *) att_align_nominal(ptr2, typalign);
            	result[i] = DatumGetFloat8(elt2);
     	}
     	
     	 bitmask2 <<= 1;
         if (bitmask2 == 0x100)
         {
             if (bitmap2)
                 bitmap2++;
             bitmask2 = 1;
         }
         }
     }
     i = Max(Max(offset1, offset2),0);
 
     for (; i < min_nitems; i++)
     {
         Datum       elt1;
         Datum       elt2;
 
         /* Get elements, checking for NULL */
         if ((bitmap1 && (*bitmap1 & bitmask1) == 0) || (bitmap2 && (*bitmap2 & bitmask2) == 0))
         {
         	if(bitmap1 && (*bitmap1 & bitmask1) == 0){
         		elt2 = fetch_att(ptr2, typbyval, typlen);
             	ptr2 = att_addlength_pointer(ptr2, typlen, ptr2);
             	ptr2 = (char *) att_align_nominal(ptr2, typalign);
             	result[i] = DatumGetFloat8(elt2);
         	}else if(bitmap2 && (*bitmap2 & bitmask2) == 0){
         		elt1 = fetch_att(ptr1, typbyval, typlen);
             	ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
             	ptr1 = (char *) att_align_nominal(ptr1, typalign);
             	result[i] = DatumGetFloat8(elt1);
         	}else{
         		result[i] = 0;
         	}
         }
         else
         {
             elt1 = fetch_att(ptr1, typbyval, typlen);
             ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
             ptr1 = (char *) att_align_nominal(ptr1, typalign);
             elt2 = fetch_att(ptr2, typbyval, typlen);
             ptr2 = att_addlength_pointer(ptr2, typlen, ptr2);
             ptr2 = (char *) att_align_nominal(ptr2, typalign);
             
             a = DatumGetFloat8(elt1);
             b = DatumGetFloat8(elt2);
             result[i] = a+b;
         }
 
         /* advance bitmap pointers if any */
         bitmask1 <<= 1;
         if (bitmask1 == 0x100)
         {
             if (bitmap1)
                 bitmap1++;
             bitmask1 = 1;
         }
          bitmask2 <<= 1;
         if (bitmask2 == 0x100)
         {
             if (bitmap2)
                 bitmap2++;
             bitmask2 = 1;
         }
      }
      if(nitems2 > nitems1){
      for(;i < max_nitems; ++i){
         Datum       elt2;
 
         /* Get elements, checking for NULL */
         if(!(bitmap2 && (*bitmap2 & bitmask2) == 0))
         {
             elt2 = fetch_att(ptr2, typbyval, typlen);
             ptr2 = att_addlength_pointer(ptr2, typlen, ptr2);
             ptr2 = (char *) att_align_nominal(ptr2, typalign);
             
             result[i] = DatumGetFloat8(elt2);
         }
 
         /* advance bitmap pointers if any */
         bitmask2 <<= 1;
         if (bitmask2 == 0x100)
         {
             if (bitmap2)
                 bitmap2++;
             bitmask2 = 1;
         }
      }
      }else{
      for(;i < max_nitems; ++i){
         Datum       elt1;
 
         /* Get elements, checking for NULL */
         if(!(bitmap1 && (*bitmap1 & bitmask1) == 0))
         {
             elt1 = fetch_att(ptr1, typbyval, typlen);
             ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
             ptr1 = (char *) att_align_nominal(ptr1, typalign);
             
             result[i] = DatumGetFloat8(elt1);
         }
 
         /* advance bitmap pointers if any */
         bitmask1 <<= 1;
         if (bitmask1 == 0x100)
         {
             if (bitmap1)
                 bitmap1++;
             bitmask1 = 1;
         }
      }
      }
      
      //result[0] = offset1;
      //result[1] = offset2;
     
     PG_FREE_IF_COPY(array1, 0);
     PG_FREE_IF_COPY(array2, 1);
 
 	ArrayType *pgarray;
	 pgarray = construct_array((Datum *)result,
		max_nitems,FLOAT8OID,
		sizeof(float8),true,'d');
     PG_RETURN_ARRAYTYPE_P(pgarray);
 }
 
 Datum array_sub( PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(array_sub);
Datum array_sub( PG_FUNCTION_ARGS)
{
  	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType  *array2 = PG_GETARG_ARRAYTYPE_P(1);
   	int         ndims1 = ARR_NDIM(array1);
   	int         ndims2 = ARR_NDIM(array2);
   	int        *dims1 = ARR_DIMS(array1);
   	int        *dims2 = ARR_DIMS(array2);
     int         nitems1 = ArrayGetNItems(ndims1, dims1);
     int         nitems2 = ArrayGetNItems(ndims2, dims2);
     Oid         element_type = ARR_ELEMTYPE(array1);
     float8        *result = (float8*) malloc(sizeof(float8)*nitems1);
     TypeCacheEntry *typentry;
     int         typlen;
     bool        typbyval;
     char        typalign;
     int         min_nitems;
     char       *ptr1;
     char       *ptr2;
     bits8      *bitmap1;
     bits8      *bitmap2;
     int         bitmask;
     int         i;
     float8 a;
     float8 b;
 
 	memset(result, 0, sizeof(float8)*nitems1);
     if (element_type != ARR_ELEMTYPE(array2))
         ereport(ERROR,
                 (errcode(ERRCODE_DATATYPE_MISMATCH),
                  errmsg("cannot compare arrays of different element types")));
 

     typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;
     if (typentry == NULL ||
         typentry->type_id != element_type)
     {
         typentry = lookup_type_cache(element_type,
                                      TYPECACHE_CMP_PROC_FINFO);
         if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
             ereport(ERROR,
                     (errcode(ERRCODE_UNDEFINED_FUNCTION),
                errmsg("could not identify a comparison function for type %s",
                       format_type_be(element_type))));
         fcinfo->flinfo->fn_extra = (void *) typentry;
     }
     typlen = typentry->typlen;
     typbyval = typentry->typbyval;
     typalign = typentry->typalign;
 
     min_nitems = Min(nitems1, nitems2);
     ptr1 = ARR_DATA_PTR(array1);
     ptr2 = ARR_DATA_PTR(array2);
     bitmap1 = ARR_NULLBITMAP(array1);
     bitmap2 = ARR_NULLBITMAP(array2);
     bitmask = 1;                /* use same bitmask for both arrays */
 
     for (i = 0; i < min_nitems; i++)
     {
         Datum       elt1;
         Datum       elt2;
 
         /* Get elements, checking for NULL */
         if ((bitmap1 && (*bitmap1 & bitmask) == 0) || (bitmap2 && (*bitmap2 & bitmask) == 0))
         {
             result[i] = 0;
         }
         else
         {
             elt1 = fetch_att(ptr1, typbyval, typlen);
             ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
             ptr1 = (char *) att_align_nominal(ptr1, typalign);
             elt2 = fetch_att(ptr2, typbyval, typlen);
             ptr2 = att_addlength_pointer(ptr2, typlen, ptr2);
             ptr2 = (char *) att_align_nominal(ptr2, typalign);
             
             a = DatumGetFloat8(elt1);
             b = DatumGetFloat8(elt2);
             result[i] = a-b;
         }
 
         /* advance bitmap pointers if any */
         bitmask <<= 1;
         if (bitmask == 0x100)
         {
             if (bitmap1)
                 bitmap1++;
             if (bitmap2)
                 bitmap2++;
             bitmask = 1;
         }
      }
 

    
     PG_FREE_IF_COPY(array1, 0);
     PG_FREE_IF_COPY(array2, 1);
 
 	ArrayType *pgarray;
	 pgarray = construct_array((Datum *)result,
		min_nitems,FLOAT8OID,
		sizeof(float8),true,'d');
     PG_RETURN_ARRAYTYPE_P(pgarray);
 }
 
  Datum array_mult( PG_FUNCTION_ARGS);
 
 PG_FUNCTION_INFO_V1(array_mult);
Datum array_mult( PG_FUNCTION_ARGS)
{
  	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType  *array2 = PG_GETARG_ARRAYTYPE_P(1);
   	int         ndims1 = ARR_NDIM(array1);
   	int         ndims2 = ARR_NDIM(array2);
   	int        *dims1 = ARR_DIMS(array1);
   	int        *dims2 = ARR_DIMS(array2);
     int         nitems1 = ArrayGetNItems(ndims1, dims1);
     int         nitems2 = ArrayGetNItems(ndims2, dims2);
     Oid         element_type = ARR_ELEMTYPE(array1);
     float8        *result = (float8*) malloc(sizeof(float8)*nitems1);
     TypeCacheEntry *typentry;
     int         typlen;
     bool        typbyval;
     char        typalign;
     int         min_nitems;
     char       *ptr1;
     char       *ptr2;
     bits8      *bitmap1;
     bits8      *bitmap2;
     int         bitmask;
     int         i;
     float8 a;
     float8 b;
 
 	memset(result, 0, sizeof(float8)*nitems1);
     if (element_type != ARR_ELEMTYPE(array2))
         ereport(ERROR,
                 (errcode(ERRCODE_DATATYPE_MISMATCH),
                  errmsg("cannot compare arrays of different element types")));
 

     typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;
     if (typentry == NULL ||
         typentry->type_id != element_type)
     {
         typentry = lookup_type_cache(element_type,
                                      TYPECACHE_CMP_PROC_FINFO);
         if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
             ereport(ERROR,
                     (errcode(ERRCODE_UNDEFINED_FUNCTION),
                errmsg("could not identify a comparison function for type %s",
                       format_type_be(element_type))));
         fcinfo->flinfo->fn_extra = (void *) typentry;
     }
     typlen = typentry->typlen;
     typbyval = typentry->typbyval;
     typalign = typentry->typalign;
 
     min_nitems = Min(nitems1, nitems2);
     ptr1 = ARR_DATA_PTR(array1);
     ptr2 = ARR_DATA_PTR(array2);
     bitmap1 = ARR_NULLBITMAP(array1);
     bitmap2 = ARR_NULLBITMAP(array2);
     bitmask = 1;                /* use same bitmask for both arrays */
 
     for (i = 0; i < min_nitems; i++)
     {
         Datum       elt1;
         Datum       elt2;
 
         /* Get elements, checking for NULL */
         if ((bitmap1 && (*bitmap1 & bitmask) == 0) || (bitmap2 && (*bitmap2 & bitmask) == 0))
         {
             result[i] = 0;
         }
         else
         {
             elt1 = fetch_att(ptr1, typbyval, typlen);
             ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
             ptr1 = (char *) att_align_nominal(ptr1, typalign);
             elt2 = fetch_att(ptr2, typbyval, typlen);
             ptr2 = att_addlength_pointer(ptr2, typlen, ptr2);
             ptr2 = (char *) att_align_nominal(ptr2, typalign);
             
             a = DatumGetFloat8(elt1);
             b = DatumGetFloat8(elt2);
             result[i] = a*b;
         }
 
         /* advance bitmap pointers if any */
         bitmask <<= 1;
         if (bitmask == 0x100)
         {
             if (bitmap1)
                 bitmap1++;
             if (bitmap2)
                 bitmap2++;
             bitmask = 1;
         }
      }
 

    
     PG_FREE_IF_COPY(array1, 0);
     PG_FREE_IF_COPY(array2, 1);
 
 	ArrayType *pgarray;
	 pgarray = construct_array((Datum *)result,
		min_nitems,FLOAT8OID,
		sizeof(float8),true,'d');
     PG_RETURN_ARRAYTYPE_P(pgarray);
 }
 
  Datum array_div( PG_FUNCTION_ARGS);
 
  PG_FUNCTION_INFO_V1(array_div);
Datum array_div( PG_FUNCTION_ARGS)
{
  	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType  *array2 = PG_GETARG_ARRAYTYPE_P(1);
   	int         ndims1 = ARR_NDIM(array1);
   	int         ndims2 = ARR_NDIM(array2);
   	int        *dims1 = ARR_DIMS(array1);
   	int        *dims2 = ARR_DIMS(array2);
     int         nitems1 = ArrayGetNItems(ndims1, dims1);
     int         nitems2 = ArrayGetNItems(ndims2, dims2);
     Oid         element_type = ARR_ELEMTYPE(array1);
     float8        *result = (float8*) malloc(sizeof(float8)*nitems1);
     TypeCacheEntry *typentry;
     int         typlen;
     bool        typbyval;
     char        typalign;
     int         min_nitems;
     char       *ptr1;
     char       *ptr2;
     bits8      *bitmap1;
     bits8      *bitmap2;
     int         bitmask;
     int         i;
     float8 a;
     float8 b;
 
 	memset(result, 0, sizeof(float8)*nitems1);
     if (element_type != ARR_ELEMTYPE(array2))
         ereport(ERROR,
                 (errcode(ERRCODE_DATATYPE_MISMATCH),
                  errmsg("cannot compare arrays of different element types")));
 

     typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;
     if (typentry == NULL ||
         typentry->type_id != element_type)
     {
         typentry = lookup_type_cache(element_type,
                                      TYPECACHE_CMP_PROC_FINFO);
         if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
             ereport(ERROR,
                     (errcode(ERRCODE_UNDEFINED_FUNCTION),
                errmsg("could not identify a comparison function for type %s",
                       format_type_be(element_type))));
         fcinfo->flinfo->fn_extra = (void *) typentry;
     }
     typlen = typentry->typlen;
     typbyval = typentry->typbyval;
     typalign = typentry->typalign;
 
     min_nitems = Min(nitems1, nitems2);
     ptr1 = ARR_DATA_PTR(array1);
     ptr2 = ARR_DATA_PTR(array2);
     bitmap1 = ARR_NULLBITMAP(array1);
     bitmap2 = ARR_NULLBITMAP(array2);
     bitmask = 1;                /* use same bitmask for both arrays */
 
     for (i = 0; i < min_nitems; i++)
     {
         Datum       elt1;
         Datum       elt2;
 
         /* Get elements, checking for NULL */
         if ((bitmap1 && (*bitmap1 & bitmask) == 0) || (bitmap2 && (*bitmap2 & bitmask) == 0))
         {
             result[i] = 0;
         }
         else
         {
             elt1 = fetch_att(ptr1, typbyval, typlen);
             ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
             ptr1 = (char *) att_align_nominal(ptr1, typalign);
             elt2 = fetch_att(ptr2, typbyval, typlen);
             ptr2 = att_addlength_pointer(ptr2, typlen, ptr2);
             ptr2 = (char *) att_align_nominal(ptr2, typalign);
             
             a = DatumGetFloat8(elt1);
             b = DatumGetFloat8(elt2);
             if(b == 0){
             	result[i] = 0;	
             }else{
             	result[i] = a/b;
             }
         }
 
         /* advance bitmap pointers if any */
         bitmask <<= 1;
         if (bitmask == 0x100)
         {
             if (bitmap1)
                 bitmap1++;
             if (bitmap2)
                 bitmap2++;
             bitmask = 1;
         }
      }
 

    
     PG_FREE_IF_COPY(array1, 0);
     PG_FREE_IF_COPY(array2, 1);
 
 	ArrayType *pgarray;
	 pgarray = construct_array((Datum *)result,
		min_nitems,FLOAT8OID,
		sizeof(float8),true,'d');
     PG_RETURN_ARRAYTYPE_P(pgarray);
 }
 
 Datum array_dot( PG_FUNCTION_ARGS);
 
  PG_FUNCTION_INFO_V1(array_dot);
Datum array_dot( PG_FUNCTION_ARGS)
{
  	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType  *array2 = PG_GETARG_ARRAYTYPE_P(1);
   	int         ndims1 = ARR_NDIM(array1);
   	int         ndims2 = ARR_NDIM(array2);
   	int        *dims1 = ARR_DIMS(array1);
   	int        *dims2 = ARR_DIMS(array2);
     int         nitems1 = ArrayGetNItems(ndims1, dims1);
     int         nitems2 = ArrayGetNItems(ndims2, dims2);
     Oid         element_type = ARR_ELEMTYPE(array1);
     float8      result = 0;
     TypeCacheEntry *typentry;
     int         typlen;
     bool        typbyval;
     char        typalign;
     int         min_nitems;
     char       *ptr1;
     char       *ptr2;
     bits8      *bitmap1;
     bits8      *bitmap2;
     int         bitmask;
     int         i;
     float8 a;
     float8 b;
 
     if (element_type != ARR_ELEMTYPE(array2))
         ereport(ERROR,
                 (errcode(ERRCODE_DATATYPE_MISMATCH),
                  errmsg("cannot compare arrays of different element types")));
 

     typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;
     if (typentry == NULL ||
         typentry->type_id != element_type)
     {
         typentry = lookup_type_cache(element_type,
                                      TYPECACHE_CMP_PROC_FINFO);
         if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
             ereport(ERROR,
                     (errcode(ERRCODE_UNDEFINED_FUNCTION),
                errmsg("could not identify a comparison function for type %s",
                       format_type_be(element_type))));
         fcinfo->flinfo->fn_extra = (void *) typentry;
     }
     typlen = typentry->typlen;
     typbyval = typentry->typbyval;
     typalign = typentry->typalign;
 
     min_nitems = Min(nitems1, nitems2);
     ptr1 = ARR_DATA_PTR(array1);
     ptr2 = ARR_DATA_PTR(array2);
     bitmap1 = ARR_NULLBITMAP(array1);
     bitmap2 = ARR_NULLBITMAP(array2);
     bitmask = 1;                /* use same bitmask for both arrays */
 
     for (i = 0; i < min_nitems; i++)
     {
         Datum       elt1;
         Datum       elt2;
 
         /* Get elements, checking for NULL */
         if (!((bitmap1 && (*bitmap1 & bitmask) == 0) || (bitmap2 && (*bitmap2 & bitmask) == 0))){
             elt1 = fetch_att(ptr1, typbyval, typlen);
             ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
             ptr1 = (char *) att_align_nominal(ptr1, typalign);
             elt2 = fetch_att(ptr2, typbyval, typlen);
             ptr2 = att_addlength_pointer(ptr2, typlen, ptr2);
             ptr2 = (char *) att_align_nominal(ptr2, typalign);
             
             a = DatumGetFloat8(elt1);
             b = DatumGetFloat8(elt2);
             result += a*b;
         }
 
         /* advance bitmap pointers if any */
         bitmask <<= 1;
         if (bitmask == 0x100)
         {
             if (bitmap1)
                 bitmap1++;
             if (bitmap2)
                 bitmap2++;
             bitmask = 1;
         }
      }
     
     PG_FREE_IF_COPY(array1, 0);
     PG_FREE_IF_COPY(array2, 1);
     PG_RETURN_FLOAT8(result);
 }
 
 Datum array_sum( PG_FUNCTION_ARGS);
 
  PG_FUNCTION_INFO_V1(array_sum);
Datum array_sum( PG_FUNCTION_ARGS)
{
  	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
   	int         ndims1 = ARR_NDIM(array1);
   	int        *dims1 = ARR_DIMS(array1);
     int         nitems1 = ArrayGetNItems(ndims1, dims1);
     Oid         element_type = ARR_ELEMTYPE(array1);
     float8      result = 0;
     TypeCacheEntry *typentry;
     int         typlen;
     bool        typbyval;
     char        typalign;
     char       *ptr1;
     bits8      *bitmap1;
     int         bitmask;
     int         i;
 
     typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;
     if (typentry == NULL ||
         typentry->type_id != element_type)
     {
         typentry = lookup_type_cache(element_type,
                                      TYPECACHE_CMP_PROC_FINFO);
         if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
             ereport(ERROR,
                     (errcode(ERRCODE_UNDEFINED_FUNCTION),
                errmsg("could not identify a comparison function for type %s",
                       format_type_be(element_type))));
         fcinfo->flinfo->fn_extra = (void *) typentry;
     }
     typlen = typentry->typlen;
     typbyval = typentry->typbyval;
     typalign = typentry->typalign;
 
     ptr1 = ARR_DATA_PTR(array1);
     bitmap1 = ARR_NULLBITMAP(array1);
     bitmask = 1;                /* use same bitmask for both arrays */
 
     for (i = 0; i < nitems1; i++)
     {
         Datum       elt1;
 
         /* Get elements, checking for NULL */
         if (!(bitmap1 && (*bitmap1 & bitmask) == 0)){
             elt1 = fetch_att(ptr1, typbyval, typlen);
             ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
             ptr1 = (char *) att_align_nominal(ptr1, typalign);
             
             result += DatumGetFloat8(elt1);
         }
 
         /* advance bitmap pointers if any */
         bitmask <<= 1;
         if (bitmask == 0x100)
         {
             if (bitmap1)
                 bitmap1++;
             bitmask = 1;
         }
      }
     
     PG_FREE_IF_COPY(array1, 0);
     PG_RETURN_FLOAT8(result);
 }
 
 Datum array_dif( PG_FUNCTION_ARGS);
 
  PG_FUNCTION_INFO_V1(array_dif);
Datum array_dif( PG_FUNCTION_ARGS)
{
  	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
	ArrayType  *array2 = PG_GETARG_ARRAYTYPE_P(1);
   	int         ndims1 = ARR_NDIM(array1);
   	int         ndims2 = ARR_NDIM(array2);
   	int        *dims1 = ARR_DIMS(array1);
   	int        *dims2 = ARR_DIMS(array2);
     int         nitems1 = ArrayGetNItems(ndims1, dims1);
     int         nitems2 = ArrayGetNItems(ndims2, dims2);
     Oid         element_type = ARR_ELEMTYPE(array1);
     float8      result = 0;
     TypeCacheEntry *typentry;
     int         typlen;
     bool        typbyval;
     char        typalign;
     int         min_nitems;
     char       *ptr1;
     char       *ptr2;
     bits8      *bitmap1;
     bits8      *bitmap2;
     int         bitmask;
     int         i;
     float8 a;
     float8 b;
 
     if (element_type != ARR_ELEMTYPE(array2))
         ereport(ERROR,
                 (errcode(ERRCODE_DATATYPE_MISMATCH),
                  errmsg("cannot compare arrays of different element types")));
 

     typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;
     if (typentry == NULL ||
         typentry->type_id != element_type)
     {
         typentry = lookup_type_cache(element_type,
                                      TYPECACHE_CMP_PROC_FINFO);
         if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
             ereport(ERROR,
                     (errcode(ERRCODE_UNDEFINED_FUNCTION),
                errmsg("could not identify a comparison function for type %s",
                       format_type_be(element_type))));
         fcinfo->flinfo->fn_extra = (void *) typentry;
     }
     typlen = typentry->typlen;
     typbyval = typentry->typbyval;
     typalign = typentry->typalign;
 
     min_nitems = Min(nitems1, nitems2);
     ptr1 = ARR_DATA_PTR(array1);
     ptr2 = ARR_DATA_PTR(array2);
     bitmap1 = ARR_NULLBITMAP(array1);
     bitmap2 = ARR_NULLBITMAP(array2);
     bitmask = 1;                /* use same bitmask for both arrays */
 
     for (i = 0; i < min_nitems; i++)
     {
         Datum       elt1;
         Datum       elt2;
 
         /* Get elements, checking for NULL */
         if (!((bitmap1 && (*bitmap1 & bitmask) == 0) || (bitmap2 && (*bitmap2 & bitmask) == 0))){
             elt1 = fetch_att(ptr1, typbyval, typlen);
             ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
             ptr1 = (char *) att_align_nominal(ptr1, typalign);
             elt2 = fetch_att(ptr2, typbyval, typlen);
             ptr2 = att_addlength_pointer(ptr2, typlen, ptr2);
             ptr2 = (char *) att_align_nominal(ptr2, typalign);
             
             a = DatumGetFloat8(elt1);
             b = DatumGetFloat8(elt2);
             result += (a-b)*(a-b);
         }
 
         /* advance bitmap pointers if any */
         bitmask <<= 1;
         if (bitmask == 0x100)
         {
             if (bitmap1)
                 bitmap1++;
             if (bitmap2)
                 bitmap2++;
             bitmask = 1;
         }
      }
     
     PG_FREE_IF_COPY(array1, 0);
     PG_FREE_IF_COPY(array2, 1);
     PG_RETURN_FLOAT8(sqrt(result));
 }
 
   Datum array_scalar_mult( PG_FUNCTION_ARGS);
 
  PG_FUNCTION_INFO_V1(array_scalar_mult);
Datum array_scalar_mult( PG_FUNCTION_ARGS)
{
  	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
	float8 scalar = PG_GETARG_FLOAT8(1);
   	int         ndims1 = ARR_NDIM(array1);
   	int        *dims1 = ARR_DIMS(array1);
     int         min_nitems = ArrayGetNItems(ndims1, dims1);
     float8        *result = (float8*) malloc(sizeof(float8)*min_nitems);
     TypeCacheEntry *typentry;
     int         typlen;
     bool        typbyval;
     char        typalign;
     char       *ptr1;
     bits8      *bitmap1;
     int         bitmask;
     Oid         element_type = ARR_ELEMTYPE(array1);
     int         i;
     float8 a;
 
 	memset(result, 0, sizeof(float8)*min_nitems);
    
     typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;
     if (typentry == NULL ||
         typentry->type_id != element_type)
     {
         typentry = lookup_type_cache(element_type,
                                      TYPECACHE_CMP_PROC_FINFO);
         if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
             ereport(ERROR,
                     (errcode(ERRCODE_UNDEFINED_FUNCTION),
                errmsg("could not identify a comparison function for type %s",
                       format_type_be(element_type))));
         fcinfo->flinfo->fn_extra = (void *) typentry;
     }
     typlen = typentry->typlen;
     typbyval = typentry->typbyval;
     typalign = typentry->typalign;
 
     ptr1 = ARR_DATA_PTR(array1);
     bitmap1 = ARR_NULLBITMAP(array1);
     bitmask = 1;                /* use same bitmask for both arrays */
 
     for (i = 0; i < min_nitems; i++)
     {
         Datum       elt1;
         if (bitmap1 && (*bitmap1 & bitmask) == 0)
         {
             result[i] = 0;
         }
         else
         {
             elt1 = fetch_att(ptr1, typbyval, typlen);
             ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
             ptr1 = (char *) att_align_nominal(ptr1, typalign);
             
             a = DatumGetFloat8(elt1);
             result[i] = a*scalar;
         }
 
         /* advance bitmap pointers if any */
         bitmask <<= 1;
         if (bitmask == 0x100)
         {
             if (bitmap1)
                 bitmap1++;
             bitmask = 1;
         }
      }
 

    
     PG_FREE_IF_COPY(array1, 0);
 
 	ArrayType *pgarray;
	 pgarray = construct_array((Datum *)result,
		min_nitems,FLOAT8OID,
		sizeof(float8),true,'d');
     PG_RETURN_ARRAYTYPE_P(pgarray);
 }
 
   Datum array_sqrt( PG_FUNCTION_ARGS);
 
  PG_FUNCTION_INFO_V1(array_sqrt);
Datum array_sqrt( PG_FUNCTION_ARGS)
{
  	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
   	int         ndims1 = ARR_NDIM(array1);
   	int        *dims1 = ARR_DIMS(array1);
     int         min_nitems = ArrayGetNItems(ndims1, dims1);
     float8        *result = (float8*) malloc(sizeof(float8)*min_nitems);
     TypeCacheEntry *typentry;
     int         typlen;
     bool        typbyval;
     char        typalign;
     char       *ptr1;
     bits8      *bitmap1;
     int         bitmask;
     Oid         element_type = ARR_ELEMTYPE(array1);
     int         i;
     float8 a;
 
 	memset(result, 0, sizeof(float8)*min_nitems);
    
     typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;
     if (typentry == NULL ||
         typentry->type_id != element_type)
     {
         typentry = lookup_type_cache(element_type,
                                      TYPECACHE_CMP_PROC_FINFO);
         if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
             ereport(ERROR,
                     (errcode(ERRCODE_UNDEFINED_FUNCTION),
                errmsg("could not identify a comparison function for type %s",
                       format_type_be(element_type))));
         fcinfo->flinfo->fn_extra = (void *) typentry;
     }
     typlen = typentry->typlen;
     typbyval = typentry->typbyval;
     typalign = typentry->typalign;
 
     ptr1 = ARR_DATA_PTR(array1);
     bitmap1 = ARR_NULLBITMAP(array1);
     bitmask = 1;                /* use same bitmask for both arrays */
 
     for (i = 0; i < min_nitems; i++)
     {
         Datum       elt1;
         if (bitmap1 && (*bitmap1 & bitmask) == 0)
         {
             result[i] = 0;
         }
         else
         {
             elt1 = fetch_att(ptr1, typbyval, typlen);
             ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
             ptr1 = (char *) att_align_nominal(ptr1, typalign);
             
             a = DatumGetFloat8(elt1);
             result[i] = sqrt(a);
         }
 
         /* advance bitmap pointers if any */
         bitmask <<= 1;
         if (bitmask == 0x100)
         {
             if (bitmap1)
                 bitmap1++;
             bitmask = 1;
         }
      }
 

    
     PG_FREE_IF_COPY(array1, 0);
 
 	ArrayType *pgarray;
	 pgarray = construct_array((Datum *)result,
		min_nitems,FLOAT8OID,
		sizeof(float8),true,'d');
     PG_RETURN_ARRAYTYPE_P(pgarray);
 }
 
 Datum array_limit( PG_FUNCTION_ARGS);
 
  PG_FUNCTION_INFO_V1(array_limit);
Datum array_limit( PG_FUNCTION_ARGS)
{
  	ArrayType  *array1 = PG_GETARG_ARRAYTYPE_P(0);
	float8 low = PG_GETARG_FLOAT8(1);
	float8 high = PG_GETARG_FLOAT8(2);
   	int         ndims1 = ARR_NDIM(array1);
   	int        *dims1 = ARR_DIMS(array1);
     int         min_nitems = ArrayGetNItems(ndims1, dims1);
     float8        *result = (float8*) malloc(sizeof(float8)*min_nitems);
     TypeCacheEntry *typentry;
     int         typlen;
     bool        typbyval;
     char        typalign;
     char       *ptr1;
     bits8      *bitmap1;
     int         bitmask;
     Oid         element_type = ARR_ELEMTYPE(array1);
     int         i;
     float8 a;
 
 	memset(result, 0, sizeof(float8)*min_nitems);
    
     typentry = (TypeCacheEntry *) fcinfo->flinfo->fn_extra;
     if (typentry == NULL ||
         typentry->type_id != element_type)
     {
         typentry = lookup_type_cache(element_type,
                                      TYPECACHE_CMP_PROC_FINFO);
         if (!OidIsValid(typentry->cmp_proc_finfo.fn_oid))
             ereport(ERROR,
                     (errcode(ERRCODE_UNDEFINED_FUNCTION),
                errmsg("could not identify a comparison function for type %s",
                       format_type_be(element_type))));
         fcinfo->flinfo->fn_extra = (void *) typentry;
     }
     typlen = typentry->typlen;
     typbyval = typentry->typbyval;
     typalign = typentry->typalign;
 
     ptr1 = ARR_DATA_PTR(array1);
     bitmap1 = ARR_NULLBITMAP(array1);
     bitmask = 1;                /* use same bitmask for both arrays */
 
     for (i = 0; i < min_nitems; i++)
     {
         Datum       elt1;
         if (bitmap1 && (*bitmap1 & bitmask) == 0)
         {
             result[i] = low;
         }
         else
         {
             elt1 = fetch_att(ptr1, typbyval, typlen);
             ptr1 = att_addlength_pointer(ptr1, typlen, ptr1);
             ptr1 = (char *) att_align_nominal(ptr1, typalign);
             
             a = DatumGetFloat8(elt1);
             result[i] = Min(Max(a, low),high);
         }
 
         /* advance bitmap pointers if any */
         bitmask <<= 1;
         if (bitmask == 0x100)
         {
             if (bitmap1)
                 bitmap1++;
             bitmask = 1;
         }
      }
 

    
     PG_FREE_IF_COPY(array1, 0);
 
 	ArrayType *pgarray;
	 pgarray = construct_array((Datum *)result,
		min_nitems,FLOAT8OID,
		sizeof(float8),true,'d');
     PG_RETURN_ARRAYTYPE_P(pgarray);
 }