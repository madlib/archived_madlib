#!/bin/sh

# Patch directly in the source tree. Ignore if the patch has already been
# applied. (Probably there are situations where CMake isn't smart enought to
# prevent that.)

# The patch makes Eigen use MADlib's memory-allocation routines
patch -N -p1 <<'EOF'
--- old/Eigen/src/Core/util/Memory.h	2012-11-05 13:22:49.000000000 -0800
+++ new/Eigen/src/Core/util/Memory.h	2013-03-04 16:37:46.000000000 -0800
@@ -196,7 +196,9 @@
   check_that_malloc_is_allowed();
 
   void *result;
-  #if !EIGEN_ALIGN
+  #if 1
+    result = madlib::defaultAllocator().allocate<madlib::dbal::FunctionContext, madlib::dbal::DoNotZero, madlib::dbal::ThrowBadAlloc>(size);
+  #elif !EIGEN_ALIGN
     result = std::malloc(size);
   #elif EIGEN_MALLOC_ALREADY_ALIGNED
     result = std::malloc(size);
@@ -219,7 +221,9 @@
 /** \internal Frees memory allocated with aligned_malloc. */
 inline void aligned_free(void *ptr)
 {
-  #if !EIGEN_ALIGN
+  #if 1
+    madlib::defaultAllocator().free<madlib::dbal::FunctionContext>(ptr);
+  #elif !EIGEN_ALIGN
     std::free(ptr);
   #elif EIGEN_MALLOC_ALREADY_ALIGNED
     std::free(ptr);
@@ -244,7 +248,9 @@
   EIGEN_UNUSED_VARIABLE(old_size);
 
   void *result;
-#if !EIGEN_ALIGN
+#if 1
+  result = madlib::defaultAllocator().reallocate<madlib::dbal::FunctionContext, madlib::dbal::DoNotZero, madlib::dbal::ThrowBadAlloc>(ptr, new_size);
+#elif !EIGEN_ALIGN
   result = std::realloc(ptr,new_size);
 #elif EIGEN_MALLOC_ALREADY_ALIGNED
   result = std::realloc(ptr,new_size);
@@ -287,7 +293,7 @@
 {
   check_that_malloc_is_allowed();
 
-  void *result = std::malloc(size);
+  void *result = madlib::defaultAllocator().allocate<madlib::dbal::FunctionContext, madlib::dbal::DoNotZero, madlib::dbal::ThrowBadAlloc>(size);
   if(!result && size)
     throw_std_bad_alloc();
   return result;
@@ -301,7 +307,7 @@
 
 template<> inline void conditional_aligned_free<false>(void *ptr)
 {
-  std::free(ptr);
+  madlib::defaultAllocator().free<madlib::dbal::FunctionContext>(ptr);
 }
 
 template<bool Align> inline void* conditional_aligned_realloc(void* ptr, size_t new_size, size_t old_size)
@@ -311,7 +317,7 @@
 
 template<> inline void* conditional_aligned_realloc<false>(void* ptr, size_t new_size, size_t)
 {
-  return std::realloc(ptr, new_size);
+  return madlib::defaultAllocator().reallocate<madlib::dbal::FunctionContext, madlib::dbal::DoNotZero, madlib::dbal::ThrowBadAlloc>(ptr, new_size);
 }
 
 /*****************************************************************************
EOF
